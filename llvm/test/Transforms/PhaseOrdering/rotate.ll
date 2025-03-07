; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -O3 -S < %s                    | FileCheck %s
; RUN: opt -passes="default<O3>" -S < %s  | FileCheck %s

; This should become a single funnel shift through a combination
; of aggressive-instcombine, simplifycfg, and instcombine.
; https://bugs.llvm.org/show_bug.cgi?id=34924

define i32 @rotl(i32 %a, i32 %b) {
; CHECK-LABEL: @rotl(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[COND:%.*]] = tail call i32 @llvm.fshl.i32(i32 [[A:%.*]], i32 [[A]], i32 [[B:%.*]])
; CHECK-NEXT:    ret i32 [[COND]]
;
entry:
  %cmp = icmp eq i32 %b, 0
  br i1 %cmp, label %end, label %rotbb

rotbb:
  %sub = sub i32 32, %b
  %shr = lshr i32 %a, %sub
  %shl = shl i32 %a, %b
  %or = or i32 %shr, %shl
  br label %end

end:
  %cond = phi i32 [ %or, %rotbb ], [ %a, %entry ]
  ret i32 %cond
}
