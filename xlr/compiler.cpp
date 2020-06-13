// ****************************************************************************
//  compiler.cpp                                                    XLR project
// ****************************************************************************
//
//   File Description:
//
//    Just-in-time (JIT) compilation of XL trees
//
//
//
//
//
//
//
//
// ****************************************************************************
// This document is released under the GNU General Public License, with the
// following clarification and exception.
//
// Linking this library statically or dynamically with other modules is making
// a combined work based on this library. Thus, the terms and conditions of the
// GNU General Public License cover the whole combination.
//
// As a special exception, the copyright holders of this library give you
// permission to link this library with independent modules to produce an
// executable, regardless of the license terms of these independent modules,
// and to copy and distribute the resulting executable under terms of your
// choice, provided that you also meet, for each linked independent module,
// the terms and conditions of the license of that module. An independent
// module is a module which is not derived from or based on this library.
// If you modify this library, you may extend this exception to your version
// of the library, but you are not obliged to do so. If you do not wish to
// do so, delete this exception statement from your version.
//
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "compiler.h"
#include "compiler-gc.h"
#include "compiler-llvm.h"
#include "unit.h"
#include "args.h"
#include "options.h"
#include "context.h"
#include "renderer.h"
#include "runtime.h"
#include "errors.h"
#include "types.h"
#include "flight_recorder.h"
#include "llvm-crap.h"

#include <iostream>
#include <sstream>
#include <cstdarg>

XL_BEGIN


// ============================================================================
//
//    Compiler - Global information about the LLVM compiler
//
// ============================================================================
//
// The Compiler class is where we store all the global information that
// persists during the lifetime of the program: LLVM data structures,
// LLVM definitions for frequently used types, XL runtime functions, ...
//

using namespace llvm;

static void* unresolved_external(const std::string& name)
// ----------------------------------------------------------------------------
//   Resolve external names that dyld doesn't know about
// ----------------------------------------------------------------------------
// This is really just to print a fancy error message
{
#if LLVM_VERSION >= 370
    if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
        return (void *) SymAddr;
#endif // 370

    std::cout.flush();
    std::cerr << "Unable to resolve external: " << name << std::endl;
    assert(0);
    return 0;
}


