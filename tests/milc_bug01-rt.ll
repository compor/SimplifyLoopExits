; test bug taken from 433.milc gaugefix()

; loop exit blocks were not unique for each exiting block 
; which caused phi node errors since there were orphaned incoming operands 
; (i.e. they had no predecessor block)

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)

define void @test() {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %gauge_iter.0 = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %old_av.0 = phi i32 [ 0, %entry ], [ 20, %for.inc ]
  %del_av.0 = phi i32 [ 0, %entry ], [ %del_av.1, %for.inc ]
  %cmp = icmp slt i32 %gauge_iter.0, 5
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  call void @sle_print(i32 %del_av.0)
  %cmp1 = icmp ne i32 %gauge_iter.0, 0
  br i1 %cmp1, label %if.then, label %if.end.4

if.then:                                          ; preds = %for.body
  %sub = sub nsw i32 20, %old_av.0
  %cmp2 = icmp slt i32 %sub, 1
  br i1 %cmp2, label %for.end, label %if.end.4

if.end.4:                                         ; preds = %if.then, %for.body
  %del_av.1 = phi i32 [ %del_av.0, %for.body ], [ %sub, %if.then ]
  %rem = srem i32 %gauge_iter.0, 2
  %cmp5 = icmp eq i32 %rem, 2
  br i1 %cmp5, label %if.then.6, label %for.inc

if.then.6:                                        ; preds = %if.end.4
  call void @sle_print(i32 13)
  br label %for.inc

for.inc:                                          ; preds = %if.end.4, %if.then.6
  %inc = add nsw i32 %gauge_iter.0, 1
  br label %for.cond

for.end:                                          ; preds = %if.then, %for.cond
  %del_av.2 = phi i32 [ %del_av.0, %for.cond ], [ %sub, %if.then ]
  %cmp8 = icmp eq i32 20, 0
  br i1 %cmp8, label %if.then.9, label %if.end.10

if.then.9:                                        ; preds = %for.end
  call void @sle_print(i32 %del_av.2)
  br label %if.end.10

if.end.10:                                        ; preds = %if.then.9, %for.end
  ret void
}

declare void @sle_print(i32)

