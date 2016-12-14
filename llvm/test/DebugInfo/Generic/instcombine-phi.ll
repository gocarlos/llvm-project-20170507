; RUN: opt -instcombine -S < %s | FileCheck %s

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; If all the operands to a phi node are of the same operation, instcombine
; will try to pull them through the phi node, combining them into a single
; operation.  Check that when it does this the combined operation does not
; have a debug location set.

; Test folding of a binary operation.  Generated from source:

; extern int foo(void);
; extern int bar(void);
; 
; int binop(int a, int b) {
;   if(a)
;     b -= foo();
;   else
;     b -= bar();
;   return b;
; }

; CHECK: define i32 @binop
; CHECK-LABEL: if.end:
; CHECK: %[[PHI:.*]] = phi i32 [ %call, %if.then ], [ %call1, %if.else ]
; CHECK: sub nsw i32 %b, %[[PHI]]
; CHECK-NOT: !dbg
; CHECK: ret i32

define i32 @binop(i32 %a, i32 %b) !dbg !6 {
entry:
  %tobool = icmp ne i32 %a, 0, !dbg !8
  br i1 %tobool, label %if.then, label %if.else, !dbg !8

if.then:                                          ; preds = %entry
  %call = call i32 @foo(), !dbg !9
  %sub = sub nsw i32 %b, %call, !dbg !10
  br label %if.end, !dbg !11

if.else:                                          ; preds = %entry
  %call1 = call i32 @bar(), !dbg !12
  %sub2 = sub nsw i32 %b, %call1, !dbg !13
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %b.addr.0 = phi i32 [ %sub, %if.then ], [ %sub2, %if.else ]
  ret i32 %b.addr.0, !dbg !14
}

declare i32 @foo()
declare i32 @bar()

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "", isOptimized: false, runtimeVersion: 0, emissionKind: LineTablesOnly, enums: !2)
!1 = !DIFile(filename: "test.c", directory: "")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!6 = distinct !DISubprogram(name: "binop", scope: !1, file: !1, line: 8, type: !7, isLocal: false, isDefinition: true, scopeLine: 8, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!7 = !DISubroutineType(types: !2)
!8 = !DILocation(line: 9, column: 6, scope: !6)
!9 = !DILocation(line: 10, column: 10, scope: !6)
!10 = !DILocation(line: 10, column: 7, scope: !6)
!11 = !DILocation(line: 10, column: 5, scope: !6)
!12 = !DILocation(line: 12, column: 10, scope: !6)
!13 = !DILocation(line: 12, column: 7, scope: !6)
!14 = !DILocation(line: 13, column: 3, scope: !6)
