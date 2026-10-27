; RUN: llc -mtriple=x86_64-unknown-linux-gnu < %s | FileCheck %s --implicit-check-not='{{(call|jmp).*yk_is_interpreting}}'
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -O0 -fast-isel < %s | FileCheck %s --check-prefix=FAST --implicit-check-not='{{(call|jmp).*yk_is_interpreting}}'
; RUN: sed '/^@__yk_thread_tracing_state =/d' %s | llc -mtriple=x86_64-unknown-linux-gnu | FileCheck %s --check-prefix=FALLBACK
; RUN: sed 's/global i8/global i32/' %s | llc -mtriple=x86_64-unknown-linux-gnu | FileCheck %s --check-prefix=FALLBACK
; RUN: sed 's/thread_local(initialexec)/thread_local/' %s | llc -mtriple=x86_64-unknown-linux-gnu | FileCheck %s --check-prefix=FALLBACK
; RUN: sed '/^@__yk_thread_tracing_state =/d' %s | llc -mtriple=x86_64-unknown-linux-gnu -O0 -fast-isel | FileCheck %s --check-prefix=FALLBACK
; RUN: sed 's/global i8/global i32/' %s | llc -mtriple=x86_64-unknown-linux-gnu -O0 -fast-isel | FileCheck %s --check-prefix=FALLBACK
; RUN: sed 's/thread_local(initialexec)/thread_local/' %s | llc -mtriple=x86_64-unknown-linux-gnu -O0 -fast-isel | FileCheck %s --check-prefix=FALLBACK

@__yk_thread_tracing_state = external thread_local(initialexec) global i8

declare zeroext i1 @yk_is_interpreting()
declare void @change_state()

; Check that yk_is_interpreting is lowered directly to a threadlocal read.
define zeroext i1 @query() {
; FALLBACK-LABEL: query:
; FALLBACK: jmp yk_is_interpreting@PLT
; CHECK-LABEL: query:
; CHECK: __yk_thread_tracing_state@GOTTPOFF
; CHECK: cmpb $0, %fs:
; CHECK: sete %al
; CHECK: retq
; FAST-LABEL: query:
; FAST: __yk_thread_tracing_state@GOTTPOFF
; FAST: %fs:
; FAST: sete
; FAST: retq
  %r = tail call zeroext i1 @yk_is_interpreting()
  ret i1 %r
}

; A call can change the tracing state: the second query must read the threadlocal again.
define zeroext i1 @query_across_call() {
; FALLBACK-LABEL: query_across_call:
; FALLBACK: callq yk_is_interpreting@PLT
; FALLBACK: callq change_state@PLT
; FALLBACK: callq yk_is_interpreting@PLT
; FALLBACK: retq
; CHECK-LABEL: query_across_call:
; CHECK: cmpb $0, %fs:
; CHECK: sete
; CHECK: callq change_state
; CHECK: cmpb $0, %fs:
; CHECK: sete
; CHECK: retq
; FAST-LABEL: query_across_call:
; FAST: %fs:
; FAST: callq change_state
; FAST: %fs:
; FAST: retq
  %before = call zeroext i1 @yk_is_interpreting()
  call void @change_state()
  %after = call zeroext i1 @yk_is_interpreting()
  %changed = xor i1 %before, %after
  ret i1 %changed
}

; Traceable functions are marked as optnone, and then use FastISel.
define zeroext i1 @query_optnone() noinline optnone {
; FALLBACK-LABEL: query_optnone:
; FALLBACK: callq yk_is_interpreting@PLT
; FALLBACK: retq
; CHECK-LABEL: query_optnone:
; CHECK: __yk_thread_tracing_state@GOTTPOFF
; CHECK: %fs:
; CHECK: sete
; CHECK: retq
; FAST-LABEL: query_optnone:
; FAST: __yk_thread_tracing_state@GOTTPOFF
; FAST: %fs:
; FAST: sete
; FAST: retq
  %r = call zeroext i1 @yk_is_interpreting()
  ret i1 %r
}
