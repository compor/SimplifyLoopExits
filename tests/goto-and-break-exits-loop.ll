; loop with goto and break exits

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

while.cond:                                       ; preds = %if.end.3, %entry
; CHECK-LABEL: old_header:
; CHECK: [[SLE_EXIT_COND1:%sle_exit_cond.*]] = select {{.*}}
; CHECK: store i1 [[SLE_EXIT_COND1]], i1* [[SLE_FLAG]]
; CHECK: [[SLE_SWITCH_COND1:%sle_switch_cond.*]] = select {{.*}} {{.*}}, i32 0, {{.*}}
; CHECK: store i32 [[SLE_SWITCH_COND1]], i32* [[SLE_SWITCH]]
  %a.0 = phi i32 [ 0, %entry ], [ %inc4, %if.end.3 ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end.3 ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
; CHECK: br i1 %tobool, label %while.body, label %sle_latch
  br i1 %tobool, label %while.body, label %while.end.loopexit

while.body:                                       ; preds = %while.cond
; CHECK-LABEL: while.body:
; CHECK: [[SLE_EXIT_COND2:%sle_exit_cond.*]] = select {{.*}}
; CHECK: store i1 [[SLE_EXIT_COND2]], i1* [[SLE_FLAG]]
; CHECK: [[SLE_SWITCH_COND2:%sle_switch_cond.*]] = select {{.*}} {{.*}}, i32 1, {{.*}}
; CHECK: store i32 [[SLE_SWITCH_COND2]], i32* [[SLE_SWITCH]]
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 3
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %while.end

if.end:                                           ; preds = %while.body
; CHECK-LABEL: if.end:
; CHECK: [[SLE_EXIT_COND3:%sle_exit_cond.*]] = select {{.*}}
; CHECK: store i1 [[SLE_EXIT_COND3]], i1* [[SLE_FLAG]]
; CHECK: [[SLE_SWITCH_COND3:%sle_switch_cond.*]] = select {{.*}} {{.*}}, i32 2, {{.*}}
; CHECK: store i32 [[SLE_SWITCH_COND3]], i32* [[SLE_SWITCH]]
  %cmp1 = icmp eq i32 %inc, 5
  br i1 %cmp1, label %if.then.2, label %if.end.3

if.then.2:                                        ; preds = %if.end
  br label %loop_exit_b

if.end.3:                                         ; preds = %if.end
  %inc4 = add nsw i32 %inc, 1
  br label %while.cond

; CHECK-LABEL: old_latch:
; CHECK: br label %sle_latch

; CHECK-LABEL: sle_latch:
; CHECK: br label %sle_header

while.end.loopexit:                               ; preds = %while.cond
  br label %while.end

; CHECK-LABEL: sle_exit:
; CHECK: [[SLE_SWITCH_LD:%sle_switch.*]] = load i32, i32* [[SLE_SWITCH]]
; CHECK: switch i32 [[SLE_SWITCH_LD]], label %while.end
; CHECK-NEXT: i32 1, label %if.then
; CHECK-NEXT: i32 2, label %if.then.2

while.end:                                        ; preds = %while.end.loopexit, %if.then
  %a.1 = phi i32 [ %inc, %if.then ], [ %a.0, %while.end.loopexit ]
  call void @sle_print(i32 %a.1)
  call void @sle_print(i32 %dec)
  br label %loop_exit_a

loop_exit_a:                                      ; preds = %while.end
  call void @sle_print(i32 %a.1)
  call void @sle_print(i32 %dec)
  %inc5 = add nsw i32 %a.1, 1
  br label %loop_exit_b

loop_exit_b:                                      ; preds = %loop_exit_a, %if.then.2
  %a.2 = phi i32 [ %inc5, %loop_exit_a ], [ %inc, %if.then.2 ]
  call void @sle_print(i32 %a.2)
  call void @sle_print(i32 %dec)
  %add = add nsw i32 %a.2, 2
  ret void
}

declare void @sle_print(i32)

