#include "ariac/semantic_analyzer/semantic_analyzer.hpp"
#include "ariac/core/scratch_buffer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_boolean_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_character_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_integer_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_floating_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_string_literal_expr(Expr* expr) {
        if (!expr->type) { expr->type = TypeInfo::get_string(); }

        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_null_expr(Expr* expr) {
        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_decl_ref_expr(Expr* expr) {
        DeclRefExpr& dr = expr->decl_ref;

        std::string pretty_ident = dr.name_specifier ? fmt::format("{}::{}", dr.name_specifier->name.identifier, dr.identifier) : fmt::format("{}", dr.identifier);

        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }

        auto resolve_with_specifier = [&]() {
            Module* mod = dr.name_specifier->name.referenced_module;

            if (mod->symbols.contains(dr.identifier)) {
                Decl* sym = mod->symbols.at(dr.identifier);
                dr.referenced_decl = sym;
                
                if (sym->visibility == DeclVisibility::Private) {
                    context.report_compiler_diagnostic(expr->loc, fmt::format("{} is private and cannot be accessed", pretty_ident));
                    context.report_compiler_diagnostic(sym->loc, "Defined here", CompilerDiagKind::Note, sym->parent_unit);
                }

                switch (sym->kind) {
                    case DeclKind::Var: {
                        if (sym->var.linkage_kind == LinkageKind::Static) {
                            context.report_compiler_diagnostic(expr->loc, fmt::format("{} has static linkage and cannot be accessed", pretty_ident));
                            context.report_compiler_diagnostic(sym->loc, "Defined here", CompilerDiagKind::Note, sym->parent_unit);
                        }

                        Module* m = context.active_module;
                        CompilationUnit* c = context.active_comp_unit;

                        context.active_module = sym->parent_module;
                        context.active_comp_unit = sym->parent_unit;

                        resolve_var_decl(sym);

                        context.active_module = m;
                        context.active_comp_unit = c;

                        expr->type = sym->var.type;
                        return;
                    }

                    case DeclKind::Function: {
                        if (sym->function.linkage_kind == LinkageKind::Static) {
                            context.report_compiler_diagnostic(expr->loc, fmt::format("{} has static linkage and cannot be accessed", pretty_ident));
                            context.report_compiler_diagnostic(sym->loc, "Defined here", CompilerDiagKind::Note, sym->parent_unit);
                        }

                        resolve_function_decl(sym);
                        expr->type = sym->function.type;
                        return;
                    }

                    case DeclKind::Struct:
                    case DeclKind::Typedef: expr->type = TypeInfo::get_error(); return;

                    case DeclKind::Generic: {
                        switch (sym->generic.decl->kind) {
                            case DeclKind::Function: {
                                resolve_function_decl(sym->generic.decl);
                                expr->type = sym->generic.decl->function.type;
                                return;
                            }

                            case DeclKind::Struct: {
                                expr->type = TypeInfo::get_error();
                                return;
                            }

                            default: ARIA_UNREACHABLE("Invalid generic decl");
                        }
                    }

                    default: ARIA_UNREACHABLE("Invalid symbol kind");
                }
            } else {
                dr.referenced_decl = &error_decl;
                context.report_compiler_diagnostic(expr->loc, fmt::format("Undeclared identifier '{}'", pretty_ident));
                expr->type = TypeInfo::get_error();
            }
        };

        auto resolve_without_specifier = [&]() {
            Module* mod = context.active_module;
            Decl* sym = nullptr;

            if (mod->symbols.contains(dr.identifier)) {
                sym = mod->symbols.at(dr.identifier);
                dr.referenced_decl = sym;
            }

            if (m_active_struct) {
                Decl* struc = m_active_struct->kind == TypeKind::Structure ? m_active_struct->struct_.source_decl : m_active_struct->generic_instantiation.resolved_decl->generic.decl;
                if (struc->struct_.field_lookup.contains(dr.identifier)) {
                    Decl* field = struc->struct_.field_lookup.at(dr.identifier);
                    TypeInfo* mem_type = nullptr;

                    switch (field->kind) {
                        case DeclKind::Field: mem_type = field->field.type; break;
                        case DeclKind::Method: mem_type = field->method.type; break;

                        default: ARIA_UNREACHABLE("Invalid field kind");
                    }

                    Expr* self = Expr::Create(expr->loc, ExprKind::Self, 
                        ExprValueKind::LValue, TypeInfo::create_pointer(m_active_struct, false), 
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

            for (Decl* type : m_generic_types) {
                ARIA_ASSERT(type->kind == DeclKind::GenericParameter, "Invalid generic parameter");

                if (type->generic_parameter.identifier == dr.identifier) {
                    dr.referenced_decl = type;
                    expr->type = TypeInfo::get_error();
                    return;
                }
            }

            for (auto& scope : m_scopes) {
                if (scope.declarations.contains(dr.identifier)) {
                    sym = scope.declarations.at(dr.identifier).source_decl;
                    dr.referenced_decl = sym;
                }
            }

            if (sym) {
                switch (sym->kind) {
                    case DeclKind::Var: {
                        Module* m = context.active_module;
                        CompilationUnit* c = context.active_comp_unit;

                        context.active_module = sym->parent_module;
                        context.active_comp_unit = sym->parent_unit;

                        resolve_var_decl(sym);

                        context.active_module = m;
                        context.active_comp_unit = c;

                        expr->type = sym->var.type;
                        return;
                    }

                    case DeclKind::Param: {
                        if (sym->param.variadic) {
                            expr->type = TypeInfo::create_slice(sym->param.type);
                        } else {
                            expr->type = sym->param.type;
                        }

                        return;
                    }

                    case DeclKind::Function: {
                        resolve_function_decl(sym);
                        expr->type = sym->function.type;
                        return;
                    }

                    case DeclKind::Struct:
                    case DeclKind::Typedef:
                    case DeclKind::Enum: expr->type = TypeInfo::get_error(); return;

                    case DeclKind::Generic: {
                        switch (sym->generic.decl->kind) {
                            case DeclKind::Function: {
                                Module* m = context.active_module;
                                CompilationUnit* c = context.active_comp_unit;
                                context.active_module = sym->parent_module;
                                context.active_comp_unit = sym->parent_unit;

                                resolve_function_decl(sym->generic.decl);

                                context.active_module = m;
                                context.active_comp_unit = c;

                                expr->type = sym->generic.decl->function.type;
                                return;
                            }

                            case DeclKind::Struct: {
                                expr->type = TypeInfo::get_error();
                                return;
                            }

                            default: ARIA_UNREACHABLE("Invalid generic decl");
                        }
                    }

                    default: ARIA_UNREACHABLE("Invalid symbol kind");
                }
            } else {
                for (Decl* im : context.active_comp_unit->imports) {
                    ARIA_ASSERT(im->kind == DeclKind::Import, "Invalid import");
                    ImportDecl& import = im->import;

                    if (!import.resolved_module) { continue; }

                    if (import.resolved_module->symbols.contains(dr.identifier)) {
                        sym = import.resolved_module->symbols.at(dr.identifier);
                        dr.referenced_decl = sym;

                        switch (sym->kind) {
                            case DeclKind::Var: {
                                context.report_compiler_diagnostic_with_notes(expr->loc, "Variables from other modules must be prefixed with the module name",
                                    { fmt::format("Did you mean to write '{}::{}'", import.resolved_module->name, dr.identifier)});

                                resolve_var_decl(sym);
                                expr->type = sym->var.type;
                                return;
                            }

                            case DeclKind::Function: {
                                context.report_compiler_diagnostic_with_notes(expr->loc, "Functions from other modules must be prefixed with the module name",
                                    { fmt::format("Did you mean to write '{}::{}'", import.resolved_module->name, dr.identifier)});

                                resolve_function_decl(sym);
                                expr->type = sym->function.type;
                                return;
                            }

                            case DeclKind::Struct: 
                            case DeclKind::Typedef:
                            case DeclKind::Enum: expr->type = TypeInfo::get_error(); return;

                            case DeclKind::Generic: {
                                if (sym->generic.decl->kind == DeclKind::Function) {
                                    context.report_compiler_diagnostic_with_notes(expr->loc, "Generic functions from other modules must be prefixed with the module name",
                                        { fmt::format("Did you mean to write '{}::{}'", import.resolved_module->name, dr.identifier)});

                                    resolve_function_decl(sym->generic.decl);
                                    expr->type = sym->generic.decl->function.type;
                                } else {
                                    expr->type = TypeInfo::get_error();
                                }

                                return;
                            }

                            default: ARIA_UNREACHABLE("Invalid symbol kind");
                        }
                    }
                }
            }

            dr.referenced_decl = &error_decl;
            context.report_compiler_diagnostic(expr->loc, fmt::format("Undeclared identifier '{}'", pretty_ident));
            expr->type = TypeInfo::get_error();
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

            if (mod == context.active_module) { return resolve_without_specifier(); }
            else { return resolve_with_specifier(); }
        }

        resolve_without_specifier();
    }

    void SemanticAnalyzer::resolve_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;

        resolve_expr(mem.parent);
        TypeInfo* parent_type = mem.parent->type;
        TypeInfo* member_type = nullptr;

        bool implicit_deref = false;
        bool searching = true;
        while (searching) {
            switch (parent_type->kind) {
                case TypeKind::Error: {
                    if (mem.parent->kind == ExprKind::DeclRef) {
                        switch (mem.parent->decl_ref.referenced_decl->kind) {
                            case DeclKind::Enum: {
                                EnumDecl& e = mem.parent->decl_ref.referenced_decl->enum_;
                                
                                if (e.field_lookup.contains(mem.member)) {
                                    Decl* d = e.field_lookup.at(mem.member);
                                    expr->type = TypeInfo::create_enum(mem.parent->decl_ref.referenced_decl);
                                    expr->value_kind = ExprValueKind::RValue;
                                    mem.referenced_member = d;
                                } else {
                                    context.report_compiler_diagnostic(expr->loc, fmt::format("Enum '{}' has no field named '{}'", e.identifier, mem.member));
                                    expr->type = TypeInfo::get_error();
                                    mem.referenced_member = &error_decl;
                                }

                                return;
                            }

                            case DeclKind::Error:
                            case DeclKind::Var:
                            case DeclKind::Param: break;

                            default: ARIA_UNREACHABLE("Invalid decl kind");
                        }
                    }

                    expr->type = TypeInfo::get_error();
                    mem.referenced_member = &error_decl;
                    return;
                }

                case TypeKind::TypeInfo: {
                    if (mem.member == "name") {
                        member_type = TypeInfo::get_string();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "kind") {
                        member_type = TypeInfo::get_string();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "size") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "types") {
                        TypeInfo* ti_ptr = TypeInfo::create_pointer(TypeInfo::get_basic(TypeKind::TypeInfo), false);
                        member_type = TypeInfo::create_slice(ti_ptr);
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Any: {
                    if (mem.member == "type") {
                        member_type = TypeInfo::get_typeinfo_ptr();
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "value") {
                        member_type = TypeInfo::get_void_ptr();
                        expr->kind = ExprKind::BuiltinMember;
                    } 

                    searching = false;
                    break;
                }

                case TypeKind::Structure: {
                    StructType& sd = parent_type->struct_;

                    StructDecl s = sd.source_decl->kind == DeclKind::Struct ? sd.source_decl->struct_ : sd.source_decl->generic.decl->struct_;
                    if (!s.field_lookup.contains(mem.member)) {
                        searching = false;
                        break;
                    }

                    Decl* fd = s.field_lookup.at(mem.member);
                    switch (fd->kind) {
                        case DeclKind::Field: member_type = fd->field.type; break;
                        case DeclKind::Method: member_type = fd->method.type; break;
                        default: ARIA_UNREACHABLE("Invalid field kind");
                    }
                    mem.referenced_member = fd;

                    if (fd->visibility == DeclVisibility::Private && mem.parent->kind != ExprKind::Self) {
                        context.report_compiler_diagnostic(expr->loc, fmt::format("'{}' is private and cannot be accessed", mem.member));
                        context.report_compiler_diagnostic(fd->loc, "Declared here", CompilerDiagKind::Note, fd->parent_unit);
                    }

                    searching = false;
                    break;
                }

                case TypeKind::GenericInstantiation: {
                    GenericInstantiationType& gi = parent_type->generic_instantiation;

                    if (gi.resolved_decl->kind == DeclKind::Generic) {
                        StructDecl s = gi.resolved_decl->generic.decl->struct_;
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
                            context.report_compiler_diagnostic(expr->loc, fmt::format("'{}' is private and cannot be accessed", mem.member));
                            context.report_compiler_diagnostic(fd->loc, "Declared here", CompilerDiagKind::Note, fd->parent_unit);
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
                        context.report_compiler_diagnostic(expr->loc, fmt::format("'{}' is private and cannot be accessed", mem.member));
                        context.report_compiler_diagnostic(fd->loc, "Declared here", CompilerDiagKind::Note, fd->parent_unit);
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Array: {
                    if (mem.member == "mem") {
                        member_type = TypeInfo::create_pointer(parent_type->array.base, false);
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = TypeInfo::get_basic(TypeKind::Sz);
                        expr->value_kind = ExprValueKind::RValue;
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Slice: {
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
                        context.report_compiler_diagnostic(expr->loc, "'.' operator allows only one level of implicit dereferencing");
                    }

                    parent_type = parent_type->pointer.base;
                    implicit_deref = true;
                    mem.implicit_deref = true;

                    require_rvalue(mem.parent);
                    break;
                }

                case TypeKind::Typedef: { parent_type = parent_type->typedef_.base; break; }

                default: {
                    context.report_compiler_diagnostic(mem.parent->loc, fmt::format("Expression must be of 'typeinfo', 'any', slice, array or struct type but is '{}'", type_info_to_string(mem.parent->type)));
                    expr->type = TypeInfo::get_error();
                    mem.referenced_member = &error_decl;
                    return;
                }
            }
        }
        

        if (!member_type) {
            context.report_compiler_diagnostic(expr->loc, fmt::format("Unknown member '{}' in '{}'", mem.member, type_info_to_string(parent_type)));
            mem.referenced_member = &error_decl;
            expr->type = TypeInfo::get_error();
            return;
        }

        expr->type = member_type;

        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_builtin_member_expr(Expr* expr) {
        MemberExpr& m = expr->member;
        resolve_expr(m.parent);
    }

    void SemanticAnalyzer::resolve_self_expr(Expr* expr) {
        if (!m_active_struct) {
            context.report_compiler_diagnostic(expr->loc, "Cannot use 'self' outside of a method");
            expr->type = TypeInfo::get_error();
            return;
        }

        if (!expr->type) {
            expr->type = TypeInfo::create_pointer(m_active_struct, false);
        }

        resolve_type(expr->type);
    }

    void SemanticAnalyzer::resolve_call_expr(Expr* expr) {
        CallExpr& call = expr->call;

        resolve_expr(call.callee);

        if (call.callee->kind == ExprKind::Error) {
            expr->type = TypeInfo::get_error();
            return;
        }

        if (call.callee->kind == ExprKind::Member) {
            expr->kind = ExprKind::MethodCall;
            resolve_method_call_expr(expr);
            return;
        } else if (call.callee->decl_ref.referenced_decl->kind == DeclKind::Struct) {
            expr->kind = ExprKind::Construct;
            expr->type = TypeInfo::create_struct(call.callee->decl_ref.referenced_decl);
            expr->construct.arguments = call.arguments;
            resolve_construct_expr(expr);
            return;
        }

        TypeInfo* calleeType = call.callee->type;

        if (calleeType->kind != TypeKind::Function && !calleeType->is_error()) {
            context.report_compiler_diagnostic(expr->loc, "Cannot call an object of non-function type");
            expr->type = TypeInfo::get_error();
            return;
        }

        if (call.callee->decl_ref.referenced_decl->kind != DeclKind::Error) {
            if (!call.callee->type->is_error()) {
                if (call.generic_arguments.size > 0) {
                    Decl* g = call.callee->decl_ref.referenced_decl;

                    if (call.callee->decl_ref.referenced_decl->kind != DeclKind::Generic) {
                        context.report_compiler_diagnostic(expr->loc, "Cannot provide generic arguments to a non generic function");
                    } else {
                        if (call.generic_arguments.size != g->generic.parameters.size) {
                            context.report_compiler_diagnostic(expr->loc, fmt::format("Mismatched generic instantiation, generic expects {} arguments but got {}", g->generic.parameters.size, call.generic_arguments.size));
                        } else {
                            for (TypeInfo* t : call.generic_arguments) {
                                resolve_type(t);
                            }

                            Decl* specilization = nullptr;
                            for (Decl* i : g->generic.specilizations) {
                                ARIA_ASSERT(i->kind == DeclKind::FunctionSpecilization, "Invalid generic specilization");

                                bool failed = false;
                                for (size_t idx = 0; idx < call.generic_arguments.size; idx++) {
                                    if (!type_is_equal(call.generic_arguments.items[idx], i->function_specilization.types.items[idx])) { failed = true; break; }
                                }

                                if (!failed) { specilization = i; }
                            }

                            if (!specilization) {
                                for (size_t i = 0; i < call.generic_arguments.size; i++) {
                                    Decl* gen_param = g->generic.parameters.items[i];
                                    TypeInfo* gen_arg = call.generic_arguments.items[i];
                                    ARIA_ASSERT(gen_param->kind == DeclKind::GenericParameter, "Invalid generic parameter");
                                    
                                    for (auto& req : gen_param->generic_parameter.requirements) {
                                        bool is_satifised = false;

                                        switch (req.kind) {
                                            case GenericRequirementKind::Integral: {
                                                is_satifised = gen_arg->is_integral();
                                                break;
                                            }

                                            case GenericRequirementKind::FloatingPoint: {
                                                is_satifised = gen_arg->is_floating_point();
                                                break;
                                            }

                                            case GenericRequirementKind::ConvertibleTo: {
                                                ConversionCost cost = get_conversion_cost(req.arg, gen_arg);
                                                if (cost.cast_needed && !cost.explicit_cast_possible) {
                                                    is_satifised = false;
                                                } else {
                                                    is_satifised = true;
                                                }
                                                break;
                                            }

                                            default: ARIA_UNREACHABLE("Invalid generic requirement");
                                        }

                                        if (!is_satifised && !gen_arg->is_error()) {
                                            context.report_compiler_diagnostic(gen_arg->loc, fmt::format("Argument '{}' does not satisfy generic requirement '{}'", i, generic_requirement_kind_to_string(req.kind)));
                                            gen_arg = TypeInfo::get_error();
                                        }
                                    }

                                    m_specialized_generic_types[gen_param->generic_parameter.identifier] = gen_arg;
                                }

                                TypeInfo* new_type = TypeInfo::dup(g->generic.decl->function.type);

                                bool prev_val = m_replace_generic_types;
                                m_replace_generic_types = true;
                                resolve_type(new_type);
                                m_replace_generic_types = false;

                                specilization = Decl::Create(g->loc, DeclKind::FunctionSpecilization, g->visibility, FunctionSpecilizationDecl(call.generic_arguments, new_type));
                                specilization->parent_module = g->parent_module;
                                specilization->parent_unit = g->parent_unit;
                                g->generic.specilizations.append(specilization);

                                calleeType = new_type;
                                call.callee->decl_ref.referenced_decl = specilization;
                                call.callee->type = calleeType;
                            }
                        }
                    }
                }

                FunctionType& fn_type = calleeType->function;

                if (fn_type.param_types.size != call.arguments.size && !fn_type.is_variadic()) {
                    context.report_compiler_diagnostic(expr->loc, fmt::format("Mismatched argument count, function expects {} but got {}", fn_type.param_types.size, call.arguments.size));
                    for (size_t i = 0; i < call.arguments.size; i++) {
                        resolve_expr(call.arguments.items[i]);
                    }
                } else {
                    for (size_t i = 0; i < fn_type.param_types.size; i++) {
                        if (fn_type.variadic == VariadicKind::Named && i == fn_type.param_types.size - 1) { break; }
                        resolve_param_initializer(fn_type.param_types.items[i], call.arguments.items[i]);
                    }

                    if (fn_type.variadic == VariadicKind::Unnamed) {
                        for (size_t i = fn_type.param_types.size; i < call.arguments.size; i++) {
                            Expr* arg = call.arguments.items[i];
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
                                context.report_compiler_diagnostic(arg->loc, fmt::format("Passing argument of non-trivial type ('{}') is not allowed", type_info_to_string(arg->type)));
                            }
                        }
                    } else if (fn_type.variadic == VariadicKind::Named) {
                        for (size_t i = fn_type.param_types.size - 1; i < call.arguments.size; i++) {
                            Expr* arg = call.arguments.items[i];
                            resolve_expr(arg);
                            require_rvalue(arg);
                        }
                    }
                }

                expr->type = fn_type.return_type;
                expr->value_kind = ExprValueKind::RValue;
                return;
            }
        } else {
            for (Expr* arg : call.arguments) {
                resolve_expr(arg);
            }
        }

        expr->type = TypeInfo::get_error();
        call.callee->kind = ExprKind::Error;
    }

    void SemanticAnalyzer::resolve_builtin_call_expr(Expr* expr) {
        BuiltinCallExpr& bc = expr->builtin_call;

        if (!expr->type) {
            switch (bc.kind) {
                case BuiltinCallKind::Sizeof: {
                    expr->type = TypeInfo::get_basic(TypeKind::Sz);
                    break;
                }

                case BuiltinCallKind::Typeid: {
                    expr->type = TypeInfo::get_typeinfo_ptr();
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid builtin call kind");
            }
        }

        if (bc.type) { resolve_type(bc.type); return; }

        if (bc.expression) {
            resolve_expr(bc.expression);

            if (bc.expression->kind == ExprKind::DeclRef) {
                switch (bc.expression->decl_ref.referenced_decl->kind) {
                    case DeclKind::Struct: {
                        if (bc.expression->decl_ref.referenced_decl->resolve_status == ResolveStatus::NotStarted) {
                            CompilationUnit* old_unit = context.active_comp_unit;
                            context.active_comp_unit = bc.expression->decl_ref.referenced_decl->parent_unit;
                            resolve_struct_decl(bc.expression->decl_ref.referenced_decl);
                            context.active_comp_unit = old_unit;
                        }

                        bc.type = TypeInfo::create_struct(bc.expression->decl_ref.referenced_decl);
                        break;
                    }

                    case DeclKind::Typedef: {
                        if (bc.expression->decl_ref.referenced_decl->resolve_status == ResolveStatus::NotStarted) {
                            CompilationUnit* old_unit = context.active_comp_unit;
                            context.active_comp_unit = bc.expression->decl_ref.referenced_decl->parent_unit;
                            resolve_typedef_decl(bc.expression->decl_ref.referenced_decl);
                            context.active_comp_unit = old_unit;
                        }

                        bc.type = TypeInfo::create_typedef(bc.expression->decl_ref.referenced_decl);
                        break;
                    }

                    case DeclKind::GenericParameter: {
                        bc.type = TypeInfo::create_generic(bc.expression->decl_ref.identifier);
                        break;
                    }

                    default: break;
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_intrinsic_call_expr(Expr* expr) {
        IntrinsicCallExpr& i = expr->intrinsic_call;

        for (Expr* arg : i.arguments) {
            resolve_expr(arg);
        }

        switch (i.kind) {
            case IntrinsicCallKind::Memcpy: {
                expr->type = TypeInfo::get_void();

                if (i.arguments.size != 4) {
                    context.report_compiler_diagnostic(expr->loc, "Call to intrinsic function '$memcpy' must have 4 arguments");
                    break;
                }

                try_insert_implicit_cast(TypeInfo::get_void_ptr(), i.arguments.items[0]); require_rvalue(i.arguments.items[0]);
                try_insert_implicit_cast(TypeInfo::get_void_ptr(), i.arguments.items[1]); require_rvalue(i.arguments.items[1]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), i.arguments.items[2]); require_rvalue(i.arguments.items[2]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Bool), i.arguments.items[3]); require_rvalue(i.arguments.items[3]);
                break;
            }

            case IntrinsicCallKind::Memset: {
                expr->type = TypeInfo::get_void();

                if (i.arguments.size != 4) {
                    context.report_compiler_diagnostic(expr->loc, "Call to intrinsic function '$memset' must have 4 arguments");
                    break;
                }

                try_insert_implicit_cast(TypeInfo::get_void_ptr(), i.arguments.items[0]); require_rvalue(i.arguments.items[0]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Char), i.arguments.items[1]); require_rvalue(i.arguments.items[1]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), i.arguments.items[2]); require_rvalue(i.arguments.items[2]);
                try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Bool), i.arguments.items[3]); require_rvalue(i.arguments.items[3]);
                break;
            }

            default: ARIA_UNREACHABLE("Invalid intrinsic call kind");
        }
    }

    void SemanticAnalyzer::resolve_construct_expr(Expr* expr) {
        ConstructExpr& construct = expr->construct;

        if (!expr->type) {
            context.report_compiler_diagnostic(expr->loc, "Construct expression requries an explicit type");
            replace_expr(expr, &error_expr);
            return;
        }

        resolve_type(expr->type);

        if (expr->type->is_structure()) {
            Decl* s = expr->type->struct_.source_decl;

            if (construct.arguments.size > s->struct_.fields.size) {
                context.report_compiler_diagnostic(expr->loc, fmt::format("Too many initializers for '{}', expected {} but provided {}", type_info_to_string(expr->type), s->struct_.fields.size, construct.arguments.size));
                expr->type = TypeInfo::get_error();
                return;
            }

            size_t i = 0;
            for (Expr* arg : construct.arguments) {
                resolve_expr(arg);

                if (!is_const_expr(arg)) { construct.is_const = false; }

                Decl* fd = s->struct_.fields.items[i++];
                if (fd->kind == DeclKind::Error) { continue; }

                try_insert_implicit_cast(fd->field.type, arg);

                if (fd->visibility == DeclVisibility::Private) {
                    context.report_compiler_diagnostic(arg->loc, fmt::format("Cannot initialize private field '{}'", fd->field.identifier));
                    context.report_compiler_diagnostic(fd->loc, "Declared here", CompilerDiagKind::Note, fd->parent_unit);
                }
            }
        } else {
            ARIA_ASSERT(expr->type->is_any(), "Type must be any");

            construct.is_const = false;

            if (construct.arguments.size > 2) {
                context.report_compiler_diagnostic(expr->loc, fmt::format("Too many initializers for '{}', expected 2 but provided {}", type_info_to_string(expr->type), construct.arguments.size));
                expr->type = TypeInfo::get_error();
                return;
            }

            for (size_t i = 0; i < construct.arguments.size; i++) {
                Expr* arg = construct.arguments.items[i];
                resolve_expr(arg);
                if (i == 0) { try_insert_implicit_cast(TypeInfo::get_typeinfo_ptr(), arg); }
                if (i == 1) { try_insert_implicit_cast(TypeInfo::get_void_ptr(), arg); }
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

        resolve_expr(mc.callee);
        TypeInfo* callee_type = mc.callee->type;

        if (!callee_type->is_method() && !callee_type->is_error()) {
            context.report_compiler_diagnostic(expr->loc, "Cannot call an object of non-method type");
            expr->type = TypeInfo::get_error();
            return;
        }

        if (mc.callee->member.referenced_member->kind != DeclKind::Error) {
            ARIA_ASSERT(mc.callee->member.referenced_member->kind == DeclKind::Method, "Invalid referenced member");

            resolve_type(mc.callee->member.referenced_member->method.type);
            FunctionType& fn_type = callee_type->function;

            if (fn_type.param_types.size != mc.arguments.size) {
                context.report_compiler_diagnostic(expr->loc, fmt::format("Mismatched argument count, method expects {} but got {}", fn_type.param_types.size, mc.arguments.size));
                for (size_t i = 0; i < mc.arguments.size; i++) {
                    resolve_expr(mc.arguments.items[i]);
                }
            } else {
                for (size_t i = 0; i < fn_type.param_types.size; i++) {
                    resolve_param_initializer(fn_type.param_types.items[i], mc.arguments.items[i]);
                }
            }

            expr->type = fn_type.return_type;
            expr->value_kind = ExprValueKind::RValue;
        } else {
            for (Expr* arg : mc.arguments) {
                resolve_expr(arg);
            }

            expr->type = TypeInfo::get_error();
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
            case TypeKind::Pointer: {
                require_rvalue(subs.array);

                if (subs.array->type->pointer.base->is_void()) {
                    context.report_compiler_diagnostic(expr->loc, "Cannot index into 'void*' because it would dereference to 'void'");
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

            default: context.report_compiler_diagnostic(subs.array->loc, "'[' operator can only be used with a pointer/slice/array"); expr->type = TypeInfo::get_error(); break;
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
                    context.report_compiler_diagnostic_with_notes(expr->loc, fmt::format("Cannot infer size of pointer type '{}'", type_info_to_string(tos.source->type)),
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
                    context.report_compiler_diagnostic(tos.source->loc, "Expression must be an lvalue");
                }

                expr->type = TypeInfo::create_slice(tos.source->type->array.base);
                break;
            }

            default: context.report_compiler_diagnostic(tos.source->loc, "Only a pointer/slice/array can be converted to a slice"); expr->type = TypeInfo::get_error(); break;
        }

        if (tos.len) { try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Sz), tos.len); }
    }

    void SemanticAnalyzer::resolve_paren_expr(Expr* expr) {
        ParenExpr& paren = expr->paren;
        resolve_expr(paren.expression);

        expr->type = paren.expression->type;
        expr->value_kind = paren.expression->value_kind;

        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
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
                context.report_compiler_diagnostic(expr->loc, fmt::format("Cannot cast from '{}' to '{}'", type_info_to_string(cast.expression->type),  type_info_to_string(dst_type)));
            }
        }

        if (expr->result_discarded) {
            context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_implicit_cast_expr(Expr* expr) {
        ImplicitCastExpr& i = expr->implicit_cast;
        resolve_type(expr->type);
        resolve_expr(i.expression);
    }

    void SemanticAnalyzer::resolve_unary_operator_expr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->unary_operator;
        
        resolve_expr(unop.expression);
        TypeInfo* type = unop.expression->type;
        
        switch (unop.op) {
            case UnaryOperatorKind::Not: {
                require_rvalue(unop.expression);

                if (!type->is_boolean()) {
                    context.report_compiler_diagnostic(unop.expression->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(type)));
                }

                expr->type = TypeInfo::get_basic(TypeKind::Bool);
                break;
            }

            case UnaryOperatorKind::Negate: {
                require_rvalue(unop.expression);
                ARIA_ASSERT(type->is_numeric(), "todo: add error message");

                if (type->is_integral()) {
                    if (type->is_unsigned()) {
                        context.report_compiler_diagnostic(expr->loc, fmt::format("Cannot negate expression of unsigned type '{}'", type_info_to_string(type)));
                    }
                }

                expr->type = type;
                break;
            }

            case UnaryOperatorKind::AddressOf: {
                if (type->is_error()) { expr->type = type; break; }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    context.report_compiler_diagnostic(expr->loc, "Address of operation ('&') requries an lvalue");
                }

                TypeInfo* new_type = TypeInfo::create_pointer(type, false);
                expr->type = new_type;
                break;
            }

            case UnaryOperatorKind::RValueAddressOf: {
                if (type->is_error()) { expr->type = type; break; }

                if (unop.expression->value_kind != ExprValueKind::RValue) {
                    context.report_compiler_diagnostic(expr->loc, "RValue address of operation ('&&') requries an rvalue");
                }

                TypeInfo* new_type = TypeInfo::create_pointer(type, false);
                expr->type = new_type;
                break;
            }

            case UnaryOperatorKind::Dereference: {
                expr->value_kind = ExprValueKind::LValue;
                if (type->is_error()) { expr->type = type; break; }

                require_rvalue(unop.expression);

                if (type->is_pointer()) {
                    if (type->pointer.base->is_void()) {
                        context.report_compiler_diagnostic(expr->loc, "Cannot dereference a void*");
                    }
                } else {
                    context.report_compiler_diagnostic(expr->loc, "Dereferencing requires a pointer type");
                    expr->type = TypeInfo::get_error();
                    break;
                }

                expr->type = type->pointer.base;
                break;
            }

            case UnaryOperatorKind::Increment: {
                if (!unop.expression->type->is_error()) {
                    if (!unop.expression->type->is_numeric()) {
                        context.report_compiler_diagnostic(unop.expression->loc, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(unop.expression->type)));
                        expr->type = TypeInfo::get_error();
                        break;
                    }
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    context.report_compiler_diagnostic(unop.expression->loc, "Expression must be a modifiable lvalue");
                    expr->type = TypeInfo::get_error();
                    break;
                }

                expr->type = unop.expression->type;
                expr->value_kind = ExprValueKind::RValue;
                break;
            }

            case UnaryOperatorKind::Decrement: {
                if (!unop.expression->type->is_error()) {
                    if (!unop.expression->type->is_numeric()) {
                        context.report_compiler_diagnostic(unop.expression->loc, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(unop.expression->type)));
                        expr->type = TypeInfo::get_error();
                        break;
                    }
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    context.report_compiler_diagnostic(unop.expression->loc, "Expression must be a modifiable lvalue");
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
                    context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
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
                    context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
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
                    context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::Eq: {
                expr->type = LHS->type;
                expr->value_kind = ExprValueKind::LValue;

                if (LHS->value_kind != ExprValueKind::LValue) {
                    context.report_compiler_diagnostic(LHS->loc, "Expression must be a modifiable lvalue");
                    return;
                }

                if (is_const_expr(LHS)) {
                    context.report_compiler_diagnostic(LHS->loc, "Cannot assign to constant expression");
                    return;
                }

                if (!is_assignable_expr(LHS)) {
                    context.report_compiler_diagnostic(LHS->loc, "Must be an assignable expression");
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
                    context.report_compiler_diagnostic(expr->loc, "Discarding result of expression", CompilerDiagKind::Warning);
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
            context.report_compiler_diagnostic(LHS->loc, "Expression must be a modifiable lvalue");
            return;
        }

        if (is_const_expr(LHS)) {
            context.report_compiler_diagnostic(LHS->loc, "Cannot assign to constant expression");
            return;
        }

        if (!is_assignable_expr(LHS)) {
            context.report_compiler_diagnostic(LHS->loc, "Must be an assignable expression");
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
            case ExprKind::Member: resolve_member_expr(expr); break;
            case ExprKind::BuiltinMember: resolve_builtin_member_expr(expr); break;
            case ExprKind::Self: resolve_self_expr(expr); break;
            case ExprKind::Call: resolve_call_expr(expr); break;
            case ExprKind::BuiltinCall: resolve_builtin_call_expr(expr); break;
            case ExprKind::IntrinsicCall: resolve_intrinsic_call_expr(expr); break;
            case ExprKind::Construct: resolve_construct_expr(expr); break;
            case ExprKind::ArrayLiteral: resolve_array_literal_expr(expr); break;
            case ExprKind::MethodCall: resolve_method_call_expr(expr); break;
            case ExprKind::ArraySubscript: resolve_array_subscript_expr(expr); break;
            case ExprKind::ToSlice: resolve_to_slice_expr(expr); break;
            case ExprKind::Paren: resolve_paren_expr(expr); break;
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
        Module* mod = nullptr;
        
        // We may be referencing ourselves
        if (compare_module_names(name.identifier, context.active_comp_unit->parent->name)) {
            name.referenced_module = context.active_comp_unit->parent;
            return;
        }

        for (Decl* import : context.active_comp_unit->imports) {
            ARIA_ASSERT(import->kind == DeclKind::Import, "Invalid import stmt");

            if (compare_module_names(name.identifier, import->import.alias.empty() ? import->import.name : import->import.alias)) {
                if (mod) {
                    context.report_compiler_diagnostic(specifier->loc, fmt::format("Ambigous name specifier '{}'", name.identifier));
                    return;
                }

                mod = import->import.resolved_module;
                break;
            }
        }

        if (!mod) {
            for (Module* mod : context.modules) {
                if (compare_module_names(name.identifier, mod->name)) {
                    context.report_compiler_diagnostic_with_notes(specifier->loc, fmt::format("Could not find module '{}'", name.identifier),
                        { fmt::format("This error can be resolved by adding 'import {0}'", mod->name) });
                    return;
                }
            }

            context.report_compiler_diagnostic(specifier->loc, fmt::format("Could not find module '{}'", name.identifier));
            return;
        } else {
            name.referenced_module = mod;
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
                return true;

            case ExprKind::DeclRef:
                return expr->decl_ref.referenced_decl->kind == DeclKind::Var && expr->decl_ref.referenced_decl->var.const_var;

            case ExprKind::Construct: {
                return expr->construct.is_const;
            }

            case ExprKind::Paren:
                return is_const_expr(expr->paren.expression);

            case ExprKind::ImplicitCast:
                return is_const_expr(expr->implicit_cast.expression);

            case ExprKind::UnaryOperator:
                return is_const_expr(expr->unary_operator.expression);

            case ExprKind::BinaryOperator:
                return is_const_expr(expr->binary_operator.lhs) && is_const_expr(expr->binary_operator.rhs);

            default: return false;
        }
    }

    Expr* SemanticAnalyzer::eval_const_expr(Expr* expr) {
        ARIA_ASSERT(is_const_expr(expr), "Cannot evaulate a non-constant expression");

        switch (expr->kind) {
            case ExprKind::Error: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Error));

            case ExprKind::BooleanLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Boolean, expr->boolean_literal.value));

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
            case ExprKind::ArraySubscript: return is_assignable_expr(expr->array_subscript.array);

            case ExprKind::Paren: return is_assignable_expr(expr->paren.expression);
            case ExprKind::Cast: return is_assignable_expr(expr->cast.expression);
            case ExprKind::ImplicitCast: return is_assignable_expr(expr->implicit_cast.expression);

            case ExprKind::UnaryOperator: {
                switch (expr->unary_operator.op) {
                    case UnaryOperatorKind::Increment:
                    case UnaryOperatorKind::Decrement: return true;

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

    void SemanticAnalyzer::try_insert_implicit_cast(TypeInfo* dst_type, Expr* src_expr) {
        ConversionCost cost = get_conversion_cost(dst_type, src_expr->type);

        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(dst_type, src_expr->type, src_expr, cost.kind);
            } else if (cost.explicit_cast_possible) {
                context.report_compiler_diagnostic_with_notes(src_expr->loc, 
                    fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(src_expr->type), type_info_to_string(dst_type)),
                    { "You can however insert an explicit cast in the code" });
            } else {
                context.report_compiler_diagnostic(src_expr->loc, 
                    fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(src_expr->type), type_info_to_string(dst_type)));
            }
        }
    }

    void SemanticAnalyzer::require_rvalue(Expr* expr) {
        if (expr->value_kind == ExprValueKind::LValue) {
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
        if (lhs->type->kind == TypeKind::Error || rhs->type->kind == TypeKind::Error) {
            e->type = TypeInfo::get_error();
            return;
        }

        if (lhs->type->kind == TypeKind::Generic || rhs->type->kind == TypeKind::Generic) {
            e->type = lhs->type;
            return;
        }

        require_rvalue(lhs);
        require_rvalue(rhs);

        if (lhs->type->is_integral()) {
            if (rhs->type->is_integral()) {
                // We want to keep the original types for error messages
                TypeInfo lhs_type = *lhs->type;
                TypeInfo rhs_type = *rhs->type;

                maybe_promote_to_int(lhs);
                maybe_promote_to_int(rhs);

                size_t l_size = lhs->type->get_bit_size();
                size_t r_size = rhs->type->get_bit_size();

                if (l_size > r_size) {
                    insert_implicit_cast(lhs->type, rhs->type, rhs, CastKind::Integral);
                } else if (r_size > l_size) {
                    insert_implicit_cast(rhs->type, lhs->type, lhs, CastKind::Integral);
                } else if (l_size == r_size) {
                    if (lhs->type->is_signed() != rhs->type->is_signed()) {
                        context.report_compiler_diagnostic_with_notes(lhs->loc, 
                            fmt::format("Mismatched types '{}' and '{}'", type_info_to_string(&lhs_type), type_info_to_string(&rhs_type)),
                            { "implicit signedness conversions are not allowed here"} );
                    }
                }

                e->type = lhs->type;
                return;
            } else if (rhs->type->is_floating_point()) {
                insert_implicit_cast(rhs->type, lhs->type, lhs, CastKind::IntegralToFloating);
                e->type = rhs->type;
                return;
            }
        } else if (lhs->type->is_floating_point() && !is_binary_operator_bit(op)) {
            if (rhs->type->is_integral()) {
                insert_implicit_cast(lhs->type, rhs->type, rhs, CastKind::IntegralToFloating);
                e->type = lhs->type;
                return;
            } else if (rhs->type->is_floating_point()) {
                size_t lSize = lhs->type->get_bit_size();
                size_t rSize = rhs->type->get_bit_size();

                if (lSize > rSize) {
                    insert_implicit_cast(lhs->type, rhs->type, rhs, CastKind::Floating);
                } else if (rSize > lSize) {
                    insert_implicit_cast(rhs->type, lhs->type, lhs, CastKind::Floating);
                }

                e->type = lhs->type;
                return;
            }
        } else if (lhs->type->is_pointer() && !is_binary_operator_bit(op)) {
            if (op == BinaryOperatorKind::Add) {
                if (rhs->type->is_integral()) {
                    insert_implicit_cast(TypeInfo::get_sz(), rhs->type, rhs, CastKind::IntegerToPointer);
                    e->type = lhs->type;
                    return;
                } else {
                    context.report_compiler_diagnostic(rhs->loc, fmt::format("Expected an integer here but got '{}'", type_info_to_string(rhs->type)));
                    e->type = lhs->type;
                    return;
                }
            }

            if (rhs->type->is_pointer()) {
                if (op == BinaryOperatorKind::Eq || op == BinaryOperatorKind::IsNotEq) {
                    insert_implicit_cast(TypeInfo::get_void_ptr(), lhs->type, lhs, CastKind::BitCast);
                    insert_implicit_cast(TypeInfo::get_void_ptr(), rhs->type, rhs, CastKind::BitCast);
                    e->type = lhs->type;
                    return;
                }
            }
        }

        context.report_compiler_diagnostic(lhs->loc + rhs->loc, fmt::format("Invalid operands to binary operator '{}' (have '{}' and '{}')", binary_op_kind_to_string(op),
            type_info_to_string(lhs->type), type_info_to_string(rhs->type)));
        e->type = TypeInfo::get_error();
        return;
    }

} // namespace ariac