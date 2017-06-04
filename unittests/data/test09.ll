define void @test() {
entry:
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %a = alloca i32, align 4
  store i32 100, i32* %i, align 4
  store i32 100, i32* %j, align 4
  store i32 0, i32* %a, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.end, %entry
  %0 = load i32, i32* %i, align 4
  %dec = add nsw i32 %0, -1
  store i32 %dec, i32* %i, align 4
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %while.end.6.loopexit

while.body:                                       ; preds = %while.cond
  %1 = load i32, i32* %a, align 4
  %inc = add nsw i32 %1, 1
  store i32 %inc, i32* %a, align 4
  br label %while.cond.1

while.cond.1:                                     ; preds = %if.end, %while.body
  %2 = load i32, i32* %j, align 4
  %dec2 = add nsw i32 %2, -1
  store i32 %dec2, i32* %j, align 4
  %tobool3 = icmp ne i32 %dec2, 0
  br i1 %tobool3, label %while.body.4, label %while.end

while.body.4:                                     ; preds = %while.cond.1
  %3 = load i32, i32* %a, align 4
  %cmp = icmp eq i32 %3, 50
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %while.body.4
  br label %while.end.6

if.end:                                           ; preds = %while.body.4
  br label %while.cond.1

while.end:                                        ; preds = %while.cond.1
  %4 = load i32, i32* %a, align 4
  %inc5 = add nsw i32 %4, 1
  store i32 %inc5, i32* %a, align 4
  br label %while.cond

while.end.6.loopexit:                             ; preds = %while.cond
  br label %while.end.6

while.end.6:                                      ; preds = %while.end.6.loopexit, %if.then
  ret void
}
