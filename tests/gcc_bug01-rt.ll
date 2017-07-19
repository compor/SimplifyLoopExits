
; based on get_qualified_type() in 403.gcc of SPEC CPU2006 v1.1

; by using the simplifycfg pass, the switch statement was created at the header
; which when lowered caused the assignment of wrong exit switch values as those
; used in the exit switch statement

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)

define i32 @test(i32 %type, i32 %type_quals) {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %t.0 = phi i32 [ 4, %entry ], [ %dec, %for.inc ]
  switch i32 %t.0, label %for.inc [
    i32 0, label %for.end
    i32 3, label %if.then
  ]

if.then:                                          ; preds = %for.cond
  %call = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 %t.0)
  br label %return

for.inc:                                          ; preds = %for.cond
  %dec = add nsw i32 %t.0, -1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %call2 = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 -1)
  br label %return

return:                                           ; preds = %for.end, %if.then
  %retval.0 = phi i32 [ %t.0, %if.then ], [ -1, %for.end ]
  ret i32 %retval.0
}

declare i32 @sle_print(...)

