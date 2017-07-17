; test bug taken from 482.sphinx3 noncomment_line()

; loop with exiting branch in its latch block

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)

@.str = private unnamed_addr constant [7 x i8] c"#hello\00", align 1

define void @test() {
entry:
  %arrayidx1 = getelementptr inbounds i8, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str, i32 0, i32 0), i64 0
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %while.body ]
  %inc = add nsw i32 %i.0, 1
  %idxprom = sext i32 %i.0 to i64
  %arrayidx = getelementptr inbounds i8, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str, i32 0, i32 0), i64 %idxprom
  %0 = load i8, i8* %arrayidx, align 1
  %tobool = icmp ne i8 %0, 0
  br i1 %tobool, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %1 = load i8, i8* %arrayidx1, align 1
  %conv = sext i8 %1 to i32
  %cmp = icmp ne i32 %conv, 35
  br i1 %cmp, label %while.end, label %while.cond

while.end:                                        ; preds = %while.body, %while.cond
  %res.0 = phi i32 [ -1, %while.cond ], [ 0, %while.body ]
  %call = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 %res.0)
  ret void
}

declare i32 @sle_print(...)