Compiler::Compiler(kstring moduleName, int argc, char **argv)
// ----------------------------------------------------------------------------
//   Initialize the various instances we may need
// ----------------------------------------------------------------------------
    : llvm(),
      booleanTy(NULL),
      integerTy(NULL), integer8Ty(NULL), integer16Ty(NULL), integer32Ty(NULL),
      realTy(NULL), real32Ty(NULL),
      characterTy(NULL), charPtrTy(NULL), textTy(NULL),
      treeTy(NULL), treePtrTy(NULL), treePtrPtrTy(NULL),
      integerTreeTy(NULL), integerTreePtrTy(NULL),
      realTreeTy(NULL), realTreePtrTy(NULL),
      textTreeTy(NULL), textTreePtrTy(NULL),
      nameTreeTy(NULL), nameTreePtrTy(NULL),
      blockTreeTy(NULL), blockTreePtrTy(NULL),
      prefixTreeTy(NULL), prefixTreePtrTy(NULL),
      postfixTreeTy(NULL), postfixTreePtrTy(NULL),
      infixTreeTy(NULL), infixTreePtrTy(NULL),
      nativeTy(NULL), nativeFnTy(NULL),
      evalTy(NULL), evalFnTy(NULL),
      infoPtrTy(NULL), contextPtrTy(NULL),
      strcmp_fn(NULL),
      xl_evaluate(NULL),
      xl_same_text(NULL), xl_same_shape(NULL),
      xl_infix_match_check(NULL), xl_type_check(NULL),
      xl_form_error(NULL), xl_stack_overflow(NULL),
      xl_new_integer(NULL), xl_new_real(NULL), xl_new_character(NULL),
      xl_new_text(NULL), xl_new_ctext(NULL), xl_new_xtext(NULL),
      xl_new_block(NULL),
      xl_new_prefix(NULL), xl_new_postfix(NULL), xl_new_infix(NULL),
      xl_fill_block(NULL),
      xl_fill_prefix(NULL), xl_fill_postfix(NULL), xl_fill_infix(NULL),
      xl_integer2real(NULL),
      xl_array_index(NULL), xl_new_closure(NULL),
      xl_recursion_count_ptr(NULL)
{
    std::vector<char *> llvmArgv;
    llvmArgv.push_back(argv[0]);
    for (int arg = 1; arg < argc; arg++)
        if (strncmp(argv[arg], "-llvm", 5) == 0)
            llvmArgv.push_back(argv[arg] + 5);

    llvm::cl::ParseCommandLineOptions(llvmArgv.size(), &llvmArgv[0]);

#ifdef CONFIG_MINGW
    llvm::sys::PrintStackTraceOnErrorSignal();
#endif
    RECORD(COMPILER, "Creating compiler");

    // Register a listener with the garbage collector
    CompilerGarbageCollectionListener *cgcl =
        new CompilerGarbageCollectionListener(this);
    Allocator<Tree>     ::Singleton()->AddListener(cgcl);
    Allocator<Integer>  ::Singleton()->AddListener(cgcl);
    Allocator<Real>     ::Singleton()->AddListener(cgcl);
    Allocator<Text>     ::Singleton()->AddListener(cgcl);
    Allocator<Name>     ::Singleton()->AddListener(cgcl);
    Allocator<Infix>    ::Singleton()->AddListener(cgcl);
    Allocator<Prefix>   ::Singleton()->AddListener(cgcl);
    Allocator<Postfix>  ::Singleton()->AddListener(cgcl);
    Allocator<Block>    ::Singleton()->AddListener(cgcl);

    llvm.SetResolver(unresolved_external);

    // Get the basic types
    booleanTy = Type::getInt1Ty(llvm);
    integerTy = llvm::IntegerType::get(llvm, 64);
    integer8Ty = llvm::IntegerType::get(llvm, 8);
    integer16Ty = llvm::IntegerType::get(llvm, 16);
    integer32Ty = llvm::IntegerType::get(llvm, 32);
    characterTy = LLVM_INTTYPE(char);
    realTy = Type::getDoubleTy(llvm);
    real32Ty = Type::getFloatTy(llvm);
    charPtrTy = PointerType::get(LLVM_INTTYPE(char), 0);
    // Create the 'text' type, assume it contains a single char *
    llvm_types textElements;
    textElements.push_back(charPtrTy);             // _M_p in gcc's impl
    textTy = StructType::get(llvm, textElements); // text

    // Create the Info and Symbol pointer types
    llvm_struct structInfoTy = llvm.OpaqueType();    // struct Info
    infoPtrTy = PointerType::get(structInfoTy, 0);   // Info *
    llvm_struct structCtxTy = llvm.OpaqueType();     // struct Context
    contextPtrTy = PointerType::get(structCtxTy, 0); // Context *
    llvm_struct structSymTy = llvm.OpaqueType();     // struct Symbols
    symbolsPtrTy = PointerType::get(structSymTy, 0); // Symbols *

    // Create the Tree and Tree pointer types
    llvm_struct structTreeTy = llvm.OpaqueType();    // struct Tree
    treePtrTy = PointerType::get(structTreeTy, 0);   // Tree *
    treePtrPtrTy = PointerType::get(treePtrTy, 0);   // Tree **

    // Create the native_fn type
    llvm_types nativeParms;
    nativeParms.push_back(contextPtrTy);
    nativeParms.push_back(treePtrTy);
    nativeTy = FunctionType::get(treePtrTy, nativeParms, false);
    nativeFnTy = PointerType::get(nativeTy, 0);

    // Create the eval_fn type
    llvm_types evalParms;
    evalParms.push_back(contextPtrTy);
    evalParms.push_back(treePtrTy);
    evalTy = FunctionType::get(treePtrTy, evalParms, false);
    evalFnTy = PointerType::get(evalTy, 0);

    // Verify that there wasn't a change in the Tree type invalidating us
    struct LocalTree
    {
        LocalTree (const Tree &o): tag(o.tag), info(o.info) {}
        ulong     tag;
        XL::Info* info;
        eval_fn   code;
        Symbols*  symbols;
    };
    // If this assert fails, you changed struct tree and need to modify here
    XL_CASSERT(sizeof(LocalTree) == sizeof(Tree));

    // Create the Tree type
    llvm_types treeElements;
    treeElements.push_back(LLVM_INTTYPE(ulong));           // tag
    treeElements.push_back(infoPtrTy);                     // info
    treeElements.push_back(evalFnTy);                      // code
    treeElements.push_back(symbolsPtrTy);                  // symbols
    treeTy = llvm.Struct(structTreeTy, treeElements);

    // Create the Integer type
    llvm_types integerElements = treeElements;
    integerElements.push_back(LLVM_INTTYPE(longlong));      // value
    integerTreeTy = StructType::get(llvm, integerElements); // struct Integer
    integerTreePtrTy = PointerType::get(integerTreeTy,0);   // Integer *

    // Create the Real type
    llvm_types realElements = treeElements;
    realElements.push_back(Type::getDoubleTy(llvm));     // value
    realTreeTy = StructType::get(llvm, realElements);    // struct Real{}
    realTreePtrTy = PointerType::get(realTreeTy, 0);     // Real *

    // Create the Text type
    llvm_types textTreeElements = treeElements;
    textTreeElements.push_back(textTy);                  // value
    textTreeElements.push_back(textTy);                  // opening
    textTreeElements.push_back(textTy);                  // closing
    textTreeTy = StructType::get(llvm, textTreeElements);// struct Text
    textTreePtrTy = PointerType::get(textTreeTy, 0);     // Text *

    // Create the Name type
    llvm_types nameElements = treeElements;
    nameElements.push_back(textTy);
    nameTreeTy = StructType::get(llvm, nameElements);    // struct Name{}
    nameTreePtrTy = PointerType::get(nameTreeTy, 0);     // Name *

    // Create the Block type
    llvm_types blockElements = treeElements;
    blockElements.push_back(treePtrTy);                  // Tree *
    blockElements.push_back(textTy);                     // opening
    blockElements.push_back(textTy);                     // closing
    blockTreeTy = StructType::get(llvm, blockElements);  // struct Block
    blockTreePtrTy = PointerType::get(blockTreeTy, 0);   // Block *

    // Create the Prefix type
    llvm_types prefixElements = treeElements;
    prefixElements.push_back(treePtrTy);                   // Tree *
    prefixElements.push_back(treePtrTy);                   // Tree *
    prefixTreeTy = StructType::get(llvm, prefixElements);  // struct Prefix
    prefixTreePtrTy = PointerType::get(prefixTreeTy, 0);   // Prefix *

    // Create the Postfix type
    llvm_types postfixElements = prefixElements;
    postfixTreeTy = StructType::get(llvm, postfixElements);  // Postfix
    postfixTreePtrTy = PointerType::get(postfixTreeTy, 0);   // Postfix *

    // Create the Infix type
    llvm_types infixElements = prefixElements;
    infixElements.push_back(textTy);                        // name
    infixTreeTy = StructType::get(llvm, infixElements);     // Infix
    infixTreePtrTy = PointerType::get(infixTreeTy, 0);      // Infix *

    // Record the type names
    llvm.SetName(booleanTy, "boolean");
    llvm.SetName(integerTy, "integer");
    llvm.SetName(characterTy, "character");
    llvm.SetName(realTy, "real");
    llvm.SetName(charPtrTy, "text");

    llvm.SetName(treeTy, "Tree");
    llvm.SetName(integerTreeTy, "Integer");
    llvm.SetName(realTreeTy, "Real");
    llvm.SetName(textTreeTy, "Text");
    llvm.SetName(blockTreeTy, "Block");
    llvm.SetName(nameTreeTy, "Name");
    llvm.SetName(prefixTreeTy, "Prefix");
    llvm.SetName(postfixTreeTy, "Postfix");
    llvm.SetName(infixTreeTy, "Infix");
    llvm.SetName(evalTy, "eval_fn");
    llvm.SetName(nativeTy, "native_fn");
    llvm.SetName(structInfoTy, "Info");
    llvm.SetName(structCtxTy, "Context");
    llvm.SetName(structSymTy, "Symbols");

    // Create one module for all extern function declarations
    llvm.CreateModule(text(moduleName) + ".externs");

    // Create references to the various runtime functions
#define FN(x) #x, (void *) XL::x
    strcmp_fn = ExternFunction("strcmp", (void *) strcmp,
                               LLVM_INTTYPE(int), 2, charPtrTy, charPtrTy);
    xl_evaluate = ExternFunction(FN(xl_evaluate),
                                 treePtrTy, 2, contextPtrTy, treePtrTy);
    xl_same_text = ExternFunction(FN(xl_same_text),
                                  booleanTy, 2, treePtrTy, charPtrTy);
    xl_same_shape = ExternFunction(FN(xl_same_shape),
                                   booleanTy, 2, treePtrTy, treePtrTy);
    xl_infix_match_check = ExternFunction(FN(xl_infix_match_check),
                                          treePtrTy, 3,
                                          contextPtrTy, treePtrTy, charPtrTy);
    xl_type_check = ExternFunction(FN(xl_type_check), treePtrTy,
                                   3, contextPtrTy, treePtrTy, treePtrTy);
    xl_form_error = ExternFunction(FN(xl_form_error),
                                   treePtrTy, 2, contextPtrTy, treePtrTy);
    xl_stack_overflow = ExternFunction(FN(xl_stack_overflow),
                                       treePtrTy, 1, treePtrTy);
    xl_new_integer = ExternFunction(FN(xl_new_integer),
                                    integerTreePtrTy, 1, integerTy);
    xl_new_real = ExternFunction(FN(xl_new_real),
                                 realTreePtrTy, 1, realTy);
    xl_new_character = ExternFunction(FN(xl_new_character),
                                      textTreePtrTy, 1, characterTy);
    xl_new_text = ExternFunction(FN(xl_new_text), textTreePtrTy, 1, textTy);
    xl_new_ctext = ExternFunction(FN(xl_new_ctext), textTreePtrTy, 1,charPtrTy);
    xl_new_xtext = ExternFunction(FN(xl_new_xtext), textTreePtrTy, 4,
                                  charPtrTy, integerTy, charPtrTy, charPtrTy);
    xl_new_block = ExternFunction(FN(xl_new_block), blockTreePtrTy, 2,
                                  blockTreePtrTy,treePtrTy);
    xl_new_prefix = ExternFunction(FN(xl_new_prefix), prefixTreePtrTy, 3,
                                   prefixTreePtrTy, treePtrTy, treePtrTy);
    xl_new_postfix = ExternFunction(FN(xl_new_postfix), postfixTreePtrTy, 3,
                                    postfixTreePtrTy, treePtrTy, treePtrTy);
    xl_new_infix = ExternFunction(FN(xl_new_infix), infixTreePtrTy, 3,
                                  infixTreePtrTy,treePtrTy,treePtrTy);
    xl_fill_block = ExternFunction(FN(xl_fill_block), blockTreePtrTy, 2,
                                  blockTreePtrTy,treePtrTy);
    xl_fill_prefix = ExternFunction(FN(xl_fill_prefix), prefixTreePtrTy, 3,
                                   prefixTreePtrTy, treePtrTy, treePtrTy);
    xl_fill_postfix = ExternFunction(FN(xl_fill_postfix), postfixTreePtrTy, 3,
                                    postfixTreePtrTy, treePtrTy, treePtrTy);
    xl_fill_infix = ExternFunction(FN(xl_fill_infix), infixTreePtrTy, 3,
                                  infixTreePtrTy,treePtrTy,treePtrTy);
    xl_integer2real = ExternFunction(FN(xl_integer2real), treePtrTy, 1,
                                     treePtrTy);
    xl_array_index = ExternFunction(FN(xl_array_index),
                                    treePtrTy, 3,
                                    contextPtrTy, treePtrTy, treePtrTy);
    xl_new_closure = ExternFunction(FN(xl_new_closure),
                                    treePtrTy, -3,
                                    evalFnTy, treePtrTy, LLVM_INTTYPE(uint));

    // Create a global value used to count recursions
    llvm::PointerType *uintPtrTy = PointerType::get(LLVM_INTTYPE(uint), 0);
    APInt addr(64, (uint64_t) &xl_recursion_count);
    xl_recursion_count_ptr = Constant::getIntegerValue(uintPtrTy, addr);

    // Initialize the llvm_entries table
    for (CompilerLLVMTableEntry *le = CompilerLLVMTable; le->name; le++)
        llvm_primitives[le->name] = le;

    // Create a new module for the generated code
    llvm.CreateModule(moduleName);
}


