echo "compiling llvm and lld"
BUILDDIR=build
mkdir $BUILDDIR ; cd $BUILDDIR
cmake -G Ninja -DCMAKE_BUILD_TYPE="MinSizeRel"  -DLLVM_USE_SPLIT_DWARF=True  -DCMAKE_INSTALL_PREFIX="../$BUILDDIR"  -DLLVM_OPTIMIZED_TABLEGEN=True  -DLLVM_BUILD_TESTS=False  -DLLVM_DEFAULT_TARGET_TRIPLE="riscv32-unknown-linux-gnu"  -DLLVM_TARGETS_TO_BUILD="RISCV" -DLLVM_ENABLE_PROJECTS="clang;lld" ../llvm
cmake --build . --target install

echo "moving components to swkit"
cp -rf bin ../../vio-swkit/tools/ -p
