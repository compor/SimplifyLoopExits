
; loop with continue statement

define void @foo() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %while.cond.backedge, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %a.0.be, %while.cond.backedge ]
  %i.0 = phi i32 [ 100, %entry ], [ %dec, %while.cond.backedge ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 50
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %while.cond.backedge

while.cond.backedge:                              ; preds = %if.then, %if.end
  %a.0.be = phi i32 [ %inc, %if.then ], [ %inc1, %if.end ]
  br label %while.cond

if.end:                                           ; preds = %while.body
  %inc1 = add nsw i32 %inc, 1
  br label %while.cond.backedge

loop_exit_original:                               ; preds = %while.cond
  ret void
}

