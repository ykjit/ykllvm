; NOTE: Check that stackmaps can track additional registers.
; RUN: llc --yk-stackmap-spillreloads-fix --yk-stackmap-add-locs < %s | FileCheck %s

; To keep this test somewhat maintainable, just check for the existence of the
; num_extras field.

; NOTE: stackmap section
; CHECK-LABEL: __LLVM_StackMaps:
; NOTE: stackmap ID
; CHECK-LABEL: .quad 444
; NOTE: stackmap offset
; CHECK-NEXT: .long {{.*}}
; NOTE: reserved
; CHECK-NEXT: .short 0
; NOTE: num locations
; CHECK-NEXT: .short {{.*}}
; NOTE: num lives
; CHECK-NEXT: .byte 1
; NOTE: type
; CHECK-NEXT: .byte 1
; NOTE: reserved
; CHECK-NEXT: .byte 0
; NOTE: size
; CHECK-NEXT: .short {{.*}}
; NOTE: reg
; CHECK-NEXT: .short {{.*}}
; NOTE: reserved
; CHECK-NEXT: .short 0
; NOTE: num extras
; CHECK-NEXT: .short {{.*}}


; ModuleID = 'ld-temp.o'

target triple = "x86_64-unknown-linux-gnu"

%struct.YkLocation = type { i64 }

@.str = private unnamed_addr constant [14 x i8] c"out of memory\00", align 1, !dbg !0
@.str.1 = private unnamed_addr constant [9 x i8] c"(stdout)\00", align 1, !dbg !7
@.str.2 = private unnamed_addr constant [8 x i8] c"(stdin)\00", align 1, !dbg !12
@.str.3 = private unnamed_addr constant [112 x i8] c"++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.\00", align 1, !dbg !17
@stdout = external dso_local local_unnamed_addr global ptr, align 8
@shadowstack_0 = thread_local global ptr null

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @interp(ptr noundef %0, ptr noundef readnone %1, ptr noundef %2, ptr noundef readnone %3, ptr noundef %4, ptr noundef %5) local_unnamed_addr #0 !dbg !34 {
  br label %7, !dbg !70

7:                                                ; preds = %6
  call void @llvm.dbg.value(metadata ptr %0, metadata !52, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %1, metadata !53, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %2, metadata !54, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %3, metadata !55, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %4, metadata !56, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %5, metadata !57, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %0, metadata !58, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %2, metadata !59, metadata !DIExpression()), !dbg !70
  %8 = icmp ult ptr %0, %1, !dbg !71
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 444, i32 0, ptr %0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i1 %8), !dbg !72
  br i1 %8, label %9, label %96, !dbg !72

9:                                                ; preds = %7
  %10 = ptrtoint ptr %0 to i64
  br label %11, !dbg !72

11:                                               ; preds = %90, %9
  %12 = phi ptr [ %2, %9 ], [ %92, %90 ]
  %13 = phi ptr [ %0, %9 ], [ %93, %90 ]
  call void @llvm.dbg.value(metadata ptr %12, metadata !59, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %13, metadata !58, metadata !DIExpression()), !dbg !70
  %14 = ptrtoint ptr %13 to i64, !dbg !73
  %15 = sub i64 %14, %10, !dbg !73
  %16 = getelementptr inbounds %struct.YkLocation, ptr %5, i64 %15, !dbg !74
  call void (i64, i32, ptr, i32, ...) @llvm.experimental.patchpoint.void(i64 0, i32 13, ptr @__ykrt_control_point, i32 3, ptr %4, ptr %16, i64 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, ptr %13), !dbg !75
  br label %17, !dbg !76

17:                                               ; preds = %11
  %18 = load i8, ptr %13, align 1, !dbg !76, !tbaa !77
  %19 = sext i8 %18 to i32, !dbg !76
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 5, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, ptr %13, i32 %19), !dbg !80
  switch i32 %19, label %90 [
    i32 62, label %20
    i32 60, label %25
    i32 43, label %29
    i32 45, label %32
    i32 46, label %35
    i32 44, label %44
    i32 91, label %50
    i32 93, label %69
  ], !dbg !80

20:                                               ; preds = %17
  %21 = getelementptr inbounds i8, ptr %12, i64 1, !dbg !81
  call void @llvm.dbg.value(metadata ptr %21, metadata !59, metadata !DIExpression()), !dbg !70
  %22 = icmp eq ptr %12, %3, !dbg !84
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 6, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %13, ptr %21, i1 %22), !dbg !85
  br i1 %22, label %23, label %90, !dbg !85

23:                                               ; preds = %20
  tail call void (i32, ptr, ...) @errx(i32 noundef 1, ptr noundef nonnull @.str) #9, !dbg !86
  br label %24, !dbg !86

24:                                               ; preds = %23
  unreachable, !dbg !86

25:                                               ; preds = %17
  %26 = icmp ugt ptr %12, %2, !dbg !87
  %27 = sext i1 %26 to i64, !dbg !90
  %28 = getelementptr inbounds i8, ptr %12, i64 %27, !dbg !90
  br label %90, !dbg !90

29:                                               ; preds = %17
  %30 = load i8, ptr %12, align 1, !dbg !91, !tbaa !77
  %31 = add i8 %30, 1, !dbg !91
  store i8 %31, ptr %12, align 1, !dbg !91, !tbaa !77
  br label %90, !dbg !93

32:                                               ; preds = %17
  %33 = load i8, ptr %12, align 1, !dbg !94, !tbaa !77
  %34 = add i8 %33, -1, !dbg !94
  store i8 %34, ptr %12, align 1, !dbg !94, !tbaa !77
  br label %90, !dbg !96

35:                                               ; preds = %17
  %36 = load i8, ptr %12, align 1, !dbg !97, !tbaa !77
  %37 = sext i8 %36 to i32, !dbg !97
  call void @llvm.dbg.value(metadata i32 %37, metadata !100, metadata !DIExpression()), !dbg !106
  %38 = load ptr, ptr @stdout, align 8, !dbg !108, !tbaa !109
  %39 = tail call i32 @putc(i32 noundef %37, ptr noundef %38), !dbg !111
  br label %40, !dbg !112

