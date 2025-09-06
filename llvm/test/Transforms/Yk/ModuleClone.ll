; RUN: llc -stop-after=yk-basicblock-tracer-pass --yk-module-clone --yk-basicblock-tracer < %s  | FileCheck %s

source_filename = "ModuleClone.c"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"Hello, world\00", align 1
@my_global = global i32 42, align 4

declare i32 @printf(ptr, ...)
declare dso_local ptr @yk_mt_new(ptr noundef)
declare dso_local void @yk_mt_hot_threshold_set(ptr noundef, i32 noundef)
declare dso_local i64 @yk_location_new()
declare dso_local void @yk_mt_control_point(ptr noundef, ptr noundef)
declare dso_local i32 @fprintf(ptr noundef, ptr noundef, ...)
declare dso_local void @yk_location_drop(i64)
declare dso_local void @yk_mt_shutdown(ptr noundef)

define dso_local i32 @g_with_address_taken(i32 %x) {
entry:
  %call = call i32 @inc(i32 %x)
  ret i32 %call
}

define dso_local i32 @f_with_address_taken(i32 %x) {
entry:
  %0 = add i32 %x, 1
  %func_ptr = alloca ptr, align 8
  store ptr @g_with_address_taken, ptr %func_ptr, align 8
  %1 = load ptr, ptr %func_ptr, align 8
  ; Indirect call to g_with_address_taken
  %2 = call i32 %1(i32 42)
  ; Direct call to g_with_address_taken
  %3 = call i32 @g_with_address_taken(i32 %0)
  %4 = add i32 %3, 2
  ret i32 %2
}

define dso_local i32 @inc(i32 %x) {
entry:
  %0 = add i32 %x, 1
  ret i32 %0
}

define dso_local i32 @my_func(i32 %x) {
entry:
  %0 = add i32 %x, 1
  %func_ptr = alloca ptr, align 8
  store ptr @f_with_address_taken, ptr %func_ptr, align 8
  %1 = load ptr, ptr %func_ptr, align 8
  ; Indirect call to f_with_address_taken
  %2 = call i32 %1(i32 42)
  ; Direct call to f_with_address_taken
  %3 = call i32 @f_with_address_taken(i32 %0)
  ret i32 %2
}

define dso_local i32 @main() {
entry:
  %0 = call i32 @my_func(i32 10)
  %1 = load i32, ptr @my_global
  %2 = call i32 (ptr, ...) @printf(ptr @.str, i32 %1)
  ret i32 0
}; 

; CHECK: source_filename = "ModuleClone.c"
; CHECK: target triple = "x86_64-pc-linux-gnu"

; Verify that functions with addresses taken are marked as unoptimised and call unoptimised variants
; CHECK-LABEL: define dso_local i32 @g_with_address_taken(i32 %x) !__yk_unopt !0 {
; CHECK-NEXT: entry:
; CHECK-NEXT:   call void @__yk_trace_basicblock(i32 8, i32 0)
; CHECK-NEXT:   %call = call i32 @__yk_unopt_inc(i32 %x)
; CHECK-NEXT:   ret i32 %call
; CHECK-NEXT: }

; Verify that address-taken functions preserve original call semantics whilst using unoptimised variants
; CHECK-LABEL: define dso_local i32 @f_with_address_taken(i32 %x) !__yk_unopt !0 {
; CHECK-NEXT: entry:
; CHECK-NEXT:   call void @__yk_trace_basicblock(i32 9, i32 0)
; CHECK-NEXT:   %0 = add i32 %x, 1
; CHECK-NEXT:   %func_ptr = alloca ptr, align 8
; CHECK-NEXT:   store ptr @g_with_address_taken, ptr %func_ptr, align 8
; CHECK-NEXT:   %1 = load ptr, ptr %func_ptr, align 8
; CHECK-NEXT:   %2 = call i32 %1(i32 42)
; CHECK-NEXT:   %3 = call i32 @g_with_address_taken(i32 %0)
; CHECK-NEXT:   %4 = add i32 %3, 2
; CHECK-NEXT:   ret i32 %2
; CHECK-NEXT: }
  
; Verify that optimised functions utilise dummy tracing calls rather than full tracing
; CHECK-LABEL: define dso_local i32 @inc(i32 %x) !__yk_opt !0 {
; CHECK-NEXT: entry:
; CHECK-NEXT:   call void @__yk_trace_basicblock_dummy(i32 10, i32 0)
; CHECK-NEXT:   %0 = add i32 %x, 1
; CHECK-NEXT:   ret i32 %0
; CHECK-NEXT: }
  
