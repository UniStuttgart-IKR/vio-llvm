add_llvm_component_library(LLVMORISCInfo
  ORISCTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  ORISC
  )
