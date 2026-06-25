#pragma once

#include "ariac/ast/stmt.hpp"
#include "ariac/core/htable.hpp"

#include <string_view>

namespace ariac {

    struct Module;
    struct CompilationUnit;

    enum class DeclKind {
        Invalid = 0,

        Error,
        TranslationUnit,
        Module,
        Var,
        Param,
        Function,
        Struct,
        Impl,
        Typedef,
        Enum,
        EnumField,
        Field,
        Method,
    };

    inline const char* decl_kind_to_string(DeclKind kind) {
        switch (kind) {
            case DeclKind::Error: return "Error";

            case DeclKind::Var: return "Var";
            case DeclKind::Param: return "Param";
            case DeclKind::Function: return "Function";
            case DeclKind::Struct: return "Struct";
            case DeclKind::Typedef: return "Typedef";
            case DeclKind::Enum: return "Enum";

            default: ARIA_UNREACHABLE();
        }
    }

    enum class DeclVisibility {
        Public,
        Private
    };

    inline const char* decl_visibility_to_string(DeclVisibility visibility) {
        switch (visibility) {
            case DeclVisibility::Public: return "public";
            case DeclVisibility::Private: return "private";

            default: ARIA_UNREACHABLE();
        }
    }

    enum class DeclAttributeKind {
        None,
        If
    };

    struct DeclAttribute {
        DeclAttribute(DeclAttributeKind kind)
            : kind(kind), arg(nullptr) {}

        DeclAttribute(DeclAttributeKind kind, Expr* arg)
            : kind(kind), arg(arg) {}

        DeclAttributeKind kind = DeclAttributeKind::None;
        Expr* arg = nullptr;
    };

    enum class ResolveStatus {
        NotStarted,
        InProgress,
        Done
    };

    enum class LinkageKind {
        None,
        Extern,
        Static
    };

    inline const char* linkage_kind_to_string(LinkageKind kind) {
        switch (kind) {
            case LinkageKind::None: return "";
            case LinkageKind::Extern: return "extern";
            case LinkageKind::Static: return "static";

            default: ARIA_UNREACHABLE();
        }
    }

    struct Expr;
    struct Stmt;

    struct ErrorDecl {
        ErrorDecl() = default;
    };

    // Represents an entire translation unit
    // This should always be the root node of the AST
    struct TranslationUnitDecl {
        TranslationUnitDecl(std::string_view name, TinyVector<Stmt*> stmts)
            : name(name), stmts(stmts) {}

        std::string_view name;
        TinyVector<Stmt*> stmts;
    };

    struct ModuleDecl {
        ModuleDecl(std::string_view name)
            : name(name) {}

        std::string_view name;
    };

    struct VarDecl {
        VarDecl(std::string_view identifier, TypeInfo* type, Expr* initializer, bool global, bool const_, LinkageKind lk)
            : identifier(identifier), type(type), initializer(initializer), global_var(global), const_var(const_), linkage_kind(lk) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        Expr* initializer = nullptr;
        bool global_var = false;
        bool const_var = false;
        LinkageKind linkage_kind = LinkageKind::None;
    };

    struct ParamDecl {
        ParamDecl(std::string_view identifier, TypeInfo* type)
            : identifier(identifier), type(type) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
    };

    struct FunctionDecl {
        FunctionDecl(std::string_view identifier, TypeInfo* type, TinyVector<Decl*> params, Stmt* body, LinkageKind linkage)
            : identifier(identifier), type(type), parameters(params), body(body), linkage_kind(linkage) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        TinyVector<Decl*> parameters;
        Stmt* body = nullptr;
        LinkageKind linkage_kind = LinkageKind::None;
    };

    struct StructDecl {
        StructDecl(std::string_view identifier, TinyVector<Decl*> fields)
            : identifier(identifier), fields(fields) {}

        std::string_view identifier;
        TinyVector<Decl*> fields;
        HTable<Decl*> field_lookup;
        TinyVector<Decl*> impls;
    };

    struct ImplDecl {
        ImplDecl(std::string_view identifier, TinyVector<Decl*> fields)
            : identifier(identifier), fields(fields) {}

