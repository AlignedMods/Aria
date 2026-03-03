#include "black_lua/internal/compiler/codegen/emitter.hpp"
#include "context.hpp"

#include <string>

namespace BlackLua::Internal {

    Emitter::Emitter(const ASTNodes* nodes, Context* ctx) {
        m_Nodes = nodes;
        m_Context = ctx;

        EmitImpl();
    }

    const CompilerReflectionData& Emitter::GetReflectionData() const {
        return m_ReflectionData;
    }

    const Emitter::OpCodes& Emitter::GetOpCodes() const {
        return m_OpCodes;
    }

    void Emitter::EmitImpl() {
        while (Peek()) {
            EmitNode(Consume());
        }

        EmitFunctions();
    }

    Node* Emitter::Peek() {
        if (m_Index < m_Nodes->size()) {
            return m_Nodes->at(m_Index);
        } else {
            return nullptr;
        }
    }

    Node* Emitter::Consume() {
        BLUA_ASSERT(m_Index < m_Nodes->size(), "Consume() of out bounds!");

        return m_Nodes->at(m_Index++);
    }

    StackSlotIndex Emitter::CompileToRuntimeStackSlot(CompileStackSlot slot) {
        if (slot.Relative) {
            if (m_CurrentStackFrame) {
                return StackSlotIndex(slot.Slot.Slot - static_cast<int32_t>(m_CurrentStackFrame->SlotCount) - 1, slot.Slot.Offset, slot.Slot.Size);
            } else {
                return StackSlotIndex(slot.Slot.Slot - m_SlotCount - 1, slot.Slot.Offset, slot.Slot.Size);
            }
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

    void Emitter::IncrementStackSlotCount() {
        (m_CurrentStackFrame) ? m_CurrentStackFrame->SlotCount++ : m_SlotCount++;
    }

    Emitter::CompileStackSlot Emitter::GetStackTop() {
        return CompileStackSlot((m_CurrentStackFrame) ? m_CurrentStackFrame->SlotCount : m_SlotCount, (m_CurrentStackFrame) ? true : false);
    }

    void Emitter::EmitDestructors(const std::vector<Declaration>& declarations) {
        for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
            auto& decl = *it;

            if (decl.Destruct) {
                if (decl.Type->Type == PrimitiveType::Array) {
                    m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(decl.Slot));
                    m_OpCodes.emplace_back(OpCodeType::CallExtern, "bl__array__destruct__");
                }
            }
        }
    }

    void Emitter::PushStackFrame() {
        m_OpCodes.emplace_back(OpCodeType::PushStackFrame);

        StackFrame* newStackFrame = m_Context->GetAllocator()->AllocateNamed<StackFrame>();
        newStackFrame->Parent = m_CurrentStackFrame;
        newStackFrame->SlotCount = (newStackFrame->Parent) ? newStackFrame->Parent->SlotCount : 0;
        m_CurrentStackFrame = newStackFrame;
    }

    void Emitter::PushCompilerStackFrame() {
        StackFrame* newStackFrame = m_Context->GetAllocator()->AllocateNamed<StackFrame>();
        newStackFrame->Parent = m_CurrentStackFrame;
        newStackFrame->SlotCount = (newStackFrame->Parent) ? newStackFrame->Parent->SlotCount : 0;
        m_CurrentStackFrame = newStackFrame;
    }

    void Emitter::PopStackFrame() {
        m_OpCodes.emplace_back(OpCodeType::PopStackFrame);
        m_CurrentStackFrame = m_CurrentStackFrame->Parent;
    }

    void Emitter::PopCompilerStackFrame() {
        m_CurrentStackFrame = m_CurrentStackFrame->Parent;
    }

    Emitter::CompileStackSlot Emitter::EmitNodeExpression(NodeExpr* expr) {
        if (ExprConstant* constant = GetNode<ExprConstant>(expr)) {
            IncrementStackSlotCount();

            if (ConstantBool* b = GetNode<ConstantBool>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(b->Value)));
            }

