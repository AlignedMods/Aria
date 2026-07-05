llvm_libs = { 
    "LLVMAArch64AsmParser.lib",
    "LLVMAArch64CodeGen.lib",
    "LLVMAArch64Desc.lib",
    "LLVMAArch64Disassembler.lib",
    "LLVMAArch64Info.lib",
    "LLVMAArch64Utils.lib",
    "LLVMLoongArchAsmParser.lib",
    "LLVMLoongArchCodeGen.lib",
    "LLVMLoongArchDesc.lib",
    "LLVMLoongArchDisassembler.lib",
    "LLVMLoongArchInfo.lib",
    "LLVMAggressiveInstCombine.lib",
    "LLVMAnalysis.lib",
    "LLVMAsmParser.lib",
    "LLVMAsmPrinter.lib",
    "LLVMBinaryFormat.lib",
    "LLVMBitReader.lib",
    "LLVMBitWriter.lib",
    "LLVMBitstreamReader.lib",
    "LLVMCFGuard.lib",
    "LLVMCFIVerify.lib",
    "LLVMCodeGen.lib",
    "LLVMCodeGenTypes.lib",
    "LLVMCore.lib",
    "LLVMCoroutines.lib",
    "LLVMCoverage.lib",
    "LLVMDWARFLinker.lib",
    "LLVMDWARFLinkerClassic.lib",
    "LLVMDWARFLinkerParallel.lib",
    "LLVMDWP.lib",
    "LLVMDebugInfoBTF.lib",
    "LLVMDebugInfoCodeView.lib",
    "LLVMDebugInfoDWARF.lib",
    "LLVMDebugInfoGSYM.lib",
    "LLVMDebugInfoLogicalView.lib",
    "LLVMDebugInfoMSF.lib",
    "LLVMDebugInfoPDB.lib",
    "LLVMDebuginfod.lib",
    "LLVMDemangle.lib",
    "LLVMDiff.lib",
    "LLVMDlltoolDriver.lib",
    "LLVMExecutionEngine.lib",
    "LLVMExegesis.lib",
    "LLVMExegesisAArch64.lib",
    "LLVMExegesisX86.lib",
    "LLVMExtensions.lib",
    "LLVMFileCheck.lib",
    "LLVMFrontendDriver.lib",
    "LLVMFrontendHLSL.lib",
    "LLVMFrontendOffloading.lib",
    "LLVMFrontendOpenACC.lib",
    "LLVMFrontendOpenMP.lib",
    "LLVMFuzzMutate.lib",
    "LLVMFuzzerCLI.lib",
    "LLVMGlobalISel.lib",
    "LLVMHipStdPar.lib",
    "LLVMIRPrinter.lib",
    "LLVMIRReader.lib",
    "LLVMInstCombine.lib",
    "LLVMInstrumentation.lib",
    "LLVMInterfaceStub.lib",
    "LLVMInterpreter.lib",
    "LLVMJITLink.lib",
    "LLVMLTO.lib",
    "LLVMLibDriver.lib",
    "LLVMLineEditor.lib",
    "LLVMLinker.lib",
    "LLVMMC.lib",
    "LLVMMCA.lib",
    "LLVMMCDisassembler.lib",
    "LLVMMCJIT.lib",
    "LLVMMCParser.lib",
    "LLVMMIRParser.lib",
    "LLVMObjCARCOpts.lib",
    "LLVMObjCopy.lib",
    "LLVMObject.lib",
    "LLVMObjectYAML.lib",
    "LLVMOption.lib",
    "LLVMOrcDebugging.lib",
    "LLVMOrcJIT.lib",
    "LLVMOrcShared.lib",
    "LLVMOrcTargetProcess.lib",
    "LLVMPasses.lib",
    "LLVMProfileData.lib",
    "LLVMRISCVAsmParser.lib",
    "LLVMRISCVCodeGen.lib",
    "LLVMRISCVDesc.lib",
    "LLVMRISCVDisassembler.lib",
    "LLVMRISCVInfo.lib",
    "LLVMRISCVTargetMCA.lib",
    "LLVMRemarks.lib",
    "LLVMRuntimeDyld.lib",
    "LLVMScalarOpts.lib",
    "LLVMSelectionDAG.lib",
    "LLVMSupport.lib",
    "LLVMSymbolize.lib",
    "LLVMTableGen.lib",
    "LLVMTableGenCommon.lib",
    "LLVMTarget.lib",
    "LLVMTargetParser.lib",
    "LLVMTextAPI.lib",
    "LLVMTextAPIBinaryReader.lib",
    "LLVMTransformUtils.lib",
    "LLVMVectorize.lib",
    "LLVMWebAssemblyAsmParser.lib",
    "LLVMWebAssemblyCodeGen.lib",
    "LLVMWebAssemblyDesc.lib",
    "LLVMWebAssemblyDisassembler.lib",
    "LLVMWebAssemblyInfo.lib",
    "LLVMWebAssemblyUtils.lib",
    "LLVMWindowsDriver.lib",
    "LLVMWindowsManifest.lib",
    "LLVMX86AsmParser.lib",
    "LLVMX86CodeGen.lib",
    "LLVMX86Desc.lib",
    "LLVMX86Disassembler.lib",
    "LLVMX86Info.lib",
    "LLVMX86TargetMCA.lib",
    "LLVMXRay.lib",
    "LLVMipo.lib",
    "LTO.lib",
    "Remarks.lib",
    "lldCOFF.lib",
    "lldCommon.lib",
    "lldELF.lib",
    "lldMachO.lib",
    "lldMinGW.lib",
    "lldWasm.lib",
    "ntdll" }

