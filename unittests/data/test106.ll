
; dead loop

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc, %while.body ]
  br i1 false, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  br label %while.cond

loop_exit_original:                               ; preds = %while.cond
  ret void
}

