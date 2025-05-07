add_llvm_component_library(LLVMORISCDesc
  ORISCInstPrinter.cpp
  ORISCMCAsmInfo.cpp
  ORISCMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  ORISCInfo
  TargetParser

  ADD_TO_COMPONENT
  ORISC
  )
