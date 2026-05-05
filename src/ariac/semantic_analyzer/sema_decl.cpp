#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_translation_unit_decl(Decl* decl) {
        TranslationUnitDecl tu = decl->translation_unit;

        for (Stmt* stmt : tu.stmts) {
            resolve_stmt(stmt);
        }
    }

    void SemanticAnalyzer::resolve_var_decl(Decl* decl) {
        VarDecl& varDecl = decl->var;
        std::string ident = fmt::format("{}", varDecl.identifier);

        if (varDecl.type) {
            resolve_type(decl->loc, decl->range, varDecl.type);

            if (varDecl.type->is_void()) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, "Cannot declare variable of 'void' type");
            }
        }

        resolve_var_initializer(decl);

        if (m_scopes.size() > 0) {
            if (m_scopes.back().declarations.contains(ident)) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_scopes.back().declarations[ident] = { varDecl.type, decl, DeclKind::Var };
        }
    }

    void SemanticAnalyzer::resolve_param_decl(Decl* decl) {
        ParamDecl& paramDecl = decl->param;
        resolve_type(decl->loc, decl->range, paramDecl.type);
        m_scopes.back().declarations[fmt::format("{}", paramDecl.identifier)] = { paramDecl.type, decl, DeclKind::Param };
    }

    void SemanticAnalyzer::resolve_function_decl(Decl* decl) {
        FunctionDecl fnDecl = decl->function;

        for (auto& attr : fnDecl.attributes) {
            if (attr.kind == FunctionDecl::AttributeKind::Unsafe) {
                m_unsafe_context = true;
            }
        }

        std::string ident = fmt::format("{}", fnDecl.identifier);
        resolve_type(decl->loc, decl->range, fnDecl.type);
        
        if (fnDecl.body) {
            m_active_return_type = fnDecl.type->function.return_type;
            push_scope();
            
            for (Decl* p : fnDecl.parameters) {
                resolve_param_decl(p);
            }
            
            if (fnDecl.body) {
                resolve_block_stmt(fnDecl.body);
            }

            if (m_scopes.back().reaches_end && !fnDecl.type->function.return_type->is_void()) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, "Control flow reaches end of function with a non void return type");
            }

            pop_scope();
            m_active_return_type = nullptr;
        }

        m_unsafe_context = false;
    }

    void SemanticAnalyzer::resolve_struct_decl(Decl* decl) {
        StructDecl& s = decl->struct_;
        std::string ident = fmt::format("{}", s.identifier);

        StructDeclaration d;
        d.identifier = s.identifier;
        d.source_decl = decl;
        
        TypeInfo* structType = TypeInfo::Create(m_context, TypeKind::Structure, false);
        structType->struct_ = d;
        std::vector<Decl*> methods;

        for (Decl* field : s.fields) {
            if (field->kind == DeclKind::Field) {
                resolve_type(field->loc, field->range, field->field.type);

                if (!type_is_trivial(field->field.type)) {
                    s.definition.trivial_dtor = false;
                }
            } else if (field->kind == DeclKind::Constructor) {
                methods.push_back(field);
            }
        }

        for (Decl* method : methods) {
            if (method->kind == DeclKind::Constructor) {
                TinyVector<Stmt*> newBody;

                for (Decl* field : s.fields) {
                    if (field->kind == DeclKind::Field) {
                        if (!type_is_trivial(field->field.type)) {
                            ARIA_TODO("Propagating constructors");
                        }
                    }
                }
                resolve_stmt(method->constructor.body);
            }
        }

        m_active_struct = TypeInfo::Dup(m_context, structType);
        
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
        
        m_active_struct = nullptr;
    }

    void SemanticAnalyzer::resolve_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Error:
            case DeclKind::Module:
            case DeclKind::OverloadedFunction:
            case DeclKind::Field:
            case DeclKind::Constructor:
            case DeclKind::Destructor:
            case DeclKind::Method:
            case DeclKind::BuiltinCopyConstructor:
            case DeclKind::BuiltinDestructor: return;

            case DeclKind::TranslationUnit: return resolve_translation_unit_decl(decl);
            case DeclKind::Var: return resolve_var_decl(decl);
            case DeclKind::Param: return resolve_param_decl(decl);
            case DeclKind::Function: return resolve_function_decl(decl);
            case DeclKind::Struct: return resolve_struct_decl(decl);

            default: ARIA_UNREACHABLE();
        }
    }

    std::string SemanticAnalyzer::mangle_function(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->identifier);

        for (size_t i = 0; i < fn->parameters.size; i++) {
            ident += type_info_to_string(fn->parameters.items[i]->param.type);

            if (i != fn->parameters.size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal