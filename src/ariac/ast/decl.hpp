#pragma once

#include "ariac/core/htable.hpp"
#include "ariac/core/vector.hpp"
#include "ariac/compilation_context.hpp"
#include "ariac/ast/generic.hpp"

#include <string_view>

namespace ariac {

    struct Module;
    struct CompilationUnit;

    struct Stmt;
    struct TypeInfo;

    enum class DeclKind {
        Invalid = 0,

        Error,
        Module,
        Import,
        Var,
        Param,
        Function,
        FunctionSpecilization,
        Struct,
        StructSpecilization,
        Impl,
        Typedef,
        Enum,
        EnumConstant,
        Field,
        Method,
        Generic,
        GenericParameter
    };

    inline const char* decl_kind_to_string(DeclKind kind) {
        switch (kind) {
            case DeclKind::Error: return "Error";

            case DeclKind::Var: return "Var";
            case DeclKind::Param: return "Param";
            case DeclKind::Function: return "Function";
            case DeclKind::FunctionSpecilization: return "FunctionSpecilization";
            case DeclKind::Struct: return "Struct";
            case DeclKind::Typedef: return "Typedef";
            case DeclKind::Enum: return "Enum";
            case DeclKind::EnumConstant: return "EnumConstant";
            case DeclKind::Field: return "Field";
            case DeclKind::Method: return "Method";
            case DeclKind::Generic: return "Generic";

            case DeclKind::GenericParameter: return "GenericParameter";

            default: ARIA_UNREACHABLE("Invalid decl kind");
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

            default: ARIA_UNREACHABLE("Invalid decl visibility");
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

            default: ARIA_UNREACHABLE("Invalid linkage kind");
        }
    }

    struct Expr;
    struct Decl;
    struct Stmt;

    struct ErrorDecl {
        ErrorDecl() = default;
    };

    struct ModuleDecl {
        ModuleDecl(std::string_view name)
            : name(name) {}

        std::string_view name;
    };

    struct ImportDecl {
        ImportDecl(std::string_view name, std::string_view alias)
            : name(name), alias(alias) {}

        ImportDecl(std::string_view name, Module* mod, bool implicit)
            : name(name), resolved_module(mod), implicit(implicit) {}

        std::string_view name;
        std::string_view alias;
        Module* resolved_module = nullptr;
        bool implicit = false;
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
        ParamDecl(std::string_view identifier, TypeInfo* type, bool variadic)
            : identifier(identifier), type(type), variadic(variadic) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        bool variadic;
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

    struct FunctionSpecilizationDecl {
        FunctionSpecilizationDecl(TinyVector<TypeInfo*> types, TypeInfo* t)
            : types(types), type(t) {}

        TinyVector<TypeInfo*> types;
        TypeInfo* type = nullptr;
        Decl* source = nullptr;
    };

    struct StructDecl {
        StructDecl(std::string_view identifier, TinyVector<Decl*> fields)
            : identifier(identifier), fields(fields) {}

        std::string_view identifier;
        TinyVector<Decl*> fields;
        HTable<Decl*> field_lookup;
        TinyVector<Decl*> impls;
        TypeInfo* type = nullptr;
    };

    struct StructSpecilizationDecl {
        StructSpecilizationDecl(TinyVector<TypeInfo*> types, Decl* source, TinyVector<Decl*> impls)
            : types(types), source(source), impls(impls) {}

        TinyVector<TypeInfo*> types;
        Decl* source = nullptr;
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

    struct EnumConstantDecl {
        EnumConstantDecl(std::string_view identifier)
            : identifier(identifier), value(nullptr) {}

        EnumConstantDecl(std::string_view identifier, Expr* value)
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

    struct GenericDecl {
        GenericDecl(TinyVector<Decl*> params, Decl* decl)
            : parameters(params), decl(decl) {}

        TinyVector<Decl*> parameters;
        Decl* decl = nullptr;
        TinyVector<Decl*> specilizations;
    };

    struct GenericParameterDecl {
        GenericParameterDecl(std::string_view ident, TinyVector<GenericRequirement> requirements, bool variadic)
            : identifier(ident), requirements(requirements), variadic(variadic) {}

        std::string_view identifier;
        TinyVector<GenericRequirement> requirements;
        bool variadic;
    };

    struct Decl {
        template <typename T>
        static inline Decl* Create(SourceLoc loc,
            DeclKind kind, DeclVisibility visibility, 
            T t = ErrorDecl{}) { return context.allocate<Decl>(loc, kind, visibility, t); }

        static Decl* dup(Decl* d);

        DeclKind kind = DeclKind::Invalid;
        DeclVisibility visibility = DeclVisibility::Public;
        TinyVector<DeclAttribute> attributes;

        SourceLoc loc;

        ResolveStatus resolve_status = ResolveStatus::NotStarted;

        Module* parent_module = nullptr;
        CompilationUnit* parent_unit = nullptr;

        union {
            ErrorDecl error;
            ModuleDecl module;
            ImportDecl import;
            VarDecl var;
            ParamDecl param;
            FunctionDecl function;
            FunctionSpecilizationDecl function_specilization;
            StructDecl struct_;
            StructSpecilizationDecl struct_specilization;
            ImplDecl impl;
            TypedefDecl typedef_;
            EnumDecl enum_;
            EnumConstantDecl enum_constant;
            FieldDecl field;
            MethodDecl method;
            GenericDecl generic;
            GenericParameterDecl generic_parameter;
        };

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ErrorDecl error)
            : loc(loc), kind(kind), visibility(visibility), error(error) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ModuleDecl module)
            : loc(loc), kind(kind), visibility(visibility), module(module) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ImportDecl import)
            : loc(loc), kind(kind), visibility(visibility), import(import) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, VarDecl var)
            : loc(loc), kind(kind), visibility(visibility), var(var) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ParamDecl param)
            : loc(loc), kind(kind), visibility(visibility), param(param) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, FunctionDecl function)
            : loc(loc), kind(kind), visibility(visibility), function(function) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, FunctionSpecilizationDecl function)
            : loc(loc), kind(kind), visibility(visibility), function_specilization(function) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, StructDecl struc)
            : loc(loc), kind(kind), visibility(visibility), struct_(struc) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, StructSpecilizationDecl struc)
            : loc(loc), kind(kind), visibility(visibility), struct_specilization(struc) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, ImplDecl impl)
            : loc(loc), kind(kind), visibility(visibility), impl(impl) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, TypedefDecl typedef_)
            : loc(loc), kind(kind), visibility(visibility), typedef_(typedef_) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, EnumDecl enum_)
            : loc(loc), kind(kind), visibility(visibility), enum_(enum_) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, EnumConstantDecl con)
            : loc(loc), kind(kind), visibility(visibility), enum_constant(con) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, FieldDecl field)
            : loc(loc), kind(kind), visibility(visibility), field(field) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, MethodDecl method)
            : loc(loc), kind(kind), visibility(visibility), method(method) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, GenericDecl generic)
            : loc(loc), kind(kind), visibility(visibility), generic(generic) {}

        Decl(SourceLoc loc, DeclKind kind, DeclVisibility visibility, GenericParameterDecl generic_p)
            : loc(loc), kind(kind), visibility(visibility), generic_parameter(generic_p) {}
    };

    inline Decl error_decl = Decl(SourceLoc(), DeclKind::Error, DeclVisibility::Public, ErrorDecl());

} // namespace ariac
