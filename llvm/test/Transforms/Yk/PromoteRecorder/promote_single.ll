; RUN: opt < %s -passes=yk-promote-recorder -S | FileCheck %s

target triple = "x86_64-unknown-linux-gnu"

define dso_local void @f(i64 noundef %x) #0 {
entry:
    ret void
}
attributes #0 = { "yk_promote"="0" }

define dso_local i32 @main(i32 noundef %argc, ptr noundef %argv) {
entry:
    %conv = sext i32 %argc to i64
; CHECK: call void (ptr, i64, ...) @__yk_record_promote_usize(ptr @promote_fname, i64 1, i64 0, i64 %conv)
    call void @f(i64 noundef %conv)
    ret i32 0
}
