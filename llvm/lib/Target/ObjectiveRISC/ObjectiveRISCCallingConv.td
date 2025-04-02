//===-- ObjectiveRISCCallingConv.td - Calling Conventions for Objective-RISC --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This describes the calling conventions for the Objective-RISC architecture.
///
//===----------------------------------------------------------------------===//

// Return Values
def RetCC_ObjectiveRISC : CallingConv<[
  CCIfPtr<CCAssignToReg<[P10]>>,	// b0
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[D10, D11, D12, D3]>>,	// a0, a1, a2, a3
]>;

// Parameters
def CC_ObjectiveRISC : CallingConv<[
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,
  CCIfType<[i32], CCAssignToReg<[D10, D11, D12, D3, D4, D5, D6, D7]>>,
  CCIfType<[ptr], CCAssignToReg<[P10, P11, P12, P3, P4, P5, P6, P7]>>,
  CCAssignToStack<4, 4>
]>;

def CSR : CalleeSavedRegs<(add
  (sequence "D%u", 8, 9),   // s0, s1
  (sequence "D%u", 18, 31), // s2, ..., s15
  (sequence "P%u", 8, 9),   // q0, q1
  (sequence "P%u", 18, 27), // q2, ..., q11
  (sequence "P%u", 28, 31)  // cnst (?), ctxt, frame (?), rpc
)>;
