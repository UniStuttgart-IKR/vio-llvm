#include "ORISCTransferStructIndicesPass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <utility>

using namespace llvm;

PreservedAnalyses TransformStructIndicesPass::run(Module &M, ModuleAnalysisManager &AM) {
    bool Changed = false;
    for (Function &F : M)
        for (BasicBlock &BB : F)
            for (Instruction &I : BB)
                if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
                    Changed |= visitGetElementPtrInst(GEP);
                    assert(GEP->getSourceElementType() && "SourceElementType null");
                }
    return Changed ? PreservedAnalyses::all() : PreservedAnalyses::none();
}

const static inline ArrayRef<Value *> NewIndexList(ArrayRef<Value *> OldIndexList, Type *Ty) {
    //Pointers and Primitives are leave nodes
    if (Ty->isPointerTy())
        return OldIndexList;
    if (!Ty->isArrayTy() && !Ty->isStructTy())
        return OldIndexList;

    SmallVector<Value *> *Res = new SmallVector<Value *>();
    ArrayRef<Value *> Tmp = OldIndexList.drop_front();
    Type *ElTy;
    Value *NewIndex;
    if (Ty->isArrayTy()) {
        ElTy = Ty->getArrayElementType();
        NewIndex = OldIndexList.front();
    } else {
        ConstantInt *CI = dyn_cast<ConstantInt>(OldIndexList.front());
        if (!CI) llvm_unreachable("StructElement Indices must be Constant!");
        unsigned Index = CI->getZExtValue();

        TransStructType *TTy = TransStructType::transform(cast<StructType>(Ty));
        if (!TTy->isMixed()) {
            ElTy = Ty->getStructElementType(Index);
            NewIndex = OldIndexList.front();
        } else if (TTy->getPointerIndex(Index) != -1) {
            ElTy = Ty->getStructElementType(TTy->getPointerIndex(Index));
            NewIndex = ConstantInt::get(CI->getType(), TTy->getPointerIndex(Index));
        } else if (TTy->getPrimitiveIndex(Index) != -1) {
            ElTy = Ty->getStructElementType(TTy->getPrimitiveIndex(Index));
            NewIndex = ConstantInt::get(CI->getType(), TTy->getPrimitiveIndex(Index));
        } else {
            llvm_unreachable("Struct is Mixed but Index is neither valid for PointerStruct not PrimStruct");
        }
    }

    Res->push_back(NewIndex);
    Tmp = NewIndexList(Tmp, ElTy);
    for (Value *V : Tmp)
        Res->push_back(V);
    return *Res;
} 

bool TransformStructIndicesPass::visitGetElementPtrInst(GetElementPtrInst *GEP){
    if (!GEP->getSourceElementType()->isStructTy())
        return false;

    TransStructType *TTy = TransStructType::transform(cast<StructType>(GEP->getSourceElementType()));
    if (!TTy->isMixed())
        return false;

    SmallVector<Value *> OldIndeces(GEP->getNumOperands()-2);
    for (unsigned i = 2; i < GEP->getNumOperands(); ++i)
        OldIndeces[i-2] = GEP->getOperand(i);

    Type *CurrTy = GEP->getSourceElementType();
    unsigned Index = 1;
    while (!CurrTy->isArrayTy() && !CurrTy->isStructTy()) {
        ++Index;
        if (CurrTy->isArrayTy()) {
            CurrTy = CurrTy->getArrayElementType();
            continue;
        }
        ConstantInt *CI = dyn_cast<ConstantInt>(GEP->getOperand(Index));
        if (!CI) llvm_unreachable("StructElement Indices must be Constant!");
        CurrTy = CurrTy->getStructElementType(CI->getZExtValue());
    }
    if (CurrTy->isPointerTy())
        GEP->setSourceElementType(TTy->getPointerStructType());
    else
        GEP->setSourceElementType(TTy->getPrimitiveStructType());

    ArrayRef<Value *> NewIndices = NewIndexList(OldIndeces, GEP->getPointerOperandType());
    for (unsigned i = 2; i < GEP->getNumOperands(); ++i)
        GEP->setOperand(i, NewIndices[i-2]);

    return true;
}

static MapVector<StructType *, TransStructType *> TransformMap;

