
; this is not a loop anymore since there is no backedge

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %entry
  %dec = add nsw i32 10, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 0, 1
  %cmp = icmp eq i32 %inc, 5
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  %cmp1 = icmp eq i32 %inc, 3
  br i1 %cmp1, label %if.then.2, label %if.else

if.then.2:                                        ; preds = %if.then
  br label %loop_exit_b

if.else:                                          ; preds = %if.then
  br label %loop_exit_b

if.end:                                           ; preds = %while.body
  br label %loop_exit_c

loop_exit_original:                                        ; preds = %while.cond
  %inc3 = add nsw i32 0, 1
  br label %loop_exit_b

loop_exit_b:                                      ; preds = %loop_exit_original, %if.else, %if.then.2
  %a.0 = phi i32 [ %inc, %if.then.2 ], [ %inc, %if.else ], [ %inc3, %loop_exit_original ]
  %add = add nsw i32 %a.0, 2
  br label %loop_exit_c

loop_exit_c:                                      ; preds = %loop_exit_b, %if.end
  %a.1 = phi i32 [ %add, %loop_exit_b ], [ %inc, %if.end ]
  %add4 = add nsw i32 %a.1, 3
  ret void
}

