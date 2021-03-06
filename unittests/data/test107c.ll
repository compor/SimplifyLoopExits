
; multiple exit loop 
; one of the extra exits overlaps with the header exit

; this is not in loop canonical/simplify form because 
; it does not have dedicated loop exits for loop_exit_original

define void @test() {
entry:
  br label %while.cond

while.cond:                                       ; preds = %if.end.6, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %inc7, %if.end.6 ]
  %i.0 = phi i32 [ 10, %entry ], [ %dec, %if.end.6 ]
  %dec = add nsw i32 %i.0, -1
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %loop_exit_original

while.body:                                       ; preds = %while.cond
  %inc = add nsw i32 %a.0, 1
  %cmp = icmp eq i32 %inc, 3
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  br label %loop_exit_original

if.end:                                           ; preds = %while.body
  %cmp1 = icmp eq i32 %inc, 5
  br i1 %cmp1, label %if.then.2, label %if.end.3

if.then.2:                                        ; preds = %if.end
  br label %loop_exit_b

if.end.3:                                         ; preds = %if.end
  %cmp4 = icmp eq i32 %inc, 7
  br i1 %cmp4, label %if.then.5, label %if.end.6

if.then.5:                                        ; preds = %if.end.3
  br label %loop_exit_c

if.end.6:                                         ; preds = %if.end.3
  %inc7 = add nsw i32 %inc, 1
  br label %while.cond

loop_exit_original:                               ; preds = %while.cond, %if.then
  %a.1 = phi i32 [ %inc, %if.then ], [ %a.0, %while.cond ]
  %inc8 = add nsw i32 %a.1, 1
  br label %loop_exit_b

loop_exit_b:                                      ; preds = %loop_exit_original, %if.then.2
  %a.2 = phi i32 [ %inc8, %loop_exit_original ], [ %inc, %if.then.2 ]
  %add = add nsw i32 %a.2, 2
  br label %loop_exit_c

loop_exit_c:                                      ; preds = %loop_exit_b, %if.then.5
  %a.3 = phi i32 [ %add, %loop_exit_b ], [ %inc, %if.then.5 ]
  %add9 = add nsw i32 %a.3, 3
  ret void
}

