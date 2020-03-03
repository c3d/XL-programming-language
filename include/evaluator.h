#ifndef EVALUATOR_H
#define EVALUATOR_H
// *****************************************************************************
// evaluator.h                                                        XL project
// *****************************************************************************
//
// File description:
//
//    An interface for all the kinds of evaluators of the XL language
//
//
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2018-2019, Christophe de Dinechin <christophe@dinechin.org>
// *****************************************************************************
// This file is part of XL
//
// XL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
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

#include "tree.h"

XL_BEGIN

class Evaluator
// ----------------------------------------------------------------------------
//   Pure virtual class defining the interface for XL tree evaluation
// ----------------------------------------------------------------------------
{
public:
    virtual ~Evaluator() {}

    virtual Tree *      Evaluate(Scope *, Tree *source) = 0;
    virtual Tree *      TypeCheck(Scope *, Tree *type, Tree *value) = 0;
};

XL_END

#endif // EVALUATOR_H
