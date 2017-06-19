
; this is just a multiple exit loop with 2 exits
; no phi nodes under the loop exits
; added in for variety

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck %s

define void @test() {
entry:
  br label %loop_cond

loop_cond:                                       ; preds = %if.end.3, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc4, %if.end.3 ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end.3 ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %loop_cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 3
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %while.body
  br label %loop_exit_a

if.else:                                          ; preds = %while.body
  %cmp1 = icmp eq i32 %inc, 5
  br i1 %cmp1, label %if.then.2, label %if.end

if.then.2:                                        ; preds = %if.else
  br label %loop_exit_b

if.end:                                           ; preds = %if.else
  br label %if.end.3

if.end.3:                                         ; preds = %if.end
  %inc4 = add nsw i32 %inc, 1
  br label %loop_cond

loop_exit_original:                               ; preds = %loop_cond
  br label %loop_exit_a

loop_exit_a:                                      ; preds = %loop_exit_original, %if.then
  br label %loop_exit_b

loop_exit_b:                                      ; preds = %loop_exit_a, %if.then.2
  ret void
}

