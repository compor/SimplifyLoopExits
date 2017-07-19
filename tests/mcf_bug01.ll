; test bug taken from 429.mcf global_opt() of SPEC CPU2006 v1.1

; def value in loop should be demoted to memory

; RUN: opt -load %bindir/%testeelib -simplify-loop-exits -S < %s | FileCheck  %s

@net = common global i32 0, align 4

define i32 @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %if.end.3, %entry
; CHECK-LABEL: sle_header:
; CHECK-NOT: phi *
  %n.0 = phi i32 [ 5, %entry ], [ %dec, %if.end.3 ]
  %new_arcs.0 = phi i32 [ -1, %entry ], [ %call, %if.end.3 ]
  %tobool = icmp ne i32 %new_arcs.0, 0
  %tobool1 = icmp ne i32 %n.0, 0
  %or.cond = and i1 %tobool, %tobool1
  br i1 %or.cond, label %if.end, label %while.end

if.end:                                           ; preds = %while.cond
  %0 = load i32, i32* @net, align 4
  %call = call i32 @price_out_impl(i32 %0)
  %cmp = icmp slt i32 %call, 0
  br i1 %cmp, label %while.end, label %if.end.3

if.end.3:                                         ; preds = %if.end
  %dec = add nsw i32 %n.0, -1
  br label %while.cond

while.end:                                        ; preds = %if.end, %while.cond
  ret i32 0
}

declare i32 @price_out_impl(i32)

