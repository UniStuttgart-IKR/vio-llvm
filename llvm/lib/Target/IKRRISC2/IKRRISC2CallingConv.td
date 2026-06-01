//===-- IKRRISC2CallingConv.td - Calling Conventions for IKRRISC2 --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This describes the calling conventions for the IKRRISC2 architectures. These
/// conventions assume Int to be 4 bytes and 4 byte aligned.
///
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Return Value Calling Conventions
//===----------------------------------------------------------------------===//

/// IKRRISC2 C return convention.
def RetCC_IKRRISC2 : CallingConv<[
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[R1, R2, R3, R4]>>,
]>;

//===----------------------------------------------------------------------===//
// IKRRISC2 C Calling Convention
//===----------------------------------------------------------------------===//

/// This is the root argument convention for the IKRRISC2 backend.
def CC_IKRRISC2 : CallingConv<[
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[R1, R2, R3, R4, R5, R6, R7, R8, R9]>>,
  CCAssignToStack<4, 4>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved Registers.
//===----------------------------------------------------------------------===//

// R1 - R9
def CSR : CalleeSavedRegs<(sequence "R%u", 10, 29)>;

