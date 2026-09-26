# Zhm backend changes

## New files (llvm/lib/Target/RISCV)

| File | Purpose |
|---|---|
| `RISCVZhmFrameLowering.{h,cpp}` | Zhm frames: `alc(i) sp` + fused `sd sp, 0(sp)`, `ld sp, 0(sp)` epilogue, grows-up layout, frame rule checks |
| `RISCVZhmIRUtils.h` | Hardware model and helpers shared by the IR passes |
| `RISCVZhmIRPasses.h` | Declarations of the three IR passes |
| `RISCVZhmEscapeAlloca.cpp` | Allocas that cannot stay in the sp frame -> alc memory |
| `RISCVZhmLegalizeIR.cpp` | Pointer-bit idioms -> itd, supervisor dtp, memcpy/memmove fixup |
| `RISCVZhmVerifyIR.cpp` | Reports everything still illegal |
| `RISCVZhmLegalizeFrameAddr.cpp` | MIR (pre-RA): address-taken stack objects -> alc |
| `RISCVZhmRemoveZeroInits.cpp` | MIR: removes zero stores into fresh alc memory |
| `RISCVInstrInfoZhm.td` | Zhm instructions, intrinsic patterns, pointer-safe equality compares |

## Pipeline

    addIRPasses    ExpandVariadics (Lowering, Zhm only)
    addPreISel     EscapeAlloca -> LegalizeIR -> VerifyIR
    addPreRegAlloc LegalizeFrameAddr (all levels), RemoveZeroInits (O1+)
    PEI            RISCVZhmFrameLowering

## Runtime symbols the compiler emits (provided by compiler-rt)

    void *__zhm_memcpy(void *dst, const void *src, size_t n);
    void *__zhm_memmove(void *dst, const void *src, size_t n);

## Test

    opt -mtriple=riscv64 -mattr=+zhm -expand-variadics-override=lowering \
      -passes='expand-variadics,riscv-zhm-escape-alloca,riscv-zhm-legalize-ir,riscv-zhm-verify-ir' in.ll
    llc -mtriple=riscv64 -mattr=+zhm -verify-machineinstrs in.ll