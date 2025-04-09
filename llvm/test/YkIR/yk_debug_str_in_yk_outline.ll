; Checks the compiler borks if there's a yk_debug_str in a yk_outline function.
;
; RUN: not llc --yk-embed-ir < %s 2>&1 | FileCheck %s

declare void @yk_debug_str(ptr)

; CHECK: error: yk_debug_str() detected in function 'f'
define void @f(ptr %p) "yk_outline" {
    call void @yk_debug_str(ptr %p) ; invalid!
	ret void
}
