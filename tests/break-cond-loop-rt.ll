; loop with break condition

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
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %while.end.loopexit

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 5
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %while.end

if.end:                                           ; preds = %while.body
  %inc1 = add nsw i32 %inc, 1
  br label %while.cond

while.end.loopexit:                               ; preds = %while.cond
  br label %while.end

while.end:                                        ; preds = %while.end.loopexit, %if.then
  %a.1 = phi i32 [ %inc, %if.then ], [ %a.0, %while.end.loopexit ]
  call void @sle_print(i32 %dec)
  call void @sle_print(i32 %a.1)
  ret void
}

declare void @sle_print(i32)

