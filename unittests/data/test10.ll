define void @test() {
entry:
  %i = alloca i32, align 4
  store i32 100, i32* %i, align 4
  br label %while.cond

while.cond:                    
  %0 = load i32, i32* %i, align 4
  %dec = add nsw i32 %0, -1
  store i32 %dec, i32* %i, align 4
  %tobool = icmp ne i32 %dec, 0
  br i1 %tobool, label %while.body, label %while.end

while.body:
  call void @foo()
  call void @bar()
  br label %while.cond

while.end:
  ret void
}

declare void @foo()

declare void @bar()

