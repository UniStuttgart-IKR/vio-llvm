add_llvm_component_library(LLVMIKRRISC2Info
  IKRRISC2TargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  IKRRISC2
  )
