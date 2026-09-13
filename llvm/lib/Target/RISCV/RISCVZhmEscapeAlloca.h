#ifndef LLVM_LIB_TARGET_RISCV_RISCVZHMESCAPEALLOCA_H
#define LLVM_LIB_TARGET_RISCV_RISCVZHMESCAPEALLOCA_H

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include <cstdint>
#include <utility>

namespace llvm {

class RISCVZhmEscapeAlloca : public RequiredPassInfoMixin<RISCVZhmEscapeAlloca>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee AllocateFn;
    Value *Zero, *One;
    const DataLayout *DL;
    std::vector<StructType *> StructTys;

    typedef SmallMapVector<StructType*, std::pair<StructType*, StructType*>, 32> BufferType;
    BufferType ReplaceBuffer;
    bool FilledReplaceBuffer = false;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitCallInst(CallInst *I);
    bool visitStoreInst(StoreInst *I);
    bool visitReturnInst(ReturnInst *I);
    bool checkArgument(Value *Arg);

    friend PassInfoMixin<RISCVZhmEscapeAlloca>;

    struct ObjectSize {
        Value *Dt;
        uint64_t DtConst;
        AllocaInst *AI;

        ObjectSize(AllocaInst *AI, 
                    Value *Dt = nullptr, 
                    uint64_t DtConst = 0){
            this->AI = AI;
            this->Dt = Dt;
            this->DtConst = DtConst;
        };

        ObjectSize operator + (const ObjectSize &Other) const {
            Value *NewDt = nullptr;
            uint64_t NewDtConst = 0;

            if (!Dt && !Other.Dt) {
                NewDtConst = DtConst + Other.DtConst;
            } else if (!Other.Dt) {
                Value *OtherDtAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    Other.DtConst);
                IRBuilder<> Builder(AI);
                NewDt = Builder.CreateAdd(Dt, OtherDtAsValue);
            } else { //!Dt
                Value *ThisDtAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    DtConst);
                IRBuilder<> Builder(AI);
                NewDt = Builder.CreateAdd(ThisDtAsValue, Other.Dt);
            }

            return {AI, NewDt, NewDtConst};
        }

        //FIXME: We assume only constant array multipliers are allowed (is this true?)
        ObjectSize operator * (const uint64_t NumElements) const {
            Value *NewDt = nullptr;
            uint64_t NewDtConst = 0;

            if (!Dt) {
                NewDtConst = DtConst * NumElements;
            } else {
                Value *NumElsAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    NumElements);
                IRBuilder<> Builder(AI);
                NewDt = Builder.CreateMul(Dt, NumElsAsValue);
            }

            return {AI, NewDt, NewDtConst};
        }
        
        ObjectSize operator * (Value *NumElementsValue) const {
            Value *NewDt = nullptr;
            uint64_t NewDtConst = 0;

            if (!Dt) {
                if (DtConst == 1) {
                    NewDt = NumElementsValue;
                } else if (DtConst != 0) {
                    IRBuilder<> Builder(AI);
                    Value *DtConstAsValue = ConstantInt::get(
                        Type::getInt32Ty(AI->getContext()),
                        DtConst);
                    NewDt = Builder.CreateMul(DtConstAsValue, NumElementsValue);   
                }
            } else {
                IRBuilder<> Builder(AI);
                NewDt = Builder.CreateMul(Dt, NumElementsValue);
            }

            return {AI, NewDt, NewDtConst};
        }
    };
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_RISCV_RISCVZHMESCAPEALLOCA_H