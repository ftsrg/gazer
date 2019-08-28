declare i32 @__VERIFIER_nondet_int(...)

declare void @__VERIFIER_error(...)

define i32 @main() {
entry:
  %call = call i32 (...) @__VERIFIER_nondet_int()
  br label %while.cond

while.cond:
  %call10 = call i32 (...) @__VERIFIER_nondet_int()
  %cmp = icmp eq i32 %call10, 0
  br i1 %cmp, label %out, label %if.end

if.end:
  %cmp11 = icmp ne i32 %call, 0
  %. = select i1 %cmp11, i32 1, i32 0
  %cmp51 = icmp ne i32 %call, 0
  %cmp53 = icmp ne i32 %., 1
  %or.cond = and i1 %cmp51, %cmp53
  br i1 %or.cond, label %ERROR, label %while.cond

out:
  ret i32 0

ERROR:
  call void (...) @__VERIFIER_error()
  unreachable
}
