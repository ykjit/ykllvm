; Checks that the shadow stack debug variable is inserted
;
; RUN: opt -passes='default<O0>,debugify,yk-shadow-stack' -S %s  | FileCheck %s

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare ptr @yk_mt_new();
declare ptr @yk_location_new();
%struct.YkLocation = type { i64 }

; The pass should insert a global variable to hold the shadow stack pointer.
; CHECK: @shadowstack_0 = thread_local global ptr null
; CHECK: ![[SHADOWSTACK:[0-9]+]] = !DILocalVariable(name: "shadowstack",{{.*}}flags: DIFlagArtificial)

define dso_local i32 @f(i32 noundef %x, i32 noundef %y, i32 noundef %z) noinline optnone {
entry:
  %retval = alloca i32, align 4
  %x.addr = alloca i32, align 4
  %y.addr = alloca i32, align 4
  %z.addr = alloca i32, align 4
  store i32 %x, ptr %x.addr, align 4
  store i32 %y, ptr %y.addr, align 4
  store i32 %z, ptr %z.addr, align 4
  %0 = load i32, ptr %x.addr, align 4
  %cmp = icmp sgt i32 %0, 3
  br i1 %cmp, label %if.then, label %if.else

if.then:
  %1 = load i32, ptr %y.addr, align 4
  %2 = load i32, ptr %z.addr, align 4
  %add = add nsw i32 %1, %2
  store i32 %add, ptr %retval, align 4
  br label %return

if.else:
  %3 = load i32, ptr %x.addr, align 4
  %4 = load i32, ptr %y.addr, align 4
  %add1 = add nsw i32 %3, %4
  store i32 %add1, ptr %retval, align 4
  br label %return

return:
  %5 = load i32, ptr %retval, align 4
  ret i32 %5
}

define dso_local void @g() optnone noinline {
entry:
  ret void
}

define dso_local i32 @main(i32 noundef %argc, ptr noundef %argv) noinline optnone {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca ptr, align 8
  %vs = alloca [3 x i32], align 4
  %i = alloca i32, align 4
  %mt_stack = alloca ptr, align 8 ; this should not end up on the shadow stack
  %loc_stack = alloca %struct.YkLocation, align 8 ; nor this.
  store i32 0, ptr %retval, align 4
  store i32 %argc, ptr %argc.addr, align 4
  store ptr %argv, ptr %argv.addr, align 8
  %mt = call ptr @yk_mt_new()
  store ptr %mt, ptr %mt_stack
  %loc = call ptr @yk_location_new()
  store ptr %loc, ptr %loc_stack
  %lrv = load i32, ptr %retval, align 4
  ret i32 %lrv
}