40:                                               ; preds = %35
  %41 = icmp eq i32 %39, -1, !dbg !112
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 7, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, ptr %13, i1 %41), !dbg !113
  br i1 %41, label %42, label %90, !dbg !113

42:                                               ; preds = %40
  tail call void (i32, ptr, ...) @err(i32 noundef 1, ptr noundef nonnull @.str.1) #9, !dbg !114
  br label %43, !dbg !114

43:                                               ; preds = %42
  unreachable, !dbg !114

44:                                               ; preds = %17
  %45 = tail call i64 @read(i32 noundef 0, ptr noundef %12, i64 noundef 1) #10, !dbg !115
  br label %46, !dbg !118

46:                                               ; preds = %44
  %47 = icmp eq i64 %45, -1, !dbg !118
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 2, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, ptr %13, i1 %47), !dbg !119
  br i1 %47, label %48, label %90, !dbg !119

48:                                               ; preds = %46
  tail call void (i32, ptr, ...) @err(i32 noundef 1, ptr noundef nonnull @.str.2) #9, !dbg !120
  br label %49, !dbg !120

49:                                               ; preds = %48
  unreachable, !dbg !120

50:                                               ; preds = %17
  %51 = load i8, ptr %12, align 1, !dbg !121, !tbaa !77
  %52 = icmp eq i8 %51, 0, !dbg !122
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 3, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, ptr %13, i1 %52), !dbg !123
  br i1 %52, label %53, label %90, !dbg !123

53:                                               ; preds = %50
  br label %54, !dbg !124

54:                                               ; preds = %53, %65
  %55 = phi ptr [ %66, %65 ], [ %13, %53 ]
  %56 = phi i32 [ %68, %65 ], [ 0, %53 ]
  br label %57, !dbg !124

57:                                               ; preds = %97, %54
  %58 = phi ptr [ %59, %97 ], [ %55, %54 ], !dbg !70
  call void @llvm.dbg.value(metadata i32 %56, metadata !60, metadata !DIExpression()), !dbg !125
  call void @llvm.dbg.value(metadata ptr %58, metadata !58, metadata !DIExpression()), !dbg !70
  %59 = getelementptr inbounds i8, ptr %58, i64 1, !dbg !126
  call void @llvm.dbg.value(metadata ptr %59, metadata !58, metadata !DIExpression()), !dbg !70
  %60 = load i8, ptr %59, align 1, !dbg !128, !tbaa !77
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 8, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, i32 %56, ptr %59, i8 %60), !dbg !130
  switch i8 %60, label %97 [
    i8 93, label %61
    i8 91, label %64
  ], !dbg !130, !llvm.loop !131

61:                                               ; preds = %57
  %62 = phi ptr [ %59, %57 ], !dbg !126
  %63 = icmp eq i32 %56, 0, !dbg !133
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 9, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, i32 %56, ptr %62, i1 %63), !dbg !136
  br i1 %63, label %88, label %65, !dbg !136

64:                                               ; preds = %57
  br label %65, !dbg !137

65:                                               ; preds = %64, %61
  %66 = phi ptr [ %62, %61 ], [ %59, %64 ]
  %67 = phi i32 [ -1, %61 ], [ 1, %64 ]
  %68 = add nsw i32 %56, %67, !dbg !137
  br label %54, !dbg !124, !llvm.loop !131

69:                                               ; preds = %17
  %70 = load i8, ptr %12, align 1, !dbg !138, !tbaa !77
  %71 = icmp eq i8 %70, 0, !dbg !139
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 10, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, ptr %13, i1 %71), !dbg !140
  br i1 %71, label %90, label %72, !dbg !140

72:                                               ; preds = %69
  br label %73, !dbg !141

73:                                               ; preds = %72, %84
  %74 = phi ptr [ %85, %84 ], [ %13, %72 ]
  %75 = phi i32 [ %87, %84 ], [ 0, %72 ]
  br label %76, !dbg !141

76:                                               ; preds = %98, %73
  %77 = phi ptr [ %78, %98 ], [ %74, %73 ], !dbg !70
  call void @llvm.dbg.value(metadata i32 %75, metadata !66, metadata !DIExpression()), !dbg !142
  call void @llvm.dbg.value(metadata ptr %77, metadata !58, metadata !DIExpression()), !dbg !70
  %78 = getelementptr inbounds i8, ptr %77, i64 -1, !dbg !143
  call void @llvm.dbg.value(metadata ptr %78, metadata !58, metadata !DIExpression()), !dbg !70
  %79 = load i8, ptr %78, align 1, !dbg !145, !tbaa !77
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 11, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, i32 %75, ptr %78, i8 %79), !dbg !147
  switch i8 %79, label %98 [
    i8 91, label %80
    i8 93, label %83
  ], !dbg !147, !llvm.loop !148

80:                                               ; preds = %76
  %81 = phi ptr [ %78, %76 ], !dbg !143
  %82 = icmp eq i32 %75, 0, !dbg !150
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 12, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %12, i32 %75, ptr %81, i1 %82), !dbg !153
  br i1 %82, label %89, label %84, !dbg !153

83:                                               ; preds = %76
  br label %84, !dbg !154

84:                                               ; preds = %83, %80
  %85 = phi ptr [ %81, %80 ], [ %78, %83 ]
  %86 = phi i32 [ -1, %80 ], [ 1, %83 ]
  %87 = add nsw i32 %75, %86, !dbg !154
  br label %73, !dbg !141, !llvm.loop !148

88:                                               ; preds = %61
  br label %90, !dbg !155

89:                                               ; preds = %80
  br label %90, !dbg !155

