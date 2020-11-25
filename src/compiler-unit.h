#ifndef COMPILER_UNIT_H
#define COMPILER_UNIT_H
// *****************************************************************************
// compiler-unit.h                                                    XL project
// *****************************************************************************
//
// File description:
//
//     Information about a single compilation unit, i.e. the code generated
//     for a particular source file. This corresponds to an LLVM module,
//     and generates a single eval_fn function, i.e. an evaluation function
//     taking a Scope and a Tree as input. If the compilation fails, the
//     Compile() function returns a nullptr.
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3+
// (C) 2018-2020, Christophe de Dinechin <christophe@dinechin.org>
// *****************************************************************************
// This file is part of XL
//
// XL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// XL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with XL, in a file named COPYING.
// If not, see <https://www.gnu.org/licenses/>.
// *****************************************************************************

#include "compiler.h"
#include "compiler-rewrites.h"
#include "compiler-types.h"
#include "llvm-crap.h"
#include <map>

XL_BEGIN

typedef std::map<Tree *, JIT::Value_p>  value_map;
typedef std::map<text, JIT::Function_p> compiled_map;
typedef std::set<JIT::Type_p>           closure_set;

class CompilerUnit
// ----------------------------------------------------------------------------
//   A unit of compilation, roughly similar to a 'module' in LLVM
// ----------------------------------------------------------------------------
{
    Compiler &          compiler;       // The compiler environment we use
    JIT &               jit;            // The JIT compiler (LLVM CRAP)
    JITModule           module;         // The module we are compiling
    Context_p           context;        // Context in which we compile
    Tree_p              source;         // The source of the program to compile
    CompilerTypes_p     types;          // Type inferences for this unit
    value_map           globals;        // Global definitions in the unit
    compiled_map        compiled;       // Already compiled functions
    closure_set         clotypes;       // Closure types

    friend class        CompilerPrototype;
    friend class        CompilerFunction;
    friend class        CompilerEval;
    friend class        CompilerExpression;

public:
    CompilerUnit(Compiler &compiler, Scope *scope, Tree *source);
    ~CompilerUnit();

public:
    // Top-level compilation for the whole unit
    eval_fn             Compile();

    // Global values (defined at the unit level)
    JIT::Value_p        Global(Tree *tree);
    void                Global(Tree *tree, JIT::Value_p value);

    // Cache of already compiled functions
    JIT::Function_p &   Compiled(Scope *,
                                 RewriteCandidate *,
                                 const JIT::Values &);
    JIT::Function_p &   CompiledUnbox(JIT::Type_p type);
    JIT::Function_p &   CompiledClosure(Scope *, Tree *expr);

    // Closure types management
    bool                IsClosureType(JIT::Type_p type);
    void                AddClosureType(JIT::Type_p type);

private:
    // Import all runtime functions
#define MTYPE(Name, Arity, Code)
#define UNARY(Name)
#define BINARY(Name)
#define CAST(Name)
#define ALIAS(Name, Arity, Original)
#define SPECIAL(Name, Arity, Code)
#define EXTERNAL(Name, RetTy, ...)      JIT::Function_p  Name;
#include "compiler-primitives.tbl"
};

XL_END

RECORDER_DECLARE(compiler_unit);

#endif // COMPILER_UNIT_H
