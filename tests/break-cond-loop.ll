; regular loop with phi nodes after loop exit

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck %s

define void @test() {
entry:
; CHECK-LABEL: entry
; CHECK: {{sle_flag.*}}
; CHECK: {{sle_switch.*}}
  %tobool = icmp ne i32 0, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  br label %after_loop

if.end:                                           ; preds = %entry
  br label %loop_cond

loop_cond:                                         ; preds = %for.inc, %if.end
  %a.0 = phi i32 [ 0, %if.end ], [ %inc, %for.inc ]
  %i.0 = phi i32 [ 0, %if.end ], [ %inc1, %for.inc ]
  %cmp = icmp slt i32 %i.0, 10
; CHECK-LABEL: loop_cond
; CHECK: {{sle_cond.* =}}
; CHECK-SAME: and
  br i1 %cmp, label %for.body, label %loop_exit_original

for.body:                                         ; preds = %loop_cond
  %inc = add nsw i32 %a.0, 1
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %inc1 = add nsw i32 %i.0, 1
  br label %loop_cond

loop_exit_original:                               ; preds = %loop_cond
  br label %after_loop

after_loop:                                       ; preds = %loop_exit_original, %if.then
  %before.0 = phi i32 [ 13, %if.then ], [ 26, %loop_exit_original ]
  %a.1 = phi i32 [ 0, %if.then ], [ %a.0, %loop_exit_original ]
  %i.1 = phi i32 [ 0, %if.then ], [ %i.0, %loop_exit_original ]
  call void @sle_print(i32 %a.1)
  call void @sle_print(i32 %i.1)
  ret void
}

declare void @sle_print(i32) #1

