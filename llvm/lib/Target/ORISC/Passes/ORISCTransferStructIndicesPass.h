#ifndef LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMSTRUCTINDICESPASS_H
#define LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMSTRUCTINDICESPASS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <string>
#include <utility>

namespace llvm {

class TransformStructIndicesPass : public PassInfoMixin<TransformStructIndicesPass>  {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
    static bool isRequired() { return true; }

private:
    FunctionCallee LoadPtrFn;
    FunctionCallee StorePtrFn;

    SmallVector<Instruction *> RemoveFromParentList = SmallVector<Instruction *>();

    bool visitGetElementPtrInst(GetElementPtrInst *GEP);
    bool visitLoadStoreInst(Instruction *I);
    bool checkParentForSimpleGEP(Value *V, bool NeedPointerStruct);
    bool insertAliasProtector(GetElementPtrInst *GEP);
    friend PassInfoMixin<TransformStructIndicesPass>;
};

class TransStructType {
private:
    static TransStructType *create(StructType *Orig, StructType *Ptr, StructType *Prim) {
        return new TransStructType(Orig, Ptr, Prim, Mixed);
    }
    static TransStructType *createPointerOnly(StructType *OS) {
        return new TransStructType(OS, PointersOnly);
    };
    static TransStructType *createPrimitiveOnly(StructType *OS) {
        return new TransStructType(OS, PrimitivesOnly);
    };

    StructType *OriginalStruct;
    StructType *PointerStruct;
    StructType *PrimitivesStruct;
    
    int *PointersIndexMap;
    int *PrimitivesIndexMap;
    enum Variant { PointersOnly, PrimitivesOnly, Mixed };
    Variant Var;

    TransStructType(StructType *Orig, StructType *Ptr, StructType *Prim, Variant V){
        OriginalStruct = Orig;
        PointerStruct = Ptr;
        PrimitivesStruct = Prim;
        Var = V;
    }

    TransStructType(StructType *Orig, Variant V){
        OriginalStruct = Orig;
        Var = V;
    }

    void setPointersIndexMap(int *Map) { PointersIndexMap = Map; }
    void setPrimitivesIndexMap(int *Map) { PrimitivesIndexMap = Map; }
public:
    static TransStructType *transform(StructType *OS);

    bool isMixed() { return Var == Mixed; }
    bool isPointersOnly() { return Var == PointersOnly; }
    bool isPrimitivesOnly() { return Var == PrimitivesOnly; }
    StructType *getPointerStructType() { return Var == Mixed ? PointerStruct : OriginalStruct; }
    StructType *getPrimitiveStructType() { return Var == Mixed ? PrimitivesStruct : OriginalStruct; }
    StructType *getOriginalStructType() { return OriginalStruct; }

    int getPointerIndex(unsigned Index) { return PointersIndexMap[Index]; }
    int getPrimitiveIndex(unsigned Index) { return PrimitivesIndexMap[Index]; }
    
    static Variant examineArray(ArrayType *ATy);
    static ArrayType *createPointerArray(ArrayType *OrigATy);
    static ArrayType *createPrimitiveArray(ArrayType *OrigATy);

private:
    static ArrayType *createArray(ArrayType *OrigATy, bool PointerVariant);
};

} // namespace llvm


#endif // LLVM_LIB_TARGET_ORISC_ORISCTRANSFORMSTRUCTINDICESPASS_H