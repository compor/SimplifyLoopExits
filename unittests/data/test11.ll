define void @test() {
entry:
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %a = alloca i32, align 4
  store i32 100, i32* %i, align 4
  store i32 0, i32* %j, align 4
  store i32 0, i32* %a, align 4
  br label %while.cond

while.cond:
  %0 = load i32, i32* %i, align 4
  %dec = add nsw i32 %0, -1
  store i32 %dec, i32* %i, align 4
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %while.end

while.body:
  store i32 0, i32* %j, align 4
  br label %for.cond

for.cond:
  %1 = load i32, i32* %j, align 4
  %cmp = icmp slt i32 %1, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %2 = load i32, i32* %a, align 4
  %inc = add nsw i32 %2, 1
  store i32 %inc, i32* %a, align 4
  br label %for.inc

for.inc:
  %3 = load i32, i32* %j, align 4
  %inc1 = add nsw i32 %3, 1
  store i32 %inc1, i32* %j, align 4
  br label %for.cond

for.end:
  br label %while.cond

while.end:
  ret void
}

