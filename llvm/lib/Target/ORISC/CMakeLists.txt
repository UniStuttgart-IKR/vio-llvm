add_llvm_component_group(ORISC)

set(LLVM_TARGET_DEFINITIONS ORISC.td)

tablegen(LLVM ORISCGenAsmWriter.inc           -gen-asm-writer)
tablegen(LLVM ORISCGenInstrInfo.inc           -gen-instr-info)
tablegen(LLVM ORISCGenRegisterInfo.inc        -gen-register-info)
tablegen(LLVM ORISCGenSubtargetInfo.inc       -gen-subtarget)
tablegen(LLVM ORISCGenDAGISel.inc             -gen-dag-isel)
tablegen(LLVM ORISCGenCallingConv.inc         -gen-callingconv)
tablegen(LLVM ORISCGenMCCodeEmitter.inc       -gen-emitter)
tablegen(LLVM ORISCGenDisassemblerTables.inc  -gen-disassembler)

add_public_tablegen_target(ORISCCommonTableGen)

add_llvm_target(ORISCCodeGen
  ORISCAsmPrinter.cpp
  ORISCFrameLowering.cpp
  Passes/ORISCPromoteInnerStructsPass.cpp
  Passes/ORISCEscapeOutgoingLocalPointersPass.cpp
  Passes/ORISCSplitMixedStructsPass.cpp
  Passes/ORISCReplaceDeAllocLibCallsPass.cpp
  Passes/ORISCReplaceNullPointersPass.cpp
  Passes/ORISCRejectUnsupportedIRPass.cpp
  Passes/ORISCEscapeAllocaPass.cpp
  Passes/ORISCBoxUnboxPointersPass.cpp
  Passes/ORISCShrinkPointerIndicesPass.cpp
  ORISCCallingConv.cpp
  ORISCISelDAGToDAG.cpp
  ORISCISelLowering.cpp
  ORISCInstrInfo.cpp
  ORISCRegisterInfo.cpp
  ORISCSubtarget.cpp
  ORISCTargetMachine.cpp
  ORISCTargetObjectFile.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  ORISCDesc
  ORISCInfo
  Passes
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  ORISC
  )

add_subdirectory(Disassembler)
add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)