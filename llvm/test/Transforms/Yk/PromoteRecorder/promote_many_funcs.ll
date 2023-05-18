; RUN: opt < %s -passes=yk-promote-recorder -S | FileCheck %s

target triple = "x86_64-unknown-linux-gnu"

define dso_local void @f(i64 noundef %w, i64 noundef %x, i64 noundef %y, i64 noundef %z) #0 {
entry:
    ret void
}
attributes #0 = { "yk_promote"="0,2,3" "noinline" }

define dso_local void @g(i64 noundef %w, i64 noundef %x, i64 noundef %y, i64 noundef %z) #1 {
entry:
; CHECK: call void (ptr, i64, ...) @__yk_record_promote_usize(ptr @promote_fname, i64 3, i64 0, i64 %z, i64 2, i64 %z, i64 3, i64 %z)
    call void @f(i64 %z, i64 %z, i64 %z, i64 %z)
    ret void
}
attributes #1 = { "yk_promote"="0,1" "noinline" }

define dso_local i32 @main(i32 noundef %argc, ptr noundef %argv) {
entry:
    %a = sext i32 %argc to i64
    %b = add i64 %a, 1
    %c = add i64 %a, 2
    %d = add i64 %a, 3
; CHECK: call void (ptr, i64, ...) @__yk_record_promote_usize(ptr @promote_fname.1, i64 3, i64 0, i64 %a, i64 2, i64 %c, i64 3, i64 %d)
    call void @f(i64 noundef %a, i64 noundef %b, i64 noundef %c, i64 noundef %d)
; CHECK: call void (ptr, i64, ...) @__yk_record_promote_usize(ptr @promote_fname.2, i64 2, i64 0, i64 %d, i64 1, i64 %c)
    call void @g(i64 noundef %d, i64 noundef %c, i64 noundef %b, i64 noundef %a)
    ret i32 0
}
