; This test failed because the flat memory model did not have support for ConstantAggregateZero initializers
; RUN: %bmc -skip-pipeline -bound 1 "%s" | FileCheck "%s"
; CHECK: Verification SUCCESSFUL

@a = internal unnamed_addr global [2 x double] zeroinitializer, align 16

define dso_local i32 @main() {
bb:
  %tmp = load i64, i64* bitcast ([2 x double]* @a to i64*), align 16
  store i64 %tmp, i64* bitcast (double* getelementptr inbounds ([2 x double], [2 x double]* @a, i64 0, i64 1) to i64*), align 8
  ret i32 0
}