Compiler::~Compiler()
// ----------------------------------------------------------------------------
//    Destructor deletes the various things we had created
// ----------------------------------------------------------------------------
{}


void Compiler::Dump()
// ----------------------------------------------------------------------------
//   Debug dump of the whole compiler program at exit
// ----------------------------------------------------------------------------
{
    IFTRACE(llvmdump)
        llvm.Dump();
    IFTRACE(llvmstats)
        llvm::PrintStatistics(llvm::errs());
}


program_fn Compiler::CompileProgram(Context *context, Tree *program)
// ----------------------------------------------------------------------------
//   Compile a whole XL program
// ----------------------------------------------------------------------------
//   This is the entry point used to compile a top-level XL program.
//   It will process all the declarations in the program and then compile
//   the rest of the code as a function taking no arguments.
{
    RECORD(COMPILER, "CompileProgram", "program", (intptr_t) program);

    if (!program)
        return NULL;

    Context_p topContext = new Context(context, context);
    CompiledUnit topUnit(this, topContext);

    if (!topUnit.TypeCheck(program))
        return NULL;
    if (!topUnit.TopLevelFunction())
        return NULL;
    llvm_value returned = topUnit.CompileTopLevel(program);
    if (!returned)
        return NULL;
    if (!topUnit.Return(returned))
        return NULL;
    return (program_fn) topUnit.Finalize(true);
}


