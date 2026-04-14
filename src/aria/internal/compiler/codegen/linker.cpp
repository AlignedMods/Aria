#include "aria/internal/compiler/codegen/linker.hpp"

namespace Aria::Internal {

    Linker::Linker(CompilationContext* ctx) {
        m_Context = ctx;

        LinkImpl();
    }

    void Linker::LinkImpl() {
        // // Merge opcodes
        // OpCodes finalOpCodes;
        // CompilerReflectionData finalReflection;
        // 
        // for (Module* mod : m_Context->Modules) {
        //     finalOpCodes.StringTable.insert(finalOpCodes.StringTable.end(), mod->Ops.StringTable.begin(), mod->Ops.StringTable.end());
        //     finalOpCodes.StringTable.insert(finalOpCodes.StringTable.end(), mod->Ops.StringTable.begin(), mod->Ops.StringTable.end());
        // 
        //     finalOpCodes.reserve(finalOpCodes.size() + mod->Ops.OpCodeTable.size());
        //     finalOpCodes.insert(finalOpCodes.end(), mod->Ops.OpCodeTable.begin(), mod->Ops.OpCodeTable.end());
        // 
        //     finalReflection.Declarations.insert(mod->ReflectionData.Declarations.begin(), mod->ReflectionData.Declarations.end());
        // }
        // 
        // m_Context->OpCodes = finalOpCodes;
        // m_Context->ReflectionData = finalReflection;
    }

} // namespace Aria::Internal