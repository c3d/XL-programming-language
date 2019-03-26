// ****************************************************************************
//  complex.xs                                      XL - An extensible language
// ****************************************************************************
//
//   File Description:
//
//      interface for complex numbers
//
//
//
//
//
//
//
//
// ****************************************************************************
//  (C) 2019 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the GNU General Public License v3
//   See LICENSE file for details.
// ****************************************************************************

use R = XL.TYPES.REAL

type complex[type real is R.real] with
// ----------------------------------------------------------------------------
//    Complex numbers taking the given `real` type
// ----------------------------------------------------------------------------

    // Complex numbers implement all of 'number'
    as number

    // Two common representations
    type cartesian              is type cartesian(Re:real, Im:real)
    type polar                  is type polar(Mod:real, Arg:real)

    // Converting to one of the representations is implicit
    Z:complex                   as cartesian
    Z:complex                   as polar
    Z:cartesian                 as complex
    Z:polar                     as complex
    X:real                      as complex

    // Special operations with real numbers as one operand
    Z:complex + X:real          as complex
    Z:complex - X:real          as complex
    Z:complex * X:real          as complex
    Z:complex / X:real          as complex
    X:real + Z:complex          as complex
    X:real - Z:complex          as complex
    X:real * Z:complex          as complex
    X:real / Z:complex          as complex

    // Notation for complex conjugate
    Conjugate Z:complex         as complex
    ~Z:complex                  as complex      is Conjugate Z


    // Real, imaginary, modulus and argument can be read or written
    Re                          : real
    Im                          : real
    Mod                         : real
    Arg                         : real

    // Unit imaginary real
    i                           as complex
    j                           as complex      is i

    // Shortcut notation for cartesian notation like `2 + 3i`
    syntax { postfix 400 "i" "j" }
    Re:real + Im:real i         as cartesian    is cartesian(Re, Im)
    Im:real i                   as cartesian    is cartesian(0, Im)
    Re:real + Im:real j         as cartesian    is cartesian(Re, Im)
    Im:real j                   as cartesian    is cartesian(0, Im)

    // Shortcut notation for polar notation like 2∠3
    syntax { infix 400 "∠" }
    syntax { postfix 400 "°" }
    Mod:real ∠ Arg:real°        as polar
    Mod:real ∠ Arg:real         as polar        is polar(Mod, Arg)

    // Some elementaty functions on complex numbers
    Abs     Z:complex           as real
    Sqrt    Z:complex           as complex

    Sin     Z:complex           as complex
    Cos     Z:complex           as complex
    Tan     Z:complex           as complex
    ArcSin  Z:complex           as complex
    ArcCos  Z:complex           as complex
    ArcTan  Z:complex           as complex

    SinH    Z:complex           as complex
    CosH    Z:complex           as complex
    TanH    Z:complex           as complex
    ArcSinH Z:complex           as complex
    ArcCosH Z:complex           as complex
    ArcTanH Z:complex           as complex

    Exp     Z:complex           as complex
    Log     Z:complex           as complex
