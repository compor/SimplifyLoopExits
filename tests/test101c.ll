; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck %s

; manually modified code to use true condition branch as loop header exit

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc, %while.body ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %while.body ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  %tobool2 = select i1 %tobool, i1 0, i1 1
; CHECK: {{sle_cond*}}
; CHECK-SAME: or
  br i1 %tobool2, label %loop_exit_original, label %while.body

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  br label %while.cond

loop_exit_original:                               ; preds = %while.cond
  call void @sle_print(i32 %a.0)
  call void @sle_print(i32 %dec)
  ret void
}

declare void @sle_print(i32)
