add_llvm_component_library(LLVMORISCDesc
  ORISCInstPrinter.cpp
  ORISCAsmBackend.cpp
  ORISCMCAsmInfo.cpp
  ORISCMCTargetDesc.cpp
  ORISCTargetStreamer.cpp
  ORISCMCCodeEmitter.cpp
  ORISCELFStreamer.cpp
  ORISCELFObjectWriter.cpp

  LINK_COMPONENTS
  BinaryFormat
  CodeGenTypes
  MC
  Support
  ORISCInfo
  TargetParser

  ADD_TO_COMPONENT
  ORISC
  )