            if (ConstantChar* c = GetNode<ConstantChar>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(c->Char));
            }

            if (ConstantInt* i = GetNode<ConstantInt>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadI32, OpCodeLoad(i->Int));
            }

            if (ConstantLong* l = GetNode<ConstantLong>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadI64, OpCodeLoad(l->Long));
            }

            if (ConstantFloat* f = GetNode<ConstantFloat>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadF32, OpCodeLoad(f->Float));
            }

            if (ConstantDouble* d = GetNode<ConstantDouble>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadF64, OpCodeLoad(d->Double));
            }

            if (ConstantString* s = GetNode<ConstantString>(constant)) {
                m_OpCodes.emplace_back(OpCodeType::LoadStr, OpCodeLoad(s->String));
            }

            return GetStackTop();
        }

        if (ExprVarRef* ref = GetNode<ExprVarRef>(expr)) {
            std::string ident = fmt::format("{}", ref->Identifier);
        
            // Loop backward through all the scopes
            StackFrame* currentStackFrame = m_CurrentStackFrame;
            while (currentStackFrame) {
                if (currentStackFrame->DeclaredSymbolMap.contains(ident)) {
                    return currentStackFrame->DeclaredSymbols[currentStackFrame->DeclaredSymbolMap.at(ident)].Slot;
                }
        
                currentStackFrame = currentStackFrame->Parent;
            }
        
            // Check global symbols
            if (m_DeclaredSymbolMap.contains(fmt::format("{}", ref->Identifier))) {
                return m_DeclaredSymbols[m_DeclaredSymbolMap.at(ident)].Slot;
            }
        
            BLUA_ASSERT(false, "Unreachable!");
            return {};
        }

        if (ExprArrayAccess* arrAccess = GetNode<ExprArrayAccess>(expr)) {
            CompileStackSlot slot = EmitNodeExpression(arrAccess->Parent);
            CompileStackSlot index = EmitNodeExpression(arrAccess->Index);
        
            m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(slot));
            IncrementStackSlotCount();
            m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(index));
            IncrementStackSlotCount();
            PushBytes(arrAccess->ResolvedType->GetSize(), arrAccess->ResolvedType);
            IncrementStackSlotCount();
            m_OpCodes.emplace_back(OpCodeType::CallExtern, "bl__array__index__");
        
            return GetStackTop();
        }

        if (ExprSelf* self = GetNode<ExprSelf>(expr)) {
            return m_CurrentStackFrame->DeclaredSymbols[m_CurrentStackFrame->DeclaredSymbolMap.at("bL__internal__self_restrict__")].Slot;
        }

        if (ExprMember* mem = GetNode<ExprMember>(expr)) {
            CompileStackSlot slot = EmitNodeExpression(mem->Parent);
        
            TypeInfo* structType = mem->ResolvedParentType;
            for (const auto& field : std::get<StructDeclaration>(structType->Data).Fields) {
                if (field.Identifier == mem->Member) {
                    CompileStackSlot s = slot;
                    s.Slot.Offset += field.Offset;
                    s.Slot.Size = field.ResolvedType->GetSize();
                    return s;
                }
            }
        
            BLUA_ASSERT(false, "Unreachable!");
        
            return {};
        }

        if (ExprMethodCall* call = GetNode<ExprMethodCall>(expr)) {
            CompileStackSlot slot = EmitNodeExpression(call->Parent);
            TypeInfo* structType = call->ResolvedParentType;
            Declaration decl = m_DeclaredSymbols[m_DeclaredSymbolMap.at(fmt::format("{}::{}", std::get<StructDeclaration>(structType->Data).Identifier, call->Member))];
        
            std::vector<CompileStackSlot> parameters; 
        
            for (size_t i = 0; i < call->Arguments.Size; i++) {
                CompileStackSlot slot = EmitNodeExpression(GetNode<NodeExpr>(call->Arguments.Items[i]));
        
                parameters.push_back(slot);
            }

            m_OpCodes.emplace_back(OpCodeType::Ref, CompileToRuntimeStackSlot(slot), "self");
            IncrementStackSlotCount();
        
            for (const auto& p : parameters) {
                m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(p));
                IncrementStackSlotCount();
            }
        
            if (decl.Size != 0) {
                PushBytes(decl.Size, decl.Type, "return slot");
                IncrementStackSlotCount();
            }
        
            m_OpCodes.emplace_back(OpCodeType::Call, decl.Slot.Slot);
            
            return GetStackTop();
        }

        if (ExprCall* call = GetNode<ExprCall>(expr)) {
            if (call->Extern) {
                std::vector<CompileStackSlot> parameters; 
        
                for (size_t i = 0; i < call->Arguments.Size; i++) {
                    CompileStackSlot slot = EmitNodeExpression(GetNode<NodeExpr>(call->Arguments.Items[i]));
                    parameters.push_back(slot);
                }
        
                for (const auto& p : parameters) {
                    m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(p));
                    IncrementStackSlotCount();
                }
        
                if (call->ResolvedReturnType->Type != PrimitiveType::Void) {
                    PushBytes(call->ResolvedReturnType->GetSize(), call->ResolvedReturnType, "return slot");
                    IncrementStackSlotCount();
                }

                m_OpCodes.emplace_back(OpCodeType::CallExtern, fmt::format("{}", call->Name));
            } else {
                Declaration decl = m_DeclaredSymbols[m_DeclaredSymbolMap.at(fmt::format("{}", call->Name))];
        
                std::vector<CompileStackSlot> parameters; 
        
                for (size_t i = 0; i < call->Arguments.Size; i++) {
                    CompileStackSlot slot = EmitNodeExpression(GetNode<NodeExpr>(call->Arguments.Items[i]));
                    parameters.push_back(slot);
                }
        
                for (const auto& p : parameters) {
                    m_OpCodes.emplace_back(OpCodeType::Dup, CompileToRuntimeStackSlot(p));
                    IncrementStackSlotCount();
                }
        
                if (call->ResolvedReturnType->Type != PrimitiveType::Void) {
                    PushBytes(call->ResolvedReturnType->GetSize(), call->ResolvedReturnType, "return slot");
                    IncrementStackSlotCount();
                }

                m_OpCodes.emplace_back(OpCodeType::Call, decl.Slot.Slot);
            }
            
            return GetStackTop();
        }
        
        if (ExprParen* paren = GetNode<ExprParen>(expr)) {
            return EmitNodeExpression(paren->Expression);
        }
        
        if (ExprCast* cast = GetNode<ExprCast>(expr)) {
            CompileStackSlot slot = EmitNodeExpression(cast->Expression);
        
            #define CASE_CAST(name) m_OpCodes.emplace_back(OpCodeType::name, CompileToRuntimeStackSlot(slot)); break

            #define CASE_CAST_OUTER_INTEGRAL(src, srcName) \
                case PrimitiveType::src: {\
                    if (cast->ResolvedDstType->IsSigned()) { \
                        switch (cast->ResolvedDstType->Type) { \
                            case PrimitiveType::Bool: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Char: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Short: CASE_CAST(Cast##srcName##ToI16); break; \
                            case PrimitiveType::Int: CASE_CAST(Cast##srcName##ToI32); break; \
                            case PrimitiveType::Long: CASE_CAST(Cast##srcName##ToI64); break; \
                        } \
                    } else { \
                        switch (cast->ResolvedDstType->Type) { \
                            case PrimitiveType::Bool: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Char: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Short: CASE_CAST(Cast##srcName##ToI16); break; \
                            case PrimitiveType::Int: CASE_CAST(Cast##srcName##ToI32); break; \
                            case PrimitiveType::Long: CASE_CAST(Cast##srcName##ToI64); break; \
                        } \
                    } \
                    break; \
                }

            #define CASE_CAST_OUTER_FLOATING(src, srcName) \
                case PrimitiveType::src: {\
                    switch (cast->ResolvedDstType->Type) { \
                        case PrimitiveType::Float: CASE_CAST(Cast##srcName##ToF32); break; \
                        case PrimitiveType::Double: CASE_CAST(Cast##srcName##ToF64); break; \
                    } \
                    break; \
                }

            if (cast->ResolvedCastType == CastType::Integral) {
                if (cast->ResolvedSrcType->IsSigned()) {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_INTEGRAL(Bool, I8)
                        CASE_CAST_OUTER_INTEGRAL(Char, I8)
                        CASE_CAST_OUTER_INTEGRAL(Short, I16)
                        CASE_CAST_OUTER_INTEGRAL(Int, I32)
                        CASE_CAST_OUTER_INTEGRAL(Long, I64)
                    }
                } else {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_INTEGRAL(Bool, U8)
                        CASE_CAST_OUTER_INTEGRAL(Char, U8)
                        CASE_CAST_OUTER_INTEGRAL(Short, U16)
                        CASE_CAST_OUTER_INTEGRAL(Int, U32)
                        CASE_CAST_OUTER_INTEGRAL(Long, U64)
                    }
                }
            } else if (cast->ResolvedCastType == CastType::Floating) {
                switch (cast->ResolvedSrcType->Type) {
                    CASE_CAST_OUTER_FLOATING(Float, F32)
                    CASE_CAST_OUTER_FLOATING(Double, F64)
                }
            } else if (cast->ResolvedCastType == CastType::IntegralToFloating) {
                if (cast->ResolvedSrcType->IsSigned()) {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_FLOATING(Bool, I8)
                        CASE_CAST_OUTER_FLOATING(Char, I8)
                        CASE_CAST_OUTER_FLOATING(Short, I16)
                        CASE_CAST_OUTER_FLOATING(Int, I32)
                        CASE_CAST_OUTER_FLOATING(Long, I64)
                    }
                } else {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_FLOATING(Bool, U8)
                        CASE_CAST_OUTER_FLOATING(Char, U8)
                        CASE_CAST_OUTER_FLOATING(Short, U16)
                        CASE_CAST_OUTER_FLOATING(Int, U32)
                        CASE_CAST_OUTER_FLOATING(Long, U64)
                    }
                }
            } else if (cast->ResolvedCastType == CastType::FloatingToIntegral) {
                switch (cast->ResolvedSrcType->Type) {
                    CASE_CAST_OUTER_INTEGRAL(Float, F32)
                    CASE_CAST_OUTER_INTEGRAL(Double, F64)
                }
            } else if (cast->ResolvedCastType == CastType::LValueToRValue) {
                return GetStackTop();
            }

            #undef CASE_CAST
            #undef CASE_CAST_OUTER_INTEGRAL
            #undef CASE_CAST_OUTER_FLOATING
            
            IncrementStackSlotCount();
            return GetStackTop();
        }

        if (ExprImplicitCast* cast = GetNode<ExprImplicitCast>(expr)) {
            CompileStackSlot slot = EmitNodeExpression(cast->Expression);
        
            #define CASE_CAST(name) m_OpCodes.emplace_back(OpCodeType::name, CompileToRuntimeStackSlot(slot)); break

            #define CASE_CAST_OUTER_INTEGRAL(src, srcName) \
                case PrimitiveType::src: {\
                    if (cast->ResolvedDstType->IsSigned()) { \
                        switch (cast->ResolvedDstType->Type) { \
                            case PrimitiveType::Bool: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Char: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Short: CASE_CAST(Cast##srcName##ToI16); break; \
                            case PrimitiveType::Int: CASE_CAST(Cast##srcName##ToI32); break; \
                            case PrimitiveType::Long: CASE_CAST(Cast##srcName##ToI64); break; \
                        } \
                    } else { \
                        switch (cast->ResolvedDstType->Type) { \
                            case PrimitiveType::Bool: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Char: CASE_CAST(Cast##srcName##ToI8); break; \
                            case PrimitiveType::Short: CASE_CAST(Cast##srcName##ToI16); break; \
                            case PrimitiveType::Int: CASE_CAST(Cast##srcName##ToI32); break; \
                            case PrimitiveType::Long: CASE_CAST(Cast##srcName##ToI64); break; \
                        } \
                    } \
                    break; \
                }

            #define CASE_CAST_OUTER_FLOATING(src, srcName) \
                case PrimitiveType::src: {\
                    switch (cast->ResolvedDstType->Type) { \
                        case PrimitiveType::Float: CASE_CAST(Cast##srcName##ToF32); break; \
                        case PrimitiveType::Double: CASE_CAST(Cast##srcName##ToF64); break; \
                    } \
                    break; \
                }

            if (cast->ResolvedCastType == CastType::Integral) {
                if (cast->ResolvedSrcType->IsSigned()) {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_INTEGRAL(Bool, I8)
                        CASE_CAST_OUTER_INTEGRAL(Char, I8)
                        CASE_CAST_OUTER_INTEGRAL(Short, I16)
                        CASE_CAST_OUTER_INTEGRAL(Int, I32)
                        CASE_CAST_OUTER_INTEGRAL(Long, I64)
                    }
                } else {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_INTEGRAL(Bool, U8)
                        CASE_CAST_OUTER_INTEGRAL(Char, U8)
                        CASE_CAST_OUTER_INTEGRAL(Short, U16)
                        CASE_CAST_OUTER_INTEGRAL(Int, U32)
                        CASE_CAST_OUTER_INTEGRAL(Long, U64)
                    }
                }
            } else if (cast->ResolvedCastType == CastType::Floating) {
                switch (cast->ResolvedSrcType->Type) {
                    CASE_CAST_OUTER_FLOATING(Float, F32)
                    CASE_CAST_OUTER_FLOATING(Double, F64)
                }
            } else if (cast->ResolvedCastType == CastType::IntegralToFloating) {
                if (cast->ResolvedSrcType->IsSigned()) {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_FLOATING(Bool, I8)
                        CASE_CAST_OUTER_FLOATING(Char, I8)
                        CASE_CAST_OUTER_FLOATING(Short, I16)
                        CASE_CAST_OUTER_FLOATING(Int, I32)
                        CASE_CAST_OUTER_FLOATING(Long, I64)
                    }
                } else {
                    switch (cast->ResolvedSrcType->Type) {
                        CASE_CAST_OUTER_FLOATING(Bool, U8)
                        CASE_CAST_OUTER_FLOATING(Char, U8)
                        CASE_CAST_OUTER_FLOATING(Short, U16)
                        CASE_CAST_OUTER_FLOATING(Int, U32)
                        CASE_CAST_OUTER_FLOATING(Long, U64)
                    }
                }
            } else if (cast->ResolvedCastType == CastType::FloatingToIntegral) {
                switch (cast->ResolvedSrcType->Type) {
                    CASE_CAST_OUTER_INTEGRAL(Float, F32)
                    CASE_CAST_OUTER_INTEGRAL(Double, F64)
                }
            } else if (cast->ResolvedCastType == CastType::LValueToRValue) {
                return slot;
            }

            #undef CASE_CAST
            #undef CASE_CAST_OUTER_INTEGRAL
            #undef CASE_CAST_OUTER_FLOATING
            
            IncrementStackSlotCount();
            return GetStackTop();
        }
       
        if (ExprUnaryOperator* unOp = GetNode<ExprUnaryOperator>(expr)) {
            CompileStackSlot slot = EmitNodeExpression(unOp->Expression);
                
            #define MATH_OP(baseOp, type, _enum) \
                if (unOp->ResolvedType->Type == PrimitiveType::_enum) { \
                    m_OpCodes.emplace_back(OpCodeType::baseOp##type, CompileToRuntimeStackSlot(slot)); \
                    IncrementStackSlotCount(); \
                }
            
            #define MATH_OP_GROUP(unaryExpr, op) case UnaryOperatorType::unaryExpr: { \
                if (unOp->ResolvedType->IsSigned()) { \
                    MATH_OP(op, I8, Bool) \
                    MATH_OP(op, I8, Char) \
                    MATH_OP(op, I16, Short) \
                    MATH_OP(op, I32, Int) \
                    MATH_OP(op, I64, Long) \
                } else { \
                    MATH_OP(op, U8, Bool) \
                    MATH_OP(op, U8, Char) \
                    MATH_OP(op, U16, Short) \
                    MATH_OP(op, U32, Int) \
                    MATH_OP(op, U64, Long) \
                } \
                \
                MATH_OP(op, F32, Float) \
                MATH_OP(op, F64, Double) \
                break; \
            }
            
            switch (unOp->Type) {
                MATH_OP_GROUP(Negate, Negate)
            }
            
            #undef MATH_OP
            #undef MATH_OP_GROUP
            
            return GetStackTop();
        }
        
        if (ExprBinaryOperator* binOp = GetNode<ExprBinaryOperator>(expr)) {
            #define MATH_OP(baseOp, type, _enum) \
                if (binOp->ResolvedSourceType->Type == PrimitiveType::_enum) { \
                    CompileStackSlot lhs = EmitNodeExpression(binOp->LHS); \
                    CompileStackSlot rhs = EmitNodeExpression(binOp->RHS); \
                    m_OpCodes.emplace_back(OpCodeType::baseOp##type, OpCodeMath(CompileToRuntimeStackSlot(lhs), CompileToRuntimeStackSlot(rhs))); \
                    IncrementStackSlotCount(); \
                    \
                    if (binOp->Type == BinaryOperatorType::AddInPlace || binOp->Type == BinaryOperatorType::SubInPlace || \
                        binOp->Type == BinaryOperatorType::MulInPlace || binOp->Type == BinaryOperatorType::DivInPlace || binOp->Type == BinaryOperatorType::ModInPlace || \
                        binOp->Type == BinaryOperatorType::AndInPlace || binOp->Type == BinaryOperatorType::OrInPlace || binOp->Type == BinaryOperatorType::XorInPlace) { \
                        m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(lhs), -1)); \
                        return lhs; \
                    } \
                }
            
            #define MATH_OP_GROUP(binExpr, op) case BinaryOperatorType::binExpr: { \
                if (binOp->ResolvedSourceType->IsSigned()) { \
                    MATH_OP(op, I8, Bool) \
                    MATH_OP(op, I8, Char) \
                    MATH_OP(op, I16, Short) \
                    MATH_OP(op, I32, Int) \
                    MATH_OP(op, I64, Long) \
                } else { \
                    MATH_OP(op, U8, Bool) \
                    MATH_OP(op, U8, Char) \
                    MATH_OP(op, U16, Short) \
                    MATH_OP(op, U32, Int) \
                    MATH_OP(op, U64, Long) \
                } \
                \
                MATH_OP(op, F32, Float) \
                MATH_OP(op, F64, Double) \
                break; \
            }
            
            switch (binOp->Type) {
                MATH_OP_GROUP(Add, Add)
                MATH_OP_GROUP(AddInPlace, Add)
                MATH_OP_GROUP(Sub, Sub);
                MATH_OP_GROUP(SubInPlace, Sub);
                MATH_OP_GROUP(Mul, Mul);
                MATH_OP_GROUP(MulInPlace, Mul);
                MATH_OP_GROUP(Div, Div);
                MATH_OP_GROUP(DivInPlace, Div);
                MATH_OP_GROUP(Mod, Mod);
                MATH_OP_GROUP(ModInPlace, Mod);
            
                MATH_OP_GROUP(Less, Lt);
                MATH_OP_GROUP(LessOrEq, Lte);
                MATH_OP_GROUP(Greater, Gt);
                MATH_OP_GROUP(GreaterOrEq, Gte);
                MATH_OP_GROUP(IsEq, Cmp);
                MATH_OP_GROUP(IsNotEq, Ncmp);

                // The reason why && and || operators have to be so complex is because they have very specific rules:
                // if the lhs of && is false, the entire expression is automatically false, and the rhs should NOT be executed
                // meanwhile if the lhs of || is true, the entire expression is automatically true, and the rhs should also NOT be executed
                // This involves creates actual stack frames and labels for an operator that could be described as just "and i8 lhs rhs"/"or i8 lhs rhs"
                // However that naive approach is not what people would usually expect
                case BinaryOperatorType::BitAnd: {
                    CompileStackSlot lhs = EmitNodeExpression(binOp->LHS);

                    int32_t ifTrue = m_LabelCount;
                    m_LabelCount++;
                    int32_t ifFalse = m_LabelCount;
                    m_LabelCount++;
                    int32_t ifEnd = m_LabelCount;
                    m_LabelCount++;
                    
                    PushBytes(1, TypeInfo::Create(m_Context, PrimitiveType::Bool));
                    IncrementStackSlotCount();
                    CompileStackSlot result = GetStackTop();

                    PushStackFrame();
                    m_OpCodes.emplace_back(OpCodeType::Jf, OpCodeJump(CompileToRuntimeStackSlot(lhs), ifFalse));
                    m_OpCodes.emplace_back(OpCodeType::Jmp, ifTrue);
                
                    m_OpCodes.emplace_back(OpCodeType::Label, ifTrue);
                    CompileStackSlot rhs = EmitNodeExpression(binOp->RHS);
                    m_OpCodes.emplace_back(OpCodeType::AndI8, OpCodeMath(CompileToRuntimeStackSlot(lhs), CompileToRuntimeStackSlot(rhs)));
                    IncrementStackSlotCount();
                    m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(result), -1));
                    m_OpCodes.emplace_back(OpCodeType::Jmp, ifEnd);
                
                    m_OpCodes.emplace_back(OpCodeType::Label, ifFalse);
                    m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(false)));
                    IncrementStackSlotCount();
                    m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(result), -1));
                    m_OpCodes.emplace_back(OpCodeType::Jmp, ifEnd);
                
                    m_OpCodes.emplace_back(OpCodeType::Label, ifEnd);
                    PopStackFrame();
                
                    return result;
                }

                case BinaryOperatorType::BitOr: {
                    CompileStackSlot lhs = EmitNodeExpression(binOp->LHS);

                    int32_t ifTrue = m_LabelCount;
                    m_LabelCount++;
                    int32_t ifFalse = m_LabelCount;
                    m_LabelCount++;
                    int32_t ifEnd = m_LabelCount;
                    m_LabelCount++;
                    
                    PushBytes(1, TypeInfo::Create(m_Context, PrimitiveType::Bool));
                    IncrementStackSlotCount();
                    CompileStackSlot result = GetStackTop();

                    PushStackFrame();
                    m_OpCodes.emplace_back(OpCodeType::Jf, OpCodeJump(CompileToRuntimeStackSlot(lhs), ifFalse));
                    m_OpCodes.emplace_back(OpCodeType::Jmp, ifTrue);
                
                    m_OpCodes.emplace_back(OpCodeType::Label, ifTrue);
                    m_OpCodes.emplace_back(OpCodeType::LoadI8, OpCodeLoad(static_cast<i8>(true)));
                    IncrementStackSlotCount();
                    m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(result), -1));
                    m_OpCodes.emplace_back(OpCodeType::Jmp, ifEnd);
                
                    m_OpCodes.emplace_back(OpCodeType::Label, ifFalse);
                    CompileStackSlot rhs = EmitNodeExpression(binOp->RHS);
                    m_OpCodes.emplace_back(OpCodeType::OrI8, OpCodeMath(CompileToRuntimeStackSlot(lhs), CompileToRuntimeStackSlot(rhs)));
                    IncrementStackSlotCount();
                    m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(result), -1));
                    m_OpCodes.emplace_back(OpCodeType::Jmp, ifEnd);
                
                    m_OpCodes.emplace_back(OpCodeType::Label, ifEnd);
                    PopStackFrame();
                
                    return result;
                }
                
                case BinaryOperatorType::Eq: {
                    CompileStackSlot lhs = EmitNodeExpression(binOp->LHS);
                    CompileStackSlot rhs = EmitNodeExpression(binOp->RHS);

                    m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(lhs), CompileToRuntimeStackSlot(rhs)));

                    return lhs;
                }
            }
            
            #undef MATH_OP
            #undef MATH_OP_GROUP
            
            return GetStackTop();
        }
    }

    void Emitter::EmitNodeCompound(NodeStmt* stmt) {
        StmtCompound* compound = GetNode<StmtCompound>(stmt);

        for (size_t i = 0; i < compound->Nodes.Size; i++) {
            EmitNode(compound->Nodes.Items[i]);
        }
    }

    void Emitter::EmitNodeVarDecl(NodeStmt* stmt) {
        StmtVarDecl* decl = GetNode<StmtVarDecl>(stmt);
        
        // If the variable declaration has a value, we load the value inline
        if (decl->Value) {
            EmitNodeExpression(decl->Value);
        } else {
            PushBytes(decl->ResolvedType->GetSize(), decl->ResolvedType, fmt::format("Declaration of {}", decl->Identifier));
            IncrementStackSlotCount();
        }
        
        auto& map = (m_CurrentStackFrame != 0) ? m_CurrentStackFrame->DeclaredSymbolMap : m_DeclaredSymbolMap;
        auto& vector = (m_CurrentStackFrame != 0) ? m_CurrentStackFrame->DeclaredSymbols : m_DeclaredSymbols;
        
        Declaration d;
        if (m_CurrentStackFrame) {
            d.Slot.Slot = m_CurrentStackFrame->SlotCount;
            d.Slot.Relative = true;
        } else {
            d.Slot.Slot = m_SlotCount;
            d.Slot.Relative = false;

            CompilerReflectionDeclaration dd;
            dd.Type = ReflectionType::Variable;
            dd.Data = d.Slot.Slot.Slot;
            dd.ResolvedType = decl->ResolvedType;
            m_ReflectionData.Declarations[fmt::format("{}", decl->Identifier)] = dd;
        }
        d.Size = decl->ResolvedType->GetSize();
        d.Type = decl->ResolvedType;
        d.Destruct = true;
        d.Identifier = decl->Identifier;
        
        vector.push_back(d);
        map[fmt::format("{}", decl->Identifier)] = vector.size() - 1;
    }

    void Emitter::EmitNodeParamDecl(NodeStmt* stmt) {
        StmtParamDecl* decl = GetNode<StmtParamDecl>(stmt);
        
        PushBytes(decl->ResolvedType->GetSize(), decl->ResolvedType, fmt::format("Declaration of {}", decl->Identifier));
        auto& map = (m_CurrentStackFrame != 0) ? m_CurrentStackFrame->DeclaredSymbolMap : m_DeclaredSymbolMap;
        auto& vector = (m_CurrentStackFrame != 0) ? m_CurrentStackFrame->DeclaredSymbols : m_DeclaredSymbols;
        
        Declaration d;
        IncrementStackSlotCount();
        if (m_CurrentStackFrame) {
            d.Slot.Slot = m_CurrentStackFrame->SlotCount;
            d.Slot.Relative = true;
        } else {
            d.Slot.Slot = m_SlotCount;
            d.Slot.Relative = false; 
        }
        d.Size = decl->ResolvedType->GetSize();
        d.Type = decl->ResolvedType;
        d.Destruct = true;
        d.Identifier = decl->Identifier;
        
        vector.push_back(d);
        map[fmt::format("{}", decl->Identifier)] = vector.size() - 1;
    }

    void Emitter::EmitNodeFunctionDecl(NodeStmt* stmt) {
        StmtFunctionDecl* decl = GetNode<StmtFunctionDecl>(stmt);

        if (decl->Extern) { return; }

        std::string ident = fmt::format("{}", decl->Name);
        Declaration d;
        d.Slot.Slot = m_LabelCount++;
        d.Size = decl->ResolvedType->GetSize();
        d.Extern = decl->Extern;
        d.Type = decl->ResolvedType;
        d.Identifier = decl->Name;
        m_DeclaredSymbols.push_back(d);
        m_DeclaredSymbolMap[ident] = m_DeclaredSymbols.size() - 1;

        m_FunctionsToDeclare[m_LabelCount - 1] = stmt;
        m_ReflectionData.Declarations[ident] = { decl->ResolvedType, ReflectionType::Function, static_cast<int32_t>(m_LabelCount - 1) };
    }

    void Emitter::EmitNodeStructDecl(NodeStmt* stmt) {
        StmtStructDecl* decl = GetNode<StmtStructDecl>(stmt);
        
        for (size_t i = 0; i < decl->Fields.Size; i++) {
            if (StmtMethodDecl* mdecl = GetNode<StmtMethodDecl>(GetNode<NodeStmt>(decl->Fields.Items[i]))) {
                std::string ident = fmt::format("{}::{}", decl->Identifier, mdecl->Name);
                Declaration d;
                d.Slot.Slot = m_LabelCount++;
                d.Size = mdecl->ResolvedType->GetSize();
                d.Type = mdecl->ResolvedType;
                d.Identifier = mdecl->Name;
                m_DeclaredSymbols.push_back(d);
                m_DeclaredSymbolMap[ident] = m_DeclaredSymbols.size() - 1;

                m_FunctionsToDeclare[m_LabelCount - 1] = GetNode<NodeStmt>(decl->Fields.Items[i]);
                m_ReflectionData.Declarations[ident] = { mdecl->ResolvedType, ReflectionType::Function, static_cast<int32_t>(m_LabelCount - 1) };
            }
        }
    }

    void Emitter::EmitNodeWhile(NodeStmt* stmt) {
        StmtWhile* wh = GetNode<StmtWhile>(stmt);

        int32_t loop = m_LabelCount;
        m_LabelCount++;
        int32_t loopEnd = m_LabelCount;
        m_LabelCount++;
        m_OpCodes.emplace_back(OpCodeType::Jmp, loop, "while loop condition");
        m_OpCodes.emplace_back(OpCodeType::Label, loop, "while loop condition");

        PushStackFrame();

        CompileStackSlot slot = EmitNodeExpression(wh->Condition);
        m_OpCodes.emplace_back(OpCodeType::Jf, OpCodeJump(CompileToRuntimeStackSlot(slot), loopEnd), "while loop end");

        EmitNodeCompound(wh->Body);

        PopStackFrame();

        m_OpCodes.emplace_back(OpCodeType::Jmp, loop, "while loop condition");
        m_OpCodes.emplace_back(OpCodeType::Label, loopEnd, "while loop end");
        m_OpCodes.emplace_back(OpCodeType::PopStackFrame);
    }

    void Emitter::EmitNodeDoWhile(NodeStmt* stmt) {
        StmtDoWhile* dwh = GetNode<StmtDoWhile>(stmt);

        int32_t loop = m_LabelCount;
        m_LabelCount++;
        int32_t loopEnd = m_LabelCount;
        m_LabelCount++;
        m_OpCodes.emplace_back(OpCodeType::Jmp, loop, "do while loop start");
        m_OpCodes.emplace_back(OpCodeType::Label, loop, "do while loop start");

        PushStackFrame();

        EmitNodeCompound(dwh->Body);
        CompileStackSlot slot = EmitNodeExpression(dwh->Condition);

        m_OpCodes.emplace_back(OpCodeType::Jf, OpCodeJump(CompileToRuntimeStackSlot(slot), loopEnd), "do while loop end");

        PopStackFrame();

        m_OpCodes.emplace_back(OpCodeType::Jmp, loop, "do while loop start");
        m_OpCodes.emplace_back(OpCodeType::Label, loopEnd, "do while loop end");
        m_OpCodes.emplace_back(OpCodeType::PopStackFrame);
    }

    void Emitter::EmitNodeIf(NodeStmt* stmt) {
        StmtIf* nif = GetNode<StmtIf>(stmt);

        PushStackFrame();

        CompileStackSlot slot = EmitNodeExpression(nif->Condition);
        int32_t ifLabel = m_LabelCount;
        int32_t elseLabel = m_LabelCount + 1;
        int32_t afterIfLabel = (nif->ElseBody) ? m_LabelCount + 2 : m_LabelCount + 1;
        m_LabelCount += 2;

        m_OpCodes.emplace_back(OpCodeType::Jt, OpCodeJump(CompileToRuntimeStackSlot(slot), ifLabel));

        if (nif->ElseBody) {
            m_OpCodes.emplace_back(OpCodeType::Jf, OpCodeJump(CompileToRuntimeStackSlot(slot), elseLabel));
            m_LabelCount += 1;
        }

        m_OpCodes.emplace_back(OpCodeType::Jmp, afterIfLabel);

        m_OpCodes.emplace_back(OpCodeType::Label, ifLabel, "if");

        EmitNodeCompound(nif->Body);
        m_OpCodes.emplace_back(OpCodeType::Jmp, afterIfLabel);

        if (nif->ElseBody) {
            m_OpCodes.emplace_back(OpCodeType::Label, elseLabel, "else");

            EmitNodeCompound(nif->ElseBody);

            m_OpCodes.emplace_back(OpCodeType::Jmp, afterIfLabel);
        }

        m_OpCodes.emplace_back(OpCodeType::Label, afterIfLabel, "after if");
        PopStackFrame();
    }

    void Emitter::EmitNodeReturn(NodeStmt* stmt) {
        StmtReturn* ret = GetNode<StmtReturn>(stmt);

        CompileStackSlot slot = EmitNodeExpression(ret->Value);

        EmitDestructors(m_CurrentStackFrame->DeclaredSymbols); // On returning from a function we need to also call destructors

        m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(CompileToRuntimeStackSlot(CompileStackSlot(-(m_CurrentStackFrame->SlotCount + 1))), CompileToRuntimeStackSlot(slot)), "return");
        m_OpCodes.emplace_back(OpCodeType::Ret);
    }

    void Emitter::EmitNodeStatement(NodeStmt* stmt) {
        if (GetNode<StmtCompound>(stmt)) {
            PushStackFrame();
            EmitNodeCompound(stmt);
            PopStackFrame();
        } else if (GetNode<StmtVarDecl>(stmt)) {
            EmitNodeVarDecl(stmt);
        } else if (GetNode<StmtFunctionDecl>(stmt)) {
            EmitNodeFunctionDecl(stmt);
        } else if (GetNode<StmtStructDecl>(stmt)) {
            EmitNodeStructDecl(stmt);
        } else if (GetNode<StmtWhile>(stmt)) {
            EmitNodeWhile(stmt);
        } else if (GetNode<StmtDoWhile>(stmt)) {
            EmitNodeDoWhile(stmt);
        } else if (GetNode<StmtIf>(stmt)) {
            EmitNodeIf(stmt);
        } else if (GetNode<StmtReturn>(stmt)) {
            EmitNodeReturn(stmt);
        }
    }

    void Emitter::EmitFunctions() {
        for (const auto&[label, stmt] : m_FunctionsToDeclare) {
            if (StmtFunctionDecl* decl = GetNode<StmtFunctionDecl>(stmt)) {
                if (decl->Body) {
                    m_OpCodes.emplace_back(OpCodeType::Label, label);

                    PushCompilerStackFrame();
                    
                    size_t returnSlot = (decl->ResolvedType->Type == PrimitiveType::Void) ? 0 : 1;
                    
                    // Inject function parameters into the scope
                    for (size_t i = 0; i < decl->Parameters.Size; i++) {
                        EmitNodeParamDecl(GetNode<NodeStmt>(decl->Parameters.Items[i]));
                    
                        // Copy the arguments into the parameters
                        m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(-1, -static_cast<int32_t>(decl->Parameters.Size + 1 + returnSlot)));
                    }
                    
                    EmitNodeCompound(decl->Body);

                    if (m_OpCodes.back().Type != OpCodeType::Ret) {
                        EmitDestructors(m_CurrentStackFrame->DeclaredSymbols);
                        m_OpCodes.emplace_back(OpCodeType::Ret);
                    }
                    
                    PopCompilerStackFrame();
                }
            } else if (StmtMethodDecl* decl = GetNode<StmtMethodDecl>(stmt)) {
                m_OpCodes.emplace_back(OpCodeType::Label, label);

                PushCompilerStackFrame();
                
                size_t returnSlot = (decl->ResolvedType->Type == PrimitiveType::Void) ? 0 : 1;

                Declaration s;
                s.Extern = false;
                s.Slot.Slot = 1;
                s.Slot.Relative = true;
                s.Destruct = false;
                m_CurrentStackFrame->DeclaredSymbols.push_back(s);
                m_CurrentStackFrame->DeclaredSymbolMap["bL__internal__self_restrict__"] = m_CurrentStackFrame->DeclaredSymbols.size() - 1;
                m_OpCodes.emplace_back(OpCodeType::Ref, -static_cast<int32_t>(decl->Parameters.Size + 1 + returnSlot));
                IncrementStackSlotCount();

                // Inject function parameters into the scope
                for (size_t i = 0; i < decl->Parameters.Size; i++) {
                    EmitNodeParamDecl(GetNode<NodeStmt>(decl->Parameters.Items[i]));
                
                    // Copy the arguments into the parameters
                    m_OpCodes.emplace_back(OpCodeType::Copy, OpCodeCopy(-1, -static_cast<int32_t>(decl->Parameters.Size + 2 + returnSlot)));
                }
                
                EmitNodeCompound(decl->Body);

                if (m_OpCodes.back().Type != OpCodeType::Ret) {
                    EmitDestructors(m_CurrentStackFrame->DeclaredSymbols);
                    m_OpCodes.emplace_back(OpCodeType::Ret);
                }
                
                PopCompilerStackFrame();
            }
        }
    }

    void Emitter::EmitNode(Node* node) {
        if (NodeExpr* expr = GetNode<NodeExpr>(node)) {
            EmitNodeExpression(expr);
        } else if (NodeStmt* stmt = GetNode<NodeStmt>(node)) {
            EmitNodeStatement(stmt);
        }
    }

} // namespace BlackLua::Internal
