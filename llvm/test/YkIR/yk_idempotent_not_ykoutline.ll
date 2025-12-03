; Checks the compiler borks if there's a __yk_promote_* in a yk_outline
; function.
;
; RUN: not llc --yk-embed-ir --yk-basicblock-tracer < %s 2>&1 | FileCheck %s

declare ptr @__yk_promote_ptr(ptr)

; CHECK: error: idempotent function f must also be annotated yk_outline
define void @f(ptr %p) "yk_idempotent" {
    call ptr @__yk_promote_ptr(ptr %p) ; invalid!
	ret void
}
