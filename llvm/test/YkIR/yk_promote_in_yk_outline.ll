; Checks the compiler borks if there's a __yk_promote_* in a yk_outline
; function.
;
; RUN: not llc --yk-embed-ir < %s 2>&1 | FileCheck %s

declare ptr @__yk_promote_ptr(ptr)

; CHECK: error: promotion detected in yk_outline annotated function 'f'
define void @f(ptr %p) "yk_outline" {
    call ptr @__yk_promote_ptr(ptr %p) ; invalid!
	ret void
}
