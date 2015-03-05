#ifndef INTERPRETER_H
#define INTERPRETER_H
// ****************************************************************************
//  interpreter.h                                                  XLR project
// ****************************************************************************
//
//   File Description:
//
//     A fully interpreted mode for XLR, that does not rely on LLVM at all
//
//
//
//
//
//
//
//
// ****************************************************************************
//  (C) 2015 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2015 Taodyne SAS
// ****************************************************************************

#include "tree.h"
#include "context.h"


XL_BEGIN

Tree *Evaluate(Context *scope, Tree *code);
Tree *TypeCheck(Context *scope, Tree *value, Tree *type);

XL_END

#endif // INTERPRETER_H