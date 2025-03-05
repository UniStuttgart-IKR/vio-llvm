# The LLVM Compiler Infrastructure

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/llvm/llvm-project/badge)](https://securityscorecards.dev/viewer/?uri=github.com/llvm/llvm-project)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/8273/badge)](https://www.bestpractices.dev/projects/8273)
[![libc++](https://github.com/llvm/llvm-project/actions/workflows/libcxx-build-and-test.yaml/badge.svg?branch=main&event=schedule)](https://github.com/llvm/llvm-project/actions/workflows/libcxx-build-and-test.yaml?query=event%3Aschedule)

Welcome to the LLVM project!

This repository contains the source code for LLVM, a toolkit for the
construction of highly optimized compilers, optimizers, and run-time
environments.

The LLVM project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer.

C-like languages use the [Clang](https://clang.llvm.org/) frontend. This
component compiles C, C++, Objective-C, and Objective-C++ code into LLVM bitcode
-- and from there into object files, using LLVM.

Other components include:
the [libc++ C++ standard library](https://libcxx.llvm.org),
the [LLD linker](https://lld.llvm.org), and more.

## Getting the Source Code and Building LLVM

Consult the
[Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm)
page for information on building and running LLVM.

For information on how to contribute to the LLVM project, please take a look at
the [Contributing to LLVM](https://llvm.org/docs/Contributing.html) guide.

## Getting in touch

Join the [LLVM Discourse forums](https://discourse.llvm.org/), [Discord
chat](https://discord.gg/xS7Z362),
[LLVM Office Hours](https://llvm.org/docs/GettingInvolved.html#office-hours) or
[Regular sync-ups](https://llvm.org/docs/GettingInvolved.html#online-sync-ups).

The LLVM project has adopted a [code of conduct](https://llvm.org/docs/CodeOfConduct.html) for
participants to all modes of communication within the project.

## How to install for vio-swkit

just do:
```
chmod +x install.sh
./install.sh
```


## How ViO was built

```
cd riscv-llvm

mkdir build

cd build

#replace <path_to_riscv_gnu_toolchain> with gnu toolchain parent dir!!
cmake -G Ninja -DCMAKE_BUILD_TYPE="Debug"  \ 
 -DLLVM_USE_SPLIT_DWARF=True \ 
 -DCMAKE_INSTALL_PREFIX="<path_to_riscv_gnu_toolchain>"   \ 
 -DLLVM_OPTIMIZED_TABLEGEN=True  \ 
 -DLLVM_BUILD_TESTS=False \ 
 -DLLVM_DEFAULT_TARGET_TRIPLE="riscv32-unknown-elf"  \ 
 -DDEFAULT_SYSROOT="<path_to_riscv_gnu_toolchain>/riscv32-unknown-elf"  \ 
 -DLLVM_TARGETS_TO_BUILD="RISCV"  \ 
 -DLLVM_ENABLE_PROJECTS="clang" ../llvm

cmake --build . --target install


cmake -G Ninja -DCMAKE_BUILD_TYPE="Debug" -DLLVM_USE_SPLIT_DWARF=True -DCMAKE_INSTALL_PREFIX="../install" -DLLVM_OPTIMIZED_TABLEGEN=True -DLLVM_BUILD_TESTS=False  -DLLVM_DEFAULT_TARGET_TRIPLE="riscv32-unknown-elf" -DLLVM_TARGETS_TO_BUILD="RISCV" -DLLVM_ENABLE_PROJECTS="clang;lld" ../llvm

../install/bin/clang-20 -S -target riscv32-unknown-elf -march=rv32i_zbb_zor -O1 -mcmodel=medany -Wno-incompatible-library-redeclaration -fno-addrsig -fomit-frame-pointer test.c

```