void Compiler::Setup(Options &options)
// ----------------------------------------------------------------------------
//   Setup the compiler after we have parsed the options
// ----------------------------------------------------------------------------
{
    uint optLevel = options.optimize_level;
    RECORD(COMPILER, "Compiler setup", "opt", optLevel);

    LLVMLinkInMCJIT();
    llvm.SetOptimizationLevel(optLevel);
}


void Compiler::Reset()
// ----------------------------------------------------------------------------
//    Clear the contents of a compiler
// ----------------------------------------------------------------------------
{
}


CompilerInfo *Compiler::Info(Tree *tree, bool create)
// ----------------------------------------------------------------------------
//   Find or create the compiler-related info for a given tree
// ----------------------------------------------------------------------------
{
    CompilerInfo *result = tree->GetInfo<CompilerInfo>();
    if (!result && create)
    {
        result = new CompilerInfo(tree);
        tree->SetInfo<CompilerInfo>(result);
    }
    return result;
}


llvm::Function * Compiler::TreeFunction(Tree *tree)
// ----------------------------------------------------------------------------
//   Return the function associated to the tree
// ----------------------------------------------------------------------------
{
    CompilerInfo *info = Info(tree);
    return info ? info->function : NULL;
}


void Compiler::SetTreeFunction(Tree *tree, llvm::Function *function)
// ----------------------------------------------------------------------------
//   Associate a function to the given tree
// ----------------------------------------------------------------------------
{
    CompilerInfo *info = Info(tree, true);
    info->function = function;
}


