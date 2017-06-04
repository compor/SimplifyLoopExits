define void @test() {
  %i = alloca i32, align 4
  %a = alloca i32, align 4
  store i32 100, i32* %i, align 4
  store i32 0, i32* %a, align 4
  br label %1

  %2 = load i32, i32* %i, align 4
  %3 = add nsw i32 %2, -1
  store i32 %3, i32* %i, align 4
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %5, label %14

  %6 = load i32, i32* %a, align 4
  %7 = add nsw i32 %6, 1
  store i32 %7, i32* %a, align 4
  %8 = load i32, i32* %a, align 4
  %9 = icmp eq i32 %8, 50
  br i1 %9, label %10, label %11

  call void @potential_exit(i32 1)
  br label %11

  %12 = load i32, i32* %a, align 4
  %13 = add nsw i32 %12, 1
  store i32 %13, i32* %a, align 4
  br label %1

  ret void
}

declare void @potential_exit(i32)
