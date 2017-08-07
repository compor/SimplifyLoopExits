; test bug taken from 429.mcf sort_basket() of SPEC CPU2006 v1.1

; loop metadata should be transfer to new loop latch when the old latch is also
; exiting or conditionally jumping to a location in addition to the header

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck  %s


@basket = common global [10 x i32] zeroinitializer, align 16

define i32 @test() {
entry:
  br label %do.body

do.body:                                          ; preds = %if.end, %entry
  %r.0 = phi i32 [ 40, %entry ], [ %r.1, %if.end ]
  %l.0 = phi i32 [ 0, %entry ], [ %l.1, %if.end ]
  %cmp = icmp slt i32 %l.0, %r.0
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %do.body
  %idxprom = sext i32 %l.0 to i64
  %arrayidx = getelementptr inbounds i32, i32* undef, i64 %idxprom
  %0 = load i32, i32* %arrayidx, align 4
  %idxprom1 = sext i32 %r.0 to i64
  %arrayidx2 = getelementptr inbounds i32, i32* undef, i64 %idxprom1
  %1 = load i32, i32* %arrayidx2, align 4
  %idxprom3 = sext i32 %l.0 to i64
  %arrayidx4 = getelementptr inbounds i32, i32* undef, i64 %idxprom3
  store i32 %1, i32* %arrayidx4, align 4
  %idxprom5 = sext i32 %r.0 to i64
  %arrayidx6 = getelementptr inbounds i32, i32* undef, i64 %idxprom5
  store i32 %0, i32* %arrayidx6, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %do.body
  %cmp7 = icmp sle i32 %l.0, %r.0
  %inc = add nsw i32 %l.0, 1
  %dec = add nsw i32 %r.0, -1
  %r.1 = select i1 %cmp7, i32 %dec, i32 %r.0
  %l.1 = select i1 %cmp7, i32 %inc, i32 %l.0
  %cmp10 = icmp sle i32 %l.1, %r.1
  br i1 %cmp10, label %do.body, label %return, !llvm.loop !0
; CHECK-LABEL: old_latch:
; CHECK-NOT: !llvm.loop !0

; CHECK-LABEL: sle_latch:
; CHECK: !llvm.loop !0

return:                                           ; preds = %if.end
  ret i32 %r.1
}

!0 = distinct !{!0, !2}
!2 = !{!"icsa.dynapar.loop.id", i32 17}
; CHECK: !{!"icsa.dynapar.loop.id", i32 17}