workspace "Aria"
    configurations { "Debug", "Release" }
    platforms { "x86", "x86_64" }

    project "aria_test"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/")
        objdir("build/obj/")

        files { "tests/**.cpp", "tests/**.hpp" }

        includedirs { "tests/", "src/", "src/vendor/catch2/", "src/vendor/fmt/include/", "src/vendor/LLVM/include/" }

        filter { "configurations:Debug", "action:vs*" }
            buildoptions { "/Zi" } -- We don't want to use 'symbols "On"' because it also changes the RuntimeLibrary

        filter "configurations:Release"
            optimize "On"

        filter { "action:vs*" }
            buildoptions { "/utf-8", "/Zc:preprocessor", "/MT" }
            libdirs { "src/vendor/LLVM/lib/windows/" }
            defines { "_ITERATOR_DEBUG_LEVEL=0" } -- Fix issue when building in debug
            links { "fmt", llvm_libs }

    project "ariac"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/")
        objdir("build/obj/")

        files { "src/ariac/**.cpp", "src/ariac/**.hpp" }

        includedirs { "src/", "src/vendor/fmt/include/", "src/vendor/LLVM/include/" }

        filter { "configurations:Debug", "action:vs*" }
            buildoptions { "/Zi" } -- We don't want to use 'symbols "On"' because it also changes the RuntimeLibrary

        filter "configurations:Release"
            optimize "On"

        filter { "action:vs*" }
            buildoptions { "/utf-8", "/Zc:preprocessor", "/MP" }
            libdirs { "src/vendor/LLVM/lib/" }
            defines { "_ITERATOR_DEBUG_LEVEL=0" } -- Fix issue when building in debug
            links { "fmt", "winmm", "Ws2_32", llvm_libs }
                
    project "fmt"
        language "C++"
        cppdialect "C++20"
        kind "StaticLib"

        targetdir("build/bin/")
        objdir("build/obj/")

        files { "src/vendor/fmt/src/format.cc", "src/vendor/fmt/src/os.cc" }

        includedirs { "src/vendor/fmt/include/" }

        filter { "action:vs*" }
            buildoptions { "/utf-8" }

        filter {}

        filter { "configurations:Debug", "action:vs*" }
            buildoptions { "/Zi" } -- We don't want to use 'symbols "On"' because it also changes the RuntimeLibrary

        filter "configurations:Release"
            optimize "On"
            symbols "On"