llvm::Function * Compiler::TreeClosure(Tree *tree)
// ----------------------------------------------------------------------------
//   Return the closure associated to the tree
// ----------------------------------------------------------------------------
{
    CompilerInfo *info = Info(tree);
    return info ? info->closure : NULL;
}


void Compiler::SetTreeClosure(Tree *tree, llvm::Function *closure)
// ----------------------------------------------------------------------------
//   Associate a closure to the given tree
// ----------------------------------------------------------------------------
{
    CompilerInfo *info = Info(tree, true);
    info->closure = closure;
}


llvm::Function *Compiler::EnterBuiltin(text name,
                                       Tree *to,
                                       TreeList parms,
                                       eval_fn code)
// ----------------------------------------------------------------------------
//   Declare a built-in function
// ----------------------------------------------------------------------------
//   The input is not technically an eval_fn, but has as many parameters as
//   there are variables in the form
{
    RECORD(COMPILER, "Enter Builtin",
           "parms", parms.size(),
           "src", (intptr_t) to,
           "code", (intptr_t) code);

    IFTRACE(llvm)
        std::cerr << "EnterBuiltin " << name
                  << " C" << (void *) code << " T" << (void *) to;

    llvm::Function *result = builtins[name];
    if (result)
    {
        IFTRACE(llvm)
            std::cerr << " existing F " << result
                      << " replaces F" << TreeFunction(to) << "\n";
        SetTreeFunction(to, result);
        SetTreeClosure(to, result);
    }
    else
    {
        // Create the LLVM function
        llvm_types parmTypes;
        parmTypes.push_back(contextPtrTy); // First arg is context pointer
        parmTypes.push_back(treePtrTy);    // Second arg is self
        for (TreeList::iterator p = parms.begin(); p != parms.end(); p++)
            parmTypes.push_back(treePtrTy);
        FunctionType *fnTy = FunctionType::get(treePtrTy, parmTypes, false);
        result = llvm.CreateExternFunction(fnTy, name);

        // Record the runtime symbol address
        sys::DynamicLibrary::AddSymbol(name, (void*) code);

        IFTRACE(llvm)
            std::cerr << " new F " << result
                      << "replaces F" << TreeFunction(to) << "\n";

        // Associate the function with the tree form
        SetTreeFunction(to, result);
        SetTreeClosure(to, result);
        builtins[name] = result;
    }

    return result;
}


