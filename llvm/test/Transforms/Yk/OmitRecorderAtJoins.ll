; RUN: llc -stop-after yk-basicblock-tracer-pass --yk-basicblock-tracer < %s | FileCheck %s

; Blocks 1 and 2 test normal recorder omission; block 5 tests omission at joins.
; CHECK-LABEL: define i32 @linear_and_join(i1 %cond)
; CHECK: call preserve_allcc void @__yk_trace_basicblock(i32 0)
; CHECK-NOT: call preserve_allcc void @__yk_trace_basicblock(i32 1)
; CHECK-NOT: call preserve_allcc void @__yk_trace_basicblock(i32 2)
; CHECK: call preserve_allcc void @__yk_trace_basicblock(i32 3)
; CHECK: call preserve_allcc void @__yk_trace_basicblock(i32 4)
; CHECK-NOT: call preserve_allcc void @__yk_trace_basicblock(i32 5)
; CHECK: call preserve_allcc void @__yk_trace_basicblock(i32 6)
; CHECK: ret i32 %value
define i32 @linear_and_join(i1 %cond) #0 {
entry:
  br label %linear
linear:
  br label %fork
fork:
  br i1 %cond, label %left, label %right
left:
  br label %join
right:
  br label %join
join:
  %value = phi i32 [ 1, %left ], [ 2, %right ]
  br label %exit
exit:
  ret i32 %value
}

attributes #0 = { noinline optnone }
