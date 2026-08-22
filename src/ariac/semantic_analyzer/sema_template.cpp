#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    bool SemanticAnalyzer::deduce_template_type(SourceLoc loc, TypeInfo* param_type, TypeInfo* arg_type, ResolvedTemplateMap& deduced_args) {
        if (param_type->is_pointer() && arg_type->is_pointer()) {
            return deduce_template_type(loc, param_type->pointer.base, arg_type->pointer.base, deduced_args);
        }

        if (param_type->is_slice() && arg_type->is_slice()) {
            return deduce_template_type(loc, param_type->slice.base, arg_type->slice.base, deduced_args);
        }

        if (param_type->is_array() && arg_type->is_array()) {
            if (param_type->array.size != arg_type->array.size) {
                return true;
            }

            return deduce_template_type(loc, param_type->array.base, arg_type->array.base, deduced_args);
        }

        if (param_type->is_struct_specilization() && arg_type->is_struct_specilization()) {
            if (param_type->struct_specilization.arguments.size != arg_type->struct_specilization.arguments.size) {
                return true;
            }

            // Note that the referenced structs could be different, and this is wrong
            // However it is not this functions job to handle those errors,
            // Therefore we can skip this check
            for (size_t i = 0; i < param_type->struct_specilization.arguments.size; i++) {
                return deduce_template_type(loc, param_type->struct_specilization.arguments[i], arg_type->struct_specilization.arguments[i], deduced_args);
            }
        }

        if (param_type->is_template()) {
            if (deduced_args.contains(param_type->template_.resolved_decl)) {
                ResolvedTemplateArg& deduced_arg = deduced_args.at(param_type->template_.resolved_decl);

                if (deduced_arg.is_deduced) {
                    ConversionCost cost = get_conversion_cost(deduced_arg.type, arg_type);

                    // We don't allow any conversions in deduced args
                    if (cost.cast_needed) {
                        report_error(loc, fmt::format("Expected argument to be of type '{}' but is '{}'", deduced_arg.type->to_string(), arg_type->to_string()));
                        report_note(deduced_arg.loc, "Type for generic deduced from this argument");
                        return false;
                    }
                }
            } else { // Deduce the type
                deduced_args[param_type->template_.resolved_decl] = ResolvedTemplateArg(arg_type, true, loc);
            }

            return true;
        }

        // We didn't deduce anything but that is not an error
        return true;
    }

} // namespace ariac