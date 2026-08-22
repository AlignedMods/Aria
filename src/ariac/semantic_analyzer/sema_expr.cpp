#include "ariac/semantic_analyzer/semantic_analyzer.hpp"
#include "ariac/core/scratch_buffer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_boolean_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_character_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_integer_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_floating_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_string_literal_expr(Expr* expr) {
        if (!expr->type) { expr->type = TypeInfo::get_string(); }

        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_null_expr(Expr* expr) {
        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_decl_ref_expr(Expr* expr) {
        DeclRefExpr& dr = expr->decl_ref;

        std::string pretty_ident = dr.name_specifier ? fmt::format("{}::{}", dr.name_specifier->name.identifier, dr.identifier) : fmt::format("{}", dr.identifier);

        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }

        if (dr.provides_generic_args) {
            for (TypeInfo* t : dr.generic_arguments) {
                resolve_type(t);
            } 
        }

        auto check_visibility = [&](Decl* sym, std::string_view decl_kind) {
            if (sym->visibility == DeclVisibility::Private && sym->parent_module != context.active_comp_unit->parent) {
                report_error(expr->loc, fmt::format("{} '{}' is private and cannot be accessed", decl_kind, pretty_ident));
                report_note(sym->loc, "Declared here");
            }  
        };

        auto resolve_symbol = [&](Decl* sym) {
            switch (sym->kind) {
                case DeclKind::Var: {
                    if (sym->var.linkage_kind == LinkageKind::Static) {
                        report_diag(expr->loc, fmt::format("{} has static linkage and cannot be accessed", pretty_ident));
                        report_diag(sym->loc, "Defined here", CompilerDiagKind::Note);
                    }

                    CompilationUnit* c = context.active_comp_unit;
                    context.active_comp_unit = sym->parent_unit;
                    resolve_var_decl(sym);
                    context.active_comp_unit = c;

                    check_visibility(sym, "variable");

                    expr->type = sym->var.type;
                    return;
                }

                case DeclKind::Param: {
                    if (sym->param.variadic) {
                        expr->type = TypeInfo::create_slice(sym->param.type);
                    } else {
                        expr->type = sym->param.type;
                    }
                
                    sym->used = true;
                    return;
                }

                case DeclKind::Function: {
                    if (sym->function.linkage_kind == LinkageKind::Static) {
                        report_diag(expr->loc, fmt::format("{} has static linkage and cannot be accessed", pretty_ident));
                        report_diag(sym->loc, "Defined here", CompilerDiagKind::Note);
                    }

                    if (!m_sema_context.call && !m_sema_context.address_of) {
                        report_diag_with_notes(expr->loc, fmt::format("Cannot use function '{}' as a value", pretty_ident),
                            { fmt::format("Did you mean to write '&{}'", pretty_ident) });
                    }

                    check_visibility(sym, "function");

                    resolve_function_decl(sym);
                    expr->type = sym->function.type;
                    return;
                }

                case DeclKind::Struct:
                case DeclKind::Typedef:
                case DeclKind::Enum: {
                    check_visibility(sym, "type");

                    replace_expr(expr, Expr::Create(expr->loc, ExprKind::TypeInfo, ExprValueKind::RValue, TypeInfo::get_typeid(), TypeInfoExpr(type_from_decl(sym))));
                    return;
                }

                case DeclKind::Template: {
                    switch (sym->template_.template_decl->kind) {
                        case DeclKind::Function: {
                            check_visibility(sym, "generic function");

                            if (!m_sema_context.call && !m_sema_context.address_of) {
                                report_diag_with_notes(expr->loc, fmt::format("Cannot use generic function '{}' as a value", pretty_ident),
                                    { fmt::format("Did you mean to write '&{}'", pretty_ident) });
                            }

                            // We let the call analysis handle this
                            if (m_sema_context.call) {
                                expr->type = TypeInfo::create_deducable_template(sym, dr.generic_arguments);
                                return;
                            }

                            if (!dr.provides_generic_args) {
                                report_error(expr->loc, fmt::format("Missing generic arguments for generic function '{}'", pretty_ident));
                                replace_expr(expr, &error_expr);
                                return;
                            }

                            if (dr.generic_arguments.size != sym->template_.parameters.size) {
                                report_error(expr->loc, fmt::format("Too {} generic argments provided, expected {} but got {}",
                                    sym->template_.parameters.size < dr.generic_arguments.size ? "many" : "few",
                                    sym->template_.parameters.size, dr.generic_arguments.size));

                                replace_expr(expr, &error_expr);
                                return;
                            }

                            bool is_any_generic = false;
                            for (TypeInfo* t : dr.generic_arguments) {
                                resolve_type(t);

                                // If we have any non expanded generic parameters, don't create any instantiations
                                if (t->is_template()) {
                                    is_any_generic = true;
                                }
                            }

                            if (is_any_generic) {
                                dr.referenced_decl = sym;
                                expr->type = sym->template_.template_decl->function.type;
                                return;
                            }

                            Decl* specilization = specialize_template_func(expr->loc, sym, dr.generic_arguments);
                            dr.referenced_decl = specilization;
                            expr->type = specilization->function.type;
                            return;
                        }

                        case DeclKind::Struct: {
                            check_visibility(sym, "generic type");

                            if (dr.provides_generic_args) {
                                TypeInfo* gi = TypeInfo::create_struct_instantation(type_from_decl(sym), dr.generic_arguments, expr->loc);
                                resolve_type(gi);
                                replace_expr(expr, Expr::Create(expr->loc, ExprKind::TypeInfo, ExprValueKind::RValue, TypeInfo::get_typeid(), TypeInfoExpr(gi)));
                            } else {
                                replace_expr(expr, Expr::Create(expr->loc, ExprKind::TypeInfo, ExprValueKind::RValue, TypeInfo::get_typeid(), TypeInfoExpr(type_from_decl(sym))));
                            }
                            return;
                        }

                        default: ARIA_UNREACHABLE("Invalid generic decl");
                    }
                }

                default: ARIA_UNREACHABLE("Invalid symbol kind");
            }
        };

        auto resolve_with_specifier = [&]() {
            Module* mod = dr.name_specifier->name.referenced_module;

            if (mod->symbols.contains(dr.identifier)) {
                Decl* sym = mod->symbols.at(dr.identifier);
                dr.referenced_decl = sym;

                resolve_symbol(sym);
            } else {
                dr.referenced_decl = &error_decl;
                report_diag(expr->loc, fmt::format("Undeclared identifier '{}' in '{}'", dr.identifier, mod->name));
                expr->type = TypeInfo::get_error();
            }
        };

        auto resolve_without_specifier = [&]() {
            Decl* sym = nullptr;

            for (auto& [_, m] : context.active_comp_unit->imported_modules) {
                if (m->symbols.contains(dr.identifier)) {
                    sym = m->symbols.at(dr.identifier);
                    dr.referenced_decl = sym;
                }
            }

            if (context.active_comp_unit->parent->symbols.contains(dr.identifier)) {
                sym = context.active_comp_unit->parent->symbols.at(dr.identifier);
                dr.referenced_decl = sym;
            }

            if (!m_functions.empty() && m_functions.back().struct_type) {
                TypeInfo* struct_type = m_functions.back().struct_type;
                HTable<Decl*> field_lookup;
                
                switch (struct_type->kind) {
                    case TypeKind::Struct: {
                        field_lookup = struct_type->struct_.get_field_lookup();
                        break;
                    }

                    case TypeKind::StructSpecilization: {
                        field_lookup = struct_type->struct_specilization.get_field_lookup();
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid self type kind");
                }

                if (field_lookup.contains(dr.identifier)) {
                    Decl* field = field_lookup.at(dr.identifier);
                    TypeInfo* mem_type = nullptr;

                    switch (field->kind) {
                        case DeclKind::Field: mem_type = field->field.type; break;
                        case DeclKind::Method: mem_type = field->method.type; break;

                        default: ARIA_UNREACHABLE("Invalid field kind");
                    }

                    Expr* self = Expr::Create(expr->loc, ExprKind::Self, 
                        ExprValueKind::LValue, TypeInfo::create_pointer(m_functions.back().struct_type, false), 
                        ErrorExpr());

                    Expr* member = Expr::Create(expr->loc, ExprKind::Member,
                        ExprValueKind::LValue, mem_type,
                        MemberExpr(dr.identifier, self));
                    member->member.implicit_deref = true;

                    member->member.referenced_member = field;
                    replace_expr(expr, member);
                    return;
                }
            }

            if (!m_generics.empty()) {
                if (m_generics.back().contains(dr.identifier)) {
                    Decl* g = m_generics.back().at(dr.identifier);
                    replace_expr(expr, Expr::Create(expr->loc, ExprKind::TypeInfo, ExprValueKind::RValue, TypeInfo::get_typeid(), TypeInfoExpr(type_from_decl(g))));
                    return;
                }
            }

            if (!m_functions.empty()) {
                for (auto& scope : m_functions.back().scopes) {
                    if (scope.declarations.contains(dr.identifier)) {
                        sym = scope.declarations.at(dr.identifier).source_decl;
                        dr.referenced_decl = sym;
                    }
                }
            }

            if (sym) {
                resolve_symbol(sym);
            } else {
                expr->type = TypeInfo::get_error();
                expr->kind = ExprKind::Error;

                report_diag(expr->loc, fmt::format("Undeclared identifier '{}'", pretty_ident));
            }
        };

        if (dr.name_specifier) {
            ARIA_ASSERT(dr.name_specifier->kind == SpecifierKind::Name, "Invalid name specifier");
            resolve_name_specifier(dr.name_specifier);

            Module* mod = dr.name_specifier->name.referenced_module;
            if (!mod) {
                dr.referenced_decl = &error_decl;
                expr->type = TypeInfo::get_error();
                return;
            }

            return resolve_with_specifier();
        }

        resolve_without_specifier();
    }

    void SemanticAnalyzer::resolve_typeinfo_expr(Expr* expr) {
        resolve_type(expr->type_info.type);
    }

    void SemanticAnalyzer::resolve_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;

        resolve_expr(mem.parent);

        if (TypeInfo* t = get_typeinfo(mem.parent)) {
            Expr* new_expr = Expr::Create(expr->loc, ExprKind::TypeMember, ExprValueKind::RValue, nullptr, TypeMemberExpr(mem.member, t));
            replace_expr(expr, new_expr);
            return resolve_type_member_expr(expr);
        }

        TypeInfo* parent_type = TypeInfo::get_flattened(mem.parent->type);
        TypeInfo* member_type = nullptr;

        expr->value_kind = mem.parent->value_kind;

        bool searching = true;
        bool implicit_deref = false;
        while (searching) {
            switch (parent_type->kind) {
                case TypeKind::Error: {
                    expr->type = TypeInfo::get_error();
                    mem.referenced_member = &error_decl;
                    return;
                }

                case TypeKind::Typeid: {
                    if (mem.member == "name") {
                        member_type = TypeInfo::get_string();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "kind") {
                        member_type = TypeInfo::get_typekind();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "size") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "inner") {
                        member_type = TypeInfo::get_typeid();
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Any: {
                    if (mem.member == "type") {
                        member_type = TypeInfo::get_typeid();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "value") {
                        member_type = TypeInfo::get_void_ptr();
                        expr->kind = ExprKind::BuiltinMember;
                    } 

                    searching = false;
                    break;
                }

                case TypeKind::Struct: {
                    StructType& sd = parent_type->struct_;

                    StructDecl s = sd.source_decl->kind == DeclKind::Struct ? sd.source_decl->struct_ : sd.source_decl->template_.template_decl->struct_;
                    if (!s.field_lookup.contains(mem.member)) {
                        searching = false;
                        break;
                    }

                    Decl* fd = s.field_lookup.at(mem.member);
                    switch (fd->kind) {
                        case DeclKind::Field: member_type = fd->field.type; break;
                        case DeclKind::Method: member_type = fd->method.type; break;
                        case DeclKind::Destructor: member_type = fd->destructor.type; break;
                        default: ARIA_UNREACHABLE("Invalid field kind");
                    }
                    mem.referenced_member = fd;

                    if (fd->visibility == DeclVisibility::Private && mem.parent->kind != ExprKind::Self) {
                        report_error(expr->loc, fmt::format("'{}' is private and cannot be accessed", mem.member));
                        report_note(fd->loc, "Declared here");
                    }

                    if (member_type->is_method() && !m_sema_context.call) {
                        report_diag_with_notes(expr->loc, fmt::format("Reference to method must be called"),
                            { "Did you mean to call it with no arguments?" });
                    }

                    searching = false;
                    break;
                }

                case TypeKind::StructSpecilization: {
                    StructSpecilizationType& gi = parent_type->struct_specilization;

                    if (!gi.resolved_decl) {
                        expr->kind = ExprKind::DependentMember;
                        member_type = TypeInfo::get_dependent();
                        searching = false;
                        break;
                    }

                    if (gi.resolved_decl->kind == DeclKind::Template) {
                        StructDecl s = gi.resolved_decl->template_.template_decl->struct_;
                        if (!s.field_lookup.contains(mem.member)) {
                            searching = false;
                            break;
                        }

                        Decl* fd = s.field_lookup.at(mem.member);
                        switch (fd->kind) {
                            case DeclKind::Error: member_type = TypeInfo::get_error(); break;
                            case DeclKind::Field: member_type = fd->field.type; break;
                            case DeclKind::Method: member_type = fd->method.type; break;
                            default: ARIA_UNREACHABLE("Invalid field kind");
                        }
                        mem.referenced_member = fd;

                        if (fd->visibility == DeclVisibility::Private && mem.parent->kind != ExprKind::Self) {
                            report_diag(expr->loc, fmt::format("'{}' is private and cannot be accessed", mem.member));
                            report_diag(fd->loc, "Declared here", CompilerDiagKind::Note);
                        }

                        searching = false;
                        break;
                    }

                    ARIA_ASSERT(gi.resolved_decl->kind == DeclKind::StructSpecilization, "Invalid generic instantiation");
                    ARIA_ASSERT(gi.resolved_decl->struct_specilization.source->kind == DeclKind::Struct, "Invalid generic struct specilization");

                    StructDecl s = gi.resolved_decl->struct_specilization.source->struct_;
                    if (!s.field_lookup.contains(mem.member)) {
                        searching = false;
                        break;
                    }

                    Decl* fd = s.field_lookup.at(mem.member);
                    switch (fd->kind) {
                        case DeclKind::Error: member_type = TypeInfo::get_error(); break;
                        case DeclKind::Field: member_type = fd->field.type; break;
                        case DeclKind::Method: member_type = fd->method.type; break;
                        default: ARIA_UNREACHABLE("Invalid field kind");
                    }
                    mem.referenced_member = fd;

                    if (fd->visibility == DeclVisibility::Private && mem.parent->kind != ExprKind::Self) {
                        report_diag(expr->loc, fmt::format("'{}' is private and cannot be accessed", mem.member));
                        report_diag(fd->loc, "Declared here", CompilerDiagKind::Note);
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Template: {
                    expr->kind = ExprKind::DependentMember;
                    member_type = TypeInfo::get_dependent();
                    searching = false;
                    break;
                }

                case TypeKind::Array: {
                    if (mem.member == "mem") {
                        member_type = TypeInfo::create_pointer(parent_type->array.base, false);
                        expr->value_kind = ExprValueKind::RValue;
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->value_kind = ExprValueKind::RValue;
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::String: {
                    if (mem.parent->value_kind == ExprValueKind::RValue) {
                        expr->value_kind = ExprValueKind::RValue;
                    }

                    if (mem.member == "mem") {
                        member_type = TypeInfo::get_char_ptr();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Slice: {
                    if (mem.parent->value_kind == ExprValueKind::RValue) {
                        expr->value_kind = ExprValueKind::RValue;
                    }

                    if (mem.member == "mem") {
                        member_type = TypeInfo::create_pointer(parent_type->slice.base, false);
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Pointer: {
                    if (implicit_deref) {
                        report_diag(expr->loc, "'.' operator allows only one level of implicit dereferencing");
                    }

                    parent_type = parent_type->pointer.base;
                    implicit_deref = true;
                    mem.implicit_deref = true;

                    require_rvalue(mem.parent);
                    break;
                }

                case TypeKind::Typedef: { parent_type = parent_type->typedef_.base; break; }

                default: {
                    report_diag(mem.parent->loc, fmt::format("Expression must be of 'typeid', 'any', slice, array or struct type but is '{}'", type_info_to_string(mem.parent->type)));
                    expr->type = TypeInfo::get_error();
                    mem.referenced_member = &error_decl;
                    return;
                }
            }
        }
        
        if (mem.parent->is_rvalue() && !implicit_deref) { insert_materialize_temporary_expr(mem.parent); }

        if (!member_type) {
            report_diag(expr->loc, fmt::format("Unknown member '{}' in '{}'", mem.member, type_info_to_string(parent_type)));
            mem.referenced_member = &error_decl;
            expr->type = TypeInfo::get_error();
            return;
        }

        expr->type = member_type;

        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_type_member_expr(Expr* expr) {
        TypeMemberExpr& mem = expr->type_member;
        resolve_type(mem.type);

        if (mem.member == "name") {
            expr->type = TypeInfo::get_string();
            return;
        }

        switch (mem.type->kind) {
            case TypeKind::Enum: {
                EnumDecl& e = mem.type->enum_.source_decl->enum_;

                if (e.field_lookup.contains(mem.member)) {
                    Decl* d = e.field_lookup.at(mem.member);
                    expr->type = TypeInfo::create_enum(mem.type->enum_.source_decl);
                    expr->value_kind = ExprValueKind::RValue;
                    mem.referenced_member = d;
                } else {
                    report_diag(expr->loc, fmt::format("Enum '{}' has no field named '{}'", e.identifier, mem.member));
                    expr->type = TypeInfo::get_error();
                }

                return;
            }

            default: {
                report_diag(expr->loc, fmt::format("No such member '{}' in type '{}'", mem.member, type_info_to_string(mem.type)));
                expr->type = TypeInfo::get_error();
                return;
            }
        }
    }

    void SemanticAnalyzer::resolve_builtin_member_expr(Expr* expr) {
        MemberExpr& m = expr->member;
        resolve_expr(m.parent);
    }

    void SemanticAnalyzer::resolve_dependent_member_expr(Expr* expr) {
        MemberExpr& m = expr->member;
        resolve_expr(m.parent);

        TypeInfo* parent_type = m.parent->type;
        
        if (m.implicit_deref) {
            parent_type = parent_type->pointer.base;
        }

        switch (parent_type->kind) {
            case TypeKind::Template: return;

            case TypeKind::StructSpecilization: {
                if (!parent_type->struct_specilization.resolved_decl) { return; }

                expr->kind = ExprKind::Member;
                resolve_member_expr(expr);
                return;
            }

            case TypeKind::Struct: {
                expr->kind = ExprKind::Member;
                resolve_member_expr(expr);
                return;
            }

            default: {
                expr->type = TypeInfo::get_error();
                report_error(expr->loc, fmt::format("Invalid type '{}' to '.' operator", m.parent->type->to_string()));
                return;
            }
        }
    }

    void SemanticAnalyzer::resolve_self_expr(Expr* expr) {
        if (!m_functions.back().struct_type) {
            report_diag(expr->loc, "Cannot use 'self' outside of a method");
            expr->type = TypeInfo::get_error();
            return;
        }

        if (!expr->type) {
            expr->type = TypeInfo::create_pointer(m_functions.back().struct_type, false);
        }

        resolve_type(expr->type);
    }

    void SemanticAnalyzer::resolve_call_expr(Expr* expr) {
        CallExpr& call = expr->call;

        bool prev_val = m_sema_context.call;
        m_sema_context.call = true;
        resolve_expr(call.callee);
        m_sema_context.call = prev_val;

        if (call.callee->kind == ExprKind::Error) {
            expr->type = TypeInfo::get_error();
            expr->kind = ExprKind::Error;
            return;
        }
        
        if (TypeInfo* t = get_typeinfo(call.callee)) {
            expr->kind = ExprKind::Construct;
            expr->type = t;
            expr->construct.arguments = call.arguments;
            resolve_construct_expr(expr);
            return;
        }

        FunctionType* fn_type = nullptr;
        TypeInfo* searching_type = TypeInfo::get_flattened(call.callee->type);

        switch (searching_type->kind) {
            case TypeKind::Method: {
                expr->kind = ExprKind::MethodCall;
                return resolve_method_call_expr(expr);
            }

            case TypeKind::DeducableTemplate: {
                resolve_template_call_expr(searching_type->deducable_template.template_, searching_type->deducable_template.args, expr->loc, call.arguments, 
                    &call.callee->decl_ref.referenced_decl, &call.callee->type);

                if (call.callee->type->is_error()) {
                    expr->type = call.callee->type;
                    return;
                }

                fn_type = &call.callee->type->function;
                break;
            }

            case TypeKind::Function: {
                fn_type = &searching_type->function;
                break;
            }

            case TypeKind::Pointer: {
                if (!searching_type->pointer.base->is_function()) {
                    report_diag(call.callee->loc, fmt::format("Only pointers to functions may be called"));
                    expr->type = TypeInfo::get_error();
                    for (Expr* arg : call.arguments) {
                        resolve_expr(arg);
                    }
                    return;
                }

                require_rvalue(call.callee);
                fn_type = &TypeInfo::get_flattened(searching_type->pointer.base)->function;
                break;
            }

            case TypeKind::Error:
            case TypeKind::Dependent: {
                expr->type = searching_type;
                for (Expr* arg : call.arguments) {
                    resolve_expr(arg);
                }
                return;
            }

            default: {
                report_error(call.callee->loc, fmt::format("Expression must be of a callable type but is '{}'", call.callee->type->to_string()));
                expr->kind = ExprKind::Error;
                expr->type = TypeInfo::get_error();
                return;
            }
        }

        if (fn_type->return_type->is_never()) {
            m_functions.back().scopes.back().reaches_end = false;
        }

        if (!resolve_call_arity(expr->loc, *fn_type, call.arguments)) {
            expr->kind = ExprKind::Error;
            expr->type = TypeInfo::get_error();
            return;
        }

        resolve_call_args(*fn_type, call.arguments);

        if (fn_type->return_type->is_never()) {
            expr->type = TypeInfo::get_void();
        } else {
            expr->type = fn_type->return_type;
        }

        expr->value_kind = ExprValueKind::RValue;
    }

    void SemanticAnalyzer::resolve_template_call_expr(Decl* template_, TinyVector<TypeInfo*> template_args, SourceLoc loc, TinyVector<Expr*> args, Decl** callee, TypeInfo** callee_type) {
        ARIA_ASSERT(template_->kind == DeclKind::Template, "Invalid parameter");
        ARIA_ASSERT(template_->template_.template_decl->kind == DeclKind::Function, "Invalid parameter");

        TemplateDecl& g = template_->template_;

        {
            bool is_any_generic = false;
            for (TypeInfo* arg : template_args) {
                resolve_type(arg);

                // If we have any non expanded generic parameters, don't create any instantiations
                if (arg->is_template()) {
                    is_any_generic = true;
                }
            }

            if (is_any_generic) {
                *callee = template_;
                *callee_type = (*callee)->template_.template_decl->function.type;
                return;
            }
        }
        
        if (g.parameters.size == template_args.size) { // Exact amount of arguments, no need for deduction
            *callee = specialize_template_func(loc, template_, template_args);
            *callee_type = (*callee)->function.type;
            return;
        }

        if (g.parameters.size < template_args.size) { // Too many arguments, report error
            report_error(loc, fmt::format("Too many generic arguments provided, expected {}, got {}", g.parameters.size, template_args.size));
            *callee = &error_decl;
            *callee_type = TypeInfo::get_error();
            return;
        }

        // First check arity, we cannot deduce anything with an incorrect argument count
        if (!resolve_call_arity(loc, g.template_decl->function.type->function, args)) {
            *callee = &error_decl;
            *callee_type = TypeInfo::get_error();
            return;
        }

        ResolvedTemplateMap deduced_args;

        // Add the provided explicit generic args
        for (size_t i = 0; i < template_args.size; i++) {
            deduced_args[g.parameters[i]] = { template_args[i], false };
        }

        for (size_t i = 0; i < g.template_decl->function.type->function.params.size; i++) {
            Expr* arg = args[i];
            resolve_expr(arg);
            if (!deduce_template_type(arg->loc, g.template_decl->function.type->function.params[i]->param.type, arg->type, deduced_args)) {
                arg->type = TypeInfo::get_error();
            }
        }

        // Create the instantiation
        TinyVector<TypeInfo*> final_args;
        bool is_any_generic = false;
        for (Decl* p : g.parameters) {
            if (!deduced_args.contains(p)) {
                report_error(loc, fmt::format("Could not deduce generic argument '{}'", p->template_param.identifier));
                deduced_args[p] = ResolvedTemplateArg(TypeInfo::get_error(), true, loc);
            }

            auto& deduced = deduced_args.at(p);

            if (deduced.type->is_template()) {
                is_any_generic = true;
            }

            final_args.append(deduced_args.at(p).type);
        }

        if (is_any_generic) {
            *callee = template_;
            *callee_type = (*callee)->template_.template_decl->function.type;
            return;
        }

        *callee = specialize_template_func(loc, template_, final_args);
        *callee_type = (*callee)->function.type;
        return;
    }

    bool SemanticAnalyzer::resolve_call_arity(SourceLoc loc, FunctionType& fn_type, TinyVector<Expr*> args) {
        switch (fn_type.variadic) {
            case VariadicKind::None: {
                if (args.size < fn_type.required_arg_count) {
                    report_error(loc, fmt::format("Too few arguments provided, expected {} but got {}", fn_type.required_arg_count, args.size));
                    return false;
                }

                if (args.size > fn_type.params.size) {
                    report_error(loc, fmt::format("Too many arguments provided, expected {} but got {}", fn_type.required_arg_count, args.size));
                    return false;
                }

                return true;
            }

            case VariadicKind::Unnamed: {
                if (args.size < fn_type.required_arg_count) {
                    report_error(loc, fmt::format("Too few arguments provided, expected at least {} but got {}", fn_type.required_arg_count, args.size));
                    return false;
                }

                return true;
            }

            case VariadicKind::Named: {
                if (args.size < fn_type.required_arg_count) {
                    report_error(loc, fmt::format("Too few arguments provided, expected at least {} but got {}", fn_type.required_arg_count, args.size));
                    return false;
                }

                return true;
            }

            default: ARIA_UNREACHABLE("Invalid variadic kind");
        }
    }

    void SemanticAnalyzer::resolve_call_args(FunctionType& fn_type, TinyVector<Expr*>& args) {
        switch (fn_type.variadic) {
            case VariadicKind::None: {
                for (size_t i = 0; i < args.size; i++) {
                    resolve_param_initializer(fn_type.params[i], args[i]);
                }

                // Default args
                for (size_t i = args.size; i < fn_type.params.size; i++) {
                    Decl* p = fn_type.params[i];
                    args.append(Expr::Create({}, ExprKind::DefaultArg, ExprValueKind::RValue, p->param.type, DefaultArgExpr(p->param.default_arg, p)));
                }

                break;
            }

            case VariadicKind::Unnamed: {
                for (size_t i = 0; i < std::min(fn_type.params.size, args.size); i++) {
                    resolve_param_initializer(fn_type.params[i], args[i]);
                }

                // Default args
                for (size_t i = args.size; i < fn_type.params.size; i++) {
                    Decl* p = fn_type.params[i];
                    args.append(Expr::Create({}, ExprKind::DefaultArg, ExprValueKind::RValue, p->param.type, DefaultArgExpr(p->param.default_arg, p)));
                }

                for (size_t i = fn_type.params.size; i < args.size; i++) {
                    Expr* arg = args[i];
                    resolve_expr(arg);

                    if (arg->type->is_integral()) {
                        require_rvalue(arg);

                        if (arg->type->get_bit_size() < 32) { // Promote to int
                            insert_implicit_cast(TypeInfo::get_basic(TypeKind::Int), arg->type, arg, CastKind::Integral);
                        }
                    } else if (arg->type->is_floating_point()) {
                        require_rvalue(arg);

                        if (arg->type->kind == TypeKind::Float) { // Promote to double
                            insert_implicit_cast(TypeInfo::get_basic(TypeKind::Double), arg->type, arg, CastKind::Floating);
                        }
                    } else if (arg->type->is_pointer()) {
                        require_rvalue(arg);
                    } else if (!arg->type->is_error()) {
                        report_diag(arg->loc, fmt::format("Passing argument of non-trivial type ('{}') is not allowed", type_info_to_string(arg->type)));
                    }
                }

                break;
            }

            case VariadicKind::Named: {
                for (size_t i = 0; i < std::min(fn_type.params.size - 1, args.size); i++) {
                    resolve_param_initializer(fn_type.params[i], args[i]);
                }

                // Default args
                for (size_t i = args.size; i < fn_type.params.size - 1; i++) {
                    Decl* p = fn_type.params[i];
                    args.append(Expr::Create({}, ExprKind::DefaultArg, ExprValueKind::RValue, p->param.type, DefaultArgExpr(p->param.default_arg, p)));
                }

                for (size_t i = fn_type.params.size - 1; i < args.size; i++) {
                    Expr* arg = args[i];
                    resolve_expr(arg);
                    require_rvalue(arg);
                }

                break;
            }

            default: ARIA_UNREACHABLE("Invalid variadic kind");
        }
    }

    void SemanticAnalyzer::resolve_builtin_call_expr(Expr* expr) {
        BuiltinCallExpr& b = expr->builtin_call;

        switch (b.kind) {
            case BuiltinCallKind::Sizeof: {
                expr->type = TypeInfo::get_basic(TypeKind::Sz);

                if (b.arguments.size != 1) {
                    report_diag(expr->loc, "Call to builtin function '@sizeof' must have 1 argument");
                    break;
                }

                resolve_expr(b.arguments.items[0]);
                break;
            }

            case BuiltinCallKind::Memcpy: {
                expr->type = TypeInfo::get_void();

                if (b.arguments.size != 4) {
                    report_diag(expr->loc, "Call to builtin function '@memcpy' must have 4 arguments");
                    break;
                }

                for (Expr* arg : b.arguments) {
                    resolve_expr(arg);
                }

                try_insert_implicit_cast(TypeInfo::get_void_ptr(), b.arguments.items[0]); require_rvalue(b.arguments.items[0]);
                try_insert_implicit_cast(TypeInfo::get_void_ptr(), b.arguments.items[1]); require_rvalue(b.arguments.items[1]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), b.arguments.items[2]); require_rvalue(b.arguments.items[2]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Bool), b.arguments.items[3]); require_rvalue(b.arguments.items[3]);
                break;
            }

            case BuiltinCallKind::Memset: {
                expr->type = TypeInfo::get_void();

                if (b.arguments.size != 4) {
                    report_diag(expr->loc, "Call to builtin function '@memset' must have 4 arguments");
                    break;
                }

                for (Expr* arg : b.arguments) {
                    resolve_expr(arg);
                }

                try_insert_implicit_cast(TypeInfo::get_void_ptr(), b.arguments.items[0]); require_rvalue(b.arguments.items[0]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Char), b.arguments.items[1]); require_rvalue(b.arguments.items[1]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), b.arguments.items[2]); require_rvalue(b.arguments.items[2]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Bool), b.arguments.items[3]); require_rvalue(b.arguments.items[3]);
                break;
            }

            case BuiltinCallKind::Defined: {
                expr->type = TypeInfo::get_basic(TypeKind::Bool);
                expr->value_kind = ExprValueKind::RValue;
                m_error_captures.emplace_back();

                bool is_any_dependent = false;
                for (Expr* arg : b.arguments) {
                    resolve_expr(arg);
                    if (arg->type->is_dependent()) { is_any_dependent = true; }
                }

                CaptureErrorContext& c = m_error_captures.back();

                if (!is_any_dependent) {
                    expr->kind = ExprKind::Const;
                    expr->const_.kind = ConstExprKind::Boolean;
                    expr->const_.boolean = !c.has_error;
                } else {
                    expr->type = TypeInfo::get_dependent();
                }

                m_error_captures.pop_back();
                break;
            }

            default: ARIA_UNREACHABLE("Invalid builtin call kind");
        }
    }

    void SemanticAnalyzer::resolve_construct_expr(Expr* expr) {
        ConstructExpr& construct = expr->construct;

        if (!expr->type) {
            report_diag(expr->loc, "Construct expression requries an explicit type");
            replace_expr(expr, &error_expr);
            return;
        }

        resolve_type(expr->type);

        for (Expr* arg : construct.arguments) {
            resolve_expr(arg);
            if (!is_const_expr(arg)) { construct.is_const = false; }
        }

        switch (expr->type->kind) {
            case TypeKind::Error: expr->type = TypeInfo::get_error(); break;

            case TypeKind::Void: {
                report_error(expr->loc, "'void' cannot be constructed");
                expr->type = TypeInfo::get_error();
                break;
            }

            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::IChar:
            case TypeKind::Short:
            case TypeKind::UShort:
            case TypeKind::Int:
            case TypeKind::UInt:
            case TypeKind::Long:
            case TypeKind::ULong:
            case TypeKind::Float:
            case TypeKind::Double:
            case TypeKind::Pointer: {
                if (construct.arguments.size == 0) { break; }

                if (construct.arguments.size == 1) {
                    try_insert_explicit_cast(expr->type, construct.arguments.items[0]);
                    break;
                }

                report_error(expr->loc, fmt::format("Expected 0 or 1 arguments but got {}", construct.arguments.size));
                break;
            }

            case TypeKind::Typeid: {
                report_error(expr->loc, "'typeid' cannot be constructed");
                expr->kind = ExprKind::Error;
                break;
            }

            case TypeKind::Any: {
                if (construct.arguments.size > 2) {
                    report_diag(expr->loc, fmt::format("Too many initializers for '{}', expected 2 but got {}", type_info_to_string(expr->type), construct.arguments.size));
                    expr->type = TypeInfo::get_error();
                    break;
                }

                size_t i = 0;
                for (Expr* arg : construct.arguments) {
                    if (i == 0) { try_insert_implicit_cast(TypeInfo::get_typeid(), arg); }
                    if (i == 1) { try_insert_implicit_cast(TypeInfo::get_void_ptr(), arg); }

                    require_rvalue(arg);
                    i++;
                }

                break;
            }

            case TypeKind::Array: {
                if (construct.arguments.size > expr->type->array.size) {
                    report_diag(expr->loc, fmt::format("Too many initializers for '{}', expected {} but got {}", type_info_to_string(expr->type), expr->type->array.size, construct.arguments.size));
                    expr->type = TypeInfo::get_error();
                    break;
                }

                for (Expr* arg : construct.arguments) {
                    try_insert_implicit_cast(expr->type->array.base, arg);
                    require_rvalue(arg);
                }

                break;
            }

            case TypeKind::Struct:
            case TypeKind::StructSpecilization: {
                // Incomplete generic instantiation
                if (expr->type->is_struct_specilization() && !expr->type->struct_specilization.needs_specilization()) {
                    break;
                }

                Decl* s = expr->type->is_struct() ? expr->type->struct_.source_decl : expr->type->struct_specilization.resolved_decl->struct_specilization.source;

                if (construct.arguments.size > s->struct_.fields.size) {
                    report_diag(expr->loc, fmt::format("Too many initializers for '{}', expected {} but got {}", type_info_to_string(expr->type), s->struct_.fields.size, construct.arguments.size));
                    expr->type = TypeInfo::get_error();
                    break;
                }

                size_t i = 0;
                for (Expr* arg : construct.arguments) {
                    Decl* fd = s->struct_.fields.items[i++];
                    if (fd->kind == DeclKind::Error) { continue; }

                    try_insert_implicit_cast(fd->field.type, arg);
                    require_rvalue(arg);

                    if (fd->visibility == DeclVisibility::Private) {
                        report_diag(arg->loc, fmt::format("Cannot initialize private field '{}'", fd->field.identifier));
                        report_diag(fd->loc, "Declared here", CompilerDiagKind::Note);
                    }
                }

                break;
            }

            default: ARIA_UNREACHABLE("Invalid type kind");
        }

        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }

        if (m_sema_context.temporary) {
            if (Decl* dtor = type_get_destructor(expr->type)) {
                insert_temporary_expr(expr, dtor);
            }
        }
    }

    void SemanticAnalyzer::resolve_array_literal_expr(Expr* expr) {
        ArrayLiteralExpr& lit = expr->array_literal;

        TypeInfo* base_type = nullptr;

        for (Expr* arg : lit.arguments) {
            resolve_expr(arg);

            if (!base_type) {
                base_type = arg->type;
            } else {
                try_insert_implicit_cast(base_type, arg);
            }
        }

        expr->type = TypeInfo::create_array(base_type, lit.arguments.size);
    }

    void SemanticAnalyzer::resolve_method_call_expr(Expr* expr) {
        CallExpr& mc = expr->call;

        bool prev_val = m_sema_context.call;
        m_sema_context.call = true;
        resolve_expr(mc.callee);
        m_sema_context.call = prev_val;

        TypeInfo* callee_type = mc.callee->type;

        if (!callee_type->is_method() && !callee_type->is_error()) {
            report_diag(expr->loc, "Cannot call an object of non-method type");
            expr->kind = ExprKind::Error;
            expr->type = TypeInfo::get_error();
            return;
        }

        switch (mc.callee->member.referenced_member->kind) {
            case DeclKind::Error: {
                expr->kind = ExprKind::Error;
                expr->type = TypeInfo::get_error();
                return;
            }

            case DeclKind::Method: {
                resolve_type(mc.callee->member.referenced_member->method.type);
                FunctionType& fn_type = callee_type->function;

                if (!resolve_call_arity(expr->loc, fn_type, mc.arguments)) {
                    expr->kind = ExprKind::Error;
                    expr->type = TypeInfo::get_error();
                    return;
                }

                resolve_call_args(fn_type, mc.arguments);
                expr->type = fn_type.return_type;

                if (m_sema_context.temporary) {
                    if (Decl* dtor = type_get_destructor(expr->type)) {
                        insert_temporary_expr(expr, dtor);
                    }
                }
                break;
            }

            case DeclKind::Destructor: {
                if (mc.arguments.size != 0) {
                    report_diag(expr->loc, fmt::format("Mismatched argument count, destructor expects 0 but got {}", mc.arguments.size));
                    for (size_t i = 0; i < mc.arguments.size; i++) {
                        resolve_expr(mc.arguments.items[i]);
                    }
                }

                expr->type = TypeInfo::get_void();
                break;
            }

            default: ARIA_UNREACHABLE("Invalid referenced member");
        }
    }

    void SemanticAnalyzer::resolve_array_subscript_expr(Expr* expr) {
        ArraySubscriptExpr& subs = expr->array_subscript;

        resolve_expr(subs.array);
        resolve_expr(subs.index);
        require_rvalue(subs.index);

        if (subs.array->type->is_error()) { expr->type = TypeInfo::get_error(); return; }
        while (subs.array->type->is_typedef()) { subs.array->type = subs.array->type->typedef_.base; }

        try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), subs.index);

        switch (subs.array->type->kind) {
            case TypeKind::String: {
                expr->type = TypeInfo::get_basic(TypeKind::Char);
                break;
            }

            case TypeKind::Pointer: {
                require_rvalue(subs.array);

                if (subs.array->type->pointer.base->is_void()) {
                    report_diag(expr->loc, "Cannot index into 'void*' because it would dereference to 'void'");
                    expr->type = TypeInfo::get_error();
                    break;
                }

                expr->type = subs.array->type->pointer.base;
                break;
            }

            case TypeKind::Slice: {
                expr->type = subs.array->type->slice.base;
                break;
            }

            case TypeKind::Array: {
                expr->type = subs.array->type->array.base;
                break;
            }

            default: {
                report_diag(subs.array->loc, fmt::format("Invalid type '{}' for array subscript", type_info_to_string(subs.array->type)));
                expr->type = TypeInfo::get_error();
                break;
            }
        }
    }

    void SemanticAnalyzer::resolve_to_slice_expr(Expr* expr) {
        ToSliceExpr& tos = expr->to_slice;

        resolve_expr(tos.source);

        if (tos.len) {
            resolve_expr(tos.len);
            require_rvalue(tos.len);
        }  

        if (tos.source->type->is_error()) { expr->type = TypeInfo::get_error(); return; }

        switch (tos.source->type->kind) {
            case TypeKind::Pointer: {
                if (!tos.len) {
                    report_diag_with_notes(expr->loc, fmt::format("Cannot infer size of pointer type '{}'", type_info_to_string(tos.source->type)),
                        { "Consider using '[:len]' instead of [..]"} );
                }

                require_rvalue(tos.source);
                expr->type = TypeInfo::create_slice(tos.source->type->pointer.base);
                break;
            }

            case TypeKind::Slice: {
                ARIA_TODO("slice to slice");
                // require_rvalue(subs.Array);
                // expr->type = subs.Array->type->Base;
                // break;
            }

            case TypeKind::Array: {
                if (tos.source->value_kind != ExprValueKind::LValue) {
                    report_diag(tos.source->loc, "Expression must be an lvalue");
                }

                expr->type = TypeInfo::create_slice(tos.source->type->array.base);
                break;
            }

            default: report_diag(tos.source->loc, "Only a pointer/slice/array can be converted to a slice"); expr->type = TypeInfo::get_error(); break;
        }

        if (tos.len) { try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), tos.len); }
    }

    void SemanticAnalyzer::resolve_materialize_temporary_expr(Expr* expr) {
        MaterializeTemporaryExpr& t = expr->materialize_temporary;
        resolve_expr(t.expression);
    }

    void SemanticAnalyzer::resolve_paren_expr(Expr* expr) {
        ParenExpr& paren = expr->paren;
        resolve_expr(paren.expression);

        expr->type = paren.expression->type;
        expr->value_kind = paren.expression->value_kind;

        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_ternary_expr(Expr* expr) {
        TernaryExpr& t = expr->ternary;

        resolve_expr(t.condition);
        require_rvalue(t.condition);
        if (!t.condition->type->is_boolean()) {
            report_error(t.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", t.condition->type->to_string()));
        }

        resolve_expr(t.first);
        require_rvalue(t.first);

        resolve_expr(t.second);
        try_insert_implicit_cast(t.first->type, t.second);
        require_rvalue(t.second);

        expr->type = t.first->type;
    }

    void SemanticAnalyzer::resolve_cast_expr(Expr* expr) {
        CastExpr& cast = expr->cast;
        
        resolve_type(cast.type);
        resolve_expr(cast.expression);
        expr->type = cast.type;

        TypeInfo* dst_type = cast.type;

        if (expr->type->is_error() || dst_type->is_error()) { return; }

        ConversionCost cost = get_conversion_cost(dst_type, cast.expression->type);
        if (cost.cast_needed) {
            if (cost.explicit_cast_possible) {
                insert_implicit_cast(dst_type, cast.expression->type, cast.expression, cost.kind);
            } else {
                report_diag(expr->loc, fmt::format("Cannot cast from '{}' to '{}'", type_info_to_string(cast.expression->type),  type_info_to_string(dst_type)));
            }
        }

        if (expr->result_discarded) {
            report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_implicit_cast_expr(Expr* expr) {
        ImplicitCastExpr& i = expr->implicit_cast;
        resolve_type(expr->type);
        resolve_expr(i.expression);
    }

    void SemanticAnalyzer::resolve_unary_operator_expr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->unary_operator;
        
        if (unop.op == UnaryOperatorKind::AddressOf) {
            bool prev_val = m_sema_context.address_of;
            m_sema_context.address_of = true;
            resolve_expr(unop.expression);
            m_sema_context.address_of = prev_val;
        } else {
            resolve_expr(unop.expression);
        }

        TypeInfo* type = unop.expression->type;
        
        switch (unop.op) {
            case UnaryOperatorKind::Not: {
                require_rvalue(unop.expression);

                if (!type->is_boolean()) {
                    report_diag(unop.expression->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(type)));
                }

                expr->type = TypeInfo::get_basic(TypeKind::Bool);
                break;
            }

            case UnaryOperatorKind::Negate: {
                require_rvalue(unop.expression);
                ARIA_ASSERT(type->is_numeric(), "todo: add error message");

                if (type->is_integral()) {
                    if (type->is_unsigned()) {
                        report_diag(expr->loc, fmt::format("Cannot negate expression of unsigned type '{}'", type_info_to_string(type)));
                    }
                }

                expr->type = type;
                break;
            }

            case UnaryOperatorKind::AddressOf: {
                if (type->is_error()) { expr->type = type; break; }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    report_diag(expr->loc, "Address of operation ('&') requries an lvalue");
                }

                TypeInfo* new_type = TypeInfo::create_pointer(type, false);
                expr->type = new_type;
                break;
            }

            case UnaryOperatorKind::RValueAddressOf: {
                if (type->is_error()) { expr->type = type; break; }

                if (!unop.expression->is_xvalue()) {
                    require_rvalue(unop.expression);
                    insert_materialize_temporary_expr(unop.expression);
                }

                TypeInfo* new_type = TypeInfo::create_pointer(type, false);
                expr->type = new_type;
                break;
            }

            case UnaryOperatorKind::Dereference: {
                if (TypeInfo* t = get_typeinfo(unop.expression)) {
                    replace_expr(expr, Expr::Create(expr->loc, ExprKind::TypeInfo, ExprValueKind::RValue, TypeInfo::get_typeid(), TypeInfoExpr(TypeInfo::create_pointer(t, false))));
                    break;
                }

                expr->value_kind = ExprValueKind::LValue;
                if (type->is_error()) { expr->type = type; break; }

                require_rvalue(unop.expression);

                if (type->is_pointer()) {
                    if (type->pointer.base->is_void()) {
                        report_diag(expr->loc, "Cannot dereference a *void");
                    } else if (type->pointer.base->is_function()) {
                        report_diag(expr->loc, fmt::format("Cannot dereference function pointer '{}'", type_info_to_string(type)));
                    }
                } else {
                    report_diag(expr->loc, "Dereferencing requires a pointer type");
                    expr->type = TypeInfo::get_error();
                    break;
                }

                expr->type = type->pointer.base;
                break;
            }

            case UnaryOperatorKind::PreIncrement:
            case UnaryOperatorKind::PreDecrement:
            case UnaryOperatorKind::PostIncrement:
            case UnaryOperatorKind::PostDecrement: {
                if (!unop.expression->type->is_error()) {
                    if (!unop.expression->type->is_numeric()) {
                        report_diag(unop.expression->loc, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(unop.expression->type)));
                        expr->type = TypeInfo::get_error();
                        break;
                    }
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    report_diag(unop.expression->loc, "Expression must be a modifiable lvalue");
                    expr->type = TypeInfo::get_error();
                    break;
                }

                expr->type = unop.expression->type;
                expr->value_kind = ExprValueKind::RValue;
                break;
            }

            default: {
                ARIA_ASSERT(false, unary_op_kind_to_string(unop.op));
                // fmt::print("{}\n", );
                // ARIA_UNREACHABLE();
            }
        }
    }

    void SemanticAnalyzer::resolve_binary_operator_expr(Expr* expr) {
        BinaryOperatorExpr& binop = expr->binary_operator;

        resolve_expr(binop.lhs);
        resolve_expr(binop.rhs);

        Expr* LHS = binop.lhs;
        Expr* RHS = binop.rhs;

        switch (binop.op) {
            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::Sub:
            case BinaryOperatorKind::Mul:
            case BinaryOperatorKind::Div:
            case BinaryOperatorKind::Mod: {
                insert_arithmetic_promotion(LHS, RHS, binop.op, expr);
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::Less:
            case BinaryOperatorKind::LessOrEq:
            case BinaryOperatorKind::Greater:
            case BinaryOperatorKind::GreaterOrEq:
            case BinaryOperatorKind::IsEq: 
            case BinaryOperatorKind::IsNotEq: {
                insert_arithmetic_promotion(LHS, RHS, binop.op, expr);
                expr->type = TypeInfo::get_basic(TypeKind::Bool); // insert_arithmetic_promotion() will set the type but we want to override the type
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::BitOr:
            case BinaryOperatorKind::BitXor:
            case BinaryOperatorKind::Shl:
            case BinaryOperatorKind::Shr: {
                insert_arithmetic_promotion(LHS, RHS, binop.op, expr);
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::Eq: {
                expr->type = LHS->type;
                expr->value_kind = ExprValueKind::LValue;

                if (LHS->value_kind != ExprValueKind::LValue) {
                    report_diag(LHS->loc, "Expression must be a modifiable lvalue");
                    return;
                }

                if (is_const_expr(LHS)) {
                    report_diag(LHS->loc, "Cannot assign to constant expression");
                    return;
                }

                if (!is_assignable_expr(LHS)) {
                    report_diag(LHS->loc, "Must be an assignable expression");
                    return;
                }

                require_rvalue(RHS);
                try_insert_implicit_cast(LHS->type, RHS);
                return;
            }

            case BinaryOperatorKind::LogAnd:
            case BinaryOperatorKind::LogOr: {
                require_rvalue(LHS);
                require_rvalue(RHS);

                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Bool), LHS);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Bool), RHS);

                expr->type = TypeInfo::get_basic(TypeKind::Bool);
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    report_diag(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
                }
                return;
            }

            default: ARIA_UNREACHABLE("Invalid binary operator");
        }
    }

    void SemanticAnalyzer::resolve_compound_assign_expr(Expr* expr) {
        CompoundAssignExpr& comp = expr->compound_assign;
        
        resolve_expr(comp.lhs);
        resolve_expr(comp.rhs);

        expr->type = comp.lhs->type;
        expr->value_kind = ExprValueKind::LValue;

        require_rvalue(comp.rhs);
        
        Expr* LHS = comp.lhs;
        Expr* RHS = comp.rhs;
       
        if (LHS->value_kind != ExprValueKind::LValue) {
            report_diag(LHS->loc, "Expression must be a modifiable lvalue");
            return;
        }

        if (is_const_expr(LHS)) {
            report_diag(LHS->loc, "Cannot assign to constant expression");
            return;
        }

        if (!is_assignable_expr(LHS)) {
            report_diag(LHS->loc, "Must be an assignable expression");
            return;
        }

        try_insert_implicit_cast(LHS->type, RHS);
    }

    void SemanticAnalyzer::resolve_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::Error: break;
            case ExprKind::BooleanLiteral: resolve_boolean_literal_expr(expr); break;
            case ExprKind::CharacterLiteral: resolve_character_literal_expr(expr); break;
            case ExprKind::IntegerLiteral: resolve_integer_literal_expr(expr); break;
            case ExprKind::FloatingLiteral: resolve_floating_literal_expr(expr); break;
            case ExprKind::StringLiteral: resolve_string_literal_expr(expr); break;
            case ExprKind::Null: resolve_null_expr(expr); break;
            case ExprKind::DeclRef: resolve_decl_ref_expr(expr); break;
            case ExprKind::TypeInfo: resolve_typeinfo_expr(expr); break;
            case ExprKind::Member: resolve_member_expr(expr); break;
            case ExprKind::BuiltinMember: resolve_builtin_member_expr(expr); break;
            case ExprKind::DependentMember: resolve_dependent_member_expr(expr); break;
            case ExprKind::TypeMember: resolve_type_member_expr(expr); break;
            case ExprKind::Self: resolve_self_expr(expr); break;
            case ExprKind::Call: resolve_call_expr(expr); break;
            case ExprKind::BuiltinCall: resolve_builtin_call_expr(expr); break;
            case ExprKind::Construct: resolve_construct_expr(expr); break;
            case ExprKind::ArrayLiteral: resolve_array_literal_expr(expr); break;
            case ExprKind::MethodCall: resolve_method_call_expr(expr); break;
            case ExprKind::ArraySubscript: resolve_array_subscript_expr(expr); break;
            case ExprKind::ToSlice: resolve_to_slice_expr(expr); break;
            case ExprKind::MaterializeTemporary: resolve_materialize_temporary_expr(expr); break;
            case ExprKind::Paren: resolve_paren_expr(expr); break;
            case ExprKind::Ternary: resolve_ternary_expr(expr); break;
            case ExprKind::Cast: resolve_cast_expr(expr); break;
            case ExprKind::ImplicitCast: resolve_implicit_cast_expr(expr); break;
            case ExprKind::UnaryOperator: resolve_unary_operator_expr(expr); break;
            case ExprKind::BinaryOperator: resolve_binary_operator_expr(expr); break;
            case ExprKind::CompoundAssign: resolve_compound_assign_expr(expr); break;
            case ExprKind::Const: break;
            default: ARIA_UNREACHABLE("Invalid expr kind");
        }
    }

    void SemanticAnalyzer::resolve_name_specifier(Specifier* specifier) {
        NameSpecifier& name = specifier->name;

        Module* parent = nullptr;

        if (name.parent) {
            resolve_name_specifier(name.parent);
            parent = name.parent->name.referenced_module;
        }

        if (!parent) {
            if (context.active_comp_unit->local_modules.contains(name.identifier)) {
                name.referenced_module = context.active_comp_unit->local_modules.at(name.identifier);
                return;
            }

            if (context.active_comp_unit->imported_modules.contains(name.identifier)) {
                name.referenced_module = context.active_comp_unit->imported_modules.at(name.identifier);
                return;
            }

            report_diag(specifier->loc, fmt::format("No such module '{}' in scope", name.identifier));
            return;
        } else {
            if (parent->child_lookup.contains(name.identifier)) {
                name.referenced_module = parent->child_lookup.at(name.identifier);
                return;
            }

            report_diag(specifier->loc, fmt::format("No such module '{}' in '{}'", name.identifier, parent->name));
            return;
        }
    }

    bool SemanticAnalyzer::is_const_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::Error:
            case ExprKind::BooleanLiteral:
            case ExprKind::CharacterLiteral:
            case ExprKind::IntegerLiteral:
            case ExprKind::FloatingLiteral:
            case ExprKind::StringLiteral:
            case ExprKind::Null:
            case ExprKind::TypeInfo:
                return true;

            case ExprKind::DeclRef:
                return expr->decl_ref.referenced_decl->kind == DeclKind::Var && expr->decl_ref.referenced_decl->var.const_var;

            case ExprKind::TypeMember:
                return expr->type_member.type->is_enum();

            case ExprKind::BuiltinCall:
                return expr->builtin_call.kind == BuiltinCallKind::Defined;

            case ExprKind::Construct:
                return expr->construct.is_const;

            case ExprKind::Paren:
                return is_const_expr(expr->paren.expression);

            case ExprKind::ImplicitCast:
                return is_const_expr(expr->implicit_cast.expression);

            case ExprKind::UnaryOperator:
                return is_const_expr(expr->unary_operator.expression);

            case ExprKind::BinaryOperator:
                return is_const_expr(expr->binary_operator.lhs) && is_const_expr(expr->binary_operator.rhs);

            case ExprKind::Const: return true;

            default: return false;
        }
    }

    Expr* SemanticAnalyzer::eval_const_expr(Expr* expr) {
        ARIA_ASSERT(is_const_expr(expr), "Cannot evaulate a non-constant expression");

        switch (expr->kind) {
            // Already evaluated
            case ExprKind::Const: return expr;

            case ExprKind::Error: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Error));

            case ExprKind::BooleanLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Boolean, expr->boolean_literal.value));

            case ExprKind::CharacterLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Integer, static_cast<u64>(expr->character_literal.value)));

            case ExprKind::IntegerLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Integer, expr->integer_literal.value));

            case ExprKind::FloatingLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Floating, expr->floating_literal.value));

            case ExprKind::StringLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::String, expr->string_literal.value));

            case ExprKind::DeclRef:
                resolve_decl(expr->decl_ref.referenced_decl);
                ARIA_ASSERT(expr->decl_ref.referenced_decl->kind == DeclKind::Var, "Referenced decl must be a var");
                ARIA_ASSERT(expr->decl_ref.referenced_decl->var.const_var, "Referenced decl must be const");

                return expr->decl_ref.referenced_decl->var.initializer;

            case ExprKind::TypeInfo:
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Typeid, expr->type_info.type));

            case ExprKind::TypeMember: {
                ARIA_ASSERT(expr->type_member.referenced_member, "Invalid type member expression");
                ARIA_ASSERT(expr->type_member.referenced_member->kind == DeclKind::EnumConstant, "Invalid type member expression");
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Integer, expr->type_member.referenced_member->enum_constant.resolved_value));
            }

            case ExprKind::Construct:
                for (Expr*& arg : expr->construct.arguments) {
                    arg = eval_const_expr(arg);
                }

                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Struct, expr->construct.arguments));

            case ExprKind::Paren:
                return eval_const_expr(expr->paren.expression);

            case ExprKind::UnaryOperator: {
                Expr* val = eval_const_expr(expr->unary_operator.expression);

                switch (expr->unary_operator.op) {
                    case UnaryOperatorKind::Negate: {
                        switch (val->const_.kind) {
                            case ConstExprKind::Integer: {
                                val->const_.integer = static_cast<u64>(-static_cast<i64>(val->const_.integer));
                                return val;
                            }

                            case ConstExprKind::Floating: {
                                val->const_.number = -val->const_.number;
                                return val;
                            }

                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }

                        ARIA_UNREACHABLE("Should never be reached");
                    }

                    default: ARIA_UNREACHABLE("Invalid unary operator");
                }

                return nullptr;
            }

            case ExprKind::ImplicitCast: {
                #define CAST(t, e) static_cast<t>(e)
                #define INT(x) Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Integer, x))
                #define FLOAT(x) Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Floating, x))

                switch (expr->implicit_cast.kind) {
                    case CastKind::Integral: {
                        if (expr->implicit_cast.expression->type->is_signed()) {
                            i64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;

                            switch (expr->type->kind) {
                                case TypeKind::Char: return INT(CAST(u64, CAST(u8, val)));
                                case TypeKind::IChar: return INT(CAST(i64, CAST(i8, val)));
                                case TypeKind::Short: return INT(CAST(i64, CAST(i16, val)));
                                case TypeKind::UShort: return INT(CAST(u64, CAST(u16, val)));
                                case TypeKind::Int: return INT(CAST(i64, CAST(i32, val)));
                                case TypeKind::UInt: return INT(CAST(u64, CAST(u32, val)));
                                case TypeKind::Long: return INT(CAST(i64, val));
                                case TypeKind::ULong: return INT(CAST(u64, val));

                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        } else {
                            u64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;

                            switch (expr->type->kind) {
                                case TypeKind::Char: return INT(CAST(u64, CAST(u8, val)));
                                case TypeKind::IChar: return INT(CAST(i64, CAST(i8, val)));
                                case TypeKind::Short: return INT(CAST(i64, CAST(i16, val)));
                                case TypeKind::UShort: return INT(CAST(u64, CAST(u16, val)));
                                case TypeKind::Int: return INT(CAST(i64, CAST(i32, val)));
                                case TypeKind::UInt: return INT(CAST(u64, CAST(u32, val)));
                                case TypeKind::Long: return INT(CAST(i64, val));
                                case TypeKind::ULong: return INT(CAST(u64, val));

                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        }

                        ARIA_UNREACHABLE("Should never be reached");
                        return nullptr;
                    }

                    case CastKind::IntegralToFloating: {
                        if (expr->implicit_cast.expression->type->is_signed()) {
                            i64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;

                            switch (expr->type->kind) {
                                case TypeKind::Float: return FLOAT(CAST(double, CAST(float, val)));
                                case TypeKind::Double: return FLOAT(CAST(double, val));

                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        } else {
                            u64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;

                            switch (expr->type->kind) {
                                case TypeKind::Float: return FLOAT(CAST(double, CAST(float, val)));
                                case TypeKind::Double: return FLOAT(CAST(double, val));

                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        }

                        ARIA_UNREACHABLE("Should never be reached");
                        return nullptr;
                    }

                    case CastKind::LValueToRValue: {
                        return eval_const_expr(expr->implicit_cast.expression);
                    }

                    default: ARIA_UNREACHABLE("Invalid cast kind");
                }

                #undef INT
                #undef CAST

                return nullptr;
            }

            case ExprKind::BinaryOperator: {
                Expr* lhs = eval_const_expr(expr->binary_operator.lhs);
                Expr* rhs = eval_const_expr(expr->binary_operator.rhs);

                switch (expr->binary_operator.op) {
                    case BinaryOperatorKind::Add: {
                        switch (lhs->const_.kind) {
                            case ConstExprKind::Integer: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Integer, lhs->const_.integer + rhs->const_.integer));
                            }

                            case ConstExprKind::Floating: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Floating, lhs->const_.number + rhs->const_.number));
                            }

                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }

                        return nullptr;
                    }

                    case BinaryOperatorKind::Mul: {
                        switch (lhs->const_.kind) {
                            case ConstExprKind::Integer: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Integer, lhs->const_.integer * rhs->const_.integer));
                            }

                            case ConstExprKind::Floating: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Floating, lhs->const_.number * rhs->const_.number));
                            }

                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }

                        return nullptr;
                    }

                    case BinaryOperatorKind::Div: {
                        switch (lhs->const_.kind) {
                            case ConstExprKind::Integer: {
                                if (lhs->type->is_signed() && rhs->type->is_signed()) {
                                    return Expr::Create(expr->loc, ExprKind::Const, 
                                        ExprValueKind::RValue, lhs->type, 
                                        ConstExpr(ConstExprKind::Integer, static_cast<i64>(lhs->const_.integer) / static_cast<i64>(rhs->const_.integer)));
                                } else if (lhs->type->is_unsigned() && rhs->type->is_unsigned()) {
                                    return Expr::Create(expr->loc, ExprKind::Const, 
                                        ExprValueKind::RValue, lhs->type, 
                                        ConstExpr(ConstExprKind::Integer, lhs->const_.integer / rhs->const_.integer));
                                } else {
                                    ARIA_UNREACHABLE("Invalid type");
                                }

                                return nullptr;
                            }

                            case ConstExprKind::Floating: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Floating, lhs->const_.number / rhs->const_.number));
                            }

                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }

                        return nullptr;
                    }

                    default: ARIA_UNREACHABLE("Invalid binary operator");
                }

                return nullptr;
            }

            default: ARIA_UNREACHABLE("Should never be reached");
        }
    }

    bool SemanticAnalyzer::is_assignable_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::DeclRef: return true;
            case ExprKind::Member: return true;
            case ExprKind::DependentMember: return true;
            case ExprKind::ArraySubscript: return is_assignable_expr(expr->array_subscript.array);

            case ExprKind::Paren: return is_assignable_expr(expr->paren.expression);
            case ExprKind::Cast: return is_assignable_expr(expr->cast.expression);
            case ExprKind::ImplicitCast: return is_assignable_expr(expr->implicit_cast.expression);

            case ExprKind::UnaryOperator: {
                switch (expr->unary_operator.op) {
                    case UnaryOperatorKind::PreIncrement:
                    case UnaryOperatorKind::PreDecrement:
                    case UnaryOperatorKind::PostIncrement:
                    case UnaryOperatorKind::PostDecrement: return true;

                    case UnaryOperatorKind::Dereference: return !expr->unary_operator.expression->type->pointer.is_const;

                    default: return false;
                }
            }

            default: return false;
        }
    }

    void SemanticAnalyzer::insert_implicit_cast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        if (cast_needs_rvalue(castKind)) {
            require_rvalue(srcExpr);
        }

        Expr* src = Expr::dup(srcExpr); // We must copy the original expression to avoid overwriting the same memory
        Expr* implicitCast = Expr::Create(src->loc, ExprKind::ImplicitCast, ExprValueKind::RValue, dstType, ImplicitCastExpr(src, castKind));

        replace_expr(srcExpr, implicitCast);
    }

    void SemanticAnalyzer::try_insert_implicit_cast(TypeInfo* dst_type, Expr* src_expr, std::string_view kind) {
        ConversionCost cost = get_conversion_cost(dst_type, src_expr->type);

        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(dst_type, src_expr->type, src_expr, cost.kind);
            } else if (cost.explicit_cast_possible) {
                if (!kind.empty()) {
                    report_diag_with_notes(src_expr->loc,
                        fmt::format("Cannot implicitly convert from '{}' to {} '{}'", type_info_to_string(src_expr->type), kind, type_info_to_string(dst_type)),
                        { "You can however insert an explicit cast in the code" }); 
                } else {
                    report_diag_with_notes(src_expr->loc,
                        fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(src_expr->type), type_info_to_string(dst_type)),
                        { "You can however insert an explicit cast in the code" }); 
                }
            } else {
                if (!kind.empty()) {
                    report_diag(src_expr->loc,
                        fmt::format("Cannot implicitly convert from '{}' to {} '{}'", type_info_to_string(src_expr->type), kind, type_info_to_string(dst_type))); 
                } else {
                    report_diag(src_expr->loc,
                        fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(src_expr->type), type_info_to_string(dst_type))); 
                }
            }
        }
    }

    void SemanticAnalyzer::try_insert_explicit_cast(TypeInfo* dst_type, Expr* src_expr) {
        ConversionCost cost = get_conversion_cost(dst_type, src_expr->type);

        if (cost.cast_needed) {
            if (cost.explicit_cast_possible) {
                insert_implicit_cast(dst_type, src_expr->type, src_expr, cost.kind);
            } else {
                report_diag(src_expr->loc, 
                    fmt::format("Cannot convert from '{}' to '{}'", type_info_to_string(src_expr->type), type_info_to_string(dst_type)));
            }
        }
    }

    void SemanticAnalyzer::require_rvalue(Expr* expr) {
        if (expr->is_lxvalue()) {
            if (expr->type->is_struct()) {
                Expr* m = Expr::Create(expr->loc, ExprKind::Move, ExprValueKind::RValue, expr->type, MoveExpr(Expr::dup(expr)));
                if (Decl* dtor = type_get_destructor(expr->type)) {
                    insert_temporary_expr(m, dtor);
                }

                replace_expr(expr, m);
                return;
            }

            insert_implicit_cast(expr->type, expr->type, expr, CastKind::LValueToRValue);
        }
    }

    void SemanticAnalyzer::maybe_promote_to_int(Expr* expr) {
        switch (expr->type->kind) {
            case TypeKind::IChar:
            case TypeKind::Short:
                insert_implicit_cast(TypeInfo::get_basic(TypeKind::Int), expr->type, expr, CastKind::Integral);
                break;

            case TypeKind::Char:
            case TypeKind::UShort:
                insert_implicit_cast(TypeInfo::get_basic(TypeKind::UInt), expr->type, expr, CastKind::Integral);
                break;

            default: break;
        }
    }

    void SemanticAnalyzer::insert_arithmetic_promotion(Expr* lhs, Expr* rhs, BinaryOperatorKind op, Expr* e) {
        TypeInfo* lty = lhs->type;
        TypeInfo* rty = rhs->type;

        if (lty->kind == TypeKind::Error || rty->kind == TypeKind::Error) {
            e->type = TypeInfo::get_error();
            return;
        }

        if (lty->kind == TypeKind::Template || rty->kind == TypeKind::Template) {
            e->type = lhs->type;
            return;
        }

        if (lty->is_typedef()) { lty = lty->typedef_.base; }
        if (rty->is_typedef()) { rty = rty->typedef_.base; }

        require_rvalue(lhs);
        require_rvalue(rhs);

        if (lty->is_integral()) {
            if (rty->is_integral()) {
                // We want to keep the original types for error messages
                TypeInfo lhs_type = *lty;
                TypeInfo rhs_type = *rty;

                size_t l_size = lty->get_bit_size();
                size_t r_size = rty->get_bit_size();

                if (l_size > r_size) {
                    insert_implicit_cast(lty, rty, rhs, CastKind::Integral);
                } else if (r_size > l_size) {
                    insert_implicit_cast(rty, lty, lhs, CastKind::Integral);
                } else if (l_size == r_size) {
                    if (lty->is_signed() != rty->is_signed()) {
                        report_diag_with_notes(lhs->loc, 
                            fmt::format("Mismatched types '{}' and '{}'", type_info_to_string(&lhs_type), type_info_to_string(&rhs_type)),
                            { "implicit signedness conversions are not allowed here"} );
                    }
                }

                e->type = lty;
                return;
            } else if (rty->is_floating_point()) {
                insert_implicit_cast(rty, lty, lhs, CastKind::IntegralToFloating);
                e->type = rty;
                return;
            }
        } else if (lty->is_floating_point() && !is_binary_operator_bit(op)) {
            if (rty->is_integral()) {
                insert_implicit_cast(lty, rty, rhs, CastKind::IntegralToFloating);
                e->type = lty;
                return;
            } else if (rty->is_floating_point()) {
                size_t lSize = lty->get_bit_size();
                size_t rSize = rty->get_bit_size();

                if (lSize > rSize) {
                    insert_implicit_cast(lty, rty, rhs, CastKind::Floating);
                } else if (rSize > lSize) {
                    insert_implicit_cast(rty, lty, lhs, CastKind::Floating);
                }

                e->type = lty;
                return;
            }
        } else if (lty->is_pointer() && !is_binary_operator_bit(op)) {
            if (op == BinaryOperatorKind::Add) {
                if (rty->is_integral()) {
                    e->type = lty;
                    return;
                } else {
                    report_diag(rhs->loc, fmt::format("Expected an integer here but got '{}'", type_info_to_string(rty)));
                    e->type = lty;
                    return;
                }
            }

            if (rty->is_pointer()) {
                if (op == BinaryOperatorKind::IsEq || op == BinaryOperatorKind::IsNotEq) {
                    insert_implicit_cast(TypeInfo::get_void_ptr(), lty, lhs, CastKind::BitCast);
                    insert_implicit_cast(TypeInfo::get_void_ptr(), rty, rhs, CastKind::BitCast);
                    e->type = lty;
                    return;
                }
            }
        } else if (lty->is_typeid() && rty->is_typeid()) {
            if (op == BinaryOperatorKind::IsEq || op == BinaryOperatorKind::IsNotEq) {
               e->type = lty;
               return;
            }
        } else if (lty->is_enum() && rty->is_enum()) {
            if (lty->enum_.source_decl == rty->enum_.source_decl) {
                if (op == BinaryOperatorKind::IsEq || op == BinaryOperatorKind::IsNotEq) {
                   e->type = lty;
                   return;
                }
            }
        }

        report_diag(lhs->loc + rhs->loc, fmt::format("Invalid operands to binary operator '{}' (have '{}' and '{}')", binary_op_kind_to_string(op),
            type_info_to_string(lty), type_info_to_string(rty)));
        e->type = TypeInfo::get_error();
        return;
    }

    void SemanticAnalyzer::insert_materialize_temporary_expr(Expr* expr) {
        Expr* temp = Expr::Create(expr->loc, ExprKind::MaterializeTemporary, ExprValueKind::XValue, expr->type, MaterializeTemporaryExpr(Expr::dup(expr)));
        replace_expr(expr, temp);
    }

    void SemanticAnalyzer::insert_temporary_expr(Expr* expr, Decl* dtor) {
        if (!m_sema_context.temporary) { return; }

        Expr* temp = Expr::Create(expr->loc, ExprKind::Temporary, ExprValueKind::RValue, expr->type, TemporaryExpr(Expr::dup(expr), dtor));
        replace_expr(expr, temp);
        m_sema_context.needs_cleanup = true;
    }

    void SemanticAnalyzer::insert_expr_with_cleanups(Expr* expr) {
        if (m_sema_context.needs_cleanup) {
            Expr* e = Expr::Create(expr->loc, ExprKind::ExprWithCleanups, expr->value_kind, expr->type, ExprWithCleanups(Expr::dup(expr)));
            replace_expr(expr, e);
            m_sema_context.needs_cleanup = false;
        }
    }

} // namespace ariac