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

        for (const auto& comp : m_Context->m_CompilationUnits) {
            finalOpCodes.reserve(finalOpCodes.size() + comp.OpCodes.size());
            finalOpCodes.insert(finalOpCodes.end(), comp.OpCodes.begin(), comp.OpCodes.end());

            finalReflection.Declarations.insert(comp.ReflectionData.Declarations.begin(), comp.ReflectionData.Declarations.end());
        }

        m_Context->SetOpCodes(finalOpCodes);
        m_Context->SetReflectionData(finalReflection);
    }

} // namespace Aria::Internal