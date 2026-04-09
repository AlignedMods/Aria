#include "aria/internal/compiler/codegen/linker.hpp"

namespace Aria::Internal {

    Linker::Linker(CompilationContext* ctx) {
        m_Context = ctx;

        LinkImpl();
    }

    void Linker::LinkImpl() {
        // Merge opcodes
        std::vector<OpCode> finalOpCodes;
        CompilerReflectionData finalReflection;

        for (Module* mod : m_Context->Modules) {
            finalOpCodes.reserve(finalOpCodes.size() + mod->OpCodes.size());
            finalOpCodes.insert(finalOpCodes.end(), mod->OpCodes.begin(), mod->OpCodes.end());

            finalReflection.Declarations.insert(mod->ReflectionData.Declarations.begin(), mod->ReflectionData.Declarations.end());
        }

        m_Context->OpCodes = finalOpCodes;
        m_Context->ReflectionData = finalReflection;
    }

} // namespace Aria::Internal