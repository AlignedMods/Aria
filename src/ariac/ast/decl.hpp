#pragma once

#include "ariac/ast/stmt.hpp"

#include <string_view>

namespace Aria::Internal {

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
        OverloadedFunction,
        Struct,
        Field,
        Constructor,
        Destructor,
        Method,
        BuiltinCopyConstructor,
        BuiltinDestructor
    };

    inline const char* decl_kind_to_string(DeclKind kind) {
        switch (kind) {
            case DeclKind::Error: return "Error";

            case DeclKind::Var: return "Var";
            case DeclKind::Param: return "Param";
            case DeclKind::Function: return "Function";
            case DeclKind::OverloadedFunction: return "OverloadedFunction";
            case DeclKind::Struct: return "Struct";

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

    enum class ResolveStatus {
        NotStarted,
        InProgress,
        Done
    };

    enum class BuiltinKind {
        String
    };

    struct Expr;
    struct Stmt;

    struct ErrorDecl {
        ErrorDecl() = default;
    };

    // Represents an entire translation unit
    // This should always be the root node of the AST
    struct TranslationUnitDecl {
        TranslationUnitDecl(TinyVector<Stmt*> stmts)
            : stmts(stmts) {}

        TinyVector<Stmt*> stmts;
    };

    struct ModuleDecl {
        ModuleDecl(std::string_view name)
            : name(name) {}

        std::string_view name;
    };

    struct VarDecl {
        VarDecl(std::string_view identifier, TypeInfo* type, Expr* initializer, bool global)
            : identifier(identifier), type(type), initializer(initializer), global_var(global) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        Expr* initializer = nullptr;
        bool global_var = false;
    };

    struct ParamDecl {
        ParamDecl(std::string_view identifier, TypeInfo* type)
            : identifier(identifier), type(type) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
    };

    struct FunctionDecl {
        enum class AttributeKind {
            None = 0,
            Extern,
            NoMangle,
            Unsafe,
        };

        struct Attribute {
            AttributeKind kind = AttributeKind::None;
            std::string_view arg;
        };

        FunctionDecl(std::string_view identifier, TypeInfo* type, TinyVector<Decl*> params, Stmt* body, TinyVector<Attribute> attrs)
            : identifier(identifier), type(type), parameters(params), body(body), attributes(attrs) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        TinyVector<Decl*> parameters;
        Stmt* body = nullptr;
        TinyVector<Attribute> attributes;
    };

    struct OverloadedFunctionDecl {
        OverloadedFunctionDecl(std::string_view identifier)
            : identifier(identifier) {}

        std::string_view identifier;
        TinyVector<Decl*> funcs;
    };

    struct StructDecl {
        struct DefinitionData {
            bool has_default_ctor : 1;

            bool has_user_dtor : 1;
            bool trivial_dtor : 1;
        };

        StructDecl(std::string_view identifier,TinyVector<Decl*> fields)
            : identifier(identifier), definition{}, fields(fields) {}

        std::string_view identifier;
        DefinitionData definition;
        TinyVector<Decl*> fields;
    };

    struct FieldDecl {
        FieldDecl(std::string_view identifier, TypeInfo* type)
            : identifier(identifier), type(type) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
    };

    struct ConstructorDecl {
        ConstructorDecl(Decl* parent, TinyVector<Decl*> parameters, Stmt* body, bool disabled)
            : parent(parent), parameters(parameters), body(body), disabled(disabled) {}

        Decl* parent = nullptr;
        TinyVector<Decl*> parameters;
        Stmt* body = nullptr;
        bool disabled = false;
    };

    struct DestructorDecl {
        DestructorDecl(Decl* parent, Stmt* body)
            : parent(parent), body(body) {}

        Decl* parent = nullptr;
        Stmt* body = nullptr;
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

    struct BuiltinCopyConstructorDecl {
        BuiltinCopyConstructorDecl(BuiltinKind kind)
            : kind(kind) {}

        BuiltinKind kind = BuiltinKind::String;
    };

    struct BuiltinDestructorDecl {
        BuiltinDestructorDecl(BuiltinKind kind)
            : kind(kind) {}

        BuiltinKind kind = BuiltinKind::String;
    };

    struct Decl {
        template <typename T>
        static inline Decl* Create(CompilationContext* ctx, 
            SourceLocation loc, SourceRange range,
            DeclKind kind, DeclVisibility visibility, 
            T t = ErrorDecl{}) { return ctx->allocate<Decl>(loc, range, kind, visibility, t); }

        DeclKind kind = DeclKind::Invalid;
        DeclVisibility visibility = DeclVisibility::Public;

        SourceLocation loc;
        SourceRange range;

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
            OverloadedFunctionDecl overloaded_function;
            StructDecl struct_;
            FieldDecl field;
            ConstructorDecl constructor;
            DestructorDecl destructor;
            MethodDecl method;
            BuiltinCopyConstructorDecl built_in_copy_constructor;
            BuiltinDestructorDecl built_in_destructor;
        };

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, ErrorDecl error)
            : loc(loc), range(range), kind(kind), visibility(visibility), error(error) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, TranslationUnitDecl translation_unit)
            : loc(loc), range(range), kind(kind), visibility(visibility), translation_unit(translation_unit) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, ModuleDecl module)
            : loc(loc), range(range), kind(kind), visibility(visibility), module(module) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, VarDecl var)
            : loc(loc), range(range), kind(kind), visibility(visibility), var(var) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, ParamDecl param)
            : loc(loc), range(range), kind(kind), visibility(visibility), param(param) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, FunctionDecl function)
            : loc(loc), range(range), kind(kind), visibility(visibility), function(function) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, OverloadedFunctionDecl overloaded_function)
            : loc(loc), range(range), kind(kind), visibility(visibility), overloaded_function(overloaded_function) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, StructDecl struc)
            : loc(loc), range(range), kind(kind), visibility(visibility), struct_(struc) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, FieldDecl field)
            : loc(loc), range(range), kind(kind), visibility(visibility), field(field) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, ConstructorDecl ctor)
            : loc(loc), range(range), kind(kind), visibility(visibility), constructor(ctor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, DestructorDecl dtor)
            : loc(loc), range(range), kind(kind), visibility(visibility), destructor(dtor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, MethodDecl method)
            : loc(loc), range(range), kind(kind), visibility(visibility), method(method) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, BuiltinCopyConstructorDecl bicc)
            : loc(loc), range(range), kind(kind), visibility(visibility), built_in_copy_constructor(bicc) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DeclVisibility visibility, BuiltinDestructorDecl bid)
            : loc(loc), range(range), kind(kind), visibility(visibility), built_in_destructor(bid) {}
    };

    inline Decl error_decl = Decl(SourceLocation(), SourceRange(), DeclKind::Error, DeclVisibility::Public, ErrorDecl());

} // namespace Aria::Internal
