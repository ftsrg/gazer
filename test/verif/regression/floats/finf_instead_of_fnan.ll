; This test failed due to an incorrect translation of FIsNaN queries to FIsInf

; RUN: %bmc -bound 1 -skip-pipeline "%s" | FileCheck "%s"
; CHECK: Verification {{SUCCESSFUL|BOUND REACHED}}

define dso_local i32 @main() local_unnamed_addr {
bb:
  %b = alloca float, align 4
  store float 0.000000e+00, float* %b, align 4
  %tmp2 = bitcast float* %b to i32*
  store i32 2143289344, i32* %tmp2, align 4
  br i1 true, label %bb8, label %bb4

bb4:                                              ; preds = %bb
  br label %bb8

bb8:                                              ; preds = %bb, %bb4
  %tmp9 = load float, float* %b, align 4
  %tmp10 = fcmp uno float %tmp9, 0.000000e+00
  %tmp11 = zext i1 %tmp10 to i32
  br i1 %tmp10, label %bb12, label %error

bb12:                                             ; preds = %bb8
  ret i32 0

error:                                            ; preds = %bb8
  call void @gazer.error_code(i16 2)
  unreachable
}

declare void @gazer.error_code(i16) local_unnamed_addr