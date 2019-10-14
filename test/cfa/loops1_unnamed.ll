; RUN: %cfa "%s" | diff -B -Z "%p/Expected/loops1_unnamed.cfa" -

; This test aims to verify that the CFA translation works even if the instructions
; in the LLVM IR source have no names.

define i32 @main() {
  %1 = call i32 @__VERIFIER_nondet_int()
  br label %2

; <label>:2:                                      ; preds = %4, %0
  %.01 = phi i32 [ 0, %0 ], [ %6, %4 ]
  %.0 = phi i32 [ 0, %0 ], [ %5, %4 ]
  %3 = icmp slt i32 %.01, %1
  br i1 %3, label %4, label %7

; <label>:4:                                      ; preds = %2
  %5 = add nsw i32 %.0, %.01
  %6 = add nsw i32 %.01, 1
  br label %2

; <label>:7:                                      ; preds = %2
  %8 = icmp eq i32 %.0, 0
  br i1 %8, label %9, label %10

; <label>:9:                                      ; preds = %7
  call void @__VERIFIER_error()
  unreachable

; <label>:10:                                     ; preds = %7
  ret i32 0
}

declare i32 @__VERIFIER_nondet_int()
declare void @__VERIFIER_error()

