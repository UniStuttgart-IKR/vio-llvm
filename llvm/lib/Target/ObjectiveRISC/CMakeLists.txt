add_llvm_component_group(ObjectiveRISC)

set(LLVM_TARGET_DEFINITIONS ObjectiveRISC.td)

tablegen(LLVM ObjectiveRISCGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM ObjectiveRISC.inc -gen-register-info)

add_llvm_target(ObjectiveRISCCodeGen
  ObjectiveRISCInstrInfo.cpp
  ObjectiveRISCISelLowering.cpp
  ObjectiveRISCRegisterInfo.cpp
  ObjectiveRISCTargetMachine.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  GlobalISel
  IPO
  MC
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils
  Vectorize

  ADD_TO_COMPONENT
  ObjectiveRISC
  )

add_subdirectory(TargetInfo)

