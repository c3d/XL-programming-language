// ****************************************************************************
//  opcodes_declare.h               (C) 1992-2009 Christophe de Dinechin (ddd)
//                                                                 XL2 project
// ****************************************************************************
//
//   File Description:
//
//     Macros used to declare built-ins.
//
//     Usage:
//     #include "opcodes_declare.h"
//     #include "builtins.tbl"
//
//     #include "opcodes_define.h"
//     #include "builtins.tbl"
//
//
// ****************************************************************************
// This document is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
// ****************************************************************************
// * File       : $RCSFile$
// * Revision   : $Revision$
// * Date       : $Date$
// ****************************************************************************

#undef INFIX
#undef PREFIX
#undef POSTFIX
#undef BLOCK
#undef NAME
#undef TYPE
#undef PARM
#undef DS
#undef RS
#undef RETURNS
#undef GROUP
#undef SYNOPSIS
#undef DESCRIPTION
#undef SEE

#ifndef XL_SCOPE
#define XL_SCOPE "xl_"
#endif // XL_SCOPE

#define DS(n) IFTRACE(builtins) std::cerr << "Builtin " #n ": " << self << '\n';

#define GROUP(grp)
#define SYNOPSIS(syno)
#define DESCRIPTION(desc)
#define RETURNS(rytpe, rdoc)
#define SEE(see)

#define INFIX(name, rtype, t1, symbol, t2, _code, docinfo)              \
    rtype##_nkp xl_##name(Tree *self,t1##_r l,t2##_r r)                 \
    {                                                                   \
        DS(symbol) _code;                                               \
    }                                                                   \
    static void xl_enter_infix_##name(Symbols *c, Main *main, text doc) \
    {                                                                   \
        Infix *ldecl = new Infix(":", new Name("l"), new Name(#t1));    \
        Infix *rdecl = new Infix(":", new Name("r"), new Name(#t2));    \
        Infix *from = new Infix(symbol, ldecl, rdecl);                  \
        Name *to = new Name(symbol);                                    \
        eval_fn fn = (eval_fn) xl_##name;                               \
        setDocumentation(from, doc);                                    \
        Rewrite *rw = c->EnterRewrite(from, to);                        \
        to->code = fn;                                                  \
        to->SetSymbols(c);                                              \
        to->Set<TypeInfo> (rtype##_type);                               \
        xl_enter_builtin(main, XL_SCOPE #name,                          \
                               to, rw->parameters, fn);                 \
    }

#define PARM(symbol, type, pdoc)      , type##_r symbol

#define PREFIX(name, rtype, symbol, parms, _code, docinfo)              \
    rtype##_nkp xl_##name(Tree *self parms)                             \
    {                                                                   \
        DS(symbol) _code;                                               \
    }                                                                   \
    static void xl_enter_prefix_##name(Symbols *c,                      \
                                       Main *main,                      \
                                       TreeList &parameters,            \
                                       text doc)                        \
    {                                                                   \
        eval_fn fn = (eval_fn) xl_##name;                               \
        if (parameters.size())                                          \
        {                                                               \
            Tree *parmtree = ParametersTree(parameters);                \
            Prefix *from = new Prefix(new Name(symbol), parmtree);      \
            Name *to = new Name(symbol);                                \
            setDocumentation(from, doc);                                \
            Rewrite *rw = c->EnterRewrite(from, to);                    \
            to->code = fn;                                              \
            to->SetSymbols(c);                                          \
            to->Set<TypeInfo> (rtype##_type);                           \
            xl_enter_builtin(main, XL_SCOPE #name,                      \
                                   to, rw->parameters, fn);             \
        }                                                               \
        else                                                            \
        {                                                               \
            Name *n  = new Name(symbol);                                \
            n->code = fn;                                               \
            n->SetSymbols (c);                                          \
            n ->Set<TypeInfo> (rtype##_type);                           \
            setDocumentation(n, doc);                                   \
            c->EnterName(symbol, n);                                    \
            TreeList noparms;                                           \
            xl_enter_builtin(main, XL_SCOPE #name, n, noparms, fn);     \
        }                                                               \
    }


#define POSTFIX(name, rtype, parms, symbol, _code, docinfo)             \
    rtype##_nkp xl_##name(Tree *self parms)                             \
    {                                                                   \
        DS(symbol) _code;                                               \
    }                                                                   \
                                                                        \
    static void xl_enter_postfix_##name(Symbols *c,                     \
                                        Main *main,                     \
                                        TreeList &parameters,           \
                                        text doc)                       \
    {                                                                   \
        Tree *parmtree = ParametersTree(parameters);                    \
        Postfix *from = new Postfix(parmtree, new Name(symbol));        \
        Name *to = new Name(symbol);                                    \
        eval_fn fn = (eval_fn) xl_##name;                               \
        setDocumentation(from, doc);                                    \
        Rewrite *rw = c->EnterRewrite(from, to);                        \
        to->code = fn;                                                  \
        to->SetSymbols(c);                                              \
        to->Set<TypeInfo> (rtype##_type);                               \
        xl_enter_builtin(main, XL_SCOPE #name,                          \
                               to, rw->parameters, to->code);           \
    }


#define BLOCK(name, rtype, open, type, close, _code, docinfo)           \
    rtype##_nkp xl_##name(Tree *self, type##_r child)                   \
    {                                                                   \
        DS(symbol) _code;                                               \
    }                                                                   \
    static void xl_enter_block_##name(Symbols *c, Main *main, doc)      \
    {                                                                   \
        Infix *parms = new Infix(":", new Name("V"), new Name(#type));  \
        Block *from = new Block(parms, open, close);                    \
        Name *to = new Name(#name);                                     \
        eval_fn fn = (eval_fn) xl_##name;                               \
        setDocumentation(from, doc);                                    \
        Rewrite *rw = c->EnterRewrite(from, to);                        \
        to->code = fn;                                                  \
        to->SetSymbols(c);                                              \
        to->Set<TypeInfo> (rtype##_type);                               \
        xl_enter_builtin(main, XL_SCOPE #name, to,                      \
                               rw->parameters, to->code);               \
    }

#define NAME(symbol)    \
    Name_p xl_##symbol;

#define TYPE(symbol)    Name_p symbol##_type;