adapter_fn Compiler::ArrayToArgsAdapter(uint numargs)
// ----------------------------------------------------------------------------
//   Generate code to call a function with N arguments
// ----------------------------------------------------------------------------
//   The generated code serves as an adapter between code that has
//   tree arguments in a C array and code that expects them as an arg-list.
//   For example, it allows you to call foo(Tree *src, Tree *a1, Tree *a2)
//   by calling generated_adapter(foo, Tree *src, Tree *args[2])
{
    IFTRACE(llvm)
        std::cerr << "EnterArrayToArgsAdapater " << numargs;

    // Check if we already computed it
    adapter_fn result = array_to_args_adapters[numargs];
    if (result)
    {
        IFTRACE(llvm)
            std::cerr << " existing C" << (void *) result << "\n";
        return result;
    }

    // We need a new independent module for this adapter with the MCJIT
    LLVMCrap::JITModule module(llvm, "xl.array2arg.adapter");

    // Generate the function type:
    // Tree *generated(Context *, native_fn, Tree *, Tree **)
    llvm_types parms;
    parms.push_back(nativeFnTy);
    parms.push_back(contextPtrTy);
    parms.push_back(treePtrTy);
    parms.push_back(treePtrPtrTy);
    FunctionType *fnType = FunctionType::get(treePtrTy, parms, false);
    llvm::Function *adapter = llvm.CreateFunction(fnType, "xl.adapter");

    // Generate the function type for the called function
    llvm_types called;
    called.push_back(contextPtrTy);
    called.push_back(treePtrTy);
    for (uint a = 0; a < numargs; a++)
        called.push_back(treePtrTy);
    FunctionType *calledType = FunctionType::get(treePtrTy, called, false);
    PointerType *calledPtrType = PointerType::get(calledType, 0);

    // Create the entry for the function we generate
    BasicBlock *entry = BasicBlock::Create(llvm, "adapt", adapter);
    IRBuilder<> code(entry);

    // Read the arguments from the function we are generating
    llvm::Function::arg_iterator inArgs = adapter->arg_begin();
    Value *fnToCall = &*inArgs++;
    Value *contextPtr = &*inArgs++;
    Value *sourceTree = &*inArgs++;
    Value *treeArray = &*inArgs++;

    // Cast the input function pointer to right type
    Value *fnTyped = code.CreateBitCast(fnToCall, calledPtrType, "xl.fnCast");

    // Add source as first argument to output arguments
    std::vector<Value *> outArgs;
    outArgs.push_back (contextPtr);
    outArgs.push_back (sourceTree);

    // Read other arguments from the input array
    for (uint a = 0; a < numargs; a++)
    {
        Value *elementPtr = code.CreateConstGEP1_32(treeArray, a);
        Value *fromArray = code.CreateLoad(elementPtr, "arg");
        outArgs.push_back(fromArray);
    }

    // Call the function
    Value *retVal = llvm.CreateCall(&code, fnTyped, outArgs);

    // Return the result
    code.CreateRet(retVal);

    if (XLTRACE(unoptimized_code) || XLTRACE(code))
    {
        errs() << "UNOPTIMIZED (ArrayToArgs):\n";
        adapter->print(errs());
    }

    // Enter the result in the map
    llvm.FinalizeFunction(adapter);
    result = (adapter_fn) llvm.FunctionPointer(adapter);
    array_to_args_adapters[numargs] = result;

    IFTRACE(llvm)
        std::cerr << " new C" << (void *) result << "\n";

    // And return it to the caller
    return result;
}


llvm::Function *Compiler::ExternFunction(kstring name, void *address,
                                         llvm_type retType, int parmCount, ...)
// ----------------------------------------------------------------------------
//   Return a Function for some given external symbol
// ----------------------------------------------------------------------------
{
    RECORD(COMPILER, "Extern Function",
           name, parmCount, "addr", (intptr_t) address);
    IFTRACE(llvm)
        std::cerr << "ExternFunction " << name
                  << " has " << parmCount << " parameters "
                  << " C" << address;

    va_list va;
    llvm_types parms;
    bool isVarArg = parmCount < 0;
    if (isVarArg)
        parmCount = -parmCount;

    va_start(va, parmCount);
    for (int i = 0; i < parmCount; i++)
    {
        Type *ty = va_arg(va, Type *);
        parms.push_back(ty);
    }
    va_end(va);
    FunctionType *fnType = FunctionType::get(retType, parms, isVarArg);
    llvm::Function *result = llvm.CreateExternFunction(fnType, name);
    sys::DynamicLibrary::AddSymbol(name, address);

    IFTRACE(llvm)
        std::cerr << " F" << result << "\n";

    return result;
}


Constant *Compiler::TreeConstant(Tree *constant)
// ----------------------------------------------------------------------------
//   Enter a constant (i.e. an Integer, Real or Text) into global map
// ----------------------------------------------------------------------------
{
    RECORD(COMPILER_DETAILS, "Tree Constant",
           "tree", (intptr_t) constant, "kind", constant->Kind());
    IFTRACE(llvm)
        std::cerr << "TreeConstant "
                  << "[" << constant << "]=" << (void *) constant << "\n";
    return llvm.CreateConstant(treePtrTy, constant);
}


llvm_value Compiler::TextConstant(llvm_builder code, text value)
// ----------------------------------------------------------------------------
//   Return a C-style string pointer for a string constant
// ----------------------------------------------------------------------------
{
    llvm_value textValue = llvm.TextConstant(code, value);
    return textValue;
}


eval_fn Compiler::MarkAsClosure(XL::Tree *closure, uint ntrees)
// ----------------------------------------------------------------------------
//    Create the closure wrapper for ntrees elements, associate to result
// ----------------------------------------------------------------------------
{
    (void) closure;
    (void) ntrees;
    return NULL;
}