90:                                               ; preds = %89, %88, %69, %50, %46, %40, %32, %29, %25, %20, %17
  %91 = phi ptr [ %13, %17 ], [ %13, %69 ], [ %13, %50 ], [ %13, %46 ], [ %13, %40 ], [ %13, %32 ], [ %13, %29 ], [ %13, %20 ], [ %13, %25 ], [ %62, %88 ], [ %81, %89 ], !dbg !70
  %92 = phi ptr [ %12, %17 ], [ %12, %69 ], [ %12, %50 ], [ %12, %46 ], [ %12, %40 ], [ %12, %32 ], [ %12, %29 ], [ %21, %20 ], [ %28, %25 ], [ %12, %88 ], [ %12, %89 ], !dbg !70
  call void @llvm.dbg.value(metadata ptr %92, metadata !59, metadata !DIExpression()), !dbg !70
  call void @llvm.dbg.value(metadata ptr %91, metadata !58, metadata !DIExpression()), !dbg !70
  %93 = getelementptr inbounds i8, ptr %91, i64 1, !dbg !155
  call void @llvm.dbg.value(metadata ptr %93, metadata !58, metadata !DIExpression()), !dbg !70
  %94 = icmp ult ptr %93, %1, !dbg !71
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 13, i32 0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5, i64 %10, ptr %92, ptr %93, i1 %94), !dbg !72
  br i1 %94, label %11, label %95, !dbg !72, !llvm.loop !156

95:                                               ; preds = %90
  br label %96, !dbg !159

96:                                               ; preds = %95, %7
  br label %100, !dbg !159

97:                                               ; preds = %57
  br label %57

98:                                               ; preds = %76
  br label %76

99:                                               ; preds = %100
  ret i32 0, !dbg !159

100:                                              ; preds = %96
  br label %99, !dbg !159
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.value(metadata %0, metadata %1, metadata %2) #1

declare !dbg !160 dso_local void @yk_mt_control_point(ptr noundef %0, ptr noundef %1) local_unnamed_addr #2

; Function Attrs: noreturn
declare !dbg !163 dso_local void @errx(i32 noundef %0, ptr noundef %1, ...) local_unnamed_addr #3

; Function Attrs: nofree nounwind
declare !dbg !169 dso_local noundef i32 @putc(i32 noundef %0, ptr nocapture noundef %1) local_unnamed_addr #4

; Function Attrs: noreturn
declare !dbg !231 dso_local void @err(i32 noundef %0, ptr noundef %1, ...) local_unnamed_addr #3

; Function Attrs: nofree
declare !dbg !232 dso_local noundef i64 @read(i32 noundef %0, ptr nocapture noundef %1, i64 noundef %2) local_unnamed_addr #5

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() local_unnamed_addr #0 !dbg !238 {
  %1 = call ptr @malloc(i64 1000000), !dbg !249
  store ptr %1, ptr @shadowstack_0, align 8, !dbg !249
  br label %2, !dbg !249

2:                                                ; preds = %0
  %3 = tail call noalias dereferenceable_or_null(30000) ptr @calloc(i64 noundef 1, i64 noundef 30000) #11, !dbg !249
  call void @llvm.dbg.value(metadata ptr %3, metadata !242, metadata !DIExpression()), !dbg !250
  %4 = icmp eq ptr %3, null, !dbg !251
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 14, i32 0, ptr %3, i1 %4), !dbg !253
  br i1 %4, label %5, label %6, !dbg !253

5:                                                ; preds = %2
  tail call void (i32, ptr, ...) @err(i32 noundef 1, ptr noundef nonnull @.str) #9, !dbg !254
  unreachable, !dbg !254

6:                                                ; preds = %2
  call void @llvm.dbg.value(metadata ptr %3, metadata !243, metadata !DIExpression(DW_OP_plus_uconst, 30000, DW_OP_stack_value)), !dbg !250
  %7 = tail call ptr @yk_mt_new(ptr noundef null) #10, !dbg !255
  call void @llvm.dbg.value(metadata ptr %7, metadata !244, metadata !DIExpression()), !dbg !250
  tail call void @yk_mt_hot_threshold_set(ptr noundef %7, i32 noundef 5) #10, !dbg !256
  call void @llvm.dbg.value(metadata i64 112, metadata !245, metadata !DIExpression()), !dbg !250
  %8 = tail call noalias dereferenceable_or_null(896) ptr @calloc(i64 noundef 112, i64 noundef 8) #11, !dbg !257
  call void @llvm.dbg.value(metadata ptr %8, metadata !246, metadata !DIExpression()), !dbg !250
  %9 = icmp eq ptr %8, null, !dbg !258
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 15, i32 0, ptr %3, ptr %7, ptr %8, i1 %9), !dbg !260
  br i1 %9, label %11, label %10, !dbg !260

10:                                               ; preds = %6
  br label %15, !dbg !261

11:                                               ; preds = %6
  tail call void (i32, ptr, ...) @err(i32 noundef 1, ptr noundef nonnull @.str) #9, !dbg !262
  unreachable, !dbg !262

12:                                               ; preds = %22
  %13 = getelementptr inbounds i8, ptr %3, i64 30000, !dbg !263
  call void @llvm.dbg.value(metadata ptr %13, metadata !243, metadata !DIExpression()), !dbg !250
  %14 = tail call i32 @interp(ptr noundef nonnull @.str.3, ptr noundef nonnull getelementptr inbounds ([112 x i8], ptr @.str.3, i64 1, i64 0), ptr noundef nonnull %3, ptr noundef nonnull %13, ptr noundef %7, ptr noundef nonnull %8), !dbg !264
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 1, i32 0, ptr %3, ptr %7, ptr %8, ptr %13), !dbg !265
  tail call void @free(ptr noundef %3) #10, !dbg !265
  tail call void @free(ptr noundef nonnull %8) #10, !dbg !266
  br label %28, !dbg !267

15:                                               ; preds = %10, %22
  %16 = phi i64 [ %25, %22 ], [ 0, %10 ]
  call void @llvm.dbg.value(metadata i64 %16, metadata !247, metadata !DIExpression()), !dbg !268
  %17 = icmp eq i64 %16, 41, !dbg !269
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 16, i32 0, ptr %3, ptr %7, ptr %8, i64 %16, i1 %17), !dbg !273
  br i1 %17, label %18, label %20, !dbg !273