; Verify that optimised functions maintain original semantics for indirect calls (to address-taken functions)
; whilst using optimised variants for direct calls
; CHECK-LABEL: define dso_local i32 @my_func(i32 %x) !__yk_opt !0 {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock_dummy(i32 11, i32 0)
; CHECK-NEXT:    %0 = add i32 %x, 1
; CHECK-NEXT:    %func_ptr = alloca ptr, align 8
; CHECK-NEXT:    store ptr @f_with_address_taken, ptr %func_ptr, align 8
; CHECK-NEXT:    %1 = load ptr, ptr %func_ptr, align 8
; CHECK-NEXT:    %2 = call i32 %1(i32 42)
; CHECK-NEXT:    %3 = call i32 @__yk_opt_f_with_address_taken(i32 %0)
; CHECK-NEXT:    ret i32 %2
; CHECK-NEXT:  }
  
; Verify that the main function is marked as optimised and uses dummy tracing
; CHECK-LABEL: define dso_local i32 @main() !__yk_opt !0 {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock_dummy(i32 12, i32 0)
; CHECK-NEXT:    %0 = call i32 @my_func(i32 10)
; CHECK-NEXT:    %1 = load i32, ptr @my_global, align 4
; CHECK-NEXT:    %2 = call i32 (ptr, ...) @printf(ptr @.str, i32 %1)
; CHECK-NEXT:    ret i32 0
; CHECK-NEXT:  }
  
; Verify that optimised clones of address-taken functions utilise dummy tracing calls
; CHECK-LABEL: define dso_local i32 @__yk_opt_g_with_address_taken(i32 %x) !__yk_opt !0
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock_dummy(i32 13, i32 0)
; CHECK-NEXT:    %call = call i32 @inc(i32 %x)
; CHECK-NEXT:    ret i32 %call
; CHECK-NEXT:  }
  
; Verify that optimised clones preserve indirect call semantics (to original address-taken functions)
; whilst using optimised variants for direct calls
; CHECK-LABEL: define dso_local i32 @__yk_opt_f_with_address_taken(i32 %x) !__yk_opt !0
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock_dummy(i32 14, i32 0)
; CHECK-NEXT:    %0 = add i32 %x, 1
; CHECK-NEXT:    %func_ptr = alloca ptr, align 8
; CHECK-NEXT:    store ptr @g_with_address_taken, ptr %func_ptr, align 8
; CHECK-NEXT:    %1 = load ptr, ptr %func_ptr, align 8
; CHECK-NEXT:    %2 = call i32 %1(i32 42)
; CHECK-NEXT:    %3 = call i32 @__yk_opt_g_with_address_taken(i32 %0)
; CHECK-NEXT:    %4 = add i32 %3, 2
; CHECK-NEXT:    ret i32 %2
; CHECK-NEXT:  }
  
; Verify that unoptimised clones have real tracing calls
; CHECK-LABEL: define dso_local i32 @__yk_unopt_inc(i32 %x) !__yk_unopt !0 {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock(i32 15, i32 0)
; CHECK-NEXT:    %0 = add i32 %x, 1
; CHECK-NEXT:    ret i32 %0
; CHECK-NEXT:  }
  
; Verify that unoptimised clones call original address-taken functions for both direct and indirect calls
; CHECK-LABEL: define dso_local i32 @__yk_unopt_my_func(i32 %x) !__yk_unopt !0 {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock(i32 16, i32 0)
; CHECK-NEXT:    %0 = add i32 %x, 1
; CHECK-NEXT:    %func_ptr = alloca ptr, align 8
; CHECK-NEXT:    store ptr @f_with_address_taken, ptr %func_ptr, align 8
; CHECK-NEXT:    %1 = load ptr, ptr %func_ptr, align 8
; CHECK-NEXT:    %2 = call i32 %1(i32 42)
; CHECK-NEXT:    %3 = call i32 @f_with_address_taken(i32 %0)
; CHECK-NEXT:    ret i32 %2
; CHECK-NEXT:  }
  
; Verify that unoptimised main function calls unoptimised variants and uses real tracing calls.
; CHECK-LABEL: define dso_local i32 @__yk_unopt_main() !__yk_unopt !0 {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    call void @__yk_trace_basicblock(i32 17, i32 0)
; CHECK-NEXT:    %0 = call i32 @__yk_unopt_my_func(i32 10)
; CHECK-NEXT:    %1 = load i32, ptr @my_global, align 4
; CHECK-NEXT:    %2 = call i32 (ptr, ...) @printf(ptr @.str, i32 %1)
; CHECK-NEXT:    ret i32 0
; CHECK-NEXT:  }
  
; Verify that the required tracing function declarations are present in the output
; CHECK-LABEL: declare void @__yk_trace_basicblock(i32, i32)  
; CHECK-LABEL: declare void @__yk_trace_basicblock_dummy(i32, i32)
