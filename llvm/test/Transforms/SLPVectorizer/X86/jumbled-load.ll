; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -S -mtriple=x86_64-unknown -mattr=+avx -slp-threshold=-10 -slp-vectorizer | FileCheck %s

@total = common global i32 0, align 4

define i32 @jumbled-load(i32* noalias nocapture %in, i32* noalias nocapture %inn, i32* noalias nocapture %out) {
; CHECK-LABEL: @jumbled-load(
; CHECK-NEXT:    [[IN_ADDR:%.*]] = getelementptr inbounds i32, i32* [[IN:%.*]], i64 0
; CHECK-NEXT:    [[GEP_1:%.*]] = getelementptr inbounds i32, i32* [[IN_ADDR]], i64 3
; CHECK-NEXT:    [[GEP_2:%.*]] = getelementptr inbounds i32, i32* [[IN_ADDR]], i64 1
; CHECK-NEXT:    [[GEP_3:%.*]] = getelementptr inbounds i32, i32* [[IN_ADDR]], i64 2
; CHECK-NEXT:    [[TMP1:%.*]] = bitcast i32* [[IN_ADDR]] to <4 x i32>*
; CHECK-NEXT:    [[TMP2:%.*]] = load <4 x i32>, <4 x i32>* [[TMP1]], align 4
; CHECK-NEXT:    [[TMP3:%.*]] = shufflevector <4 x i32> [[TMP2]], <4 x i32> undef, <4 x i32> <i32 1, i32 3, i32 2, i32 0>
; CHECK-NEXT:    [[INN_ADDR:%.*]] = getelementptr inbounds i32, i32* [[INN:%.*]], i64 0
; CHECK-NEXT:    [[GEP_4:%.*]] = getelementptr inbounds i32, i32* [[INN_ADDR]], i64 2
; CHECK-NEXT:    [[GEP_5:%.*]] = getelementptr inbounds i32, i32* [[INN_ADDR]], i64 3
; CHECK-NEXT:    [[GEP_6:%.*]] = getelementptr inbounds i32, i32* [[INN_ADDR]], i64 1
; CHECK-NEXT:    [[TMP4:%.*]] = bitcast i32* [[INN_ADDR]] to <4 x i32>*
; CHECK-NEXT:    [[TMP5:%.*]] = load <4 x i32>, <4 x i32>* [[TMP4]], align 4
; CHECK-NEXT:    [[TMP6:%.*]] = shufflevector <4 x i32> [[TMP5]], <4 x i32> undef, <4 x i32> <i32 0, i32 1, i32 3, i32 2>
; CHECK-NEXT:    [[TMP7:%.*]] = mul <4 x i32> [[TMP3]], [[TMP6]]
; CHECK-NEXT:    [[GEP_7:%.*]] = getelementptr inbounds i32, i32* [[OUT:%.*]], i64 0
; CHECK-NEXT:    [[GEP_8:%.*]] = getelementptr inbounds i32, i32* [[OUT]], i64 1
; CHECK-NEXT:    [[GEP_9:%.*]] = getelementptr inbounds i32, i32* [[OUT]], i64 2
; CHECK-NEXT:    [[GEP_10:%.*]] = getelementptr inbounds i32, i32* [[OUT]], i64 3
; CHECK-NEXT:    [[TMP8:%.*]] = bitcast i32* [[GEP_7]] to <4 x i32>*
; CHECK-NEXT:    store <4 x i32> [[TMP7]], <4 x i32>* [[TMP8]], align 4
; CHECK-NEXT:    ret i32 undef
;
  %in.addr = getelementptr inbounds i32, i32* %in, i64 0
  %load.1 = load i32, i32* %in.addr, align 4
  %gep.1 = getelementptr inbounds i32, i32* %in.addr, i64 3
  %load.2 = load i32, i32* %gep.1, align 4
  %gep.2 = getelementptr inbounds i32, i32* %in.addr, i64 1
  %load.3 = load i32, i32* %gep.2, align 4
  %gep.3 = getelementptr inbounds i32, i32* %in.addr, i64 2
  %load.4 = load i32, i32* %gep.3, align 4
  %inn.addr = getelementptr inbounds i32, i32* %inn, i64 0
  %load.5 = load i32, i32* %inn.addr, align 4
  %gep.4 = getelementptr inbounds i32, i32* %inn.addr, i64 2
  %load.6 = load i32, i32* %gep.4, align 4
  %gep.5 = getelementptr inbounds i32, i32* %inn.addr, i64 3
  %load.7 = load i32, i32* %gep.5, align 4
  %gep.6 = getelementptr inbounds i32, i32* %inn.addr, i64 1
  %load.8 = load i32, i32* %gep.6, align 4
  %mul.1 = mul i32 %load.3, %load.5
  %mul.2 = mul i32 %load.2, %load.8
  %mul.3 = mul i32 %load.4, %load.7
  %mul.4 = mul i32 %load.1, %load.6
  %gep.7 = getelementptr inbounds i32, i32* %out, i64 0
  store i32 %mul.1, i32* %gep.7, align 4
  %gep.8 = getelementptr inbounds i32, i32* %out, i64 1
  store i32 %mul.2, i32* %gep.8, align 4
  %gep.9 = getelementptr inbounds i32, i32* %out, i64 2
  store i32 %mul.3, i32* %gep.9, align 4
  %gep.10 = getelementptr inbounds i32, i32* %out, i64 3
  store i32 %mul.4, i32* %gep.10, align 4

  ret i32 undef
}

; Make sure we can sort loads even if they have non-constant offsets, as long as
; the offset *differences* are constant and computable by SCEV.
define void @scev(i64 %N, i32* nocapture readonly %b, i32* nocapture readonly %c) {
; CHECK-LABEL: @scev(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP_OUTER:%.*]] = icmp sgt i64 [[N:%.*]], 0
; CHECK-NEXT:    br i1 [[CMP_OUTER]], label [[FOR_BODY_PREHEADER:%.*]], label [[FOR_END:%.*]]
; CHECK:       for.body.preheader:
; CHECK-NEXT:    br label [[FOR_BODY:%.*]]
; CHECK:       for.body:
; CHECK-NEXT:    [[I_P:%.*]] = phi i64 [ [[ADD21:%.*]], [[FOR_BODY]] ], [ 0, [[FOR_BODY_PREHEADER]] ]
; CHECK-NEXT:    [[TMP0:%.*]] = phi <4 x i32> [ [[TMP14:%.*]], [[FOR_BODY]] ], [ zeroinitializer, [[FOR_BODY_PREHEADER]] ]
; CHECK-NEXT:    [[ARRAYIDX:%.*]] = getelementptr inbounds i32, i32* [[B:%.*]], i64 [[I_P]]
; CHECK-NEXT:    [[ARRAYIDX1:%.*]] = getelementptr inbounds i32, i32* [[C:%.*]], i64 [[I_P]]
; CHECK-NEXT:    [[ADD3:%.*]] = or i64 [[I_P]], 1
; CHECK-NEXT:    [[ARRAYIDX4:%.*]] = getelementptr inbounds i32, i32* [[B]], i64 [[ADD3]]
; CHECK-NEXT:    [[ARRAYIDX6:%.*]] = getelementptr inbounds i32, i32* [[C]], i64 [[ADD3]]
; CHECK-NEXT:    [[ADD9:%.*]] = or i64 [[I_P]], 2
; CHECK-NEXT:    [[ARRAYIDX10:%.*]] = getelementptr inbounds i32, i32* [[B]], i64 [[ADD9]]
; CHECK-NEXT:    [[ARRAYIDX12:%.*]] = getelementptr inbounds i32, i32* [[C]], i64 [[ADD9]]
; CHECK-NEXT:    [[ADD15:%.*]] = or i64 [[I_P]], 3
; CHECK-NEXT:    [[ARRAYIDX16:%.*]] = getelementptr inbounds i32, i32* [[B]], i64 [[ADD15]]
; CHECK-NEXT:    [[TMP1:%.*]] = bitcast i32* [[ARRAYIDX]] to <4 x i32>*
; CHECK-NEXT:    [[TMP2:%.*]] = load <4 x i32>, <4 x i32>* [[TMP1]], align 4
; CHECK-NEXT:    [[TMP3:%.*]] = shufflevector <4 x i32> [[TMP2]], <4 x i32> undef, <4 x i32> <i32 2, i32 1, i32 0, i32 3>
; CHECK-NEXT:    [[TMP4:%.*]] = bitcast i32* [[ARRAYIDX]] to <4 x i32>*
; CHECK-NEXT:    [[TMP5:%.*]] = load <4 x i32>, <4 x i32>* [[TMP4]], align 4
; CHECK-NEXT:    [[TMP6:%.*]] = shufflevector <4 x i32> [[TMP5]], <4 x i32> undef, <4 x i32> <i32 2, i32 1, i32 0, i32 3>
; CHECK-NEXT:    [[ARRAYIDX18:%.*]] = getelementptr inbounds i32, i32* [[C]], i64 [[ADD15]]
; CHECK-NEXT:    [[TMP7:%.*]] = bitcast i32* [[ARRAYIDX1]] to <4 x i32>*
; CHECK-NEXT:    [[TMP8:%.*]] = load <4 x i32>, <4 x i32>* [[TMP7]], align 4
; CHECK-NEXT:    [[TMP9:%.*]] = shufflevector <4 x i32> [[TMP8]], <4 x i32> undef, <4 x i32> <i32 2, i32 1, i32 0, i32 3>
; CHECK-NEXT:    [[TMP10:%.*]] = bitcast i32* [[ARRAYIDX1]] to <4 x i32>*
; CHECK-NEXT:    [[TMP11:%.*]] = load <4 x i32>, <4 x i32>* [[TMP10]], align 4
; CHECK-NEXT:    [[TMP12:%.*]] = shufflevector <4 x i32> [[TMP11]], <4 x i32> undef, <4 x i32> <i32 2, i32 1, i32 0, i32 3>
; CHECK-NEXT:    [[TMP13:%.*]] = add <4 x i32> [[TMP3]], [[TMP0]]
; CHECK-NEXT:    [[TMP14]] = add <4 x i32> [[TMP13]], [[TMP12]]
; CHECK-NEXT:    [[ADD21]] = add nuw nsw i64 [[I_P]], 4
; CHECK-NEXT:    [[CMP:%.*]] = icmp slt i64 [[ADD21]], [[N]]
; CHECK-NEXT:    br i1 [[CMP]], label [[FOR_BODY]], label [[FOR_END_LOOPEXIT:%.*]]
; CHECK:       for.end.loopexit:
; CHECK-NEXT:    br label [[FOR_END]]
; CHECK:       for.end:
; CHECK-NEXT:    [[TMP15:%.*]] = phi <4 x i32> [ zeroinitializer, [[ENTRY:%.*]] ], [ [[TMP14]], [[FOR_END_LOOPEXIT]] ]
; CHECK-NEXT:    [[TMP16:%.*]] = extractelement <4 x i32> [[TMP15]], i32 0
; CHECK-NEXT:    [[TMP17:%.*]] = extractelement <4 x i32> [[TMP15]], i32 1
; CHECK-NEXT:    [[ADD22:%.*]] = add nsw i32 [[TMP17]], [[TMP16]]
; CHECK-NEXT:    [[TMP18:%.*]] = extractelement <4 x i32> [[TMP15]], i32 2
; CHECK-NEXT:    [[ADD23:%.*]] = add nsw i32 [[ADD22]], [[TMP18]]
; CHECK-NEXT:    [[TMP19:%.*]] = extractelement <4 x i32> [[TMP15]], i32 3
; CHECK-NEXT:    [[ADD24:%.*]] = add nsw i32 [[ADD23]], [[TMP19]]
; CHECK-NEXT:    store i32 [[ADD24]], i32* @total, align 4
; CHECK-NEXT:    ret void
;
entry:
  %cmp.outer = icmp sgt i64 %N, 0
  br i1 %cmp.outer, label %for.body.preheader, label %for.end

for.body.preheader:                               ; preds = %entry
  br label %for.body

for.body:                                         ; preds = %for.body.preheader, %for.body
  %a4.p = phi i32 [ %add20, %for.body ], [ 0, %for.body.preheader ]
  %a3.p = phi i32 [ %add2, %for.body ], [ 0, %for.body.preheader ]
  %a2.p = phi i32 [ %add8, %for.body ], [ 0, %for.body.preheader ]
  %a1.p = phi i32 [ %add14, %for.body ], [ 0, %for.body.preheader ]
  %i.p = phi i64 [ %add21, %for.body ], [ 0, %for.body.preheader ]
  %arrayidx = getelementptr inbounds i32, i32* %b, i64 %i.p
  %0 = load i32, i32* %arrayidx, align 4
  %arrayidx1 = getelementptr inbounds i32, i32* %c, i64 %i.p
  %1 = load i32, i32* %arrayidx1, align 4
  %add = add i32 %0, %a3.p
  %add2 = add i32 %add, %1
  %add3 = or i64 %i.p, 1
  %arrayidx4 = getelementptr inbounds i32, i32* %b, i64 %add3
  %2 = load i32, i32* %arrayidx4, align 4
  %arrayidx6 = getelementptr inbounds i32, i32* %c, i64 %add3
  %3 = load i32, i32* %arrayidx6, align 4
  %add7 = add i32 %2, %a2.p
  %add8 = add i32 %add7, %3
  %add9 = or i64 %i.p, 2
  %arrayidx10 = getelementptr inbounds i32, i32* %b, i64 %add9
  %4 = load i32, i32* %arrayidx10, align 4
  %arrayidx12 = getelementptr inbounds i32, i32* %c, i64 %add9
  %5 = load i32, i32* %arrayidx12, align 4
  %add13 = add i32 %4, %a1.p
  %add14 = add i32 %add13, %5
  %add15 = or i64 %i.p, 3
  %arrayidx16 = getelementptr inbounds i32, i32* %b, i64 %add15
  %6 = load i32, i32* %arrayidx16, align 4
  %arrayidx18 = getelementptr inbounds i32, i32* %c, i64 %add15
  %7 = load i32, i32* %arrayidx18, align 4
  %add19 = add i32 %6, %a4.p
  %add20 = add i32 %add19, %7
  %add21 = add nuw nsw i64 %i.p, 4
  %cmp = icmp slt i64 %add21, %N
  br i1 %cmp, label %for.body, label %for.end.loopexit

for.end.loopexit:                                 ; preds = %for.body
  br label %for.end

for.end:                                          ; preds = %for.end.loopexit, %entry
  %a1.0.lcssa = phi i32 [ 0, %entry ], [ %add14, %for.end.loopexit ]
  %a2.0.lcssa = phi i32 [ 0, %entry ], [ %add8, %for.end.loopexit ]
  %a3.0.lcssa = phi i32 [ 0, %entry ], [ %add2, %for.end.loopexit ]
  %a4.0.lcssa = phi i32 [ 0, %entry ], [ %add20, %for.end.loopexit ]
  %add22 = add nsw i32 %a2.0.lcssa, %a1.0.lcssa
  %add23 = add nsw i32 %add22, %a3.0.lcssa
  %add24 = add nsw i32 %add23, %a4.0.lcssa
  store i32 %add24, i32* @total, align 4
  ret void
}
