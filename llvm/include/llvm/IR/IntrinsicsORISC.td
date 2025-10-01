let TargetPrefix = "orisc" in {

def int_orisc_allocate : Intrinsic<[llvm_ptr_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem, IntrHasSideEffects, IntrNoDuplicate, IntrNoMerge, Throws]>;

def int_orisc_loadpointer  : Intrinsic<[llvm_ptr_ty],    [llvm_ptr_ty, llvm_i32_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_loadprim_u32 : Intrinsic<[llvm_anyint_ty], [llvm_ptr_ty, llvm_i32_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_loadprim_u16 : Intrinsic<[llvm_anyint_ty], [llvm_ptr_ty, llvm_i32_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_loadprim_i16 : Intrinsic<[llvm_anyint_ty], [llvm_ptr_ty, llvm_i32_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_loadprim_u8  : Intrinsic<[llvm_anyint_ty], [llvm_ptr_ty, llvm_i32_ty], [IntrReadMem, IntrSpeculatable]>;
def int_orisc_loadprim_i8  : Intrinsic<[llvm_anyint_ty], [llvm_ptr_ty, llvm_i32_ty], [IntrReadMem, IntrSpeculatable]>;

def int_orisc_storepointer  : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty, llvm_ptr_ty],    [IntrWriteMem, IntrSpeculatable]>;
def int_orisc_storeprim_i32 : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty, llvm_anyint_ty], [IntrWriteMem, IntrSpeculatable]>;
def int_orisc_storeprim_i16 : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty, llvm_anyint_ty], [IntrWriteMem, IntrSpeculatable]>;
def int_orisc_storeprim_i8  : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty, llvm_anyint_ty], [IntrWriteMem, IntrSpeculatable]>;

def int_orisc_store  : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty, llvm_any_ty], [IntrWriteMem, IntrSpeculatable]>;

def int_orisc_gep  : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty, llvm_i32_ty], [IntrNoMem, IntrSpeculatable]>;

}