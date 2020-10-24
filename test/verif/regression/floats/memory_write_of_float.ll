; This test failed due to unsupported memory writes for floats in the float memory model

; RUN: %bmc -bound 1 -skip-pipeline -memory=flat "%s" | FileCheck "%s"
; CHECK: Verification SUCCESSFUL

define i32 @main() {
bb:
  %b = alloca float, align 4
  store float 0.000000e+00, float* %b, align 4
  br i1 false, label %bb2, label %bb6

bb2:                                              ; preds = %bb
  br label %bb6

bb6:                                              ; preds = %bb2, %bb
  %tmp7 = load float, float* %b, align 4
  %tmp8 = fadd float %tmp7, -4.251000e+03
  %tmp9 = fcmp une float %tmp8, 0.000000e+00
  br i1 %tmp9, label %bb10, label %error

bb10:                                             ; preds = %bb6
  ret i32 0

error:                                            ; preds = %bb6
  call void @gazer.error_code(i16 2)
  unreachable
}

declare void @gazer.error_code(i16)
