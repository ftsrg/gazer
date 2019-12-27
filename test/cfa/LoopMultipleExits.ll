; RUN: %cfa -no-simplify-expr -elim-vars=off -memory=havoc "%s" | diff -B -Z "%p/Expected/LoopMultipleExits.cfa" -

declare i32 @__VERIFIER_nondet_int()

define i32 @main() {
entry:
    %limit = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.calculate ]
    %sum = phi i32 [ 0, %entry ], [ %s, %loop.calculate ]
    %cond = icmp slt i32 %i, %limit
    br i1 %cond, label %loop.body, label %loop.end
loop.body:
    %a = call i32 @__VERIFIER_nondet_int()
    %error.cond = icmp slt i32 %a, 0
    br i1 %error.cond, label %error, label %loop.calculate
loop.calculate:
    %i1 = add nsw i32 %i, 1
    %s = add nsw i32 %sum, %a
    %c = call i32 @__VERIFIER_nondet_int()
    %c1 = trunc i32 %c to i1
    br i1 %c1, label %loop.end, label %loop.header
loop.end:
    ret i32 %sum
error:
    ret i32 1
}