#include "aria/internal/compiler/codegen/emitter.hpp"
#include "aria/internal/compiler/ast/ast.hpp"

namespace Aria::Internal {

    // Some code i got from cppreference (https://en.cppreference.com/w/cpp/utility/variant/visit)
    template<class... Ts>
    struct Overloads : Ts... { using Ts::operator()...; };

    Emitter::Emitter(CompilationContext* ctx) {
        m_Context = ctx;
        m_RootASTNode = ctx->GetRootASTNode();

        EmitImpl();
    }

    void Emitter::EmitImpl() {
        m_OpCodes.emplace_back(OpCodeType::Function, "_start$()");
        m_OpCodes.emplace_back(OpCodeType::Label, "_entry$");
        m_OpCodes.emplace_back(OpCodeType::PushSF);
        PushStackFrame("_start$()");

        EmitStmt(m_RootASTNode);

        PopStackFrame();
        m_OpCodes.emplace_back(OpCodeType::Ret);

        EmitFunctions();

        m_Context->SetOpCodes(m_OpCodes);
        m_Context->SetReflectionData(m_ReflectionData);
    }

    void Emitter::EmitBooleanConstantExpr(Expr* expr, ExprValueType type) {
        BooleanConstantExpr* bc = GetNode<BooleanConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(bc->GetValue()), bc->GetResolvedType()));
    }

    void Emitter::EmitCharacterConstantExpr(Expr* expr, ExprValueType type) {
        CharacterConstantExpr* cc = GetNode<CharacterConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(cc->GetValue(), cc->GetResolvedType()));
    }

    void Emitter::EmitIntegerConstantExpr(Expr* expr,ExprValueType type) {
        IntegerConstantExpr* ic = GetNode<IntegerConstantExpr>(expr);

        const auto visitor = Overloads
        {
            [this, ic](i8 i)  { m_OpCodes.emplace_back(OpCodeType::LoadI8,  OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](i16 i) { m_OpCodes.emplace_back(OpCodeType::LoadI16, OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](i32 i) { m_OpCodes.emplace_back(OpCodeType::LoadI32, OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](i64 i) { m_OpCodes.emplace_back(OpCodeType::LoadI64, OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](u8 i)  { m_OpCodes.emplace_back(OpCodeType::LoadU8,  OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](u16 i) { m_OpCodes.emplace_back(OpCodeType::LoadU16, OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](u32 i) { m_OpCodes.emplace_back(OpCodeType::LoadU32, OpCodeLoad(i, ic->GetResolvedType())); },
            [this, ic](u64 i) { m_OpCodes.emplace_back(OpCodeType::LoadU64, OpCodeLoad(i, ic->GetResolvedType())); },
        };

        std::visit(visitor, ic->GetValue());
    }

    void Emitter::EmitFloatingConstantExpr(Expr* expr, ExprValueType type) {
        FloatingConstantExpr* fc = GetNode<FloatingConstantExpr>(expr);

        const auto visitor = Overloads
        {
            [this, fc](f32 f) { m_OpCodes.emplace_back(OpCodeType::LoadF32, OpCodeLoad(f, fc->GetResolvedType())); },
            [this, fc](f64 f) { m_OpCodes.emplace_back(OpCodeType::LoadF64, OpCodeLoad(f, fc->GetResolvedType())); },
        };

        std::visit(visitor, fc->GetValue());
    }

    void Emitter::EmitStringConstantExpr(Expr* expr, ExprValueType type) {
        StringConstantExpr* sc = GetNode<StringConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadStr, OpCodeLoad(sc->GetValue(), sc->GetResolvedType()));
    }

    void Emitter::EmitDeclRefExpr(Expr* expr, ExprValueType type) {
        DeclRefExpr* declRef = GetNode<DeclRefExpr>(expr);

        // This may look a bit messy but all it is doing is emitting the correct op code based off of what we need
        if (declRef->GetType() == DeclRefType::LocalVar) {
            for (size_t i = m_ActiveStackFrame.Scopes.size(); i > 0; i--) {
                Scope& s = m_ActiveStackFrame.Scopes[i - 1];
                if (s.DeclaredSymbolMap.contains(declRef->GetIdentifier())) {
                    Declaration& decl = s.DeclaredSymbols[s.DeclaredSymbolMap.at(declRef->GetIdentifier())];
                    
                    if (type == ExprValueType::LValue) {
                        if (expr->GetResolvedType()->IsReference()) {
                            m_OpCodes.emplace_back(OpCodeType::LoadLocal, std::get<size_t>(decl.Data));
                        } else {
                            m_OpCodes.emplace_back(OpCodeType::LoadPtrLocal, std::get<size_t>(decl.Data));
                        }
                    } else if (type == ExprValueType::RValue) {
                        m_OpCodes.emplace_back(OpCodeType::LoadLocal, std::get<size_t>(decl.Data));

                        if (expr->GetResolvedType()->IsReference()) {
                            expr->GetResolvedType()->Reference = false;
                            m_OpCodes.emplace_back(OpCodeType::Deref, TypeGetSize(expr->GetResolvedType()));
                            expr->GetResolvedType()->Reference = true;
                        }
                    }
                }
            }
        } else if (declRef->GetType() == DeclRefType::ParamVar) {
            if (type == ExprValueType::LValue) {
                if (expr->GetResolvedType()->IsReference()) {
                    m_OpCodes.emplace_back(OpCodeType::LoadArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));
                } else {
                    m_OpCodes.emplace_back(OpCodeType::LoadPtrArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));
                }
            } else if (type == ExprValueType::RValue) {
                m_OpCodes.emplace_back(OpCodeType::LoadArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));

                if (expr->GetResolvedType()->IsReference()) {
                    expr->GetResolvedType()->Reference = false;
                    m_OpCodes.emplace_back(OpCodeType::Deref, TypeGetSize(expr->GetResolvedType()));
                    expr->GetResolvedType()->Reference = true;
                }
            }
        } else if (declRef->GetType() == DeclRefType::GlobalVar) {
            if (type == ExprValueType::LValue) {
                if (expr->GetResolvedType()->IsReference()) {
                    m_OpCodes.emplace_back(OpCodeType::LoadGlobal, declRef->GetIdentifier());
                } else {
                    m_OpCodes.emplace_back(OpCodeType::LoadPtrGlobal, declRef->GetIdentifier());
                }
            } else if (type == ExprValueType::RValue) {
                m_OpCodes.emplace_back(OpCodeType::LoadGlobal, declRef->GetIdentifier());

                if (expr->GetResolvedType()->IsReference()) {
                    expr->GetResolvedType()->Reference = false;
                    m_OpCodes.emplace_back(OpCodeType::Deref, TypeGetSize(expr->GetResolvedType()));
                    expr->GetResolvedType()->Reference = true;
                }
            }

            IncrementStackSlotCount();
        } else if (declRef->GetType() == DeclRefType::Function) {
            m_OpCodes.emplace_back(OpCodeType::LoadFunc, fmt::format("{}()", declRef->GetRawIdentifier()));
            IncrementStackSlotCount();
        }
    }

    void Emitter::EmitMemberExpr(Expr* expr, ExprValueType type) {
        MemberExpr* mem = GetNode<MemberExpr>(expr);

        EmitExpr(mem->GetParent(), ExprValueType::LValue);
        StructDeclaration& sd = std::get<StructDeclaration>(mem->GetParentType()->Data);

        StructDecl* s = GetNode<StructDecl>(sd.SourceDecl);

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                if (fd->GetRawIdentifier() == mem->GetMember()) {
                    RuntimeStructDeclaration& sdecl = m_Structs.at(s->GetIdentifier());

                    if (type == ExprValueType::LValue) {
                        m_OpCodes.emplace_back(OpCodeType::LoadPtrOffset, OpCodeOffset(sdecl.FieldOffsets.at(fd->GetIdentifier()), sdecl.Size, fd->GetResolvedType()));
                    } else if (type == ExprValueType::RValue) {
                        m_OpCodes.emplace_back(OpCodeType::LoadOffset, OpCodeOffset(sdecl.FieldOffsets.at(fd->GetIdentifier()), sdecl.Size, fd->GetResolvedType()));
                    }
                }
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                if (md->GetRawIdentifier() == mem->GetMember()) {
                    m_OpCodes.emplace_back(OpCodeType::LoadFunc, md->GetIdentifier());
                    ARIA_ASSERT(false, "todo");
                }
            }  
        }
    }

    void Emitter::EmitCallExpr(Expr* expr, ExprValueType type) {
        CallExpr* call = GetNode<CallExpr>(expr);

        EmitExpr(call->GetCallee(), call->GetCallee()->GetValueType());

        for (size_t i = 0; i < call->GetArguments().Size; i++) {
            EmitExpr(call->GetArguments().Items[i], call->GetArguments().Items[i]->GetValueType());
            m_OpCodes.emplace_back(OpCodeType::DeclareArg, i);
        }

        size_t retCount = 0;
        TypeInfo* retType = call->GetResolvedType();
        if (retType->Type != PrimitiveType::Void) {
            m_OpCodes.emplace_back(OpCodeType::Alloca, OpCodeAlloca(TypeGetSize(retType), retType));
            IncrementStackSlotCount();
            retCount = 1;
        }

        m_OpCodes.emplace_back(OpCodeType::Call, OpCodeCall(call->GetArguments().Size));
    }

    void Emitter::EmitParenExpr(Expr* expr, ExprValueType type) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        EmitExpr(paren->GetChildExpr(), paren->GetValueType());
    }

    void Emitter::EmitImplicitCastExpr(Expr* expr, ExprValueType type) {
        ImplicitCastExpr* cast = GetNode<ImplicitCastExpr>(expr);

        #define CASE_CAST(dstResolvedType, dstVMType, srcVMType) \
            if (dstType->Type == PrimitiveType::dstResolvedType) { \
                m_OpCodes.emplace_back(OpCodeType::Cast##srcVMType##To##dstVMType, OpCodeCast(dstType)); \
            }

        #define CASE_CAST_TO_INTEGRAL(srcResolvedType, srcVMType) \
            if (srcType->Type == PrimitiveType::srcResolvedType) { \
                CASE_CAST(Char,   I8,  srcVMType) \
                CASE_CAST(UChar,  U8,  srcVMType) \
                CASE_CAST(Short,  I16, srcVMType) \
                CASE_CAST(UShort, U16, srcVMType) \
                CASE_CAST(Int,    I32, srcVMType) \
                CASE_CAST(UInt,   U32, srcVMType) \
                CASE_CAST(Long,   I64, srcVMType) \
                CASE_CAST(ULong,  U64, srcVMType) \
            }

        #define CASE_CAST_TO_FLOATING(srcResolvedType, srcVMType) \
            if (srcType->Type == PrimitiveType::srcResolvedType) { \
                CASE_CAST(Float,  F32, srcVMType) \
                CASE_CAST(Double, F64, srcVMType) \
            }
        
        if (cast->GetCastType() == CastType::LValueToRValue) {
            return EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);
        } else if (cast->GetCastType() == CastType::Integral) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_INTEGRAL(Char,   I8)
            CASE_CAST_TO_INTEGRAL(UChar,  U8)
            CASE_CAST_TO_INTEGRAL(Short,  I16)
            CASE_CAST_TO_INTEGRAL(UShort, U16)
            CASE_CAST_TO_INTEGRAL(Int,    I32)
            CASE_CAST_TO_INTEGRAL(UInt,   U32)
            CASE_CAST_TO_INTEGRAL(Long,   I64)
            CASE_CAST_TO_INTEGRAL(ULong,  U64)

            return;
        } else if (cast->GetCastType() == CastType::IntegralToFloating) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_FLOATING(Char,   I8)
            CASE_CAST_TO_FLOATING(UChar,  U8)
            CASE_CAST_TO_FLOATING(Short,  I16)
            CASE_CAST_TO_FLOATING(UShort, U16)
            CASE_CAST_TO_FLOATING(Int,    I32)
            CASE_CAST_TO_FLOATING(UInt,   U32)
            CASE_CAST_TO_FLOATING(Long,   I64)
            CASE_CAST_TO_FLOATING(ULong,  U64)

            return;
        } else if (cast->GetCastType() == CastType::Floating) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_FLOATING(Float,  F32)
            CASE_CAST_TO_FLOATING(Double, F64)

            return;
        } else if (cast->GetCastType() == CastType::FloatingToIntegral) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_INTEGRAL(Float,  F32)
            CASE_CAST_TO_INTEGRAL(Double, F64)

            return;
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCastExpr(Expr* expr, ExprValueType type) {
        CastExpr* cast = GetNode<CastExpr>(expr);

        if (cast->GetCastType() == CastType::LValueToRValue) {
            return EmitDeclRefExpr(cast->GetChildExpr(), ExprValueType::RValue);
        } else if (cast->GetCastType() == CastType::Integral) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_INTEGRAL(Char,   I8)
            CASE_CAST_TO_INTEGRAL(UChar,  U8)
            CASE_CAST_TO_INTEGRAL(Short,  I16)
            CASE_CAST_TO_INTEGRAL(UShort, U16)
            CASE_CAST_TO_INTEGRAL(Int,    I32)
            CASE_CAST_TO_INTEGRAL(UInt,   U32)
            CASE_CAST_TO_INTEGRAL(Long,   I64)
            CASE_CAST_TO_INTEGRAL(ULong,  U64)

            return;
        } else if (cast->GetCastType() == CastType::IntegralToFloating) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_FLOATING(Char,   I8)
            CASE_CAST_TO_FLOATING(UChar,  U8)
            CASE_CAST_TO_FLOATING(Short,  I16)
            CASE_CAST_TO_FLOATING(UShort, U16)
            CASE_CAST_TO_FLOATING(Int,    I32)
            CASE_CAST_TO_FLOATING(UInt,   U32)
            CASE_CAST_TO_FLOATING(Long,   I64)
            CASE_CAST_TO_FLOATING(ULong,  U64)

            return;
        } else if (cast->GetCastType() == CastType::Floating) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_FLOATING(Float,  F32)
            CASE_CAST_TO_FLOATING(Double, F64)

            return;
        } else if (cast->GetCastType() == CastType::FloatingToIntegral) {
            EmitExpr(cast->GetChildExpr(), ExprValueType::RValue);

            TypeInfo* dstType = cast->GetResolvedType();
            TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();

            CASE_CAST_TO_INTEGRAL(Float,  F32)
            CASE_CAST_TO_INTEGRAL(Double, F64)

            return;
        }

        #undef CASE_CAST_TO_INTEGRAL
        #undef CASE_CAST_TO_FLOATING

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitUnaryOperatorExpr(Expr* expr, ExprValueType type) {
        UnaryOperatorExpr* unop = GetNode<UnaryOperatorExpr>(expr);

        #define UNOP(baseOp, type, _enum) \
            if (unop->GetChildExpr()->GetResolvedType()->Type == PrimitiveType::_enum) { \
                EmitExpr(unop->GetChildExpr(), unop->GetChildExpr()->GetValueType()); \
                m_OpCodes.emplace_back(OpCodeType::baseOp##type); \
                IncrementStackSlotCount(); \
            }
            
        #define UNOP_GROUP(unExpr, op) case UnaryOperatorType::unExpr: { \
            UNOP(op, I8,  Char) \
            UNOP(op, I16, Short) \
            UNOP(op, I32, Int) \
            UNOP(op, I64, Long) \
            \
            UNOP(op, U8,  UChar) \
            UNOP(op, U16, UShort) \
            UNOP(op, U32, UInt) \
            UNOP(op, U64, ULong) \
            \
            UNOP(op, F32, Float) \
            UNOP(op, F64, Double) \
            break; \
        }

        switch (unop->GetUnaryOperator()) {
            UNOP_GROUP(Negate, Neg)
        }
    }

    void Emitter::EmitBinaryOperatorExpr(Expr* expr, ExprValueType type) {
        BinaryOperatorExpr* binop = GetNode<BinaryOperatorExpr>(expr);
      

        #define BINOP(baseOp, type, _enum) \
            if (binop->GetLHS()->GetResolvedType()->Type == PrimitiveType::_enum) { \
                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueType()); \
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueType()); \
                m_OpCodes.emplace_back(OpCodeType::baseOp##type, OpCodeMath(binop->GetResolvedType())); \
                IncrementStackSlotCount(); \
            }
            
        #define BINOP_GROUP(binExpr, op) case BinaryOperatorType::binExpr: { \
            BINOP(op, I8,  Bool) \
            \
            BINOP(op, I8,  Char) \
            BINOP(op, I16, Short) \
            BINOP(op, I32, Int) \
            BINOP(op, I64, Long) \
            \
            BINOP(op, U8,  UChar) \
            BINOP(op, U16, UShort) \
            BINOP(op, U32, UInt) \
            BINOP(op, U64, ULong) \
            \
            BINOP(op, F32, Float) \
            BINOP(op, F64, Double) \
            break; \
        }
        
        switch (binop->GetBinaryOperator()) {
            BINOP_GROUP(Add,         Add)
            BINOP_GROUP(Sub,         Sub)
            BINOP_GROUP(Mul,         Mul)
            BINOP_GROUP(Div,         Div)
            BINOP_GROUP(Mod,         Mod)
            BINOP_GROUP(Less,        Lt)
            BINOP_GROUP(LessOrEq,    Lte)
            BINOP_GROUP(Greater,     Gt)
            BINOP_GROUP(GreaterOrEq, Lte)
            BINOP_GROUP(IsEq,        Cmp)
            BINOP_GROUP(IsNotEq,     Ncmp)
        
            case BinaryOperatorType::BitAnd: {
                std::string andEnd = fmt::format("and.end_{}", m_AndCounter);
                m_AndCounter++;

                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueType());
                m_OpCodes.emplace_back(OpCodeType::Jf, andEnd);
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueType());
                m_OpCodes.emplace_back(OpCodeType::AndI8, OpCodeMath(binop->GetResolvedType()));
                m_OpCodes.emplace_back(OpCodeType::Jmp, andEnd);

                m_OpCodes.emplace_back(OpCodeType::Label, andEnd);

                break;
            }

            case BinaryOperatorType::BitOr: {
                std::string orEnd = fmt::format("or.end_{}", m_OrCounter);
                m_OrCounter++;

                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueType());
                m_OpCodes.emplace_back(OpCodeType::Jt, orEnd);
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueType());
                m_OpCodes.emplace_back(OpCodeType::OrI8, OpCodeMath(binop->GetResolvedType()));
                m_OpCodes.emplace_back(OpCodeType::Jmp, orEnd);

                m_OpCodes.emplace_back(OpCodeType::Label, orEnd);

                break;
            }

            case BinaryOperatorType::Eq: {
                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueType());
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueType());

                m_OpCodes.emplace_back(OpCodeType::Store);
                break;
            }
        }
    }

    void Emitter::EmitExpr(Expr* expr, ExprValueType type) {
        if (GetNode<BooleanConstantExpr>(expr)) {
            return EmitBooleanConstantExpr(expr, type);
        } else if (GetNode<CharacterConstantExpr>(expr)) {
            return EmitCharacterConstantExpr(expr, type);
        } else if (GetNode<IntegerConstantExpr>(expr)) {
            return EmitIntegerConstantExpr(expr, type);
        } else if (GetNode<FloatingConstantExpr>(expr)) {
            return EmitFloatingConstantExpr(expr, type);
        } else if (GetNode<StringConstantExpr>(expr)) {
            return EmitStringConstantExpr(expr, type);
        } else if (GetNode<DeclRefExpr>(expr)) {
            return EmitDeclRefExpr(expr, type);
        } else if (GetNode<MemberExpr>(expr)) {
            return EmitMemberExpr(expr, type);
        } else if (GetNode<CallExpr>(expr)) {
            return EmitCallExpr(expr, type);
        } else if (GetNode<ParenExpr>(expr)) {
            return EmitParenExpr(expr, type);
        } else if (GetNode<CastExpr>(expr)) {
            return EmitCastExpr(expr, type);
        } else if (GetNode<ImplicitCastExpr>(expr)) {
            return EmitImplicitCastExpr(expr, type);
        } else if (GetNode<UnaryOperatorExpr>(expr)) {
            return EmitUnaryOperatorExpr(expr, type);
        } else if (GetNode<BinaryOperatorExpr>(expr)) {
            return EmitBinaryOperatorExpr(expr, type);
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl* tu = GetNode<TranslationUnitDecl>(decl);

        for (Stmt* stmt : tu->GetStmts()) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitVarDecl(Decl* decl) {
        VarDecl* varDecl = GetNode<VarDecl>(decl);

        if (varDecl->GetDefaultValue()) {
            EmitExpr(varDecl->GetDefaultValue(), varDecl->GetDefaultValue()->GetValueType());
        } else {
            m_OpCodes.emplace_back(OpCodeType::Alloca, OpCodeAlloca(TypeGetSize(varDecl->GetResolvedType()), varDecl->GetResolvedType()));
            IncrementStackSlotCount();
        }

        Declaration d;
        d.Type = varDecl->GetResolvedType();

        if (IsGlobalScope()) {
            m_OpCodes.emplace_back(OpCodeType::DeclareGlobal, varDecl->GetIdentifier());

            m_GlobalScope.DeclaredSymbols.push_back(d);
            m_GlobalScope.DeclaredSymbolMap[varDecl->GetIdentifier()] = m_GlobalScope.DeclaredSymbols.size() - 1;
        } else {
            m_OpCodes.emplace_back(OpCodeType::DeclareLocal, m_ActiveStackFrame.LocalCount++);

            m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
            m_ActiveStackFrame.Scopes.back().DeclaredSymbolMap[varDecl->GetIdentifier()] = m_ActiveStackFrame.Scopes.back().DeclaredSymbols.size() - 1;
        }
    }
    
    void Emitter::EmitParamDecl(Decl* decl) {
        ParamDecl* param = GetNode<ParamDecl>(decl);
        m_ActiveStackFrame.Parameters[param->GetIdentifier()] = m_ActiveStackFrame.ParameterCount++;
    }

    void Emitter::EmitFunctionDecl(Decl* decl) {
        FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl);

        if (fnDecl->IsExtern()) { return; }

        m_FunctionsToDeclare[fmt::format("{}()", fnDecl->GetRawIdentifier())] = decl;
    }

    void Emitter::EmitStructDecl(Decl* decl) {
        StructDecl* s = GetNode<StructDecl>(decl);

        RuntimeStructDeclaration sd;

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                sd.FieldOffsets[fd->GetIdentifier()] = sd.Size;
                sd.Size += TypeGetSize(fd->GetResolvedType());
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                ARIA_ASSERT(false, "todo!");
            }
        }

        m_Structs[s->GetIdentifier()] = sd;
    }

    void Emitter::EmitDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            return EmitTranslationUnitDecl(decl);
        } else if (GetNode<VarDecl>(decl)) {
            return EmitVarDecl(decl);
        } else if (GetNode<FunctionDecl>(decl)) {
            return EmitFunctionDecl(decl);
        } else if (GetNode<StructDecl>(decl)) {
            return EmitStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* stmt : compound->GetStmts()) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitWhileStmt(Stmt* stmt) {
        WhileStmt* wh = GetNode<WhileStmt>(stmt);

        std::string loopStart = fmt::format("while.start_{}", m_WhileCounter);
        std::string loopEnd = fmt::format("while.end_{}", m_WhileCounter);
        m_WhileCounter++;

        m_OpCodes.emplace_back(OpCodeType::Label, loopStart);
        EmitExpr(wh->GetCondition(), wh->GetCondition()->GetValueType());
        m_OpCodes.emplace_back(OpCodeType::Jf, loopEnd);
        EmitStmt(wh->GetBody());

        m_OpCodes.emplace_back(OpCodeType::Jmp, loopStart);

        m_OpCodes.emplace_back(OpCodeType::Label, loopEnd);
    }

    void Emitter::EmitDoWhileStmt(Stmt* stmt) {
        DoWhileStmt* wh = GetNode<DoWhileStmt>(stmt);

        std::string loopStart = fmt::format("dowhile.start_{}", m_DoWhileCounter);
        std::string loopEnd = fmt::format("dowhile.end_{}", m_DoWhileCounter);
        m_DoWhileCounter++;

        m_OpCodes.emplace_back(OpCodeType::Label, loopStart);
        EmitStmt(wh->GetBody());

        EmitExpr(wh->GetCondition(), wh->GetCondition()->GetValueType());
        m_OpCodes.emplace_back(OpCodeType::Jt, loopStart);
    }

    void Emitter::EmitForStmt(Stmt* stmt) { ARIA_ASSERT(false, "todo: Emitter::EmitForStmt()"); }

    void Emitter::EmitIfStmt(Stmt* stmt) {
        IfStmt* ifs = GetNode<IfStmt>(stmt);

        EmitExpr(ifs->GetCondition(), ifs->GetCondition()->GetValueType());
        std::string ifBody = fmt::format("if.body_{}", m_IfCounter);
        std::string ifEnd = fmt::format("if.end_{}", m_IfCounter);

        m_OpCodes.emplace_back(OpCodeType::Jt, ifBody);
        m_OpCodes.emplace_back(OpCodeType::Jmp, ifEnd);

        m_OpCodes.emplace_back(OpCodeType::Label, ifBody);
        EmitStmt(ifs->GetBody());
        m_OpCodes.emplace_back(OpCodeType::Label, ifEnd);

        m_IfCounter++;
    }

    void Emitter::EmitReturnStmt(Stmt* stmt) {
        ReturnStmt* ret = GetNode<ReturnStmt>(stmt);
        if (ret->GetValue()) {
            m_OpCodes.emplace_back(OpCodeType::LoadPtrRet);
            EmitExpr(ret->GetValue(), ret->GetValue()->GetValueType());
            m_OpCodes.emplace_back(OpCodeType::Store);
        }
        
        m_OpCodes.emplace_back(OpCodeType::PopSF);
        m_OpCodes.emplace_back(OpCodeType::Ret);
    }

    void Emitter::EmitStmt(Stmt* stmt) {
        if (GetNode<CompoundStmt>(stmt)) {
            PushScope();
            EmitCompoundStmt(stmt);
            PopScope();
            return;
        } else if (GetNode<WhileStmt>(stmt)) {
            EmitWhileStmt(stmt);
            return;
        } else if (GetNode<DoWhileStmt>(stmt)) {
            EmitDoWhileStmt(stmt);
            return;
        } else if (GetNode<ForStmt>(stmt)) {
            EmitForStmt(stmt);
            return;
        } else if (GetNode<IfStmt>(stmt)) {
            EmitIfStmt(stmt);
            return;
        } else if (GetNode<ReturnStmt>(stmt)) {
            EmitReturnStmt(stmt);
            return;
        } else if (Expr* expr = GetNode<Expr>(stmt)) {
            EmitExpr(expr, expr->GetValueType());
            return;
        } else if (Decl* decl = GetNode<Decl>(stmt)) {
            EmitDecl(decl);
            return;
        }

        ARIA_UNREACHABLE();
    }

    bool Emitter::IsStartStackFrame() {
        return m_ActiveStackFrame.Name == "_start$()";
    }

    bool Emitter::IsGlobalScope() {
        return IsStartStackFrame() && m_ActiveStackFrame.Scopes.size() == 1;
    }

    void Emitter::IncrementStackSlotCount() {
        m_ActiveStackFrame.SlotCount++;
    }

    void Emitter::EmitDestructors(const std::vector<Declaration>& declarations) {
        for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
            auto& decl = *it;

            // if (decl.Destruct) {
            //     if (decl.Type->Type == PrimitiveType::Array) {
            //         m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(decl.Slot));
            //         m_OpCodes.emplace_back(OpCodeType::CallExtern, "bl__array__destruct__");
            //     }
            // }
        }
    }

    void Emitter::PushStackFrame(const std::string& name) {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Scopes.emplace_back();
        m_ActiveStackFrame.Name = name;
    }

    void Emitter::PopStackFrame() {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Name.clear();
        m_ActiveStackFrame.Parameters.clear();
        m_ActiveStackFrame.ParameterCount = 0;
        m_ActiveStackFrame.LocalCount = 0;

        // Reset counters
        m_AndCounter = 0;
        m_OrCounter = 0;
        m_WhileCounter = 0;
        m_DoWhileCounter = 0;
        m_IfCounter = 0;
    }

    void Emitter::PushScope() {
        m_ActiveStackFrame.Scopes.emplace_back();
    }

    void Emitter::PopScope() {
        m_ActiveStackFrame.Scopes.pop_back();
    }

    void Emitter::EmitFunctions() {
        for (const auto&[name, decl] : m_FunctionsToDeclare) {
            if (FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl)) {
                if (fnDecl->GetBody()) {
                    m_OpCodes.emplace_back(OpCodeType::Function, name);
                    m_OpCodes.emplace_back(OpCodeType::Label, "_entry$");

                    m_OpCodes.emplace_back(OpCodeType::PushSF);
                    PushStackFrame(name);
                    
                    size_t returnSlot = (fnDecl->GetResolvedType()->Type == PrimitiveType::Void) ? 0 : 1;
                    
                    for (ParamDecl* p : fnDecl->GetParameters()) {
                        EmitParamDecl(p);
                    }
                    
                    EmitCompoundStmt(fnDecl->GetBody());

                    if (m_OpCodes.back().Type != OpCodeType::Ret) {
                        m_OpCodes.emplace_back(OpCodeType::PopSF);
                        m_OpCodes.emplace_back(OpCodeType::Ret);
                    }

                    PopStackFrame();

                    CompilerReflectionDeclaration d;
                    d.ResolvedType = fnDecl->GetResolvedType();
                    d.ResolvedTypeSize = TypeGetSize(fnDecl->GetResolvedType());
                    d.Type = ReflectionType::Function;

                    m_ReflectionData.Declarations[name] = d;
                }
            }
        }
    }

    size_t Emitter::TypeGetSize(TypeInfo* t) {
        if (t->IsReference()) {
            return sizeof(void*); // NOTE: This may not always be safe, if we ever want to target different architectures this will need to be changed
        }

        switch (t->Type) {
            case PrimitiveType::Void:   return 0;

            case PrimitiveType::Bool:   return 1;

            case PrimitiveType::Char:   return 1;
            case PrimitiveType::UChar:  return 1;
            case PrimitiveType::Short:  return 2;
            case PrimitiveType::UShort: return 2;
            case PrimitiveType::Int:    return 4;
            case PrimitiveType::UInt:   return 4;
            case PrimitiveType::Long:   return 8;
            case PrimitiveType::ULong:  return 8;

            case PrimitiveType::Float:  return 4;
            case PrimitiveType::Double: return 8;

            case PrimitiveType::Structure: {
                StructDeclaration& sdecl = std::get<StructDeclaration>(t->Data);
                return m_Structs.at(fmt::format("{}", sdecl.Identifier)).Size;
            }

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal
