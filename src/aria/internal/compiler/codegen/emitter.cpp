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
        PushStackFrame("_start$()");

        EmitStmt(m_RootASTNode);

        m_ActiveStackFrame = {}; // We do not actually pop any stack frame, since the _start$() stack frame is essentially a global scope
        m_OpCodes.emplace_back(OpCodeType::Ret);

        EmitFunctions();

        m_Context->SetOpCodes(m_OpCodes);
    }

    void Emitter::EmitBooleanConstantExpr(Expr* expr) {
        BooleanConstantExpr* bc = GetNode<BooleanConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(bc->GetValue()), bc->GetResolvedType()));
        IncrementStackSlotCount();
    }

    void Emitter::EmitCharacterConstantExpr(Expr* expr) {
        CharacterConstantExpr* cc = GetNode<CharacterConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(cc->GetValue(), cc->GetResolvedType()));
        IncrementStackSlotCount();
    }

    void Emitter::EmitIntegerConstantExpr(Expr* expr) {
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
        IncrementStackSlotCount();
    }

    void Emitter::EmitFloatingConstantExpr(Expr* expr) {
        FloatingConstantExpr* fc = GetNode<FloatingConstantExpr>(expr);

        const auto visitor = Overloads
        {
            [this, fc](f32 f) { m_OpCodes.emplace_back(OpCodeType::LoadF32, OpCodeLoad(f, fc->GetResolvedType())); },
            [this, fc](f64 f) { m_OpCodes.emplace_back(OpCodeType::LoadF64, OpCodeLoad(f, fc->GetResolvedType())); },
        };

        std::visit(visitor, fc->GetValue());
        IncrementStackSlotCount();
    }

    void Emitter::EmitStringConstantExpr(Expr* expr) {
        StringConstantExpr* sc = GetNode<StringConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadStr, OpCodeLoad(sc->GetValue(), sc->GetResolvedType()));
        IncrementStackSlotCount();
    }

    void Emitter::EmitDeclRefExpr(Expr* expr, bool lvalue) {
        DeclRefExpr* declRef = GetNode<DeclRefExpr>(expr);

        if (declRef->GetType() == DeclRefType::LocalVar) {
            for (size_t i = m_ActiveStackFrame.Scopes.size(); i > 0; i--) {
                Scope& s = m_ActiveStackFrame.Scopes[i - 1];
                if (s.DeclaredSymbolMap.contains(declRef->GetIdentifier())) {
                    Declaration& decl = s.DeclaredSymbols[s.DeclaredSymbolMap.at(declRef->GetIdentifier())];
                    
                    if (lvalue) {
                        m_OpCodes.emplace_back(OpCodeType::LoadPtrLocal, std::get<size_t>(decl.Data));
                    } else {
                        m_OpCodes.emplace_back(OpCodeType::LoadLocal, std::get<size_t>(decl.Data));
                    }

                    IncrementStackSlotCount();
                }
            }
        } else if (declRef->GetType() == DeclRefType::ParamVar) {
            ARIA_ASSERT(lvalue == false, "Cannot get a ParamVar as an lvalue!");

            m_OpCodes.emplace_back(OpCodeType::LoadArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));
            IncrementStackSlotCount();
        } else if (declRef->GetType() == DeclRefType::GlobalVar) {
            if (lvalue) {
                m_OpCodes.emplace_back(OpCodeType::LoadPtrGlobal, declRef->GetIdentifier());
            } else {
                m_OpCodes.emplace_back(OpCodeType::LoadGlobal, declRef->GetIdentifier());
            }

            IncrementStackSlotCount();
        } else if (declRef->GetType() == DeclRefType::Function) {
            m_OpCodes.emplace_back(OpCodeType::LoadFunc, fmt::format("{}()", declRef->GetRawIdentifier()));
            IncrementStackSlotCount();
        }
    }

    void Emitter::EmitCallExpr(Expr* expr) {
        CallExpr* call = GetNode<CallExpr>(expr);

        size_t retCount = 0;
        TypeInfo* retType = call->GetResolvedType();
        if (retType->Type != PrimitiveType::Void) {
            m_OpCodes.emplace_back(OpCodeType::Alloca, OpCodeAlloca(retType->GetSize(), retType));
            IncrementStackSlotCount();
            retCount = 1;
        }

        EmitExpr(call->GetCallee());

        for (size_t i = call->GetArguments().Size; i > 0; i--) {
            EmitExpr(call->GetArguments().Items[i - 1]);
        }

        m_OpCodes.emplace_back(OpCodeType::Call, OpCodeCall(call->GetArguments().Size));
    }

    void Emitter::EmitParenExpr(Expr* expr) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        EmitExpr(paren->GetChildExpr());
    }

    void Emitter::EmitImplicitCastExpr(Expr* expr) {
        ImplicitCastExpr* cast = GetNode<ImplicitCastExpr>(expr);
        
        // The concept of an lvalue to rvalue cast is essentially to just load whatever value an lvalue holds
        // Here this is done via a dup
        if (cast->GetCastType() == CastType::LValueToRValue) {
            return EmitDeclRefExpr(cast->GetChildExpr(), false);
        }

        ARIA_ASSERT(false, "todo");
    }

    void Emitter::EmitUnaryOperatorExpr(Expr* expr) {
        UnaryOperatorExpr* unop = GetNode<UnaryOperatorExpr>(expr);

        #define UNOP(baseOp, type, _enum) \
            if (unop->GetChildExpr()->GetResolvedType()->Type == PrimitiveType::_enum) { \
                EmitExpr(unop->GetChildExpr()); \
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

    void Emitter::EmitBinaryOperatorExpr(Expr* expr) {
        BinaryOperatorExpr* binop = GetNode<BinaryOperatorExpr>(expr);
       
        #define BINOP(baseOp, type, _enum) \
            if (binop->GetLHS()->GetResolvedType()->Type == PrimitiveType::_enum) { \
                EmitExpr(binop->GetLHS()); \
                EmitExpr(binop->GetRHS()); \
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
        
            case BinaryOperatorType::Eq: {
                EmitExpr(binop->GetLHS());
                EmitExpr(binop->GetRHS());

                m_OpCodes.emplace_back(OpCodeType::Store);
            }
        }
    }

    void Emitter::EmitExpr(Expr* expr) {
        if (GetNode<BooleanConstantExpr>(expr)) {
            return EmitBooleanConstantExpr(expr);
        } else if (GetNode<CharacterConstantExpr>(expr)) {
            return EmitCharacterConstantExpr(expr);
        } else if (GetNode<IntegerConstantExpr>(expr)) {
            return EmitIntegerConstantExpr(expr);
        } else if (GetNode<FloatingConstantExpr>(expr)) {
            return EmitFloatingConstantExpr(expr);
        } else if (GetNode<StringConstantExpr>(expr)) {
            return EmitStringConstantExpr(expr);
        } else if (GetNode<DeclRefExpr>(expr)) {
            return EmitDeclRefExpr(expr, true);
        } else if (GetNode<CallExpr>(expr)) {
            return EmitCallExpr(expr);
        } else if (GetNode<ParenExpr>(expr)) {
            return EmitParenExpr(expr);
        } else if (GetNode<ImplicitCastExpr>(expr)) {
            return EmitImplicitCastExpr(expr);
        } else if (GetNode<UnaryOperatorExpr>(expr)) {
            return EmitUnaryOperatorExpr(expr);
        } else if (GetNode<BinaryOperatorExpr>(expr)) {
            return EmitBinaryOperatorExpr(expr);
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
            EmitExpr(varDecl->GetDefaultValue());
        } else {
            m_OpCodes.emplace_back(OpCodeType::Alloca, OpCodeAlloca(varDecl->GetResolvedType()->GetSize(), varDecl->GetResolvedType()));
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

    void Emitter::EmitDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            EmitTranslationUnitDecl(decl);
            return;
        } else if (GetNode<VarDecl>(decl)) {
            EmitVarDecl(decl);
            return;
        } else if (GetNode<FunctionDecl>(decl)) {
            EmitFunctionDecl(decl);
            return;
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* stmt : compound->GetStmts()) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitWhileStmt(Stmt* stmt) { ARIA_ASSERT(false, "todo: Emitter::EmitWhileStmt()"); }
    void Emitter::EmitDoWhileStmt(Stmt* stmt) { ARIA_ASSERT(false, "todo: Emitter::EmitDoWhileStmt()"); }
    void Emitter::EmitForStmt(Stmt* stmt) { ARIA_ASSERT(false, "todo: Emitter::EmitForStmt()"); }

    void Emitter::EmitIfStmt(Stmt* stmt) {
        IfStmt* ifs = GetNode<IfStmt>(stmt);

        EmitExpr(ifs->GetCondition());
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
            EmitExpr(ret->GetValue());
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
            EmitExpr(expr);
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
        m_OpCodes.emplace_back(OpCodeType::PushSF);
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Scopes.emplace_back();
        m_ActiveStackFrame.Name = name;
    }

    void Emitter::PopStackFrame() {
        m_OpCodes.emplace_back(OpCodeType::PopSF);
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Name.clear();
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

                    PushStackFrame(name);
                    
                    size_t returnSlot = (fnDecl->GetResolvedType()->Type == PrimitiveType::Void) ? 0 : 1;
                    
                    for (ParamDecl* p : fnDecl->GetParameters()) {
                        EmitParamDecl(p);
                    }
                    
                    EmitCompoundStmt(fnDecl->GetBody());

                    if (m_OpCodes.back().Type != OpCodeType::Ret) {
                        PopStackFrame();
                        m_OpCodes.emplace_back(OpCodeType::Ret);
                    }
                }
            }
        }
    }

} // namespace Aria::Internal
