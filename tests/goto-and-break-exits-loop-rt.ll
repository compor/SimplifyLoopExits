; loop with goto and break exits

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %if.end.3, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc4, %if.end.3 ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end.3 ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %while.end.loopexit

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 3
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %while.end

if.end:                                           ; preds = %while.body
  %cmp1 = icmp eq i32 %inc, 5
  br i1 %cmp1, label %if.then.2, label %if.end.3

if.then.2:                                        ; preds = %if.end
  br label %loop_exit_b

if.end.3:                                         ; preds = %if.end
  %inc4 = add nsw i32 %inc, 1
  br label %while.cond

while.end.loopexit:                               ; preds = %while.cond
  br label %while.end

while.end:                                        ; preds = %while.end.loopexit, %if.then
  %a.1 = phi i32 [ %inc, %if.then ], [ %a.0, %while.end.loopexit ]
  call void @sle_print(i32 %a.1)
  call void @sle_print(i32 %dec)
  br label %loop_exit_a

loop_exit_a:                                      ; preds = %while.end
  call void @sle_print(i32 %a.1)
  call void @sle_print(i32 %dec)
  %inc5 = add nsw i32 %a.1, 1
  br label %loop_exit_b

loop_exit_b:                                      ; preds = %loop_exit_a, %if.then.2
  %a.2 = phi i32 [ %inc5, %loop_exit_a ], [ %inc, %if.then.2 ]
  call void @sle_print(i32 %a.2)
  call void @sle_print(i32 %dec)
  %add = add nsw i32 %a.2, 2
  ret void
}

declare void @sle_print(i32)

