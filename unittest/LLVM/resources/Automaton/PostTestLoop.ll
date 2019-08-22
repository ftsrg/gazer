declare i32 @__VERIFIER_nondet_int()

define i32 @main() {
entry:
    %limit = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.header ]
    %i1 = add nsw i32 %i, 1
    %cond = icmp slt i32 %i1, %limit
    br i1 %cond, label %loop.header, label %loop.end
loop.end:
    ret i32 0
}
