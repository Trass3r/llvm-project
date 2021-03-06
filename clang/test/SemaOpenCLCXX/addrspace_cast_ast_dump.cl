// RUN: %clang_cc1 %s -triple spir-unknown-unknown -cl-std=clc++ -pedantic -verify -ast-dump | FileCheck %s

// expected-no-diagnostics

//CHECK:`-FunctionDecl {{.*}} bar 'void (__global int *__private)'
//CHECK:  |-ParmVarDecl {{.*}} used gl '__global int *__private'
//CHECK:      `-VarDecl {{.*}} gen '__generic int *__private' cinit
//CHECK:        `-CXXAddrspaceCastExpr {{.*}} '__generic int *' addrspace_cast<__generic int *> <AddressSpaceConversion>
//CHECK:          `-DeclRefExpr {{.*}} '__global int *__private' lvalue ParmVar {{.*}} 'gl' '__global int *__private'

void bar(global int *gl) {
  int *gen = addrspace_cast<int *>(gl);
}
