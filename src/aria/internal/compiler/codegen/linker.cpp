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

        for (CompilationUnit* comp : m_Context->CompilationUnits) {
            finalOpCodes.reserve(finalOpCodes.size() + comp->OpCodes.size());
            finalOpCodes.insert(finalOpCodes.end(), comp->OpCodes.begin(), comp->OpCodes.end());

            finalReflection.Declarations.insert(comp->ReflectionData.Declarations.begin(), comp->ReflectionData.Declarations.end());
        }

        m_Context->OpCodes = finalOpCodes;
        m_Context->ReflectionData = finalReflection;
    }

} // namespace Aria::Internal