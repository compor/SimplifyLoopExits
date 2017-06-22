; loop with break exit

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck %s

define void @test() {
entry:
; CHECK-LABEL: entry
; CHECK: {{sle_flag.*}}
; CHECK: {{sle_switch.*}}
  br label %loop_cond

loop_cond:                                       ; preds = %if.end, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc1, %if.end ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
; CHECK-LABEL: loop_cond
; CHECK: {{sle_cond.* =}}
; CHECK-SAME: and
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %loop_cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 5
; CHECK-LABEL: while.body
; CHECK: {{sle_switch.*}}
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %loop_exit_break

if.end:                                           ; preds = %while.body
  %inc1 = add nsw i32 %inc, 1
  br label %loop_cond

loop_exit_original:                               ; preds = %loop_cond
  br label %loop_exit_break

loop_exit_break:                                  ; preds = %loop_exit_original, %if.then
  ret void
}

