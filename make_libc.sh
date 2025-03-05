TARGET="-march=rv32i_zbb_zhm"
C_COMPILER="/usr/local/llvm/bin/clang"
CXX_COMPILER="/usr/local/llvm/bin/clang++"

cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_USE_SPLIT_DWARF=True -DCMAKE_INSTALL_PREFIX="../build" \
-DLLVM_OPTIMIZED_TABLEGEN=True -DLLVM_BUILD_TESTS=False -DLLVM_DEFAULT_TARGET_TRIPLE="riscv32-unknown-elf" \
-DLLVM_TARGETS_TO_BUILD="RISCV" \
-DCMAKE_C_COMPILER=$C_COMPILER -DCMAKE_CXX_COMPILER=$CXX_COMPILER -DLLVM_ENABLE_PROJECTS="clang;lld" ../llvm
cmake --build . --target install
cp -rf lib ../../vio-swkit/tools/ -p
cd ..