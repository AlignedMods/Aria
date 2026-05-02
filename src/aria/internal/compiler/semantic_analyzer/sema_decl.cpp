#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_TranslationUnit_decl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;

        for (Stmt* stmt : tu.Stmts) {
            resolve_stmt(stmt);
        }
    }

    void SemanticAnalyzer::resolve_Module_decl(Decl* decl) {}

    void SemanticAnalyzer::resolve_Var_decl(Decl* decl) {
        VarDecl& varDecl = decl->Var;
        std::string ident = fmt::format("{}", varDecl.Identifier);

        if (varDecl.Type) {
            resolve_type(decl->Loc, decl->Range, varDecl.Type);

            if (varDecl.Type->is_void()) {
                m_Context->report_compiler_diagnostic(decl->Loc, decl->Range, "Cannot declare variable of 'void' type");
            }
        }

        resolve_var_initializer(decl);

        if (m_Scopes.size() > 0) {
            if (m_Scopes.back().Declarations.contains(ident)) {
                m_Context->report_compiler_diagnostic(decl->Loc, decl->Range, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_Scopes.back().Declarations[ident] = { varDecl.Type, decl, DeclKind::Var };
        }
    }

    void SemanticAnalyzer::resolve_Param_decl(Decl* decl) {
        ParamDecl& paramDecl = decl->Param;
        resolve_type(decl->Loc, decl->Range, paramDecl.Type);
        m_Scopes.back().Declarations[fmt::format("{}", paramDecl.Identifier)] = { paramDecl.Type, decl, DeclKind::Param };
    }

    void SemanticAnalyzer::resolve_Function_decl(Decl* decl) {
        FunctionDecl fnDecl = decl->Function;

        for (auto& attr : fnDecl.Attributes) {
            if (attr.Kind == FunctionDecl::AttributeKind::Unsafe) {
                m_UnsafeContext = true;
            }
        }

        std::string ident = fmt::format("{}", fnDecl.Identifier);
        resolve_type(decl->Loc, decl->Range, fnDecl.Type);
        
        if (fnDecl.Body) {
            m_ActiveReturnType = fnDecl.Type->Function.ReturnType;
            push_scope();
            
            for (Decl* p : fnDecl.Parameters) {
                resolve_Param_decl(p);
            }
            
            if (fnDecl.Body) {
                resolve_Block_stmt(fnDecl.Body);
            }

            if (m_Scopes.back().ReachesEnd && !fnDecl.Type->Function.ReturnType->is_void()) {
                m_Context->report_compiler_diagnostic(decl->Loc, decl->Range, "Control flow reaches end of function with a non void return type");
            }

            pop_scope();
            m_ActiveReturnType = nullptr;
        }

        m_UnsafeContext = false;
    }

    void SemanticAnalyzer::resolve_OverloadedFunction_decl(Decl* decl) {}

    void SemanticAnalyzer::resolve_Struct_decl(Decl* decl) {
        StructDecl& s = decl->Struct;
        std::string ident = fmt::format("{}", s.Identifier);

        StructDeclaration d;
        d.Identifier = s.Identifier;
        d.SourceDecl = decl;
        
        TypeInfo* structType = TypeInfo::Create(m_Context, TypeKind::Structure, false);
        structType->Struct = d;
        std::vector<Decl*> methods;

        for (Decl* field : s.Fields) {
            if (field->Kind == DeclKind::Field) {
                resolve_type(field->Loc, field->Range, field->Field.Type);

                if (!type_is_trivial(field->Field.Type)) {
                    s.Definition.TrivialDtor = false;
                }
            } else if (field->Kind == DeclKind::Constructor) {
                methods.push_back(field);
            }
        }

        for (Decl* method : methods) {
            if (method->Kind == DeclKind::Constructor) {
                TinyVector<Stmt*> newBody;

                for (Decl* field : s.Fields) {
                    if (field->Kind == DeclKind::Field) {
                        if (!type_is_trivial(field->Field.Type)) {
                            ARIA_TODO("Propagating constructors");
                        }
                    }
                }
                resolve_stmt(method->Constructor.Body);
            }
        }

        m_ActiveStruct = TypeInfo::Dup(m_Context, structType);
        
        // for (MethodDecl* md : methods) {
        //     TypeInfo* returnType = GetTypeInfoFromString(md->GetParsedType());
        //     TinyVector<TypeInfo*> paramTypes;
        //     m_ActiveReturnType = returnType;
        //     
        // 
        //     m_Declarations.emplace_back();
        // 
        //     for (ParamDecl* p : md->GetParameters()) {
        //         HandleParamDecl(p);
        //         TypeInfo* pType = p->GetResolvedType();
        //         paramTypes.Append(m_Context, pType);
        //     }
        // 
        //     // We make the function visible to itself by declaring it before the body
        //     FunctionDeclaration fd;
        //     fd.ParamTypes = paramTypes;
        //     fd.ReturnType = returnType;
        //     
        //     TypeInfo* resolvedType = TypeInfo::Create(m_Context, PrimitiveType::Function, false, fd);
        //     md->SetResolvedType(resolvedType);
        // 
        //     std::string ident = md->GetIdentifier();
        //     m_Declarations.front()[ident] = { md->GetResolvedType(), decl, DeclRefKind::Function };
        // 
        //     if (md->GetBody()) {
        //         HandleCompoundStmt(md->GetBody());
        //     }
        // 
        //     m_Declarations.pop_back();
        //     m_ActiveReturnType = nullptr;
        // }
        
        m_ActiveStruct = nullptr;
    }

    void SemanticAnalyzer::resolve_Field_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_Constructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_Destructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_Method_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_BuiltinCopyConstructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_BuiltinDestructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::resolve_decl(Decl* decl) {
        #define DECL_CASE(kind) resolve_##kind##_decl(decl)
        #include "aria/internal/compiler/ast/decl_switch.hpp"
        #undef DECL_CASE
    }

    std::string SemanticAnalyzer::mangle_function(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->Identifier);

        for (size_t i = 0; i < fn->Parameters.Size; i++) {
            ident += type_info_to_string(fn->Parameters.Items[i]->Param.Type);

            if (i != fn->Parameters.Size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal