; RUNNING TEST EXAMPLE: llvm-lit llvm/test/Transforms/Yk/ConditionalPromoteCalls.ll
; RUN: llc -stop-after yk-conditional-promote-calls-pass --yk-conditional-promote-calls < %s | FileCheck %s

; Test that the ConditionalPromoteCalls pass transforms promote calls to check the
; thread-local tracing state before making the call.
;
; The transformation creates three blocks per promote call:
;   - promote.check: checks tracing state, marked with "promote-check-bb" metadata
;   - do_promote: contains the actual promote call, marked with "promote-bb" metadata
;   - promote.continue: contains the PHI, marked with "promote-continue-bb" metadata
;
; When not tracing (state == 0), we branch directly to promote.continue,
; skipping the promote call.

; Declare the promote function
declare i32 @__yk_promote_i32(i32)

; Declare the idempotent promote function
declare i32 @__yk_idempotent_promote_i32(i32)

; CHECK-LABEL: @test_promote
; CHECK: entry:
; CHECK:   br label %entry.promote.check
;
; Check block with metadata marker
; CHECK: entry.promote.check:
; CHECK:   load i8, ptr @__yk_thread_tracing_state, {{.*}}!yk-promote-bb-purpose
; CHECK:   icmp eq i8
; CHECK:   br i1 {{.*}}, label %entry.promote.continue, label %entry.do_promote
;
; Do promote block with metadata marker
; CHECK: entry.do_promote:
; CHECK:   call i32 @__yk_promote_i32(i32 %val), !yk-promote-bb-purpose
; CHECK:   br label %entry.promote.continue
;
; Continue block with metadata marker on PHI
; CHECK: entry.promote.continue:
; CHECK:   phi i32 [ %promoted, %entry.do_promote ], [ %val, %entry.promote.check ], !yk-promote-bb-purpose

define dso_local i32 @test_promote(i32 %val) {
entry:
  %promoted = call i32 @__yk_promote_i32(i32 %val)
  %result = add i32 %promoted, 1
  ret i32 %result
}

; CHECK-LABEL: @test_idempotent_promote
; CHECK: entry:
; CHECK:   br label %entry.promote.check
; CHECK: entry.promote.check:
; CHECK:   load i8, ptr @__yk_thread_tracing_state, {{.*}}!yk-promote-bb-purpose
; CHECK:   icmp eq i8
; CHECK:   br i1 {{.*}}, label %entry.promote.continue, label %entry.do_promote
; CHECK: entry.do_promote:
; CHECK:   call i32 @__yk_idempotent_promote_i32(i32 %val), !yk-promote-bb-purpose
; CHECK:   br label %entry.promote.continue
; CHECK: entry.promote.continue:
; CHECK:   phi i32 [ %promoted, %entry.do_promote ], [ %val, %entry.promote.check ], !yk-promote-bb-purpose
define dso_local i32 @test_idempotent_promote(i32 %val) {
entry:
  %promoted = call i32 @__yk_idempotent_promote_i32(i32 %val)
  %result = mul i32 %promoted, 2
  ret i32 %result
}

; CHECK-LABEL: @test_multiple_promotes
; Test that multiple promote calls in the same function are all transformed.
; CHECK: load i8, ptr @__yk_thread_tracing_state, {{.*}}!yk-promote-bb-purpose
; CHECK: br i1 {{.*}}, label %entry.promote.continue, label %entry.do_promote
; CHECK: call i32 @__yk_promote_i32{{.*}}!yk-promote-bb-purpose
; CHECK: phi i32 {{.*}}!yk-promote-bb-purpose
; CHECK: load i8, ptr @__yk_thread_tracing_state, {{.*}}!yk-promote-bb-purpose
; CHECK: call i32 @__yk_promote_i32{{.*}}!yk-promote-bb-purpose
; CHECK: phi i32 {{.*}}!yk-promote-bb-purpose
define dso_local i32 @test_multiple_promotes(i32 %a, i32 %b) {
entry:
  %promoted_a = call i32 @__yk_promote_i32(i32 %a)
  %promoted_b = call i32 @__yk_promote_i32(i32 %b)
  %result = add i32 %promoted_a, %promoted_b
  ret i32 %result
}

; CHECK-LABEL: @test_no_promote
; Test that functions without promote calls are unchanged.
; CHECK-NOT: @__yk_thread_tracing_state
; CHECK: add i32
; CHECK: ret i32
define dso_local i32 @test_no_promote(i32 %val) {
entry:
  %result = add i32 %val, 42
  ret i32 %result
}

; Verify metadata definitions exist
; CHECK: !{!"promote-check-bb"}
; CHECK: !{!"promote-bb"}
; CHECK: !{!"promote-continue-bb"}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "target-cpu"="x86-64" }

!llvm.module.flags = !{!0, !1, !2}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 1}
