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

    Emitter::CompileMemRef Emitter::EmitBooleanConstantExpr(Expr* expr) {
        BooleanConstantExpr* bc = GetNode<BooleanConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(bc->GetValue()), bc->GetResolvedType()));
        IncrementStackSlotCount();
        return GetStackTop(1);
    }

    Emitter::CompileMemRef Emitter::EmitCharacterConstantExpr(Expr* expr) {
        CharacterConstantExpr* cc = GetNode<CharacterConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(cc->GetValue(), cc->GetResolvedType()));
        IncrementStackSlotCount();
        return GetStackTop(1);
    }

    Emitter::CompileMemRef Emitter::EmitIntegerConstantExpr(Expr* expr) {
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
        return GetStackTop(ic->GetResolvedType()->GetSize());
    }

    Emitter::CompileMemRef Emitter::EmitFloatingConstantExpr(Expr* expr) {
        FloatingConstantExpr* fc = GetNode<FloatingConstantExpr>(expr);

        const auto visitor = Overloads
        {
            [this, fc](f32 f) { m_OpCodes.emplace_back(OpCodeType::LoadF32, OpCodeLoad(f, fc->GetResolvedType())); },
            [this, fc](f64 f) { m_OpCodes.emplace_back(OpCodeType::LoadF64, OpCodeLoad(f, fc->GetResolvedType())); },
        };

        std::visit(visitor, fc->GetValue());
        IncrementStackSlotCount();
        return GetStackTop(fc->GetResolvedType()->GetSize());
    }

    Emitter::CompileMemRef Emitter::EmitStringConstantExpr(Expr* expr) {
        StringConstantExpr* sc = GetNode<StringConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadStr, OpCodeLoad(sc->GetValue(), sc->GetResolvedType()));
        IncrementStackSlotCount();
        return GetStackTop(sc->GetResolvedType()->GetSize());
    }

    Emitter::CompileMemRef Emitter::EmitDeclRefExpr(Expr* expr) {
        DeclRefExpr* declRef = GetNode<DeclRefExpr>(expr);

        if (declRef->GetType() == DeclRefType::LocalVar) {
            for (size_t i = m_ActiveStackFrame.Scopes.size(); i > 0; i--) {
                Scope& s = m_ActiveStackFrame.Scopes[i - 1];
                if (s.DeclaredSymbolMap.contains(declRef->GetIdentifier())) {
                    Declaration& decl = s.DeclaredSymbols[s.DeclaredSymbolMap.at(declRef->GetIdentifier())];
                    return CompileMemRef(decl.Mem);
                }
            }
        } else if (declRef->GetType() == DeclRefType::GlobalVar) {
            return CompileMemRef(GlobalVarRef(declRef->GetIdentifier()));
        } else if (declRef->GetType() == DeclRefType::Function) {
            return CompileMemRef(FunctionRef(fmt::format("{}()", declRef->GetRawIdentifier())));
        }

        ARIA_UNREACHABLE();
    }

    Emitter::CompileMemRef Emitter::EmitCallExpr(Expr* expr) {
        CallExpr* call = GetNode<CallExpr>(expr);

        std::vector<CompileMemRef> args;
        for (Expr* arg : call->GetArguments()) {
            args.push_back(EmitExpr(arg));
        }

        for (auto& mem : args) {
            m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeMemRef(mem));
            IncrementStackSlotCount();
        }
        
        bool retCount = 0;
        TypeInfo* retType = call->GetResolvedType();
        if (retType->Type != PrimitiveType::Void) {
            m_OpCodes.emplace_back(OpCodeType::Alloca, OpCodeAlloca(retType->GetSize(), retType));
            IncrementStackSlotCount();
            retCount = 1;
        }

        CompileMemRef callee = EmitExpr(call->GetCallee());

        if (call->IsExtern()) {
            m_OpCodes.emplace_back(OpCodeType::CallExtern, OpCodeCall(CompileToRuntimeMemRef(callee), args.size(), retCount));
        } else {
            m_OpCodes.emplace_back(OpCodeType::Call, OpCodeCall(CompileToRuntimeMemRef(callee), args.size(), retCount));
        }
        return GetStackTop(retType->GetSize());
    }

    Emitter::CompileMemRef Emitter::EmitParenExpr(Expr* expr) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        return EmitExpr(paren->GetChildExpr());
    }

    Emitter::CompileMemRef Emitter::EmitImplicitCastExpr(Expr* expr) {
        ImplicitCastExpr* cast = GetNode<ImplicitCastExpr>(expr);
        CompileMemRef child = EmitExpr(cast->GetChildExpr());

        // The concept of an lvalue to rvalue cast is essentially to just load whatever value an lvalue holds
        // Here this is done via a dup
        if (cast->GetCastType() == CastType::LValueToRValue) {
            m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeMemRef(child));
            IncrementStackSlotCount();
            return GetStackTop(cast->GetResolvedType()->GetSize());
        }

        ARIA_ASSERT(false, "todo");
    }

    Emitter::CompileMemRef Emitter::EmitBinaryOperatorExpr(Expr* expr) {
        BinaryOperatorExpr* binop = GetNode<BinaryOperatorExpr>(expr);
       
        #define BINOP(baseOp, type, _enum) \
            if (binop->GetLHS()->GetResolvedType()->Type == PrimitiveType::_enum) { \
                auto LHS = EmitExpr(binop->GetLHS()); \
                auto RHS = EmitExpr(binop->GetRHS()); \
                m_OpCodes.emplace_back(OpCodeType::baseOp##type, OpCodeMath(CompileToRuntimeMemRef(LHS), CompileToRuntimeMemRef(RHS))); \
                IncrementStackSlotCount(); \
                return GetStackTop(binop->GetResolvedType()->GetSize()); \
            }
            
        #define BINOP_GROUP(binExpr, op) case BinaryOperatorType::binExpr: { \
            if (binop->GetLHS()->GetResolvedType()->IsSigned()) { \
                BINOP(op, I8, Bool) \
                BINOP(op, I8, Char) \
                BINOP(op, I16, Short) \
                BINOP(op, I32, Int) \
                BINOP(op, I64, Long) \
            } else { \
                BINOP(op, U8, Bool) \
                BINOP(op, U8, Char) \
                BINOP(op, U16, Short) \
                BINOP(op, U32, Int) \
                BINOP(op, U64, Long) \
            } \
            \
            BINOP(op, F32, Float) \
            BINOP(op, F64, Double) \
            break; \
        }

        switch (binop->GetBinaryOperator()) {
            BINOP_GROUP(Add, Add)
            BINOP_GROUP(Sub, Sub)
            BINOP_GROUP(Mul, Mul)
            BINOP_GROUP(Div, Div)
            BINOP_GROUP(Mod, Mod)

            case BinaryOperatorType::Eq: {
                auto LHS = EmitExpr(binop->GetLHS());
                auto RHS = EmitExpr(binop->GetRHS());

                m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeMemRef(LHS), CompileToRuntimeMemRef(RHS)));
                return LHS;
            }
        }

        ARIA_UNREACHABLE();
    }

    Emitter::CompileMemRef Emitter::EmitExpr(Expr* expr) {
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
            return EmitDeclRefExpr(expr);
        } else if (GetNode<CallExpr>(expr)) {
            return EmitCallExpr(expr);
        } else if (GetNode<ParenExpr>(expr)) {
            return EmitParenExpr(expr);
        } else if (GetNode<ImplicitCastExpr>(expr)) {
            return EmitImplicitCastExpr(expr);
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
        d.Mem = GetStackTop(varDecl->GetResolvedType()->GetSize());
        d.Type = varDecl->GetResolvedType();

        if (IsGlobalScope()) {
            m_OpCodes.emplace_back(OpCodeType::SetGlobal, OpCodeSetGlobal(varDecl->GetIdentifier()));

            m_GlobalScope.DeclaredSymbols.push_back(d);
            m_GlobalScope.DeclaredSymbolMap[varDecl->GetIdentifier()] = m_GlobalScope.DeclaredSymbols.size() - 1;
        } else {
            m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
            m_ActiveStackFrame.Scopes.back().DeclaredSymbolMap[varDecl->GetIdentifier()] = m_ActiveStackFrame.Scopes.back().DeclaredSymbols.size() - 1;
        }
    }
    
    void Emitter::EmitParamDecl(Decl* decl, const MemRef& mem) {
        ParamDecl* paramDecl = GetNode<ParamDecl>(decl);

        m_OpCodes.emplace_back(OpCodeType::Dup, mem);
        IncrementStackSlotCount();

        Declaration d;
        d.Mem = GetStackTop(paramDecl->GetResolvedType()->GetSize());
        d.Type = paramDecl->GetResolvedType();
        m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
        m_ActiveStackFrame.Scopes.back().DeclaredSymbolMap[paramDecl->GetIdentifier()] = m_ActiveStackFrame.Scopes.back().DeclaredSymbols.size() - 1;
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
    void Emitter::EmitIfStmt(Stmt* stmt) { ARIA_ASSERT(false, "todo: Emitter::EmitIfStmt()"); }

    void Emitter::EmitReturnStmt(Stmt* stmt) {
        ReturnStmt* ret = GetNode<ReturnStmt>(stmt);
        if (ret->GetValue()) {
            CompileMemRef val = EmitExpr(ret->GetValue());
            MemRef retMem = { StackSlotRef(-(m_ActiveStackFrame.SlotCount + 1), ret->GetValue()->GetResolvedType()->GetSize()) };

            m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(retMem, CompileToRuntimeMemRef(val)));
        }
        
        PopStackFrame();
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

    MemRef Emitter::CompileToRuntimeMemRef(CompileMemRef slot) {
        return slot.Mem;
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

    Emitter::CompileMemRef Emitter::GetStackTop(size_t size, size_t offset) {
        return CompileMemRef(StackSlotRef(m_ActiveStackFrame.SlotCount - 1, size, offset));
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
                        int32_t argSlot = -static_cast<int32_t>(fnDecl->GetParameters().Size + returnSlot); // The slot where the argument gets passed from
                        EmitParamDecl(p, { StackSlotRef(argSlot, p->GetResolvedType()->GetSize()) });
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