void Compiler::MachineType(Tree *tree, llvm_type mtype)
// ----------------------------------------------------------------------------
//   Record a machine type association that spans multiple units
// ----------------------------------------------------------------------------
{
    machineTypes[tree] = mtype;
}


llvm_type Compiler::MachineType(Tree *tree)
// ----------------------------------------------------------------------------
//    Return the LLVM type associated to a given XL type name
// ----------------------------------------------------------------------------
{
    // Check the special cases, e.g. boxed structs associated to type names
    type_map::iterator found = machineTypes.find(tree);
    if (found != machineTypes.end())
        return (*found).second;

    // Check all "basic" types in basics.tbl
    if (tree == boolean_type || tree == xl_true || tree == xl_false)
        return booleanTy;
    if (tree == integer_type|| tree == integer64_type ||
        tree == unsigned_type || tree == unsigned64_type ||
        tree->Kind() == INTEGER)
        return integerTy;
    if (tree == real_type || tree == real64_type || tree->Kind() == REAL)
        return realTy;
    if (tree == character_type)
        return characterTy;
    if (tree == text_type)
        return charPtrTy;
    if (Text *text = tree->AsText())
    {
        if (text->opening == "'" && text->closing == "'")
            return characterTy;
        if (text->opening == "\"" && text->closing == "\"")
            return charPtrTy;
    }

    // Sized types
    if (tree == integer8_type || tree == unsigned8_type)
        return integer8Ty;
    if (tree == integer16_type || tree == unsigned16_type)
        return integer16Ty;
    if (tree == integer32_type || tree == unsigned32_type)
        return integer32Ty;
    if (tree == real32_type)
        return real32Ty;

    // Check special tree types in basics.tbl
    if (tree == symbol_type || tree == name_type || tree == operator_type)
        return nameTreePtrTy;
    if (tree == infix_type)
        return infixTreePtrTy;
    if (tree == prefix_type)
        return prefixTreePtrTy;
    if (tree == postfix_type)
        return postfixTreePtrTy;
    if (tree == block_type)
        return blockTreePtrTy;

    // Otherwise, it's a Tree *
    return treePtrTy;
}


llvm_type Compiler::TreeMachineType(Tree *tree)
// ----------------------------------------------------------------------------
//    Return the LLVM tree type associated to a given XL expression
// ----------------------------------------------------------------------------
{
    switch(tree->Kind())
    {
    case INTEGER:
        return integerTreePtrTy;
    case REAL:
        return realTreePtrTy;
    case TEXT:
        return textTreePtrTy;
    case NAME:
        return nameTreePtrTy;
    case INFIX:
        return infixTreePtrTy;
    case PREFIX:
        return prefixTreePtrTy;
    case POSTFIX:
        return postfixTreePtrTy;
    case BLOCK:
        return blockTreePtrTy;
    }
    assert(!"Invalid tree type");
    return treePtrTy;
}


llvm_function Compiler::UnboxFunction(Context_p ctx, llvm_type type, Tree *form)
// ----------------------------------------------------------------------------
//    Create a function transforming a boxed (structure) value into tree form
// ----------------------------------------------------------------------------
{
    // Check if we have a matching boxing function
    std::ostringstream out;
    out << "Unbox" << (void *) type << ";" << (void *) ctx;

    llvm::Function * &fn = FunctionFor(out.str());
    if (fn)
        return fn;

    // Get original form representing that data type
    llvm_type mtype = TreeMachineType(form);

    // Create a function taking a boxed type as an argument, returning a tree
    llvm_types signature;
    signature.push_back(type);
    FunctionType *ftype = FunctionType::get(mtype, signature, false);
    CompiledUnit unit(this, ctx);
    fn = unit.InitializeFunction(ftype, NULL, "xl.unbox", false, false);

    // Take the first input argument, which is the boxed value.
    llvm::Function::arg_iterator args = fn->arg_begin();
    llvm_value arg = &*args++;

    // Generate code to create the unboxed tree
    uint index = 0;
    llvm_value tree = unit.Unbox(arg, form, index);
    tree = unit.Autobox(tree, treePtrTy);
    unit.Return(tree);

    return fn;
}


llvm_value Compiler::Primitive(CompiledUnit &unit,
                               llvm_builder builder, text name,
                               uint arity, llvm_value *args)
// ----------------------------------------------------------------------------
//   Invoke an LLVM primitive, assuming it's found in the table
// ----------------------------------------------------------------------------
{
    // Find the entry in the primitives table
    llvm_entry_table::iterator found = llvm_primitives.find(name);
    if (found == llvm_primitives.end())
        return NULL;

    // If the entry doesn't have the expected arity, give up
    CompilerLLVMTableEntry *entry = (*found).second;
    if (entry->arity != arity)
        return NULL;

    // Invoke the entry
    llvm_value result = entry->function(unit, builder, args);
    return result;
}


