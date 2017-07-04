; regular loop with phi nodes after loop exit

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck  %s

define void @test() {
entry:
; CHECK-LABEL: entry:
; CHECK: [[SLE_FLAG:%sle_flag.*]] = alloca i1
; CHECK: store i1 true, i1* [[SLE_FLAG]]
; CHECK: [[SLE_SWITCH:%sle_switch.*]] = alloca i32
; CHECK: store i32 0, i32* [[SLE_SWITCH]]
; CHECK: br label %sle_header
  br label %while.cond

; CHECK-LABEL: sle_header:
; CHECK: [[HEADER_COND:%.*]] = load i1, i1* [[SLE_FLAG]]
; CHECK: br i1 [[HEADER_COND]], label %old_header, label %sle_exit

while.cond:                                       ; preds = %if.end, %entry
; CHECK-LABEL: old_header:
; CHECK: [[SLE_EXIT_COND:%sle_exit_cond.*]] = select {{.*}}
; CHECK: store i1 [[SLE_EXIT_COND]], i1* [[SLE_FLAG]]
; CHECK: [[SLE_SWITCH_COND:%sle_switch_cond.*]] = select {{.*}} {{.*}}, i32 0, {{.*}}
; CHECK: store i32 [[SLE_SWITCH_COND]], i32* [[SLE_SWITCH]]
  %a.0 = phi i32 [ 0, %entry ], [ %inc1, %if.end ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
; CHECK: br i1 %tobool, label %while.body, label %sle_latch
  br i1 %tobool, label %while.body, label %while.end.loopexit

while.body:                                       ; preds = %while.cond
; CHECK-LABEL: while.body:
; CHECK: [[SLE_EXIT_COND:%sle_exit_cond.*]] = select {{.*}}
; CHECK: store i1 [[SLE_EXIT_COND]], i1* [[SLE_FLAG]]
; CHECK: [[SLE_SWITCH_COND:%sle_switch_cond.*]] = select {{.*}} {{.*}}, i32 1, {{.*}}
; CHECK: store i32 [[SLE_SWITCH_COND]], i32* [[SLE_SWITCH]]
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 5
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %while.end

; CHECK-LABEL: old_latch:
; CHECK: br label %sle_latch

; CHECK-LABEL: sle_latch:
; CHECK: br label %sle_header

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

; CHECK-LABEL: sle_exit:
; CHECK: [[SLE_SWITCH_LD:%sle_switch.*]] = load i32, i32* [[SLE_SWITCH]]
; CHECK: switch i32 [[SLE_SWITCH_LD]], label %while.end.loopexit
; CHECK-NEXT: i32 1, label %if.then
}

declare void @sle_print(i32) #1

