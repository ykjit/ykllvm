; RUN: llc -stop-after=yk-module-clone-pass --yk-module-clone < %s  | FileCheck %s

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

define dso_local i32 @my_func(i32 %x) {
entry:
  %0 = add i32 %x, 1
  ret i32 %0
}

define dso_local i32 @main() {
entry:
  %0 = call i32 @my_func(i32 10)
  %1 = load i32, ptr @my_global
  %2 = call i32 (ptr, ...) @printf(ptr @.str, i32 %1)
  ret i32 0
}

; ===========================================
; File header checks
; ===========================================
; CHECK: source_filename = "ModuleClone.c"
; CHECK: target triple = "x86_64-pc-linux-gnu"

; ===========================================
; Global variable and string checks
; ===========================================
; CHECK: @.str = private unnamed_addr constant [13 x i8] c"Hello, world\00", align 1
; CHECK: @my_global = global i32 42, align 4

; ===========================================
; Declaration checks
; ===========================================
; CHECK: declare i32 @printf(ptr, ...)
; CHECK: declare dso_local ptr @yk_mt_new(ptr noundef)
; CHECK: declare dso_local void @yk_mt_hot_threshold_set(ptr noundef, i32 noundef)
; CHECK: declare dso_local i64 @yk_location_new()
; CHECK: declare dso_local void @yk_mt_control_point(ptr noundef, ptr noundef)
; CHECK: declare dso_local i32 @fprintf(ptr noundef, ptr noundef, ...)
; CHECK: declare dso_local void @yk_location_drop(i64)
; CHECK: declare dso_local void @yk_mt_shutdown(ptr noundef)

; ===========================================
; Check original function: my_func
; ===========================================
; CHECK-LABEL: define dso_local i32 @my_func(i32 %x)
; CHECK-NEXT: entry:
; CHECK-NEXT: %0 = add i32 %x, 1
; CHECK-NEXT: ret i32 %0

; ===========================================
; Check original function: main
; ===========================================
; CHECK-LABEL: define dso_local i32 @main()
; CHECK-NEXT: entry:
; CHECK-NEXT: %0 = call i32 @my_func(i32 10)
; CHECK-NEXT: %1 = load i32, ptr @my_global
; CHECK-NEXT: %2 = call i32 (ptr, ...) @printf

; ===========================================
; Check cloned function: __yk_clone_my_func
; ===========================================
; CHECK-LABEL: define dso_local i32 @__yk_clone_my_func(i32 %x)
; CHECK-NEXT: entry:
; CHECK-NEXT: %0 = add i32 %x, 1
; CHECK-NEXT: ret i32 %0

; ===========================================
; Check cloned function: __yk_clone_main
; ===========================================
; CHECK-LABEL: define dso_local i32 @__yk_clone_main()
; CHECK-NEXT: entry:
; CHECK-NEXT: %0 = call i32 @__yk_clone_my_func(i32 10)
; CHECK-NEXT: %1 = load i32, ptr @my_global
; CHECK-NEXT: %2 = call i32 (ptr, ...) @printf

