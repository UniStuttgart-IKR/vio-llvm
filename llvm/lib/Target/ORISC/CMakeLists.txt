add_llvm_component_group(ORISC)

set(LLVM_TARGET_DEFINITIONS ORISC.td)

tablegen(LLVM ORISCGenAsmWriter.inc      -gen-asm-writer)
tablegen(LLVM ORISCGenInstrInfo.inc      -gen-instr-info)
tablegen(LLVM ORISCGenRegisterInfo.inc   -gen-register-info)
tablegen(LLVM ORISCGenSubtargetInfo.inc  -gen-subtarget)
tablegen(LLVM ORISCGenDAGISel.inc        -gen-dag-isel)
tablegen(LLVM ORISCGenCallingConv.inc    -gen-callingconv)

add_public_tablegen_target(ORISCCommonTableGen)

add_llvm_target(ORISCCodeGen
  ORISCAsmPrinter.cpp
  ORISCFrameLowering.cpp
  Passes/ORISCTransferStructIndicesPass.cpp
  Passes/ORISCTransformLoadStorePointerPass.cpp
  Passes/ORISCEliminatePointerRedundanciesPass.cpp
  Passes/ORISCRejectUnsupportedIRPass.cpp
  Passes/ORISCMoveAllocaOnHeapPass.cpp
  ORISCISelDAGToDAG.cpp
  ORISCISelLowering.cpp
  ORISCInstrInfo.cpp
  ORISCRegisterInfo.cpp
  ORISCSubtarget.cpp
  ORISCTargetMachine.cpp

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

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)