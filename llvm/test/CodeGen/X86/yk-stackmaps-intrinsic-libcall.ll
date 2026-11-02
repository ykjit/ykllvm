; RUN: llc -mtriple=x86_64-unknown-linux-gnu --yk-insert-stackmaps --yk-stackmap-spillreloads-fix -stop-after=fix-stackmaps-spill-reloads %s -o - | FileCheck %s
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -code-model=large --yk-insert-stackmaps --yk-stackmap-spillreloads-fix -stop-after=fix-stackmaps-spill-reloads %s -o - | FileCheck %s

; Intrinsics such as memset may be lowered to calls in a way that means they
; don't have a stackmap. We need to make sure that subsequent branches don't
; accidentally have their stackmaps associated with such calls.

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1 immarg)

define i32 @memset_then_update(ptr %dst, i64 %len, ptr %counter, i32 %limit) #0 {
; CHECK-LABEL: name: memset_then_update
; CHECK: CALL64
; CHECK-NOT: STACKMAP
; CHECK: = MOV32rm
; CHECK: INC32r
; CHECK: MOV32mr
; CHECK: SETCCr
; CHECK-NEXT: STACKMAP
entry:
  call void @llvm.memset.p0.i64(ptr %dst, i8 0, i64 %len, i1 false)
  %old = load volatile i32, ptr %counter
  %new = add i32 %old, 1
  store volatile i32 %new, ptr %counter
  %cmp = icmp sgt i32 %new, %limit
  br i1 %cmp, label %then, label %else

then:
  ret i32 %new

else:
  ret i32 %limit
}

; switch stackmaps need to be treated similarly.
define i32 @memset_then_switch(ptr %dst, i64 %len, ptr %counter) #0 {
; CHECK-LABEL: name: memset_then_switch
; CHECK: CALL64
; CHECK-NOT: STACKMAP
; CHECK: = MOV32rm
; CHECK: INC32r
; CHECK: MOV32mr
; CHECK: STACKMAP
entry:
  call void @llvm.memset.p0.i64(ptr %dst, i8 0, i64 %len, i1 false)
  %old = load volatile i32, ptr %counter
  %new = add i32 %old, 1
  store volatile i32 %new, ptr %counter
  switch i32 %new, label %other [
    i32 0, label %zero
    i32 10, label %ten
  ]

zero:
  ret i32 1

ten:
  ret i32 2

other:
  ret i32 %new
}

attributes #0 = { noinline nounwind "frame-pointer"="all" }
