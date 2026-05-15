; RUN: opt --passes=yk-module-clone -S < %s | FileCheck %s
;
; The pass emits originals in their original order, then appends all clones
; at the end of the module. Tests follow that ordering.

; --- Originals ---

; Test 1a: plain function — original carries no attribute group.
; CHECK-LABEL: define {{.*}} @simple(i32 %x)
; CHECK: ret i32

; Test 2a: caller original still calls the unoptimised callee.
; CHECK-LABEL: define {{.*}} @caller
; CHECK: call i32 @simple

; Test 3a: address-taken function — tracing-state check prepended.
; CHECK-LABEL: define {{.*}} @addr_taken
; CHECK: load i8, ptr @__yk_thread_tracing_state
; CHECK: icmp eq i8 {{.*}}, 0
; CHECK: not_tracing{{.*}}:
; CHECK: tail call {{.*}} @__yk_opt_addr_taken
; CHECK: orig_body:

; Test 4: function containing the control point — no tracing check.
; CHECK-LABEL: define {{.*}} @with_cp
; CHECK-NOT: load i8, ptr @__yk_thread_tracing_state
; CHECK: call void @yk_mt_control_point

; Test 5a: yk_indirect_inline address-taken — tracing check IS prepended.
; CHECK-LABEL: define {{.*}} @indirect_inline
; CHECK: load i8, ptr @__yk_thread_tracing_state
; CHECK: not_tracing{{.*}}:
; CHECK: tail call {{.*}} @__yk_opt_indirect_inline
; CHECK: orig_body:

; --- Clones (appended after all originals) ---

; Test 1b: __yk_opt_simple clone exists.
; CHECK-LABEL: define {{.*}} @__yk_opt_simple
; CHECK: ret i32

; Test 2b: __yk_opt_caller calls the optimised callee.
; CHECK-LABEL: define {{.*}} @__yk_opt_caller
; CHECK: call i32 @__yk_opt_simple

; CHECK-LABEL: define {{.*}} @__yk_opt_addr_taken
; CHECK: ret i32

; CHECK-LABEL: define {{.*}} @__yk_opt_indirect_inline
; CHECK: ret i32

; All clones carry yk_outline (in an attribute group).
; CHECK: attributes #{{[0-9]+}} = { "yk_outline" }

; --- IR ---

define i32 @simple(i32 %x) {
  %r = add i32 %x, 1
  ret i32 %r
}

define i32 @caller(i32 %x) {
  %r = call i32 @simple(i32 %x)
  ret i32 %r
}

@fp = global ptr @addr_taken

define i32 @addr_taken(i32 %x) {
  %r = mul i32 %x, 2
  ret i32 %r
}

declare void @yk_mt_control_point(ptr, ptr)

define void @with_cp(ptr %mt, ptr %loc) {
  call void @yk_mt_control_point(ptr %mt, ptr %loc)
  ret void
}

@fp2 = global ptr @indirect_inline

define i32 @indirect_inline(i32 %x) "yk_indirect_inline" {
  %r = mul i32 %x, 3
  ret i32 %r
}

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"wchar_size", i32 4}