        std::string_view identifier;
        TinyVector<Decl*> fields;
        HTable<Decl*> field_lookup;
        Decl* parent = nullptr;
    };

    struct TypedefDecl {
        TypedefDecl(TypeInfo* type, std::string_view identifier)
            : type(type), identifier(identifier) {}

        TypeInfo* type = nullptr;
        std::string_view identifier;
    };

    struct EnumDecl {
        EnumDecl(TinyVector<Decl*> fields, std::string_view identifier)
            : fields(fields), identifier(identifier) {}

        TinyVector<Decl*> fields;
        HTable<Decl*> field_lookup;
        std::string_view identifier;
    };

    struct EnumFieldDecl {
        EnumFieldDecl(std::string_view identifier)
            : identifier(identifier), value(nullptr) {}

        EnumFieldDecl(std::string_view identifier, Expr* value)
            : identifier(identifier), value(value) {}

        std::string_view identifier;
        Expr* value = nullptr;
        u64 resolved_value = 0;
    };

    struct FieldDecl {
        FieldDecl(std::string_view identifier, TypeInfo* type)
            : identifier(identifier), type(type) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
    };

    struct MethodDecl {
        MethodDecl(Decl* parent, std::string_view identifier, TypeInfo* type, TinyVector<Decl*> parameters, Stmt* body)
            : parent(parent), identifier(identifier), type(type), parameters(parameters), body(body) {}

        Decl* parent = nullptr;
        std::string_view identifier;
        TypeInfo* type = nullptr;
        TinyVector<Decl*> parameters;
        Stmt* body = nullptr;
    };

    struct Decl {
        template <typename T>
        static inline Decl* Create(CompilationContext* ctx, 
            SourceLoc loc,
            DeclKind kind, DeclVisibility visibility, 
            T t = ErrorDecl{}) { return ctx->allocate<Decl>(loc, kind, visibility, t); }

        DeclKind kind = DeclKind::Invalid;
        DeclVisibility visibility = DeclVisibility::Public;
        TinyVector<DeclAttribute> attributes;

        SourceLoc loc;

        ResolveStatus resolve_status = ResolveStatus::NotStarted;

        Module* parent_module = nullptr;
        CompilationUnit* parent_unit = nullptr;

        union {
            ErrorDecl error;
            TranslationUnitDecl translation_unit;
            ModuleDecl module;
            VarDecl var;
            ParamDecl param;
            FunctionDecl function;
            StructDecl struct_;
            ImplDecl impl;
            TypedefDecl typedef_;
            EnumDecl enum_;
            EnumFieldDecl enum_field;
            FieldDecl field;
            MethodDecl method;
        };

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ErrorDecl error)
            : loc(loc), kind(kind), visibility(visibility), error(error) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, TranslationUnitDecl translation_unit)
            : loc(loc), kind(kind), visibility(visibility), translation_unit(translation_unit) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ModuleDecl module)
            : loc(loc), kind(kind), visibility(visibility), module(module) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, VarDecl var)
            : loc(loc), kind(kind), visibility(visibility), var(var) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ParamDecl param)
            : loc(loc), kind(kind), visibility(visibility), param(param) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, FunctionDecl function)
            : loc(loc), kind(kind), visibility(visibility), function(function) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, StructDecl struc)
            : loc(loc), kind(kind), visibility(visibility), struct_(struc) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ImplDecl impl)
            : loc(loc), kind(kind), visibility(visibility), impl(impl) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, TypedefDecl typedef_)
            : loc(loc), kind(kind), visibility(visibility), typedef_(typedef_) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, EnumDecl enum_)
            : loc(loc), kind(kind), visibility(visibility), enum_(enum_) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, EnumFieldDecl field)
            : loc(loc), kind(kind), visibility(visibility), enum_field(field) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, FieldDecl field)
            : loc(loc), kind(kind), visibility(visibility), field(field) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, MethodDecl method)
            : loc(loc), kind(kind), visibility(visibility), method(method) {}
    };

    inline Decl error_decl = Decl(SourceLoc(), DeclKind::Error, DeclVisibility::Public, ErrorDecl());

} // namespace ariac
