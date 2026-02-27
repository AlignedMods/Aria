#pragma once

#include "internal/compiler/variable_type.hpp"
#include "internal/compiler/core/string_builder.hpp"
#include "internal/compiler/ast/expr.hpp"

namespace BlackLua::Internal {
    
    enum class ConversionType {
        None,
        Promotion,
        Narrowing,
        SignChange
    };

    struct ConversionCost {
        ConversionType ConversionType = ConversionType::None;
        CastType CastType = CastType::Integral;

        bool CastNeeded = false;
        bool SignedMismatch = false;
        bool LValueMismatch = false;
        bool ImplicitCastPossible = false;
        bool ExplicitCastPossible = false;

        VariableType* Source = nullptr;
        VariableType* Destination = nullptr;
    };

    class TypeChecker {
    public:
        TypeChecker(Context* ctx);

        VariableType* GetVariableTypeFromString(StringView str);

        // type1 is the destination type and type2 is the source type
        ConversionCost GetConversionCost(VariableType* dst, VariableType* src);
        void InsertImplicitCast(VariableType* type1, VariableType* type2, NodeExpr* expr1, NodeExpr* expr2);

        VariableType* RequireRValue(VariableType* type, NodeExpr* expr); // Inserts an implicit cast for lvalue to rvalue conversion (returns the new type)
        VariableType* RequireLValue(VariableType* type, NodeExpr* expr); // This will not insert any implicit cast, just issues an error (returns the same type)

    private:
        template <typename T>
        T* Allocate() {
            return m_Context->GetAllocator()->AllocateNamed<T>();
        }

        template <typename T, typename... Args>
        T* Allocate(Args&&... args) {
            return m_Context->GetAllocator()->AllocateNamed<T>(std::forward<Args>(args)...);
        }

    private:
        Context* m_Context = nullptr;
    };

} // namespace BlackLua::Internal