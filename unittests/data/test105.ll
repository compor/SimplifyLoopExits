
; infinite loop

define void @test() {
entry:
  br label %while.body

while.body:                                       ; preds = %entry, %while.body
  %a.0 = phi i32 [ 0, %entry ], [ %inc, %while.body ]
  %inc = add nsw i32 %a.0, 1
  br label %while.body

loop_exit_original:                               ; No predecessors!
  ret void
}

