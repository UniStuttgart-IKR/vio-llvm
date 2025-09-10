; ModuleID = '/u/bulk/home/stud/leylknci/vio-llvm/build/bin/test.c'
source_filename = "/u/bulk/home/stud/leylknci/vio-llvm/build/bin/test.c"
target datalayout = "E-p:32:32-i32:32:32-i16:16:16-i8:8:8-n32"
target triple = "orisc"

%struct_prm.Outer = type { i8, [15 x i32], [12 x %struct_prm.Inner], i32, [4 x i8] }
%struct_prm.Inner = type { i32, i8, i16, i32, float, double, [15 x i32], [4 x i8] }
%struct_ptr.Outer = type { ptr, [12 x %struct_ptr.Inner] }
%struct_ptr.Inner = type { ptr, ptr, ptr, ptr }

; Function Attrs: noinline nounwind optnone
define dso_local void @f(ptr noundef %outer, ptr noundef %innerP, ptr noundef %f, ptr noundef %i, ptr noundef %c, i32 noundef %x) #0 {
entry:
  %outer.addr = alloca ptr, align 4
  %innerP.addr = alloca ptr, align 4
  %f.addr = alloca ptr, align 4
  %i.addr = alloca ptr, align 4
  %c.addr = alloca ptr, align 4
  %x.addr = alloca i32, align 4
  %inner = alloca ptr, align 4
  call void @llvm.orisc.storepointer(ptr %outer, ptr %outer.addr)
  call void @llvm.orisc.storepointer(ptr %innerP, ptr %innerP.addr)
  call void @llvm.orisc.storepointer(ptr %f, ptr %f.addr)
  call void @llvm.orisc.storepointer(ptr %i, ptr %i.addr)
  call void @llvm.orisc.storepointer(ptr %c, ptr %c.addr)
  store i32 %x, ptr %x.addr, align 4
  %0 = call ptr @llvm.orisc.loadpointer(ptr %innerP.addr)
  %1 = load ptr, ptr %innerP.addr, align 4
  call void @llvm.orisc.storepointer(ptr %0, ptr %inner)
  %2 = call ptr @llvm.orisc.loadpointer(ptr %outer.addr)
  %3 = load ptr, ptr %outer.addr, align 4
  %4 = load i32, ptr %x.addr, align 4
  %5 = inttoptr i32 %4 to ptr
  %cmp = icmp eq ptr %2, %5
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %6 = call ptr @llvm.orisc.loadpointer(ptr %outer.addr)
  %7 = load ptr, ptr %outer.addr, align 4
  %arrayidx = getelementptr inbounds %struct_prm.Outer, ptr %6, i32 4
  %d = getelementptr inbounds nuw %struct_prm.Outer, ptr %arrayidx, i32 0, i32 3
  store i32 69, ptr %d, align 8
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %8 = call ptr @llvm.orisc.loadpointer(ptr %outer.addr)
  %9 = load ptr, ptr %outer.addr, align 4
  %arrayidx1 = getelementptr inbounds %struct_prm.Outer, ptr %8, i32 0
  %f2 = getelementptr inbounds nuw %struct_prm.Outer, ptr %8, i32 0, i32 0
  store i8 24, ptr %8, align 8
  %10 = call ptr @llvm.orisc.loadpointer(ptr %inner)
  %11 = load ptr, ptr %inner, align 4
  %12 = call ptr @llvm.orisc.loadpointer(ptr %outer.addr)
  %13 = load ptr, ptr %outer.addr, align 4
  %arrayidx3 = getelementptr inbounds %struct_prm.Outer, ptr %12, i32 0
  %inner_a = getelementptr inbounds nuw %struct_ptr.Outer, ptr %12, i32 0, i32 0
  call void @llvm.orisc.storepointer(ptr %10, ptr %12)
  %14 = call ptr @llvm.orisc.loadpointer(ptr %i.addr)
  %15 = load ptr, ptr %i.addr, align 4
  %16 = call ptr @llvm.orisc.loadpointer(ptr %inner)
  %17 = load ptr, ptr %inner, align 4
  %hallo = getelementptr inbounds nuw %struct_ptr.Inner, ptr %16, i32 0, i32 0
  call void @llvm.orisc.storepointer(ptr %14, ptr %16)
  %18 = load i32, ptr %x.addr, align 4
  %19 = call ptr @llvm.orisc.loadpointer(ptr %f.addr)
  %20 = load ptr, ptr %f.addr, align 4
  store i32 %18, ptr %19, align 4
  ret void
}

; Function Attrs: nounwind speculatable memory(read)
declare ptr @llvm.orisc.loadpointer(ptr) #1

; Function Attrs: nounwind speculatable memory(write)
declare void @llvm.orisc.storepointer(ptr, ptr) #2

attributes #0 = { noinline nounwind optnone "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #1 = { nounwind speculatable memory(read) }
attributes #2 = { nounwind speculatable memory(write) }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"frame-pointer", i32 2}
!2 = !{!"clang version 21.0.0git (https://github.com/UniStuttgart-IKR/vio-llvm.git cc82268450957d53c92a997230550cc3a5d17127)"}
