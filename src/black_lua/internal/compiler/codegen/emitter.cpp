#include "black_lua/internal/compiler/codegen/emitter.hpp"
#include "black_lua/internal/compiler/ast/ast.hpp"

namespace BlackLua::Internal {

    // Some code i got from cppreference (https://en.cppreference.com/w/cpp/utility/variant/visit)
    template<class... Ts>
    struct Overloads : Ts... { using Ts::operator()...; };

    Emitter::Emitter(CompilationContext* ctx) {
        m_Context = ctx;
        m_RootASTNode = ctx->GetRootASTNode();

        EmitImpl();
    }

    void Emitter::EmitImpl() {
        m_StackFrames.resize(1); // Implicit global stack frame
        
        EmitStmt(m_RootASTNode);
        EmitFunctions();

        m_Context->SetOpCodes(m_OpCodes);
    }

    Emitter::CompileStackSlot Emitter::EmitBooleanConstantExpr(Expr* expr) {
        BooleanConstantExpr* bc = GetNode<BooleanConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(bc->GetValue()), bc->GetResolvedType()));
        return GetStackTop();
    }

    Emitter::CompileStackSlot Emitter::EmitCharacterConstantExpr(Expr* expr) {
        CharacterConstantExpr* cc = GetNode<CharacterConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(cc->GetValue(), cc->GetResolvedType()));
        return GetStackTop();
    }

    Emitter::CompileStackSlot Emitter::EmitIntegerConstantExpr(Expr* expr) {
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
        return GetStackTop();
    }

    Emitter::CompileStackSlot Emitter::EmitFloatingConstantExpr(Expr* expr) {
        FloatingConstantExpr* fc = GetNode<FloatingConstantExpr>(expr);

        const auto visitor = Overloads
        {
            [this, fc](f32 f) { m_OpCodes.emplace_back(OpCodeType::LoadF32, OpCodeLoad(f, fc->GetResolvedType())); },
            [this, fc](f64 f) { m_OpCodes.emplace_back(OpCodeType::LoadF64, OpCodeLoad(f, fc->GetResolvedType())); },
        };

        std::visit(visitor, fc->GetValue());
        return GetStackTop();
    }

    Emitter::CompileStackSlot Emitter::EmitStringConstantExpr(Expr* expr) {
        StringConstantExpr* sc = GetNode<StringConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeType::LoadStr, OpCodeLoad(sc->GetValue(), sc->GetResolvedType()));
        return GetStackTop();
    }

    Emitter::CompileStackSlot Emitter::EmitExpr(Expr* expr) {
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
        }

        BLUA_UNREACHABLE();
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
            PushBytes(varDecl->GetResolvedType()->GetSize(), varDecl->GetResolvedType());
            IncrementStackSlotCount();
        }

        Declaration d;
        d.Slot = GetStackTop();
        d.Type = varDecl->GetResolvedType();

        m_StackFrames.back().DeclaredSymbols.push_back(d);
        m_StackFrames.back().DeclaredSymbolMap[varDecl->GetIdentifier()] = m_StackFrames.back().DeclaredSymbols.size() - 1;
    }
    
    void Emitter::EmitParamDecl(Decl* decl) {
        ParamDecl* paramDecl = GetNode<ParamDecl>(decl);

        PushBytes(paramDecl->GetResolvedType()->GetSize(), paramDecl->GetResolvedType());
        IncrementStackSlotCount();
    }

    void Emitter::EmitFunctionDecl(Decl* decl) {
        FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl);

        if (fnDecl->IsExtern()) { return; }

        Declaration d;
        d.Slot.Slot = m_LabelCount++;
        d.Type = fnDecl->GetResolvedType();

        m_StackFrames.back().DeclaredSymbols.push_back(d);
        m_StackFrames.back().DeclaredSymbolMap[fnDecl->GetIdentifier()] = m_StackFrames.back().DeclaredSymbols.size() - 1;
    }

    void Emitter::EmitDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            EmitTranslationUnitDecl(decl);
            return;
        } else if (GetNode<VarDecl>(decl)) {
            EmitVarDecl(decl);
            return;
        } else if (GetNode<ParamDecl>(decl)) {
            EmitParamDecl(decl);
            return;
        } else if (GetNode<FunctionDecl>(decl)) {
            EmitFunctionDecl(decl);
            return;
        }

        BLUA_UNREACHABLE();
    }

    void Emitter::EmitCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* stmt : compound->GetStmts()) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitWhileStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo: Emitter::EmitWhileStmt()"); }
    void Emitter::EmitDoWhileStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo: Emitter::EmitDoWhileStmt()"); }
    void Emitter::EmitForStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo: Emitter::EmitForStmt()"); }
    void Emitter::EmitIfStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo: Emitter::EmitIfStmt()"); }

    void Emitter::EmitStmt(Stmt* stmt) {
        if (GetNode<CompoundStmt>(stmt)) {
            EmitCompoundStmt(stmt);
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
        } else if (Expr* expr = GetNode<Expr>(stmt)) {
            EmitExpr(expr);
            return;
        } else if (Decl* decl = GetNode<Decl>(stmt)) {
            EmitDecl(decl);
            return;
        }

        BLUA_UNREACHABLE();
    }

    StackSlotIndex Emitter::CompileToRuntimeStackSlot(CompileStackSlot slot) {
        if (slot.Relative) {
            return StackSlotIndex(slot.Slot.Slot - static_cast<i32>(m_StackFrames.back().SlotCount) - 1, slot.Slot.Offset, slot.Slot.Size);
        } else {
            return slot.Slot;
        }
        
        BLUA_ASSERT(false, "Unreachable!");
    }

    int32_t Emitter::CreateLabel(const std::string& debugData) {
        m_OpCodes.emplace_back(OpCodeType::Label, static_cast<int32_t>(m_LabelCount), debugData);
        m_LabelCount++;
        return static_cast<int32_t>(m_LabelCount - 1);
    }

    void Emitter::PushBytes(size_t bytes, TypeInfo* type, const std::string& debugData) {
        m_OpCodes.emplace_back(OpCodeType::Push, OpCodePush(bytes, type), debugData);
    }

    bool Emitter::IsStackFrameGlobal() { return m_StackFrames.size() == 1; }

    void Emitter::IncrementStackSlotCount() {
        m_StackFrames.back().SlotCount++;
    }

    Emitter::CompileStackSlot Emitter::GetStackTop() {
        if (IsStackFrameGlobal()) {
            return {m_StackFrames.back().SlotCount, false};
        } else {
            return {m_StackFrames.back().SlotCount, true};
        }
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

    void Emitter::PushStackFrame() {
        m_OpCodes.emplace_back(OpCodeType::PushStackFrame);

        StackFrame sf;
        sf.SlotCount = m_StackFrames.back().SlotCount;

        m_StackFrames.push_back(sf);
    }

    void Emitter::PushCompilerStackFrame() {
        StackFrame sf;
        sf.SlotCount = m_StackFrames.back().SlotCount;

        m_StackFrames.push_back(sf);
    }

    void Emitter::PopStackFrame() {
        m_OpCodes.emplace_back(OpCodeType::PopStackFrame);
        m_StackFrames.pop_back();
    }

    void Emitter::PopCompilerStackFrame() {
        m_StackFrames.pop_back();
    }

    void Emitter::EmitFunctions() {
        for (const auto&[label, stmt] : m_FunctionsToDeclare) {
            if (FunctionDecl* decl = GetNode<FunctionDecl>(stmt)) {
                if (decl->GetBody()) {
                    m_OpCodes.emplace_back(OpCodeType::Label, label);

                    PushCompilerStackFrame();
                    
                    size_t returnSlot = (decl->GetResolvedType()->Type == PrimitiveType::Void) ? 0 : 1;
                    
                    for (ParamDecl* p : decl->GetParameters()) {
                        EmitParamDecl(p);
                        int32_t argSlot = -static_cast<int32_t>(decl->GetParameters().Size + 1 + returnSlot); // The slot where the argument got passed from
                        m_OpCodes.emplace_back(OpCodeType::Dup, argSlot);
                    }
                    
                    EmitCompoundStmt(decl->GetBody());

                    if (m_OpCodes.back().Type != OpCodeType::Ret) {
                        EmitDestructors(m_StackFrames.back().DeclaredSymbols);
                        m_OpCodes.emplace_back(OpCodeType::Ret);
                    }
                    
                    PopCompilerStackFrame();
                }
            }
        }
    }

} // namespace BlackLua::Internal
