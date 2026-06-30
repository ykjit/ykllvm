; RUN: llc -stop-after fix-stackmaps-spill-reloads --yk-insert-stackmaps --yk-stackmap-spillreloads-fix -mtriple=x86_64-- < %s | FileCheck %s

; Direct calls to external declarations do not get call stackmaps. If a branch
; stackmap follows such a call, the spill/reload fixer must not move that branch
; stackmap back to the external call.

declare i32 @external(i32)

define i32 @external_then_branch(i32 %x, i32 %y) #0 {
; CHECK-LABEL: name: external_then_branch
; CHECK:       CALL64pcrel32 target-flags(x86-plt) @external
; CHECK:       CMP32rr
; CHECK:       renamable $cl = SETCCr
; CHECK:       STACKMAP {{.*}} $cl, 3
; CHECK:       CMP32rr
; CHECK:       JCC_1
entry:
  %e = call i32 @external(i32 %x)
  %cmp = icmp sgt i32 %e, %y
  br i1 %cmp, label %then, label %else

then:
  %t = add i32 %e, %x
  ret i32 %t

else:
  %f = sub i32 %y, %x
  ret i32 %f
}

define i32 @indirect_then_branch(ptr %fp, i32 %x, i32 %y) #0 {
; CHECK-LABEL: name: indirect_then_branch
; CHECK:       CALL64r
; CHECK:       STACKMAP
; CHECK:       CMP32rr
; CHECK:       renamable $cl = SETCCr
; CHECK:       STACKMAP {{.*}} $cl, 3
; CHECK:       CMP32rr
; CHECK:       JCC_1
entry:
  %i = call i32 %fp(i32 %x)
  %cmp = icmp sgt i32 %i, %y
  br i1 %cmp, label %then, label %else

then:
  %t = add i32 %i, %x
  ret i32 %t

else:
  %f = sub i32 %y, %x
  ret i32 %f
}

attributes #0 = { noinline nounwind "frame-pointer"="all" }
