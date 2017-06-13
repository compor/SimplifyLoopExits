; loop with break exit

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %if.end, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc1, %if.end ]
  %i.0 = phi i32 [ 100, %entry ], [ %dec, %if.end ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 50
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %loop_exit_break

if.end:                                           ; preds = %while.body
  %inc1 = add nsw i32 %inc, 1
  br label %while.cond

loop_exit_original:                               ; preds = %while.cond
  br label %loop_exit_break

loop_exit_break:                                  ; preds = %loop_exit_original, %if.then
  ret void
}

