add_llvm_component_library(LLVMORISCPasses
  ORISCPromoteInnerStructsPass.cpp
  ORISCEscapeOutgoingLocalPointersPass.cpp
  ORISCSplitMixedStructsPass.cpp
  ORISCReplaceDeAllocLibCallsPass.cpp
  ORISCReplaceNullPointersPass.cpp
  ORISCRejectUnsupportedIRPass.cpp
  ORISCEscapeAllocaPass.cpp
  ORISCBoxUnboxPointersPass.cpp
  ORISCShrinkPointerIndicesPass.cpp
  ORISCSwitchToIndirectbrPass.cpp

  LINK_COMPONENTS
  Analysis
  Core
  Passes
  MC
  Support

  ADD_TO_COMPONENT
  ORISC
)