18:                                               ; preds = %15
  %19 = tail call i64 @yk_location_new() #10, !dbg !274
  br label %22, !dbg !275

20:                                               ; preds = %15
  %21 = tail call i64 @yk_location_null() #10, !dbg !276
  br label %22

22:                                               ; preds = %20, %18
  %23 = phi i64 [ %19, %18 ], [ %21, %20 ], !dbg !277
  %24 = getelementptr inbounds %struct.YkLocation, ptr %8, i64 %16, !dbg !277
  store i64 %23, ptr %24, align 8, !dbg !277
  %25 = add nuw nsw i64 %16, 1, !dbg !278
  call void @llvm.dbg.value(metadata i64 %25, metadata !247, metadata !DIExpression()), !dbg !268
  %26 = icmp eq i64 %25, 112, !dbg !279
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 17, i32 0, ptr %3, ptr %7, ptr %8, i64 %25, i1 %26), !dbg !261
  br i1 %26, label %12, label %15, !dbg !261, !llvm.loop !280

27:                                               ; preds = %28
  ret i32 0, !dbg !267

28:                                               ; preds = %12
  br label %27, !dbg !267
}

; Function Attrs: mustprogress nofree nounwind willreturn allockind("alloc,zeroed") allocsize(0,1) memory(inaccessiblemem: readwrite)
declare !dbg !282 dso_local noalias noundef ptr @calloc(i64 noundef %0, i64 noundef %1) local_unnamed_addr #6

declare !dbg !286 dso_local ptr @yk_mt_new(ptr noundef %0) local_unnamed_addr #2

declare !dbg !290 dso_local void @yk_mt_hot_threshold_set(ptr noundef %0, i32 noundef %1) local_unnamed_addr #2

; Function Attrs: mustprogress nounwind willreturn allockind("free") memory(argmem: readwrite, inaccessiblemem: readwrite)
declare !dbg !298 dso_local void @free(ptr allocptr nocapture noundef %0) local_unnamed_addr #7

declare !dbg !301 dso_local i64 @yk_location_new() local_unnamed_addr #2

declare !dbg !304 dso_local i64 @yk_location_null() local_unnamed_addr #2

declare ptr @malloc(i64 %0)

declare void @__ykrt_control_point(ptr %0, ptr %1, i64 %2)

declare void @llvm.experimental.patchpoint.void(i64 %0, i32 %1, ptr %2, i32 %3, ...)

; Function Attrs: nocallback nofree nosync willreturn
declare void @llvm.experimental.stackmap(i64 %0, i32 %1, ...) #8

attributes #0 = { noinline nounwind optnone uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "yk_outline" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { noreturn "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nofree "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { mustprogress nofree nounwind willreturn allockind("alloc,zeroed") allocsize(0,1) memory(inaccessiblemem: readwrite) "alloc-family"="malloc" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { mustprogress nounwind willreturn allockind("free") memory(argmem: readwrite, inaccessiblemem: readwrite) "alloc-family"="malloc" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nocallback nofree nosync willreturn }
attributes #9 = { noreturn nounwind }
attributes #10 = { nounwind }
attributes #11 = { nounwind allocsize(0,1) }

