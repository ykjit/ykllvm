; RUN: opt -passes='lto<O2>' -yk-dont-opt-func-abi -S %s | FileCheck %s --check-prefix=PRESERVE
; RUN: opt -passes='lto<O2>' -yk-dont-opt-func-abi=false -S %s | FileCheck %s --check-prefix=PROMOTE
;
; Check that the "argument promotion" pass doesn't change the ABI of two
; functions whose ABIs we need to be compatible.
;
; PRESERVE-LABEL: define internal {{.*}}void @callee(ptr
; PRESERVE: load i32, ptr
; PROMOTE-LABEL: define internal {{.*}}void @callee(i32
; PROMOTE-NOT: load
; PROMOTE: call void @sink(i32

declare void @sink(i32)

define i32 @load_arg(ptr %p) alwaysinline {
  %v = load i32, ptr %p
  ret i32 %v
}

define internal void @callee(ptr %p) noinline {
  %v = call i32 @load_arg(ptr %p)
  call void @sink(i32 %v)
  ret void
}

define void @entry(ptr %p) {
  call void @callee(ptr %p)
  ret void
}
