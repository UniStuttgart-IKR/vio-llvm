add_llvm_component_group(IKRRISC2)

set(LLVM_TARGET_DEFINITIONS IKRRISC2.td)

tablegen(LLVM IKRRISC2GenAsmWriter.inc      -gen-asm-writer)
tablegen(LLVM IKRRISC2GenInstrInfo.inc      -gen-instr-info)
tablegen(LLVM IKRRISC2GenRegisterInfo.inc   -gen-register-info)
tablegen(LLVM IKRRISC2GenSubtargetInfo.inc  -gen-subtarget)
tablegen(LLVM IKRRISC2GenDAGISel.inc        -gen-dag-isel)
tablegen(LLVM IKRRISC2GenCallingConv.inc    -gen-callingconv)

add_public_tablegen_target(IKRRISC2CommonTableGen)

add_llvm_target(IKRRISC2CodeGen
  IKRRISC2AsmPrinter.cpp
  IKRRISC2FrameLowering.cpp
  IKRRISC2ISelDAGToDAG.cpp
  IKRRISC2ISelLowering.cpp
  IKRRISC2InstrInfo.cpp
  IKRRISC2RegisterInfo.cpp
  IKRRISC2Subtarget.cpp
  IKRRISC2TargetMachine.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  IKRRISC2Desc
  IKRRISC2Info
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  IKRRISC2
  )

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)