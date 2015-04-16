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

// ============================================================================
// 
//    Main entry points
// 
// ============================================================================

Tree *Evaluate(Context *context, Tree *code);
Tree *EvaluateClosure(Context *context, Tree *code);
Tree *TypeCheck(Context *scope, Tree *type, Tree *value);



// ============================================================================
// 
//    Closure and opcode management
// 
// ============================================================================

Tree *IsClosure(Tree *value, Context_p *context);
Tree *MakeClosure(Context *context, Tree *value);

struct Opcode;
Opcode *SetInfo(Infix *decl, Opcode *opcode);
Opcode *OpcodeInfo(Infix *decl);



// ============================================================================
//
//    Closure management (keeping scoping information with values)
//
// ============================================================================

struct ScopeInfo : Info
// ----------------------------------------------------------------------------
//   Mark a given tree as a scope
// ----------------------------------------------------------------------------
{
    TreeList    declarations;
};


inline Tree *IsClosure(Tree *tree, Context_p *context)
// ----------------------------------------------------------------------------
//   Check if something is a closure, if so set scope and/or context
// ----------------------------------------------------------------------------
{
    if (Scope *closure = tree->AsPrefix())
    {
        if (Scope *scope = ScopeParent(closure))
        {
            if (scope->GetInfo<ScopeInfo>())
            {
                // We normally have a scope on the left
                if (context)
                    *context = new Context(scope);
                return closure->right;
            }
        }
    }
    return NULL;
}


inline Tree *MakeClosure(Context *ctx, Tree *value)
// ----------------------------------------------------------------------------
//   Create a closure encapsulating the current context
// ----------------------------------------------------------------------------
{
    Context_p context = ctx;

retry:
    kind valueKind = value->Kind();

    if (valueKind >= NAME || context->HasRewritesFor(valueKind))
    {
        if (valueKind == NAME)
        {
            if (Tree *bound = context->Bound(value))
            {
                if (Tree *inside = IsClosure(bound, &context))
                {
                    if (value != inside)
                    {
                        value = inside;
                        goto retry;
                    }
                }
                if (value != bound)
                {
                    value = bound;
                    goto retry;
                }
            }
        }

        Prefix *prefix = value->AsPrefix();
        if (!prefix || !prefix->left->GetInfo<ScopeInfo>())
        {
            Scope *scope = context->CurrentScope();
            XL_ASSERT(scope->GetInfo<ScopeInfo>());
            value = new Prefix(scope, value);
        }
    }
    return value;
}



// ============================================================================
// 
//    Inline implementations for main entry points
// 
// ============================================================================

inline Tree *Evaluate(Context *context, Tree *what)
// ----------------------------------------------------------------------------
//    Evaluate 'what', finding the final, non-closure result
// ----------------------------------------------------------------------------
{
    Tree *result = EvaluateClosure(context, what);
    if (Tree *inside = IsClosure(result, NULL))
        result = inside;
    return result;
}


XL_END

#endif // INTERPRETER_H