bool Compiler::MarkAsClosureType(llvm_type type)
// ----------------------------------------------------------------------------
//   Record which types are used as closures
// ----------------------------------------------------------------------------
{
    assert(type->isPointerTy() && "CLosures should be pointers");
    if (IsClosureType(type))
        return false;
    closure_types.push_back(type);
    return true;
}


bool Compiler::IsClosureType(llvm_type type)
// ----------------------------------------------------------------------------
//   Return true if the type is a closure type
// ----------------------------------------------------------------------------
{
    if (type->isPointerTy())
    {
        llvm_types::iterator begin = closure_types.begin();
        llvm_types::iterator end = closure_types.end();
        for (llvm_types::iterator t = begin; t != end; t++)
            if (type == *t)
                return true;
    }
    return false;
}


text Compiler::FunctionKey(Rewrite *rw, llvm_values &args)
// ----------------------------------------------------------------------------
//    Return a unique function key corresponding to a given overload
// ----------------------------------------------------------------------------
{
    std::ostringstream out;
    out << (void *) rw;

    for (llvm_values::iterator a = args.begin(); a != args.end(); a++)
    {
        llvm_value value = *a;
        llvm_type type = value->getType();
        out << ';' << (void *) type;
    }

    return out.str();
}


text Compiler::ClosureKey(Tree *tree, Context *context)
// ----------------------------------------------------------------------------
//    Return a unique function key corresponding to a given closure
// ----------------------------------------------------------------------------
{
    std::ostringstream out;
    out << (void *) tree << "@" << (void *) context;
    return out.str();
}


bool Compiler::FreeResources(Tree *tree)
// ----------------------------------------------------------------------------
//   Free the LLVM resources associated to the tree, if any
// ----------------------------------------------------------------------------
//   In the first pass, we need to clear the body and machine code for all
//   functions. This is because if we have foo() calling bar() and bar()
//   calling foo(), we will get an LLVM assert deleting one while the
//   other's body still makes a reference.
{
    bool result = true;

    IFTRACE(llvm)
        std::cerr << "FreeResources T" << (void *) tree;

    CompilerInfo *info = Info(tree);
    if (!info)
    {
        IFTRACE(llvm)
            std::cerr << " has no info\n";
        return true;
    }

    // Avoid purging built-in functions (bug #991)
    if (info->IsBuiltin())
    {
        IFTRACE(llvm)
            std::cerr << " is a built-in, don't purge it\n";
    }
    else
    {
        // Drop function reference if any
        if (llvm::Function *f = info->function)
        {
            bool inUse = !f->use_empty();

            IFTRACE(llvm)
                std::cerr << " function F" << f
                          << (inUse ? " in use" : " unused");

            if (inUse)
            {
                // Defer deletion until later
                result = false;
            }
            else
            {
                // Not in use, we can delete it directly
                f->eraseFromParent();
                info->function = NULL;
                tree->code = NULL; // Bug #1011: Tree may remain live for global
            }
        }

        // Drop closure function reference if any
        if (llvm::Function *f = info->closure)
        {
            bool inUse = !f->use_empty();

            IFTRACE(llvm)
                std::cerr << " closure F" << f
                          << (inUse ? " in use" : " unused");

            if (inUse)
            {
                // Defer deletion until later
                result = false;
            }
            else
            {
                // Not in use, we can delete it directly
                f->eraseFromParent();
                info->closure = NULL;
            }
        }
    }

    IFTRACE(llvm)
        std::cerr << (result ? " Delete\n" : "Preserved\n");

    return result;
}

XL_END


// ============================================================================
//
//    Global variables
//
// ============================================================================

text llvm_crap_error_string;



// ============================================================================
//
//    Debug helpers
//
// ============================================================================

void debugm(XL::value_map &m)
// ----------------------------------------------------------------------------
//   Dump a value map from the debugger
// ----------------------------------------------------------------------------
{
    XL::value_map::iterator i;
    for (i = m.begin(); i != m.end(); i++)
        llvm::errs() << "map[" << (*i).first << "]=" << *(*i).second << '\n';
}


void debugv(llvm::Value *v)
// ----------------------------------------------------------------------------
//   Dump a value for the debugger
// ----------------------------------------------------------------------------
{
    llvm::errs() << *v << "\n";
}


void debugv(llvm::Type *t)
// ----------------------------------------------------------------------------
//   Dump a value for the debugger
// ----------------------------------------------------------------------------
{
    llvm::errs() << *t << "\n";
}
