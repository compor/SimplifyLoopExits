define void @test() {
  %a = alloca i32, align 4
  store i32 0, i32* %a, align 4
  br label %1

  br i1 false, label %2, label %5
  %3 = load i32, i32* %a, align 4
  %4 = add nsw i32 %3, 1
  store i32 %4, i32* %a, align 4
  br label %1

  ret void
}
