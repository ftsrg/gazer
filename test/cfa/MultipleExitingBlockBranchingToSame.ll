; RUN: %cfa -memory=flat "%s" | /usr/bin/diff -B -Z "%p/Expected/MultipleExitingBlockBranchingToSame.cfa" -

define internal void @a(i32 %arg) {
bb:
  %tmp = icmp ne i32 %arg, 0
  call void @llvm.assume(i1 %tmp)
  ret void
}

define i32 @main() {
bb:
  br label %bb3

bb3:                                              ; preds = %.split.split, %bb
  %b.0 = phi float [ 1.600000e+01, %bb ], [ %tmp4, %.split.split ]
  %tmp = fmul float %b.0, 3.000000e+00
  %tmp4 = fmul float %tmp, 2.500000e-01
  %tmp5 = call i1 @gazer.dummy_nondet.i1()
  br i1 %tmp5, label %a_fail, label %.split

.split:                                           ; preds = %bb3
  call void @a(i32 12)
  %tmp6 = fptosi float %tmp4 to i32
  %tmp7 = call i1 @gazer.dummy_nondet.i1()
  br i1 %tmp7, label %a_fail, label %.split.split

.split.split:                                     ; preds = %.split
  call void @a(i32 %tmp6)
  br label %bb3

a_fail:                                           ; preds = %.split, %bb3
  %tmp8 = phi i32 [ 12, %bb3 ], [ %tmp6, %.split ]
  %tmp9 = icmp eq i32 %tmp8, 0
  br i1 %tmp9, label %error1, label %bb10

bb10:                                             ; preds = %a_fail
  br label %UnifiedUnreachableBlock

error1:                                           ; preds = %a_fail
  call void @gazer.error_code(i16 1)
  br label %UnifiedUnreachableBlock

UnifiedUnreachableBlock:                          ; preds = %error1, %bb10
  unreachable
}

declare void @gazer.error_code(i16) local_unnamed_addr

declare void @gazer.dummy.void() local_unnamed_addr

declare i1 @gazer.dummy_nondet.i1() local_unnamed_addr

declare void @llvm.assume(i1)