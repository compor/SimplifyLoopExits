define void @test() {
  %a = alloca i32, align 4
  store i32 0, i32* %a, align 4
  br label %1

  %2 = load i32, i32* %a, align 4
  %3 = add nsw i32 %2, 1
  store i32 %3, i32* %a, align 4
  br label %1

  ret void
}
