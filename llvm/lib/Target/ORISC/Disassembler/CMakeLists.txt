add_llvm_component_library(LLVMORISCDisassembler
  ORISCDisassembler.cpp

  LINK_COMPONENTS
  MCDisassembler
  Support
  ORISCDesc
  ORISCInfo

  ADD_TO_COMPONENT
  ORISC
  )
