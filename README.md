# XL - An extensible language

> **WARNING**: XL is a work in progress. Even if there are some bits
> and pieces that happen to already work, XL is presently not suitable
> for any serious programming. Examples given below may sometimes simply not
> work. Take it as a painful reminder that the work is far from finished,
> and, who knows, as an idea for a contribution.
> See [HISTORY](docs/HANDBOOK.adoc#history) for how we came to the present mess, and
> [Compiler status](#compiler-status) for information about what is
> expected to work, and [Compiler overview](#compiler-overview) for a
> quick overview of the compiler internals.

XL is an extensible programming language designed to accomodate a
variety of programming needs with ease. Being _extensible_ means that
the language is designed to make it very easy for programmers to adapt
the language to suit their needs, for example by adding new
programming constructs. In XL, extending the language is a routine
operation, much like adding a function or creating a class in more
traditional programming languages.

As a validation of this bold claim, XL has a single fundamental operator,
the [definition operator](#semantics-one-operator-to-rule-them-all),
which you write `[Pattern] is [Implementation`, where] `[Pattern]` is
a program pattern, like `X+Y`, and `[Implementation]` explains how to
translate that pattern, for example `Add X, Y`.

Everything that is built-in in most other programming languages, from
basic data types to arithmetic to conditionals to loops is provided by
the standard library in XL. You can replace these constructs if you
want, or add your own. Adding a new kind of loop is not more difficult
in XL than adding a function, and it uses the same syntax.

* [Docker Quickstart](#docker-quickstart)
* [Simple examples](#a-few-simple-examples)
* [Dialects and use cases](#dialects-and-use-cases)
* [If you come from another language](#if-you-come-from-another-language)
* [One operator to rule them all](#semantics-one-operator-to-rule-them-all)
* [Syntax: Look, Ma, no keywords!](#syntax-look-ma-no-keywords)
* [XL as a functional language](#XL-as-a-functional-language)
* [Subtelty #1: Expression vs statement](#subtlety-1-expression-vs-statement)
* [Subtelty #2: Infix vs. Prefix](#subtlety-2-infix-vs-prefix)
* [Subtelty #3: Delayed evaluation](#subtlety-3-delayed-evaluation)
* [Subtelty #4: Closures](#subtlety-4-closures)

For more information, please consult the
[XL handbook](https://c3d.github.io/xl/HANDBOOK.html),
also available in [asciidoc format](docs/HANDBOOK.adoc) and
[PDF format](docs/HANDBOOK.pdf)

**WARNING** This documentation, like the compiler, is work in progress
and presently extremely messy, incomplete and inaccurate.


## Docker Quickstart

To quickly try out the intrepreter, install Docker and run the following commands:

```
docker build . -t xl
docker run -i xl
```

(Note: to see output, you need to send an EOF signal using Control-D)

## A few simple examples

A program computing the factorial of numbers between 1 and 5 would be
written as follows:
```xl
0! is 1
N! is N * (N-1)!

for I in 1..5 loop
    print "The factorial of ", I, " is ", I!
```


As a testament to its extensible nature, fundamental operations in XL
are defined in the standard library, including operations that would
be implemented using keywords in more traditional languages. For
example, the `if` statement in XL is defined by the following code:

```xl
if [[true]]  then TrueClause else FalseClause   is TrueClause
if [[false]] then TrueClause else FalseClause   is FalseClause
if [[true]]  then TrueClause                    is TrueClause
if [[false]] then TrueClause                    is false
```

Similarly, the `while` loop is defined as follows:

```xl
while Condition loop Body is
    if Condition then
        Body
        while Condition loop Body
```

The standard library also provides implementations for usual
operations. For example, if you evaluate `1+3`, this is done through a
definition for `+` on `integer` values that looks like the following
(where `...` denotes some implementation-dependent code):

```xl
X:integer + Y:integer is ...
```


## Dialects and use cases

Two dialects of XL further demonstrate the extensibility of the language

* [Tao3D](http://tao3d.sf.net) focuses on real-time 3D animations
  and can be used as a scriptable presentation software, or as someone
  once described it, a sort of real-time 3D LaTeX Lisp. In Tao3D, you
  describe a slide with a program that looks like the following code:

  ```xl
  import Slides

  slide "A simple slide example",
      * "This looks like some kind of markdown language"
      * "But code makes it powerful: your mouse is on the " & position
      position is
          if mouse_x < 0 then "left" else "right"
  ```

   >The examples above use the new syntax in XL, with `is` as its definition
   >operator. Older variants of the language used `->` instead. If
   >you downloaded a pre-built binary of Tao3D, chances are that you need
   >to replace `is` with `->` for the code above to work as intended.

* [ELFE](http://github.com/c3d/elfe), formerly ELIOT (Extensible
  Language for the Internet of things) was an experiment on how to
  write distributed software that looks like a single program, for
  example to control swarms of devices in the context of the Internet
  of Things. An example of a simple ELFE program would be:

  ```xl
  WORKER is "worker.mycorp.com"
  MIN_TEMP is 25
  MAX_TEMP is 55

  invoke WORKER,
      every 2s,
          reply
              display temperature

  display Temp:real is
      print "The temperature of ", WORKER, " is ", Temp
  ```

> The present branch, `bigmerge`, is an ongoing effort to _reconverge_
> the various dialects of XL. At the moment, it should pass most of
> the ELFE-level tests, although this is not actively tested. Getting
> it to support Tao3D is somewhat more difficult and may take some time.


## If you come from another language

If you are familiar with other programming languages, here are a few
things that may surprise you about XL.

* There are no keywords. In C, `if` is a keyword. In XL, it's just a name.
* The language is designed primarily to be readable and writable by humans.
  For example, there are [special parsing rules](#subtlety-1-expression-vs-statement) to match how we read the code.
* The language is _homoiconic_, i.e. programs are data, like in Lisp.
  This forms the basis of XL extensibility.
* Evaluation is defined entirely in terms of rewrites of a very simple abstract.
  syntax tree that represents the program being evaluated.
* The precedence of all operators is dynamic, in the sense that it's
  loaded from a [configuration file](src/xl.syntax)
* The language is primarily defined by its own
  [standard library](src/builtins.xl), rather than by special rules in
  the compiler.


### Semantics: One operator to rule them all

XL has one fundamental operator, `is`, the _definition operator_.
This operator can be read as *transforms into*, i.e. it transforms the
code that is on the left into the code that is on the right.

<details>
<summary>It can define simple variables</summary>

```xl
pi              is      3.1415926
```
</details>

<details>
<summary>It can define lists</summary>

```xl
words           is      "xylophage", "zygomatic", "barfitude"
```
</details>

<details>
<summary>It can define functions</summary>

```xl
abs X           is      if X < 0 then -X else X
```

</details>
<details>
<summary>It can define operators</summary>

```xl
X ≠ Y           is      not X = Y
```


</details>
<details>
<summary>It can define specializations for particular inputs</summary>

```xl
0!              is      1
N!  when N > 0  is      N * (N-1)!
```

</details>
<details>
<summary>It can define notations using arbitrary combinations of operators</summary>

```xl
A in B..C       is      A >= B and A <= C
```

</details>
<details>
<summary>It can define optimizations using specializations</summary>

```xl
X * 1           is      X
X + 0           is      X
```

</details>
<details>
<summary>It can define program structures</summary>

```xl
loop Body       is      Body; loop Body
```


</details>
<details>
<summary>It can define types</summary>

```xl
type complex    is      polar or cartesian
type cartesian  is      cartesian(re:number, im:number)
type polar      is      polar(mod:number, arg:number)
```

Note that types in XL indicate the shape of parse trees. In other
words, the `cartesian` type above will match any parse tree that takes
the shape of the word `cartesian` followed by two numbers, like for
example `cartesian(1,5)`.
</details>
<details>
<summary>It can define higher-order functions, i.e. functions that return functions</summary>

```xl
adder N         is      (X is N + X)
add3            is      adder 3

 // This will compute 8
 add3 5
```
This makes XL a truly functional language.

</details>
<details>
<summary>It can define maps associating a key to a value</summary>

```xl
my_map is
    0 is 4
    1 is 0
    8 is "World"
    27 is 32
    N when N < 45 is N + 1

// The following is "World"
my_map 8

// The following is 32
my_map[27]

// The following is 45
my_map (44)
```

This provides a functionality roughly equivalent to `std::map` in
C++. However, it's really nothing more than a regular function with a
number of special cases. The compiler can optimize some special kinds
of mapping to provide an efficient implementation.

</details>
<details>
<summary>It can define templates (C++ terminology) or generics (Ada terminology)</summary>_

```xl
// An (inefficient) implementation of a generic 1-based array type
type array [1] of T is
    Value : T
    1 is Value
type array [N] of T when N > 1 is
    Head  : array[N-1] of T
    Tail  : T
    I when I<N is Head[I]
    I when I=N is Tail

A : array[5] of integer
for I in 1..5 loop
    A[I] := I * I
```

</details>
<details>
<summary>It can define variadic functions</summary>

```xl
min X       is X
min X, Y    is { Z is min Y; if X < Z then X else Z }

// Computes 4
min 7, 42, 20, 8, 4, 5, 30
```
</details>

In short, the single `is` operator covers all kinds of declarations
that are found in other languages, using a single, easy to read
syntax.


### Syntax: Look, Ma, no keywords!

XL has no keywords. Instead, the syntax relies on a rather simple
[recursive descent](https://en.wikipedia.org/wiki/Recursive_descent_parser)
[parser](src/parser.cpp).

THe parser generates a parse tree made of 8 node types. The first four
node types are leaf nodes:

 * `Integer` is for integer numbers such as `2` or `16#FFFF_FFFF`.
 * `Real` is for real numbers such as `2.5` or `2#1.001_001_001#e-3`
 * `Text` is for text values such as `"Hello"` or `'World'`. Text can
   be encoded using UTF-8
 * `Name` is for names and symbols such as `ABC` or `**`

The last four node types are inner nodes:

 * `Infix` are nodes where a named operator separates the operands,
   e.g. `A+B` or `A and B`.
 * `Prefix` are nodes where the operator precedes the operand, e.g.
   `+X` or `sin X`. By default, functions are prefix.
 * `Postfix` are nodes where the operator follows the operand, e.g.
   `3%` or `5km`.
 * `Block` are nodes with only one child surrounded by delimiters,
   such as `(A)`, `[A]` or `{A}`.

Of note, the line separator is an infix that separates statements,
much like the semi-colon `;`. The comma `,` infix is traditionally
used to build lists or to separate the argument of
functions. Indentation forms a special kind of block.

For example, the following code:

```xl
    tell "foo",
        if A < B+C then
            hello
        world
```

parses as a prefix `tell`, with an infix `,` as its right argument. On
the left of the `,` there is the text `"foo"`. On the right, there is
an indentation block with a child that is an infix line separator. On
the left of the line separator is the `if` statement. On the right is
the name `world`.

This parser is dynamically configurable, with the default priorities
being defined by the [xl.syntax](src/xl.syntax) file.

Parse trees are the fundamendal data structure in XL. Any data or
program can be represented as a parse tree. Program evaluation is
defined as transformation of parse trees.


### XL as a functional language

XL can be seen as a functional language, where functions are
first-class entities, i.e. you can manipulate them, pass them around,
etc:

```xl
adder X:integer is (Y is Y + X)

add3 is adder 3
add5 is adder 5

print "3+2=", add3 2
print "5+17=", add5 17
print "8+2=", (adder 8) 2
```

However, it is a bit different in the sense that the core data
structure is the parse tree. Some specific parse trees, for example
`A+B`, are not naturally reduced to a function call, although they are
subject to the same evaluation rules based on tree rewrites.


### Subtlety #1: expression vs. statement

The XL parse tree is designed to represent programs in a way that
is relatively natural for human beings. In that sense, it departs from
languages such as Lisp or SmallTalk.

However, being readable for humans requires a few special rules to
match the way we read expressions. Consider for example the following:

```xl
write sin X, cos Y
```

Most human beings parse this as meaning `write (sin(X),cos(Y))`,
i.e. we call `write` with two values resulting from evaluating `sin X`
and `cos Y`. This is not entirely logical. If `write` takes
comma-separated arguments, why wouldn't `sin` also take
comma-separated arguments? In other words, why doesn't this parse as
`write(sin(X, cos(Y))`?

This shows that humans have a notion of *expressions*
vs. *statements*. Expressions such as `sin X` have higher priority
than commas and require parentheses if you want multiple arguments. By
contrast, statements such as `write` have lower priority, and will
take comma-separated argument lists. An indent or `{ }` block begins a
statement, whereas parentheses `()` or square brackets `[]` begin an
expression.

There are rare cases where the default rule will not achieve the
desired objective, and you will need additional parentheses.

### Subtlety #2: infix vs. prefix

Another special rule is that XL will use the presence of a space on
only one side of an operator to disambiguate between an infix or a
prefix. For example:

```xl
write -A   // write (-A)
B - A      // (B - A)
```

### Subtlety #3: Delayed evaluation

When you pass an argument to a function, evaluation happens only when
necessary. Deferred evaluation may happen multiple times, which is
necessary in many cases, but awful for performance if you do it by
mistake.

Consider the following definition of `every`:

```xl
every Duration, Body is
    loop
        Body
        sleep Duration
```

In that case, we want the `Body` to be evaluated every iteration,
since this is typically an operation that we want to execute at each
loop. Is the same true for `Duration`? Most likely, no.

One way to force evaluation is to give a type to the argument. If you
want to force early evaluation of the argument, and to check that it
is a real value, you can do it as follows:

```xl
every Duration:real, Body is
    loop
        Body
        sleep Duration
```

### Subtlelty #4: Closures

Like many functional languages, XL ensures that the value of
variables is preserved for the evaluation of a given body. Consider
for example:

```xl
adder X:integer is (Y is Y + X)
add3 := adder 3
```

In that case, `adder 3` will bind `X` to value `3`, but then the
returned value outlives the scope where `X` was declared. However, `X`
is referred to in the code. So the returned value is a *closure* which
integrates the binding `X is 3`.


## Compiler status
Work items for the XL compiler (will be turned into GitHub issues)

### Language definition
- [X] Language definition documentation [published](https://c3d.github.com/xl)
- [X] Quickly-scanned To-Do list (this section)
- [ ] Finish [chapter on compilation](https://c3d.github.io/xl/#compiling-xl)
- [ ] Finish [chapter on programming paradigms](https://c3d.github.io/xl/#programming-paradigms)
- [ ] Description of the [module system](https://c3d.github.io/xl/#modules)
- [ ] Description of the [standard library](https://c3d.github.io/xl/#standard-library)
- [ ] Decide whether it's `import`, `use` or both (`import` doing the
      importing  and `use` bringing the referenced expression in scope).

### Recent language changes
- [X] Switch from `->` to `is` as the definition operator
- [ ] Switch type definition from `type Pattern` to `matching Pattern`
      ([issue #5](https://github.com/c3d/xl/issues/5))
- [ ] Implement syntactic sugar (can it be lib only?), e.g. `type X is Y`
      and `module X with Y` or `to Copy(...) is blah`
      ([issue #6](https://github.com/c3d/xl/issues/6))
- [ ] Support for nested functions and proper nested scopes.
      See [issue #8](https://github.com/c3d/xl/issues/8).
- [ ] Scope injection and scoping, i.e. meaning of `scope.Foo` and
      `scope Foo` in the language. Deal with `(scope) foo`, etc.
      See [scoping](https://c3d.github.io/xl/#scoping) and
      [issue #9](https://github.com/c3d/xl/issues/9).
- [ ] Safe implementation of `for` loop using scope injection
      (see [name parameters](https://c3d.github.io/xl/#name-parameters)).
      This is [issue #7](https://github.com/c3d/xl/issues/7).
- [ ] Implement metabox (`[[true]]`). This is [issue #10](https://github.com/c3d/xl/issues/10).
- [ ] Write the interface and implementation of the `type` type
      ([issue #11](https://github.com/c3d/xl/issues/11)).
- [ ] Implement union types (`T1 or T2`), as well as `and` and `not`.
      See [issue #12](https://github.com/c3d/xl/issues/12)
- [ ] Revisit dynamic dispatch based on types.
      See [issue #13](https://github.com/c3d/xl/issues/13)
- [ ] Implement type inheritance checks (`Derived like Base`).
      See [issue #14](https://github.com/c3d/xl/issues/14)
- [ ] Generate `lifetime` values.
      See [issue #15](https://github.com/c3d/xl/issues/15)
- [ ] Implement `own` and `ref` types.
      See [issue #16](https://github.com/c3d/xl/issues/16)
- [ ] Implement `in`, `out`, `inout` types.
      See [issue #17](https://github.com/c3d/xl/issues/17)
- [ ] Implement creation and destruction.
      See [issue #18](https://github.com/c3d/xl/issues/18)

### Symbol table
- [x] Symbol table as an XL parse tree
- [ ] Symbol table storing actual definitions, using `is` operator.
      Currently, the symbol table uses a "nonsensical" prefix.
      See [issue #19](https://github.com/c3d/xl/issues/19)

### Scanner
- [x] Scanner
- [ ] Scan version numbers correctly, e.g. `1.20.5` not parsed as `(1.2).5`
- [ ] Better Unicode classification of letters versus
      punctuation. Currently, only ASCII punctuation is recognized as such.
- [ ] Option to transparently convert `->` to `is` (for Tao3D importing)
- [ ] Scanner-time processing of `syntax` statements from imported files

### Parser
- [x] Parser
- [ ] "Binary" terminal node holding arbitrary binary data
- [ ] "Literal" terminal node holding arbitrary text parsed from regexp
- [ ] Add more error checks for failure cases, better error reporting

### Renderer
- [x] Renderer (`-show` option)
- [x] Style renderer with `-style` option and `.stylesheet` files
- [x] Preserving comments in renderer output
- [ ] Rendering binary or regexp nodes

### Runtime
- [x] Foreign function interface (FFI) with easy macro (see `native.h`
- [ ] Tag all useful runtime functions with FFI
- [ ] Find a strategy for migrating Tao3D 1500 runtime calls
- [ ] Cleanup the runtime from obsolete / useless functions

### Interpreter (-O0)
- [x] Simple interpreter (`-O0` or `-i`)
- [ ] Explicit FFI for interpreter (`extern` C syntax)
- [ ] Add "sandboxed" mode (on by default for interpreter) that
      disables the above
- [ ] Connect `native.h` FFI to interpreter
- [ ] Implement modules in interpreter
- [ ] Implement `error` and `compile_error` evaluation rules in interpreter
- [ ] Update type system to recognize `matching` prefix instead of `type`

### Bytecode interpreter (-O1) BROKEN, LIKELY NOT TO BE REPAIRED
- [X] Bytecode interpreter (broken, do not use)
- [ ] Fix bytecode interpreter or get rid of its remnants (`-O2`)
- [ ] Redesign bytecode interpreter as emitting LLVM byte code?

### LLVM "fast compiler" (-O2)
Simplistic compiler that does only run-time type analysis
- [X] Fast compiler functionally similar to what was used in Tao3D
- [ ] Find strategy to re-connect it to Tao3D

### LLVM optimized compiler (-O3)
- [x] LLVM-based compiler
- [x] LLVM-CRAP (adapting to multiple versions of LLVM)
- [x] `native.h` for building FFI
- [x] Option to emit LLVM bitcode (`-B` or `-emit_ir`)
- [ ] Option to pass bitcode to LLVM bitcode compiler
      (Automatically do something like `xl -B ... | llc -filetype=asm`)
- [ ] Option to directly emit disassembly
- [ ] Pass LLVM options to LLVM (`-llvm-foo`)
- [ ] Replace algo-W type analysis with forward-only type analysis.
- [ ] Implement
      [auto-boxing](https://c3d.github.io/xl/#machine-representation)
      for
      [parameter scopes](https://c3d.github.io/xl/#pattern-matching-scope)
- [ ] Self-compiling compiler
- [ ] Rebuild Tao3D on top of "new" XL

### Build system
- [X] Portable auto-configuring makefile (Using `make-it-quick`)
- [X] Targets for `opt`, `debug`, `release` and `check`
- [X] Target for `install`
- [ ] Fix issue with too much stuff being rebuilt for `install`

### CI / Testing
- [X] Basic QE framework / testing script (`alltest` script, `make check`)
- [X] Fix breakage in `make check` with macOS Catalina DYLD_LIBRARY_PATH
- [X] Basic CI in GitLab
- [X] Repair breakage in GitLab CI
- [X] Add tests for variants of LLVM in GitLab CI
- [X] Basic CI in GitHub

### Modules
- [ ] Reimplement module system

### Standard library
- [x] Standard library
- [x] Overall structure (WIP)
- [ ] Types
- [ ] Arrays
- [ ] STL-style containers
- [ ] Algorithms
- [ ] I/Os
- [ ] Threading / synchronization
- [ ] Math and numerics
- [ ] Standard math functions
- [X] Complex module
- [ ] Vectors
- [ ] Matrices
- [ ] Text processing
- [ ] Time
- [ ] Graphics (Tao3D based?)

## Compiler overview

The interpreter / compiler recently went through a rather heavy
merge of several incompatible branches. As a result, it inherited
the best of the various branches, but is overall quite broken.
There are actually several, somewhat incompatible versions of
the language, some of which reside in different binaries, some
in the primary binary.

The primary binary resides in the `src` directory. It is written
in C++, and there is currently no real plan to self-compile it,
although there are plans to use it as a basis for a self-compiling
compiler bootstrap someday.

That primary binary contains a single scanner and parser for the
XL language, but three different ways to evaluate it, which are
instances of the C++ `Evaluator` class. These three ways to evaluate
XL are selected using the `-O` option.

* `-O0` or `-i` selects an interpreter. This interpreter is
essentially similar to what used to be the ELFE implementation
of the language, i.e. a very small implementation that performs
evaluation using parse tree rewrites. It sort of works, passes
most tests with `make check`, and is overall sane, if a bit slow
(similar to `bash` in my testing). It can be used for example
as an extension language for your application, and does not draw
much in terms of dependencies. You would add your own vocabulary
using simple-to-write "modules". See the `Makefile` for examples.
That part is the only one I can advertise as possibly useful.
In particular, it correctly runs the examples in the `demo` directory,
which are the older ELFE demos, i.e. distributed programming
from a single source code.

* `-O1` selects the `FastCompiler`, which is basically an
adaptation of the compiler used in the Tao3D program, with
the intent to allow the `master` branch of XL to be able to
support Tao3D again without major incompatibilities. It generates
machine code using LLVM, but the generated code is relatively
inefficient because it manipulates the parse tree. For example,
an integer value is always represented by an `Integer` pointer,
so there is always an indirection to access it. Also, while
forward-porting that compiler to a version of the compiler that
had dropped it, I broke a number of things. So under repair,
and not currently able to support Tao3D yet.

* `-O2` and above select the `Compiler` class, which is an
ongoing attempt at implementing XL the way I always wanted to,
using type inference to generate efficient code. Presently, the
type inference is so badly broken that it's likely to reject
a number of very valid programs, including the basic factorial
example. I have hope, though. At some point, that implementation
was able to compete with C on relatively simple programs, but
only with a lot of type annotations. I'm trying to achieve the
same result without the type annotations. We're getting there.
Like `-O1`, `-O2` output uses LLVM to generate machine code, but
that time, it's good machine code.

If you think 3 implementations is bad, wait. There is more.
There is a `Bytecode` class that is yet another evaluator
that attempted to generate a bytecode so as to accelerate
interpreted evaluation, without having to bring in LLVM and
all the risks associated with dynamic code generation (e.g. if
you use XL as an extension language). Unfortunately, that
bytecode experiment went nowhere. It's slow, ridden with bugs,
and has severely damaged other parts of the compiler. I can't
wait to expurge it from the compiler.

So now that's it, right? Well... No.

You see, the current XL started life as a "runtime" language
for the "real" XL. The original XL looked more like Ada,
and had very different type semantics.
See [HISTORY](docs/HANDBOOK.adoc#history)
for all the gory details. Suffice it to say here that this
compiler resides in the `xl2` directory (because, yes, it was
already version 2 of the language). One reason for me to keep
it is that it's the only version of the compiler that ever
came close to self-compiling it. So I keep it around to remind
myself of various neat tricks that XL made possible, like the
`translate` instruction.

Now, you are really done, right? Well... There's one more.

See, I really want the compiler to self-compile. So in order
to prepare for that, there is a `native` directory where I
store tidbits of what the future compiler and library would
look like. Except that this is really an exploratory scratchpad,
so the various modules are not even consistent with one another...
But ultimately, if everything goes according to plan, the C++
compiler should be able to compile `native` in order to generate
a compiler that would compile itself.
