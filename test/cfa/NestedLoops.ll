; RUN: %cfa -no-simplify-expr -elim-vars=off -memory=havoc "%s" | /usr/bin/diff -B -Z "%p/Expected/NestedLoops.cfa" -

declare i32 @__VERIFIER_nondet_int()

define i32 @main() {
entry:
    %c1 = call i32 @__VERIFIER_nondet_int()
    %c2 = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %result = phi i32 [ 0, %entry ], [ %result1, %loop.latch ]
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.latch ]
    %loop.cond = icmp slt i32 %i, %c1
    br i1 %loop.cond, label %loop.body, label %exit
loop.body:
    %x = call i32 @__VERIFIER_nondet_int()
    br label %nested.header
nested.header:
    %s  = phi i32 [ 0, %loop.body ], [ %s2, %nested.body ]
    %j  = phi i32 [ 0, %loop.body ], [ %j1, %nested.body ]
    %nested.cond = icmp slt i32 %j, %c2
    br i1 %nested.cond, label %nested.body, label %loop.latch
nested.body:
    %y  = call i32 @__VERIFIER_nondet_int()
    %s1 = add nsw i32 %x, %y
    %s2 = add nsw i32 %s, %s1
    %j1 = add nsw i32 %j, 1
    br label %nested.header
loop.latch:
    %result1 = add nsw i32 %result, %s
    %i1 = add nsw i32 %i, 1
    br label %loop.header
exit:
    ret i32 %result
}
