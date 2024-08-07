; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -passes=slp-vectorizer -S -o - -mtriple=x86_64-unknown-linux -mcpu=bdver2 -pass-remarks-output=%t | FileCheck %s
; RUN: FileCheck --input-file=%t --check-prefix=YAML %s

; YAML: --- !Missed
; YAML-NEXT: Pass:            slp-vectorizer
; YAML-NEXT: Name:            NotPossible
; YAML-NEXT: Function:        g
; YAML-NEXT: Args:
; YAML-NEXT:   - String:          'Cannot SLP vectorize list: vectorization was impossible'
; YAML-NEXT:   - String:          ' with available vectorization factors'

define <2 x i32> @g(<2 x i32> %x, i32 %a, i32 %b) {
; CHECK-LABEL: @g(
; CHECK-NEXT:    [[X1:%.*]] = extractelement <2 x i32> [[X:%.*]], i32 1
; CHECK-NEXT:    [[X1X1:%.*]] = mul i32 [[X1]], [[X1]]
; CHECK-NEXT:    [[AB:%.*]] = mul i32 [[A:%.*]], [[B:%.*]]
; CHECK-NEXT:    [[INS1:%.*]] = insertelement <2 x i32> poison, i32 [[X1X1]], i32 0
; CHECK-NEXT:    [[INS2:%.*]] = insertelement <2 x i32> [[INS1]], i32 [[AB]], i32 1
; CHECK-NEXT:    ret <2 x i32> [[INS2]]
;
  %x1 = extractelement <2 x i32> %x, i32 1
  %x1x1 = mul i32 %x1, %x1
  %ab = mul i32 %a, %b
  %ins1 = insertelement <2 x i32> poison, i32 %x1x1, i32 0
  %ins2 = insertelement <2 x i32> %ins1, i32 %ab, i32 1
  ret <2 x i32> %ins2
}

