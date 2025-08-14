#ifndef LLVM_LIB_TARGET_ORISC_ORISCMOVEALLOCAONHEAPPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCMOVEALLOCAONHEAPPASS_H

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

class MoveAllocaOnHeapPass : public PassInfoMixin<MoveAllocaOnHeapPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }
    
private:
    FunctionCallee AllocateFn;
    Value *Zero, *One;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitCallInst(CallInst *I);
    bool visitStoreInst(StoreInst *I);
    bool visitReturnInst(ReturnInst *I);
    bool checkArgument(Value *Arg);

    friend PassInfoMixin<MoveAllocaOnHeapPass>;

    struct ObjectSize {
        Value *Pi;
        uint64_t PiConst;
        Value *Dt;
        uint64_t DtConst;
        AllocaInst *AI;

        ObjectSize(AllocaInst *AI, 
                    Value *Pi = nullptr, 
                    uint64_t PiConst = 0, 
                    Value *Dt = nullptr, 
                    uint64_t DtConst = 0){
            this->AI = AI;
            this->Pi = Pi;
            this->PiConst = PiConst;
            this->Dt = Dt;
            this->DtConst = DtConst;
        };

        ObjectSize operator + (const ObjectSize &Other) const {
            Value *NewPi = nullptr;
            uint64_t NewPiConst = 0;
            Value *NewDt = nullptr;
            uint64_t NewDtConst = 0;

            if (!Pi && !Other.Pi) {
                NewPiConst = PiConst + Other.PiConst;
            } else if (!Other.Pi) {
                Value *OtherPiAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    Other.PiConst);
                IRBuilder<> Builder(AI);
                NewPi = Builder.CreateAdd(Pi, OtherPiAsValue);
            } else { //!Pi
                Value *ThisPiAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    PiConst);
                IRBuilder<> Builder(AI);
                NewPi = Builder.CreateAdd(ThisPiAsValue, Other.Pi);
            }

            if (!Dt && !Other.Dt) {
                NewDtConst = DtConst + Other.DtConst;
            } else if (!Other.Dt) {
                Value *OtherDtAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    Other.DtConst);
                IRBuilder<> Builder(AI);
                NewPi = Builder.CreateAdd(Dt, OtherDtAsValue);
            } else { //!Dt
                Value *ThisDtAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    DtConst);
                IRBuilder<> Builder(AI);
                NewDt = Builder.CreateAdd(ThisDtAsValue, Other.Pi);
            }

            return {AI, NewPi, NewPiConst, NewDt, NewDtConst};
        }

        //FIXME: We assume only constant array multipliers are allowed (is this true?)
        ObjectSize operator * (const uint64_t NumElements) const {
            Value *NewPi = nullptr;
            uint64_t NewPiConst = 0;
            Value *NewDt = nullptr;
            uint64_t NewDtConst = 0;

            if (!Pi) {
                NewPiConst = PiConst * NumElements;
            } else {
                Value *NumElsAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    NumElements);
                IRBuilder<> Builder(AI);
                NewPi = Builder.CreateMul(Pi, NumElsAsValue);
            }
            if (!Dt) {
                NewDtConst = DtConst * NumElements;
            } else {
                Value *NumElsAsValue = ConstantInt::get(
                    Type::getInt32Ty(AI->getContext()),
                    NumElements);
                IRBuilder<> Builder(AI);
                NewDt = Builder.CreateMul(Dt, NumElsAsValue);
            }

            return {AI, NewPi, NewPiConst, NewDt, NewDtConst};
        }
    };

    void addTypeSizeToObjectSize(Type *AllocatedType, ObjectSize *OS);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ORISC_ORISCMOVEALLOCAONHEAPPASS_H