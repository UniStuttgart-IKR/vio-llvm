add_llvm_component_library(LLVMObjectiveRISCInfo
  ObjectiveRISCTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  ObjectiveRISC
  )
