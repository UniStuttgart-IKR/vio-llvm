let TargetPrefix = "orisc" in {

def int_orisc_allocate : Intrinsic<[llvm_ptr_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem, IntrHasSideEffects, IntrNoDuplicate, IntrNoMerge, Throws]>;
def int_orisc_loadpointer : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_storepointer : Intrinsic<[], [llvm_ptr_ty, llvm_ptr_ty], [IntrWriteMem, IntrSpeculatable]>;
def int_orisc_loadprim : Intrinsic<[llvm_anyint_ty], [llvm_ptr_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_storeprim : Intrinsic<[], [llvm_ptr_ty, llvm_anyint_ty], [IntrWriteMem, IntrSpeculatable]>;
}