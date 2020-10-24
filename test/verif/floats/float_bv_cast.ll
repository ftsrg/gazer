; RUN: %bmc -skip-pipeline "%s"
; CHECK: Verification SUCCESSFUL

declare double @gazer.undef_value.double()

define i32 @main()  {
bb:
  %undefv.i = call double @gazer.undef_value.double()
  %tmp = bitcast double %undefv.i to i64
  %.sroa.0.0.extract.trunc.i = trunc i64 %tmp to i32
  %tmp1 = uitofp i32 %.sroa.0.0.extract.trunc.i to float
  ret i32 0
}
