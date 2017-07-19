; test bug taken from FChoose() in 456.hmmer of SPEC CPU 2006 v1.1 */

; this created a runtime bug where when the new loop latch was taken immediately
; causing the header phi node to evaluate to a garbage value from memory because
; that operand of the phi node was rewritten to use the new latch

; it is safer to demote all header phis to memory and avoid having to track
; every single value that is affected by the CFG changes

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s -o %t
; RUN: llvm-as %t -o %t.xform.bc
; RUN: llvm-as %s -o %t.orig.bc
; RUN: clang -o %t.orig %t.orig.bc %utilitydir/%utilitylib
; RUN: clang -o %t.xform %t.xform.bc %utilitydir/%utilitylib
; RUN: diff <(%t.xform) <(%t.orig)

@sre_random.k = internal global i32 0, align 4
@sre_random.r = internal global [4 x float] [float 5.000000e-01, float 0x3FB99999A0000000, float 0x3FC99999A0000000, float 0x3FA99999A0000000], align 16
@test.p = private unnamed_addr constant [6 x float] [float 1.000000e+00, float 9.000000e+00, float 7.000000e+00, float 3.000000e+00, float 4.500000e+00, float 0x4020666660000000], align 16

define float @sre_random() {
entry:
  %0 = load i32, i32* @sre_random.k, align 4
  %rem = srem i32 %0, 4
  store i32 %rem, i32* @sre_random.k, align 4
  %1 = load i32, i32* @sre_random.k, align 4
  %idxprom = sext i32 %1 to i64
  %arrayidx = getelementptr inbounds [4 x float], [4 x float]* @sre_random.r, i32 0, i64 %idxprom
  %2 = load float, float* %arrayidx, align 4
  %conv = fptosi float %2 to i32
  %3 = load i32, i32* @sre_random.k, align 4
  %inc = add nsw i32 %3, 1
  store i32 %inc, i32* @sre_random.k, align 4
  %conv1 = sitofp i32 %conv to float
  ret float %conv1
}

define void @test() {
entry:
  %p = alloca [6 x float], align 16
  %0 = bitcast [6 x float]* %p to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* bitcast ([6 x float]* @test.p to i8*), i64 24, i32 16, i1 false)
  %call = call float @sre_random()
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %sum.0 = phi float [ 0.000000e+00, %entry ], [ %add, %for.inc ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %cmp = icmp slt i32 %i.0, 6
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %idxprom = sext i32 %i.0 to i64
  %arrayidx = getelementptr inbounds [6 x float], [6 x float]* %p, i32 0, i64 %idxprom
  %1 = load float, float* %arrayidx, align 4
  %add = fadd float %sum.0, %1
  %cmp1 = fcmp olt float %call, %add
  br i1 %cmp1, label %if.then, label %for.inc

if.then:                                          ; preds = %for.body
  %call2 = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 %i.0)
  %call3 = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 666)
  br label %return

for.inc:                                          ; preds = %for.body
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %call4 = call float @sre_random()
  %conv = sitofp i32 6 to float
  %mul = fmul float %call4, %conv
  %conv5 = fptosi float %mul to i32
  %call6 = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 %conv5)
  %call7 = call i32 (i32, ...) bitcast (i32 (...)* @sle_print to i32 (i32, ...)*)(i32 777)
  br label %return

return:                                           ; preds = %for.end, %if.then
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i32, i1)

declare i32 @sle_print(...)

