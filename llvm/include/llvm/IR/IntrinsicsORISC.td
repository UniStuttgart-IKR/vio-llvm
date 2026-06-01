def ptr_indx_ty : LLVMQualPointerType<1>;

let TargetPrefix = "orisc" in {

def int_orisc_allocate             : Intrinsic<[llvm_ptr_ty], [llvm_i32_ty, llvm_i32_ty],   [IntrNoMem,    IntrHasSideEffects, IntrNoDuplicate, IntrNoMerge, Throws]>;
def int_orisc_box                  : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty, ptr_indx_ty],   [IntrNoMem,    IntrHasSideEffects, IntrNoDuplicate, IntrNoMerge, Throws]>;
def int_orisc_gep                  : Intrinsic<[ptr_indx_ty], [llvm_ptr_ty, ptr_indx_ty],   [IntrNoMem,    IntrSpeculatable]>;
def int_orisc_unbox_base           : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty],                [IntrNoMem,    IntrSpeculatable]>;
def int_orisc_unbox_index          : Intrinsic<[ptr_indx_ty], [llvm_ptr_ty],                [IntrNoMem,    IntrSpeculatable]>;

def int_orisc_null                 : Intrinsic<[llvm_ptr_ty], [],                           [IntrNoMem,    IntrSpeculatable]>;

}