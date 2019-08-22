define i32 @calculate(i32 %x, i32 %y) {
    %sum = add nsw i32 %x, %y
    ret i32 %sum
}
