#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;

        for (Stmt* stmt : tu.Stmts) {
            ResolveStmt(stmt);
        }
    }

    void SemanticAnalyzer::ResolveVarDecl(Decl* decl) {
        VarDecl& varDecl = decl->Var;
        std::string ident = fmt::format("{}", varDecl.Identifier);

        if (varDecl.Type) {
            ResolveType(decl->Loc, decl->Range, varDecl.Type);

            if (varDecl.Type->IsVoid()) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, "Cannot declare variable of 'void' type");
            }

            if (varDecl.Type->IsPointer() && !m_UnsafeContext) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, "Cannot declare variable of pointer type in a safe context");
            }
        }

        ResolveVarInitializer(decl);

        if (m_Scopes.size() > 0) {
            if (m_Scopes.back().Declarations.contains(ident)) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_Scopes.back().Declarations[ident] = { varDecl.Type, decl, DeclKind::Var };
        }
    }

    void SemanticAnalyzer::ResolveParamDecl(Decl* decl) {
        ParamDecl& paramDecl = decl->Param;
        ResolveType(decl->Loc, decl->Range, paramDecl.Type);
        m_Scopes.back().Declarations[fmt::format("{}", paramDecl.Identifier)] = { paramDecl.Type, decl, DeclKind::Param };
    }

    void SemanticAnalyzer::ResolveFunctionDecl(Decl* decl) {
        FunctionDecl fnDecl = decl->Function;

        for (auto& attr : decl->Attributes) {
            if (attr.Kind == DeclAttributeKind::Unsafe) {
                m_UnsafeContext = true;
            }
        }

        std::string ident = fmt::format("{}", fnDecl.Identifier);
        ResolveType(decl->Loc, decl->Range, fnDecl.Type);
        
        if (fnDecl.Body) {
            m_CanReachEndOfFunction = true;
            m_ActiveReturnType = std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType;
            PushScope();
            
            for (Decl* p : fnDecl.Parameters) {
                ResolveParamDecl(p);
            }
            
            if (fnDecl.Body) {
                ResolveBlockStmt(fnDecl.Body);
            }
            
            PopScope();
            m_ActiveReturnType = nullptr;

            if (m_CanReachEndOfFunction && !std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType->IsVoid()) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, "Control flow reaches end of function with a non void return type");
            }
        }

        m_UnsafeContext = false;
    }

    void SemanticAnalyzer::ResolveStructDecl(Decl* decl) {
        StructDecl& s = decl->Struct;
        std::string ident = fmt::format("{}", s.Identifier);

        StructDeclaration d;
        d.Identifier = s.Identifier;
        d.SourceDecl = decl;
        
        TypeInfo* structType = TypeInfo::Create(m_Context, PrimitiveType::Structure, false, d);
        std::vector<Decl*> methods;

        for (Decl* field : s.Fields) {
            if (field->Kind == DeclKind::Field) {
                ResolveType(field->Loc, field->Range, field->Field.Type);

                if (!TypeIsTrivial(field->Field.Type)) {
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
                        if (!TypeIsTrivial(field->Field.Type)) {
                            ARIA_TODO("Propagating constructors");
                        }
                    }
                }
                ResolveStmt(method->Constructor.Body);
            }
        }

        m_ActiveStruct = TypeInfo::Create(m_Context, structType->Type, true, structType->Data);
        
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

    void SemanticAnalyzer::ResolveDecl(Decl* decl) {
        if (decl->Kind == DeclKind::Error) { return; }
        else if (decl->Kind == DeclKind::TranslationUnit) {
            return ResolveTranslationUnitDecl(decl);
        } else if (decl->Kind == DeclKind::Module) {
            return;
        } else if (decl->Kind == DeclKind::Var) {
            return ResolveVarDecl(decl);
        } else if (decl->Kind == DeclKind::Param) {
            return ResolveParamDecl(decl);
        } else if (decl->Kind == DeclKind::Function) {
            return ResolveFunctionDecl(decl);
        } else if (decl->Kind == DeclKind::OverloadedFunction) {
            return;
        } else if (decl->Kind == DeclKind::Struct) {
            return ResolveStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    std::string SemanticAnalyzer::MangleFunction(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->Identifier);

        for (size_t i = 0; i < fn->Parameters.Size; i++) {
            ident += TypeInfoToString(fn->Parameters.Items[i]->Param.Type);

            if (i != fn->Parameters.Size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal