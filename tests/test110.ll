
; multiple exit loop 
; it containts exiting block with more than 2 targets

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)
; XFAIL: *

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %sw.epilog, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc2, %sw.epilog ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %sw.epilog ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  switch i32 %inc, label %sw.default [
    i32 3, label %sw.bb
    i32 5, label %sw.bb.1
  ]

sw.bb:                                            ; preds = %while.body
  br label %loop_exit_a

sw.bb.1:                                          ; preds = %while.body
  br label %loop_exit_b

sw.default:                                       ; preds = %while.body
  br label %sw.epilog

sw.epilog:                                        ; preds = %sw.default
  %inc2 = add nsw i32 %inc, 1
  br label %while.cond

loop_exit_original:                               ; preds = %while.cond
  br label %loop_exit_a

loop_exit_a:                                      ; preds = %loop_exit_original, %sw.bb
  %a.1 = phi i32 [ %inc, %sw.bb ], [ %a.0, %loop_exit_original ]
  %inc3 = add nsw i32 %a.1, 1
  br label %loop_exit_b

loop_exit_b:                                      ; preds = %loop_exit_a, %sw.bb.1
  %a.2 = phi i32 [ %inc, %sw.bb.1 ], [ %inc3, %loop_exit_a ]
  %add = add nsw i32 %a.2, 2
  %add4 = add nsw i32 %add, 3
  ret void
}

declare void @sle_print(i32)

