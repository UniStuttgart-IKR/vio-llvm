let TargetPrefix = "orisc" in {

def int_orisc_aliasprotector : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty], [IntrNoMem, IntrWillReturn, IntrSpeculatable]>;
def int_orisc_loadpointer : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_storepointer : Intrinsic<[], [llvm_ptr_ty, llvm_ptr_ty], [IntrWriteMem, IntrSpeculatable]>;

}