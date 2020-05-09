; RUN: %cfa -no-simplify-expr -elim-vars=off -memory=havoc "%s" | /usr/bin/diff -B -Z "%p/Expected/NestedLoopExit.cfa" -

define i32 @main() {
bb:
  br label %bb5

bb5:
  br label %bb6

bb6:
  br i1 undef, label %bb8, label %bb9

bb8:
  br i1 undef, label %bb6, label %.preheader.preheader

.preheader.preheader:
  br label %.preheader

.preheader:
  br label %.preheader

bb9:
  br i1 undef, label %bb5, label %bb10

bb10:
  ret i32 0
}
