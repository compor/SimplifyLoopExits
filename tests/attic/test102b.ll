; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)
 
; Handcrafted exampled where the loop exit has predecessors that are not part of
; the loop (not "dedicated" as per LLVM).
; This is used to display the effects of phi nodes at a loop exit block.

define void @test() {
entry:
  %tobool = icmp ne i32 0, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  br label %original_loop_exit

if.end:                                           ; preds = %entry
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %if.end
  %a.0 = phi i32 [ 0, %if.end ], [ %inc, %for.inc ]
  %i.0 = phi i32 [ 0, %if.end ], [ %inc1, %for.inc ]
  %cmp = icmp slt i32 %i.0, 10
  br i1 %cmp, label %for.body, label %original_loop_exit

for.body:                                         ; preds = %for.cond
  %inc = add nsw i32 %a.0, 1
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %inc1 = add nsw i32 %i.0, 1
  br label %for.cond

original_loop_exit:                               ; preds = %for.cond, %if.then
  %handcrafted = phi i32 [ 34, %for.cond ], [ 37, %if.then ]
  br label %after_loop

after_loop:                                       ; preds = %original_loop_exit
  call void @sle_print(i32 %handcrafted)
  ret void
}

declare void @sle_print(i32)

