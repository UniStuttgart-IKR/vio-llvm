#include "llvm/ADT/MapVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include <vector>

namespace llvm {

    inline unsigned align(Module *M, unsigned PrimitivesBytes, std::vector<Type*> *Primitives, Type *CurrentTy) {
        Align A = M->getDataLayout().getABITypeAlign(CurrentTy);
        uint64_t Offset = offsetToAlignment(PrimitivesBytes, A);
        while (Offset > 0) {
            if (Offset % 8 == 0 && Offset / 8 > 0) {
                Primitives->push_back(Type::getInt64Ty(M->getContext()));
                PrimitivesBytes += 8;
                Offset -= 8;
            } else if (Offset % 4 == 0 && Offset / 4 > 0) {
                Primitives->push_back(Type::getInt32Ty(M->getContext()));
                PrimitivesBytes += 4;
                Offset -= 4;
            } else if (Offset % 2 == 0 && Offset / 2 > 0) {
                Primitives->push_back(Type::getInt16Ty(M->getContext()));
                PrimitivesBytes += 2;
                Offset -= 2;
            } else if (Offset > 0) {
                Primitives->push_back(Type::getInt8Ty(M->getContext()));
                PrimitivesBytes += 1;
                Offset -= 1;
            }
        }
        return PrimitivesBytes;
    }

    typedef SmallMapVector<StructType*, std::pair<StructType*, StructType*>, 32> BufferType;
    typedef SmallMapVector<StructType*, std::vector<unsigned>, 32> BufferIndices;

    inline void splitStructs(Module &M, std::vector<StructType *> StructTypes, BufferType *ReplaceBuffer, BufferIndices *ReplaceIndices = nullptr) {
        for (StructType *ST : StructTypes){
            unsigned PrimitivesBytes = 0;
            std::vector<Type*> *Primitives = new std::vector<Type*>();
            std::vector<Type*> *Pointers = new std::vector<Type*>();
            std::vector<unsigned> NewIndices = std::vector<unsigned>();
            for (unsigned i = 0; i < ST->getNumElements(); ++i) {
                if (ST->getElementType(i)->isPointerTy()) {
                    NewIndices.push_back(Pointers->size());
                    Pointers->push_back(ST->getElementType(i));
                } else if (!ST->getElementType(i)->isArrayTy()) { //aka is Primitive
                    PrimitivesBytes = align(&M, PrimitivesBytes, Primitives, ST->getElementType(i));
                    NewIndices.push_back(Primitives->size());
                    Primitives->push_back(ST->getElementType(i));
                    PrimitivesBytes += (ST->getElementType(i)->getPrimitiveSizeInBits()/8);
                } else { // is Array
                    ArrayType *ATy = cast<ArrayType>(ST->getElementType(i));
                    while (ATy->getElementType()->isArrayTy())
                        ATy = cast<ArrayType>(ATy->getElementType());
                    if (ATy->getElementType()->isPointerTy()) {
                        NewIndices.push_back(Pointers->size());
                        Pointers->push_back(ST->getElementType(i));
                    } else {
                        PrimitivesBytes = align(&M, PrimitivesBytes, Primitives, ST->getElementType(i));
                        NewIndices.push_back(Primitives->size());
                        Primitives->push_back(ST->getElementType(i));
                        PrimitivesBytes += (ST->getElementType(i)->getPrimitiveSizeInBits()/8);
                    }
                }
            }
            if (Pointers->size() > 0 && Primitives->size() > 0) {
                if (ReplaceIndices)
                    ReplaceIndices->insert_or_assign(ST, NewIndices);
                std::string OldName = ST->getStructName().str();
                StructType *NewPrmStruct = StructType::create(ArrayRef<Type*>(*Primitives), OldName.append(".prm"));
                StructType *NewPtrStruct = StructType::create(ArrayRef<Type*>(*Pointers), OldName.substr(0, OldName.size()-4).append(".ptr"));
                std::pair<StructType*, StructType*> NewPair = { NewPtrStruct, NewPrmStruct };
                ReplaceBuffer->insert_or_assign(ST, NewPair);
            }
        }
    }
} // namespace llvm