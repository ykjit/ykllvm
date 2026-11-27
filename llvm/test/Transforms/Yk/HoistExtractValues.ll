; RUN: llc --yk-hoist-extractvalues --yk-insert-stackmaps -print-after=yk-stackmaps -o /dev/null < %s 2>&1 | FileCheck %s

declare { i1, i32 } @make_struct()

; An `extractvalue` in a later block is moved to just after the call, so the
; stackmap at the branch tracks the fields `%found` and `%flags` instead of
; `%struct`.
; CHECK-LABEL: @call(
; CHECK: %struct = call { i1, i32 } @make_struct()
; CHECK-NEXT: %found = extractvalue { i1, i32 } %struct, 0
; CHECK-NEXT: %flags = extractvalue { i1, i32 } %struct, 1
; CHECK-NEXT: call {{.*}}@llvm.experimental.stackmap(i64 1, i32 0, i1 %found, i32 %flags)
; CHECK-NEXT: br
define i32 @call() {
entry:
  %struct = call { i1, i32 } @make_struct()
  %found = extractvalue { i1, i32 } %struct, 0
  br i1 %found, label %is_found, label %not_found
is_found:
  %flags = extractvalue { i1, i32 } %struct, 1
  ret i32 %flags
not_found:
  ret i32 0
}

; An `extractvalue` of a phi is moved to the start of the phi's block, since
; it cannot be placed between phis.
; CHECK-LABEL: @phi(
; CHECK: %merged = phi
; CHECK-NEXT: %flags = extractvalue { i1, i32 } %merged, 1
; CHECK-NEXT: call
define i32 @phi(i1 %cond) {
entry:
  %first = call { i1, i32 } @make_struct()
  br i1 %cond, label %other, label %join
other:
  %second = call { i1, i32 } @make_struct()
  br label %join
join:
  %merged = phi { i1, i32 } [ %first, %entry ], [ %second, %other ]
  call { i1, i32 } @make_struct()
  %flags = extractvalue { i1, i32 } %merged, 1
  ret i32 %flags
}
