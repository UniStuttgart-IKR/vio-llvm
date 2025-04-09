add_llvm_component_library(LLVMIKRRISC2Desc
  IKRRISC2InstPrinter.cpp
  IKRRISC2MCAsmInfo.cpp
  IKRRISC2MCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser

  ADD_TO_COMPONENT
  IKRRISC2
  )