TransStructType *TransStructType::transform(StructType *OS){
    if (TransformMap.contains(OS))
        return TransformMap[OS];

    SmallVector<Type *> PtrsT;
    SmallVector<Type *> PrimT;
    SmallVector<int> PtrsI;
    SmallVector<int> PrimI;

    for (Type *Ty : OS->elements()){
        PtrsI.push_back(-1);
        PrimI.push_back(-1);
        if (Ty->isArrayTy()) {
            ArrayType *ATy = dyn_cast<ArrayType>(Ty);
            Variant V = examineArray(ATy);
            switch (V) {
                case Mixed:
                    PtrsI[PtrsI.size()-1] = PtrsT.size();
                    PrimI[PrimI.size()-1] = PrimT.size();
                    PtrsT.push_back(createPointerArray(ATy));
                    PrimT.push_back(createPrimitiveArray(ATy));
                    break;
                case PointersOnly:
                    PtrsI[PtrsI.size()-1] = PtrsT.size();
                    PtrsT.push_back(ATy);
                    break;
                case PrimitivesOnly:
                    PrimI[PrimI.size()-1] = PrimT.size();
                    PrimT.push_back(ATy);
                    break;
            }
        } else if (Ty->isPointerTy()) {
            PtrsI[PtrsI.size()-1] = PtrsT.size();
            PtrsT.push_back(Ty);
        } else if (Ty->isStructTy()){
            TransStructType *TTy = TransStructType::transform(cast<StructType>(Ty));
            if (TTy->isPointersOnly() || TTy->isMixed()) {
                PtrsI[PtrsI.size()-1] = PtrsT.size();
                PtrsT.push_back(TTy->getPointerStructType());
            }
            if (TTy->isPrimitivesOnly() || TTy->isMixed()) {
                PrimI[PrimI.size()-1] = PrimT.size();
                PrimT.push_back(TTy->getPrimitiveStructType());
            }
        } else { //Primitive Types
            PrimI[PrimI.size()-1] = PrimT.size();
            PrimT.push_back(Ty);
        }
    }

    TransStructType *Result;
    if (PtrsT.empty()) {
        Result = new TransStructType(OS, PrimitivesOnly);
        Result->setPrimitivesIndexMap(PrimI);
    } else if (PrimT.empty()){
        Result = new TransStructType(OS, PointersOnly);
        Result->setPointersIndexMap(PtrsI);
    } else {
        std::string *NewPtrStr = new std::string("struct_ptr");
        std::string *NewPrmStr = new std::string("struct_prm");
        NewPtrStr->append(OS->getStructName().drop_front(6));
        NewPrmStr->append(OS->getStructName().drop_front(6));
        StructType *NewPtrs = StructType::create(PtrsT, *NewPtrStr);
        bool test = NewPtrs->isSized();
        StructType *NewPrms = StructType::create(PrimT, *NewPrmStr);
        test = NewPrms->isSized();
        Result = new TransStructType(OS, NewPtrs, NewPrms, Mixed);
        Result->setPointersIndexMap(PtrsI);
        Result->setPrimitivesIndexMap(PrimI);
    }
    
    TransformMap.insert_or_assign(OS, Result);
    return Result;
}

TransStructType::Variant TransStructType::examineArray(ArrayType *ATy){
    if (ATy->getElementType()->isPointerTy())
        return PointersOnly;
    if (ATy->getElementType()->isStructTy()) {
        transform(cast<StructType>(ATy->getElementType()));
        return Mixed;
    }
    if (ATy->getElementType()->isArrayTy())
        return examineArray(cast<ArrayType>(ATy->getElementType()));
    return PrimitivesOnly;
}

ArrayType *TransStructType::createPrimitiveArray(ArrayType *OrigATy){ 
    return createArray(OrigATy, false);
}

ArrayType *TransStructType::createPointerArray(ArrayType *OrigATy){ 
    return createArray(OrigATy, true);
}

ArrayType *TransStructType::createArray(ArrayType *OrigATy, bool WantPtrs){
    if (OrigATy->getElementType()->isArrayTy()) {
        ArrayType *Res = createArray(cast<ArrayType>(OrigATy->getElementType()), WantPtrs);
        return ArrayType::get(Res, OrigATy->getNumElements());
    }
    TransStructType *Res = TransStructType::transform(cast<StructType>(OrigATy->getElementType()));
    return ArrayType::get(WantPtrs ? Res->getPointerStructType() 
                                                : Res->getPrimitiveStructType(),
                            OrigATy->getNumElements());
}