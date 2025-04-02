add_llvm_component_group(IKRRISC2)

set(LLVM_TARGET_DEFINITIONS IKRRISC2.td)

# tablegen(LLVM IKRRISC2GenInstrInfo.inc -gen-instr-info)
# tablegen(LLVM IKRRISC2.inc -gen-register-info)

add_llvm_target(IKRRISC2CodeGen

  LINK_COMPONENTS
  Target

  ADD_TO_COMPONENT
  IKRRISC2
  )

add_subdirectory(TargetInfo)