!llvm.dbg.cu = !{!22}
!llvm.ident = !{!27}
!llvm.module.flags = !{!28, !29, !30, !31, !32, !33}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(scope: null, file: !2, line: 43, type: !3, isLocal: true, isDefinition: true)
!2 = !DIFile(filename: "c/bf.O2.c", directory: "/home/vext01/research/yk/tests", checksumkind: CSK_MD5, checksum: "e9eddf0dbad579b35465786e9ce6966f")
!3 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 112, elements: !5)
!4 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!5 = !{!6}
!6 = !DISubrange(count: 14)
!7 = !DIGlobalVariableExpression(var: !8, expr: !DIExpression())
!8 = distinct !DIGlobalVariable(scope: null, file: !2, line: 61, type: !9, isLocal: true, isDefinition: true)
!9 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 72, elements: !10)
!10 = !{!11}
!11 = !DISubrange(count: 9)
!12 = !DIGlobalVariableExpression(var: !13, expr: !DIExpression())
!13 = distinct !DIGlobalVariable(scope: null, file: !2, line: 66, type: !14, isLocal: true, isDefinition: true)
!14 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 64, elements: !15)
!15 = !{!16}
!16 = !DISubrange(count: 8)
!17 = !DIGlobalVariableExpression(var: !18, expr: !DIExpression())
!18 = distinct !DIGlobalVariable(scope: null, file: !2, line: 121, type: !19, isLocal: true, isDefinition: true)
!19 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 896, elements: !20)
!20 = !{!21}
!21 = !DISubrange(count: 112)
!22 = distinct !DICompileUnit(language: DW_LANG_C11, file: !23, producer: "clang version 18.0.0 (https://github.com/ykjit/ykllvm 2a08a7a580605802277abd81ac9f9b1c9f3d157d)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !24, globals: !26, splitDebugInlining: false, nameTableKind: None)
!23 = !DIFile(filename: "/home/vext01/research/yk/tests/c/bf.O2.c", directory: "/home/vext01/research/yk/tests", checksumkind: CSK_MD5, checksum: "e9eddf0dbad579b35465786e9ce6966f")
!24 = !{!25}
!25 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!26 = !{!0, !7, !12, !17}
!27 = !{!"clang version 18.0.0 (https://github.com/ykjit/ykllvm 2a08a7a580605802277abd81ac9f9b1c9f3d157d)"}
!28 = !{i32 7, !"Dwarf Version", i32 5}
!29 = !{i32 2, !"Debug Info Version", i32 3}
!30 = !{i32 1, !"wchar_size", i32 4}
!31 = !{i32 7, !"uwtable", i32 2}
!32 = !{i32 1, !"ThinLTO", i32 0}
!33 = !{i32 1, !"EnableSplitLTOUnit", i32 1}
!34 = distinct !DISubprogram(name: "interp", scope: !2, file: !2, line: 34, type: !35, scopeLine: 35, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !22, retainedNodes: !51)
!35 = !DISubroutineType(types: !36)
!36 = !{!37, !38, !38, !38, !38, !39, !43}
!37 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!38 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
!39 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !40, size: 64)
!40 = !DIDerivedType(tag: DW_TAG_typedef, name: "YkMT", file: !41, line: 31, baseType: !42)
!41 = !DIFile(filename: "../bin/../ykcapi/yk.h", directory: "/home/vext01/research/yk/tests", checksumkind: CSK_MD5, checksum: "5871ef868908ff612eec2856e8c8cdc1")
!42 = !DICompositeType(tag: DW_TAG_structure_type, name: "YkMT", file: !41, line: 31, flags: DIFlagFwdDecl)
!43 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !44, size: 64)
!44 = !DIDerivedType(tag: DW_TAG_typedef, name: "YkLocation", file: !41, line: 23, baseType: !45)
!45 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !41, line: 21, size: 64, elements: !46)
!46 = !{!47}
!47 = !DIDerivedType(tag: DW_TAG_member, name: "state", scope: !45, file: !41, line: 22, baseType: !48, size: 64)
!48 = !DIDerivedType(tag: DW_TAG_typedef, name: "uintptr_t", file: !49, line: 90, baseType: !50)
!49 = !DIFile(filename: "/usr/include/stdint.h", directory: "", checksumkind: CSK_MD5, checksum: "a48e64edacc5b19f56c99745232c963c")
!50 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!51 = !{!52, !53, !54, !55, !56, !57, !58, !59, !60, !66}
!52 = !DILocalVariable(name: "prog", arg: 1, scope: !34, file: !2, line: 34, type: !38)
!53 = !DILocalVariable(name: "prog_end", arg: 2, scope: !34, file: !2, line: 34, type: !38)
!54 = !DILocalVariable(name: "cells", arg: 3, scope: !34, file: !2, line: 34, type: !38)
!55 = !DILocalVariable(name: "cells_end", arg: 4, scope: !34, file: !2, line: 34, type: !38)
!56 = !DILocalVariable(name: "mt", arg: 5, scope: !34, file: !2, line: 34, type: !39)
!57 = !DILocalVariable(name: "yklocs", arg: 6, scope: !34, file: !2, line: 35, type: !43)
!58 = !DILocalVariable(name: "inst", scope: !34, file: !2, line: 36, type: !38)
!59 = !DILocalVariable(name: "cell", scope: !34, file: !2, line: 37, type: !38)
!60 = !DILocalVariable(name: "count", scope: !61, file: !2, line: 71, type: !37)
!61 = distinct !DILexicalBlock(scope: !62, file: !2, line: 70, column: 23)
!62 = distinct !DILexicalBlock(scope: !63, file: !2, line: 70, column: 11)
!63 = distinct !DILexicalBlock(scope: !64, file: !2, line: 69, column: 15)
!64 = distinct !DILexicalBlock(scope: !65, file: !2, line: 40, column: 20)
!65 = distinct !DILexicalBlock(scope: !34, file: !2, line: 38, column: 27)
!66 = !DILocalVariable(name: "count", scope: !67, file: !2, line: 86, type: !37)
!67 = distinct !DILexicalBlock(scope: !68, file: !2, line: 85, column: 23)
!68 = distinct !DILexicalBlock(scope: !69, file: !2, line: 85, column: 11)
!69 = distinct !DILexicalBlock(scope: !64, file: !2, line: 84, column: 15)
!70 = !DILocation(line: 0, scope: !34)
!71 = !DILocation(line: 38, column: 15, scope: !34)
!72 = !DILocation(line: 38, column: 3, scope: !34)
!73 = !DILocation(line: 39, column: 42, scope: !65)
!74 = !DILocation(line: 39, column: 30, scope: !65)
!75 = !DILocation(line: 39, column: 5, scope: !65)
!76 = !DILocation(line: 40, column: 13, scope: !65)
!77 = !{!78, !78, i64 0}
!78 = !{!"omnipotent char", !79, i64 0}
!79 = !{!"Simple C/C++ TBAA"}
!80 = !DILocation(line: 40, column: 5, scope: !65)
!81 = !DILocation(line: 42, column: 15, scope: !82)
!82 = distinct !DILexicalBlock(scope: !83, file: !2, line: 42, column: 11)
!83 = distinct !DILexicalBlock(scope: !64, file: !2, line: 41, column: 15)
!84 = !DILocation(line: 42, column: 18, scope: !82)
!85 = !DILocation(line: 42, column: 11, scope: !83)
!86 = !DILocation(line: 43, column: 9, scope: !82)
!87 = !DILocation(line: 47, column: 16, scope: !88)
!88 = distinct !DILexicalBlock(scope: !89, file: !2, line: 47, column: 11)
!89 = distinct !DILexicalBlock(scope: !64, file: !2, line: 46, column: 15)
!90 = !DILocation(line: 47, column: 11, scope: !89)
!91 = !DILocation(line: 52, column: 14, scope: !92)
!92 = distinct !DILexicalBlock(scope: !64, file: !2, line: 51, column: 15)
!93 = !DILocation(line: 53, column: 7, scope: !92)
!94 = !DILocation(line: 56, column: 14, scope: !95)
!95 = distinct !DILexicalBlock(scope: !64, file: !2, line: 55, column: 15)
!96 = !DILocation(line: 57, column: 7, scope: !95)
!97 = !DILocation(line: 60, column: 19, scope: !98)
!98 = distinct !DILexicalBlock(scope: !99, file: !2, line: 60, column: 11)
!99 = distinct !DILexicalBlock(scope: !64, file: !2, line: 59, column: 15)
!100 = !DILocalVariable(name: "__c", arg: 1, scope: !101, file: !102, line: 82, type: !37)
!101 = distinct !DISubprogram(name: "putchar", scope: !102, file: !102, line: 82, type: !103, scopeLine: 83, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !22, retainedNodes: !105)
!102 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdio.h", directory: "", checksumkind: CSK_MD5, checksum: "c10e343656e7a2bf1044ef4e4442d902")
!103 = !DISubroutineType(types: !104)
!104 = !{!37, !37}
!105 = !{!100}
!106 = !DILocation(line: 0, scope: !101, inlinedAt: !107)
!107 = distinct !DILocation(line: 60, column: 11, scope: !98)
!108 = !DILocation(line: 84, column: 21, scope: !101, inlinedAt: !107)
!109 = !{!110, !110, i64 0}
!110 = !{!"any pointer", !78, i64 0}
!111 = !DILocation(line: 84, column: 10, scope: !101, inlinedAt: !107)
!112 = !DILocation(line: 60, column: 26, scope: !98)
!113 = !DILocation(line: 60, column: 11, scope: !99)
!114 = !DILocation(line: 61, column: 9, scope: !98)
!115 = !DILocation(line: 65, column: 11, scope: !116)
!116 = distinct !DILexicalBlock(scope: !117, file: !2, line: 65, column: 11)
!117 = distinct !DILexicalBlock(scope: !64, file: !2, line: 64, column: 15)
!118 = !DILocation(line: 65, column: 39, scope: !116)
!119 = !DILocation(line: 65, column: 11, scope: !117)
!120 = !DILocation(line: 66, column: 9, scope: !116)
!121 = !DILocation(line: 70, column: 11, scope: !62)
!122 = !DILocation(line: 70, column: 17, scope: !62)
!123 = !DILocation(line: 70, column: 11, scope: !63)
!124 = !DILocation(line: 72, column: 9, scope: !61)
!125 = !DILocation(line: 0, scope: !61)
!126 = !DILocation(line: 73, column: 15, scope: !127)
!127 = distinct !DILexicalBlock(scope: !61, file: !2, line: 72, column: 22)
!128 = !DILocation(line: 74, column: 15, scope: !129)
!129 = distinct !DILexicalBlock(scope: !127, file: !2, line: 74, column: 15)
!130 = !DILocation(line: 74, column: 15, scope: !127)
!131 = distinct !{!131, !124, !132}
!132 = !DILocation(line: 80, column: 9, scope: !61)
!133 = !DILocation(line: 75, column: 23, scope: !134)
!134 = distinct !DILexicalBlock(scope: !135, file: !2, line: 75, column: 17)
!135 = distinct !DILexicalBlock(scope: !129, file: !2, line: 74, column: 29)
!136 = !DILocation(line: 75, column: 17, scope: !135)
!137 = !DILocation(line: 0, scope: !129)
!138 = !DILocation(line: 85, column: 11, scope: !68)
!139 = !DILocation(line: 85, column: 17, scope: !68)
!140 = !DILocation(line: 85, column: 11, scope: !69)
!141 = !DILocation(line: 87, column: 9, scope: !67)
!142 = !DILocation(line: 0, scope: !67)
!143 = !DILocation(line: 88, column: 15, scope: !144)
!144 = distinct !DILexicalBlock(scope: !67, file: !2, line: 87, column: 22)
!145 = !DILocation(line: 89, column: 15, scope: !146)
!146 = distinct !DILexicalBlock(scope: !144, file: !2, line: 89, column: 15)
!147 = !DILocation(line: 89, column: 15, scope: !144)
!148 = distinct !{!148, !141, !149}
!149 = !DILocation(line: 95, column: 9, scope: !67)
!150 = !DILocation(line: 90, column: 23, scope: !151)
!151 = distinct !DILexicalBlock(scope: !152, file: !2, line: 90, column: 17)
!152 = distinct !DILexicalBlock(scope: !146, file: !2, line: 89, column: 29)
!153 = !DILocation(line: 90, column: 17, scope: !152)
!154 = !DILocation(line: 0, scope: !146)
!155 = !DILocation(line: 102, column: 9, scope: !65)
!156 = distinct !{!156, !72, !157, !158}
!157 = !DILocation(line: 103, column: 3, scope: !34)
!158 = !{!"llvm.loop.mustprogress"}
!159 = !DILocation(line: 104, column: 3, scope: !34)
!160 = !DISubprogram(name: "yk_mt_control_point", scope: !41, file: !41, line: 52, type: !161, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!161 = !DISubroutineType(types: !162)
!162 = !{null, !39, !43}
!163 = !DISubprogram(name: "errx", scope: !164, file: !164, line: 50, type: !165, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!164 = !DIFile(filename: "/usr/include/err.h", directory: "", checksumkind: CSK_MD5, checksum: "c1d02a7722f9cc1994ce2e5c5e6150dd")
!165 = !DISubroutineType(types: !166)
!166 = !{null, !37, !167, null}
!167 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !168, size: 64)
!168 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !4)
!169 = !DISubprogram(name: "putc", scope: !170, file: !170, line: 550, type: !171, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!170 = !DIFile(filename: "/usr/include/stdio.h", directory: "", checksumkind: CSK_MD5, checksum: "b5a90985555f47bfb88eff5a8f0f5b72")
!171 = !DISubroutineType(types: !172)
!172 = !{!37, !37, !173}
!173 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !174, size: 64)
!174 = !DIDerivedType(tag: DW_TAG_typedef, name: "FILE", file: !175, line: 7, baseType: !176)
!175 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types/FILE.h", directory: "", checksumkind: CSK_MD5, checksum: "571f9fb6223c42439075fdde11a0de5d")
!176 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "_IO_FILE", file: !177, line: 49, size: 1728, elements: !178)
!177 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types/struct_FILE.h", directory: "", checksumkind: CSK_MD5, checksum: "1bad07471b7974df4ecc1d1c2ca207e6")
!178 = !{!179, !180, !181, !182, !183, !184, !185, !186, !187, !188, !189, !190, !191, !194, !196, !197, !198, !202, !204, !206, !210, !213, !215, !218, !221, !222, !223, !226, !227}
!179 = !DIDerivedType(tag: DW_TAG_member, name: "_flags", scope: !176, file: !177, line: 51, baseType: !37, size: 32)
!180 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_read_ptr", scope: !176, file: !177, line: 54, baseType: !38, size: 64, offset: 64)
!181 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_read_end", scope: !176, file: !177, line: 55, baseType: !38, size: 64, offset: 128)
!182 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_read_base", scope: !176, file: !177, line: 56, baseType: !38, size: 64, offset: 192)
!183 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_write_base", scope: !176, file: !177, line: 57, baseType: !38, size: 64, offset: 256)
!184 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_write_ptr", scope: !176, file: !177, line: 58, baseType: !38, size: 64, offset: 320)
!185 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_write_end", scope: !176, file: !177, line: 59, baseType: !38, size: 64, offset: 384)
!186 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_buf_base", scope: !176, file: !177, line: 60, baseType: !38, size: 64, offset: 448)
!187 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_buf_end", scope: !176, file: !177, line: 61, baseType: !38, size: 64, offset: 512)
!188 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_save_base", scope: !176, file: !177, line: 64, baseType: !38, size: 64, offset: 576)
!189 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_backup_base", scope: !176, file: !177, line: 65, baseType: !38, size: 64, offset: 640)
!190 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_save_end", scope: !176, file: !177, line: 66, baseType: !38, size: 64, offset: 704)
!191 = !DIDerivedType(tag: DW_TAG_member, name: "_markers", scope: !176, file: !177, line: 68, baseType: !192, size: 64, offset: 768)
!192 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !193, size: 64)
!193 = !DICompositeType(tag: DW_TAG_structure_type, name: "_IO_marker", file: !177, line: 36, flags: DIFlagFwdDecl)
!194 = !DIDerivedType(tag: DW_TAG_member, name: "_chain", scope: !176, file: !177, line: 70, baseType: !195, size: 64, offset: 832)
!195 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !176, size: 64)
!196 = !DIDerivedType(tag: DW_TAG_member, name: "_fileno", scope: !176, file: !177, line: 72, baseType: !37, size: 32, offset: 896)
!197 = !DIDerivedType(tag: DW_TAG_member, name: "_flags2", scope: !176, file: !177, line: 73, baseType: !37, size: 32, offset: 928)
!198 = !DIDerivedType(tag: DW_TAG_member, name: "_old_offset", scope: !176, file: !177, line: 74, baseType: !199, size: 64, offset: 960)
!199 = !DIDerivedType(tag: DW_TAG_typedef, name: "__off_t", file: !200, line: 152, baseType: !201)
!200 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "", checksumkind: CSK_MD5, checksum: "d108b5f93a74c50510d7d9bc0ab36df9")
!201 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!202 = !DIDerivedType(tag: DW_TAG_member, name: "_cur_column", scope: !176, file: !177, line: 77, baseType: !203, size: 16, offset: 1024)
!203 = !DIBasicType(name: "unsigned short", size: 16, encoding: DW_ATE_unsigned)
!204 = !DIDerivedType(tag: DW_TAG_member, name: "_vtable_offset", scope: !176, file: !177, line: 78, baseType: !205, size: 8, offset: 1040)
!205 = !DIBasicType(name: "signed char", size: 8, encoding: DW_ATE_signed_char)
!206 = !DIDerivedType(tag: DW_TAG_member, name: "_shortbuf", scope: !176, file: !177, line: 79, baseType: !207, size: 8, offset: 1048)
!207 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 8, elements: !208)
!208 = !{!209}
!209 = !DISubrange(count: 1)
!210 = !DIDerivedType(tag: DW_TAG_member, name: "_lock", scope: !176, file: !177, line: 81, baseType: !211, size: 64, offset: 1088)
!211 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !212, size: 64)
!212 = !DIDerivedType(tag: DW_TAG_typedef, name: "_IO_lock_t", file: !177, line: 43, baseType: null)
!213 = !DIDerivedType(tag: DW_TAG_member, name: "_offset", scope: !176, file: !177, line: 89, baseType: !214, size: 64, offset: 1152)
!214 = !DIDerivedType(tag: DW_TAG_typedef, name: "__off64_t", file: !200, line: 153, baseType: !201)
!215 = !DIDerivedType(tag: DW_TAG_member, name: "_codecvt", scope: !176, file: !177, line: 91, baseType: !216, size: 64, offset: 1216)
!216 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !217, size: 64)
!217 = !DICompositeType(tag: DW_TAG_structure_type, name: "_IO_codecvt", file: !177, line: 37, flags: DIFlagFwdDecl)
!218 = !DIDerivedType(tag: DW_TAG_member, name: "_wide_data", scope: !176, file: !177, line: 92, baseType: !219, size: 64, offset: 1280)
!219 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !220, size: 64)
!220 = !DICompositeType(tag: DW_TAG_structure_type, name: "_IO_wide_data", file: !177, line: 38, flags: DIFlagFwdDecl)
!221 = !DIDerivedType(tag: DW_TAG_member, name: "_freeres_list", scope: !176, file: !177, line: 93, baseType: !195, size: 64, offset: 1344)
!222 = !DIDerivedType(tag: DW_TAG_member, name: "_freeres_buf", scope: !176, file: !177, line: 94, baseType: !25, size: 64, offset: 1408)
!223 = !DIDerivedType(tag: DW_TAG_member, name: "__pad5", scope: !176, file: !177, line: 95, baseType: !224, size: 64, offset: 1472)
!224 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !225, line: 13, baseType: !50)
!225 = !DIFile(filename: "target/debug/ykllvm/lib/clang/18/include/__stddef_size_t.h", directory: "/home/vext01/research/yk", checksumkind: CSK_MD5, checksum: "405db6ea5fb824de326715f26fa9fab5")
!226 = !DIDerivedType(tag: DW_TAG_member, name: "_mode", scope: !176, file: !177, line: 96, baseType: !37, size: 32, offset: 1536)
!227 = !DIDerivedType(tag: DW_TAG_member, name: "_unused2", scope: !176, file: !177, line: 98, baseType: !228, size: 160, offset: 1568)
!228 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 160, elements: !229)
!229 = !{!230}
!230 = !DISubrange(count: 20)
!231 = !DISubprogram(name: "err", scope: !164, file: !164, line: 46, type: !165, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!232 = !DISubprogram(name: "read", scope: !233, file: !233, line: 371, type: !234, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!233 = !DIFile(filename: "/usr/include/unistd.h", directory: "", checksumkind: CSK_MD5, checksum: "ed37c2e6f30ba31a8b41e4d70547c39c")
!234 = !DISubroutineType(types: !235)
!235 = !{!236, !37, !25, !224}
!236 = !DIDerivedType(tag: DW_TAG_typedef, name: "ssize_t", file: !170, line: 77, baseType: !237)
!237 = !DIDerivedType(tag: DW_TAG_typedef, name: "__ssize_t", file: !200, line: 194, baseType: !201)
!238 = distinct !DISubprogram(name: "main", scope: !2, file: !2, line: 107, type: !239, scopeLine: 107, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !22, retainedNodes: !241)
!239 = !DISubroutineType(types: !240)
!240 = !{!37}
!241 = !{!242, !243, !244, !245, !246, !247}
!242 = !DILocalVariable(name: "cells", scope: !238, file: !2, line: 108, type: !38)
!243 = !DILocalVariable(name: "cells_end", scope: !238, file: !2, line: 111, type: !38)
!244 = !DILocalVariable(name: "mt", scope: !238, file: !2, line: 113, type: !39)
!245 = !DILocalVariable(name: "prog_len", scope: !238, file: !2, line: 116, type: !224)
!246 = !DILocalVariable(name: "yklocs", scope: !238, file: !2, line: 117, type: !43)
!247 = !DILocalVariable(name: "i", scope: !248, file: !2, line: 120, type: !224)
!248 = distinct !DILexicalBlock(scope: !238, file: !2, line: 120, column: 3)
!249 = !DILocation(line: 108, column: 17, scope: !238)
!250 = !DILocation(line: 0, scope: !238)
!251 = !DILocation(line: 109, column: 13, scope: !252)
!252 = distinct !DILexicalBlock(scope: !238, file: !2, line: 109, column: 7)
!253 = !DILocation(line: 109, column: 7, scope: !238)
!254 = !DILocation(line: 110, column: 5, scope: !252)
!255 = !DILocation(line: 113, column: 14, scope: !238)
!256 = !DILocation(line: 114, column: 3, scope: !238)
!257 = !DILocation(line: 117, column: 24, scope: !238)
!258 = !DILocation(line: 118, column: 14, scope: !259)
!259 = distinct !DILexicalBlock(scope: !238, file: !2, line: 118, column: 7)
!260 = !DILocation(line: 118, column: 7, scope: !238)
!261 = !DILocation(line: 120, column: 3, scope: !248)
!262 = !DILocation(line: 119, column: 5, scope: !259)
!263 = !DILocation(line: 111, column: 27, scope: !238)
!264 = !DILocation(line: 127, column: 3, scope: !238)
!265 = !DILocation(line: 129, column: 3, scope: !238)
!266 = !DILocation(line: 130, column: 3, scope: !238)
!267 = !DILocation(line: 131, column: 1, scope: !238)
!268 = !DILocation(line: 0, scope: !248)
!269 = !DILocation(line: 121, column: 23, scope: !270)
!270 = distinct !DILexicalBlock(scope: !271, file: !2, line: 121, column: 9)
!271 = distinct !DILexicalBlock(scope: !272, file: !2, line: 120, column: 41)
!272 = distinct !DILexicalBlock(scope: !248, file: !2, line: 120, column: 3)
!273 = !DILocation(line: 121, column: 9, scope: !271)
!274 = !DILocation(line: 122, column: 19, scope: !270)
!275 = !DILocation(line: 122, column: 7, scope: !270)
!276 = !DILocation(line: 124, column: 19, scope: !270)
!277 = !DILocation(line: 0, scope: !270)
!278 = !DILocation(line: 120, column: 37, scope: !272)
!279 = !DILocation(line: 120, column: 24, scope: !272)
!280 = distinct !{!280, !261, !281, !158}
!281 = !DILocation(line: 125, column: 3, scope: !248)
!282 = !DISubprogram(name: "calloc", scope: !283, file: !283, line: 556, type: !284, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!283 = !DIFile(filename: "/usr/include/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "4b47dc81a92f1fe77a152c0aac236718")
!284 = !DISubroutineType(types: !285)
!285 = !{!25, !224, !224}
!286 = !DISubprogram(name: "yk_mt_new", scope: !41, file: !41, line: 40, type: !287, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!287 = !DISubroutineType(types: !288)
!288 = !{!39, !289}
!289 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !38, size: 64)
!290 = !DISubprogram(name: "yk_mt_hot_threshold_set", scope: !41, file: !41, line: 55, type: !291, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!291 = !DISubroutineType(types: !292)
!292 = !{null, !39, !293}
!293 = !DIDerivedType(tag: DW_TAG_typedef, name: "YkHotThreshold", file: !41, line: 26, baseType: !294)
!294 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint32_t", file: !295, line: 26, baseType: !296)
!295 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h", directory: "", checksumkind: CSK_MD5, checksum: "2bf2ae53c58c01b1a1b9383b5195125c")
!296 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint32_t", file: !200, line: 42, baseType: !297)
!297 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!298 = !DISubprogram(name: "free", scope: !283, file: !283, line: 568, type: !299, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!299 = !DISubroutineType(types: !300)
!300 = !{null, !25}
!301 = !DISubprogram(name: "yk_location_new", scope: !41, file: !41, line: 65, type: !302, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!302 = !DISubroutineType(types: !303)
!303 = !{!44}
!304 = !DISubprogram(name: "yk_location_null", scope: !41, file: !41, line: 79, type: !302, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
