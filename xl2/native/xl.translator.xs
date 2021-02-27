// *****************************************************************************
// xl.translator.xs                                                   XL project
// *****************************************************************************
//
// File description:
//
//     The basic XL translator
//
//     NOTE: 'translation' statements introduce new entries into that
//     namespace. For instance a 'translation Semantics' will add a
//     'Semantics' function into that namespace for Semantcs rewrites
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3+
// (C) 2003-2009,2015,2020, Christophe de Dinechin <christophe@dinechin.org>
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

import PT = XL.PARSER.TREE
import IO = XL.TEXT_IO
import SYM = XL.SYMBOLS
import BC = XL.BYTECODE


module XL.TRANSLATOR with
// ----------------------------------------------------------------------------
//   The transator deals with global translation phases and data structures
// ----------------------------------------------------------------------------

    procedure Compile(input : PT.tree)
    procedure Compile(input : PT.tree; output : text)
    procedure Compile(input : PT.tree; output : IO.output_file)

    context           : SYM.symbol_table := nil
    global_context    : SYM.symbol_table := nil
    function_context  : SYM.symbol_table := nil
    main_context      : SYM.symbol_table := nil
    top_context       : SYM.symbol_table := nil
    full_compile      : boolean := false
    verbose           : boolean := false
    runtime           : text := "default"
    runtime_name      : text := ""
    syntax_file       : text := "xl.syntax"
    style_file        : text := "xl.stylesheet"
    bytecode_file     : text := "xl.bytecode"

    nop               : BC.bytecode

    procedure InitializeTranslator

    procedure ChangeRuntime(newRuntime : text)
    function RuntimePath(fileName : text) return text

    // Invokation of Semantics on scopes
    // In XL, all symbols in a scope are visible within that scope
    // This function deals with the two-pass declaration process
    type scope_kind is enumeration
        scopeMain, scopeModule, scopeGlobal,
        scopeFunction, scopeLocal,
        scopeField, scopeArgs

    procedure CopyScopeProperties(toTable   : SYM.symbol_table;
                                  fromTable : SYM.symbol_table)
    function ScopeSemantics (input   : PT.tree;
                             scope   : scope_kind) return BC.bytecode
    function ScopeSemantics (input   : PT.tree;
                             scope   : scope_kind;
                             modname : PT.tree) return BC.bytecode
    function PostScopeSemantics (input   : PT.tree;
                                 scope   : scope_kind;
                                 modname : PT.tree) return BC.bytecode

    // Tentative translation of a specific compiler phase
    type attempt_data
    type attempt is access to attempt_data
    function BeginAttempt() return attempt
    procedure ClearAttempt(what : attempt; globals : boolean)
    function EndAttempt(what : attempt) return boolean
    function TentativeSemantics(what : PT.tree) return PT.tree

    // Recursive implementation of something
    type recurse_fn is function(input : PT.tree) return BC.bytecode
    function Recurse(input : PT.tree;
                     what  : recurse_fn) return BC.bytecode

    // Append two trees
    function Append (t1 : PT.tree; t2 : PT.tree) return PT.tree


    // The following functions populate the local context
    procedure AddScopeDecl(table: SYM.symbol_table; decl : PT.tree)
    procedure AddScopeInit(table: SYM.symbol_table; init : PT.tree)
    procedure AddScopeTerm(table: SYM.symbol_table; term : PT.tree)
    procedure AddGlobalDecl(decl : PT.tree)
    procedure AddGlobalInit(init : PT.tree)
    procedure AddGlobalTerm(term : PT.tree)


    // The following functions are automatically generated

    // PluginsInit contains code initializing translation entries in
    // the symbols table
    // XLSemantics and XLOptimization contain the code generated by
    // 'translation XLSemantics' and 'translation XLOptimization'
    procedure PluginsInit                                      // Generated
    function XLEvaluateType(input: PT.tree) return BC.bytecode // Generated
    function XLDeclarations(input: PT.tree) return BC.bytecode // Generated
    function XLSemantics(input: PT.tree) return BC.bytecode    // Generated
    function XLOptimizations(input: PT.tree) return BC.bytecode// Generated

    function XLEnterFunction(input: PT.tree) return PT.tree    // Generated
    function XLEnterIterator(input: PT.tree) return PT.tree    // Generated
    function XLEnterGeneric(input: PT.tree) return PT.tree     // Generated
    function XLConstant(input : PT.tree) return PT.tree        // Generated
    function XLMacros(input : PT.tree) return PT.tree          // Generated
    function XL2C(input : PT.tree) return PT.tree              // Generated
    function XLNormalizeGeneric(input : PT.tree) return PT.tree// Generated
