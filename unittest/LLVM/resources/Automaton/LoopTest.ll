declare i32 @__VERIFIER_nondet_int()

define i32 @calculate(i32 %x, i32 %y) {
    %sum = add nsw i32 %x, %y
    ret i32 %sum
}

define i32 @main() {
entry:
    %limit = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.body ]
    %sum = phi i32 [ 0, %entry ], [ %s, %loop.body ]
    %cond = icmp slt i32 %i, %limit
    br i1 %cond, label %loop.body, label %loop.end
loop.body:
    %a = call i32 @__VERIFIER_nondet_int()
    %s = call i32 @calculate(i32 %a, i32 %sum)
    %i1 = add nsw i32 %i, 1
    br label %loop.header
loop.end:
    ret i32 %sum
}
