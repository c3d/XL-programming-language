// ****************************************************************************
//  xl.cpp                                                        XLR project
// ****************************************************************************
//
//   File Description:
//
//    Main entry point of the XL runtime and compiler
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
//  (C) 2010 Jerome Forissier <jerome@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "main.h"
#include "basics.h"
#include "compiler.h"
#include "flight_recorder.h"
#include <recorder/recorder.h>

#if CONFIG_USE_SBRK
#include <unistd.h>
#endif // CONFIG_USE_SBRK

RECORDER(main, 32, "Main entry point of the compiler");

int main(int argc, char **argv)
// ----------------------------------------------------------------------------
//   Parse the command line and run the compiler phases
// ----------------------------------------------------------------------------
{
    recorder_dump_on_common_signals(0, 0);
    record(main, "Starting %+s", argv[0]);
    for (int a = 1; a < argc; a++)
        record(main, "Argument %d is %+s", a, argv[a]);
    XL::FlightRecorder::Initialize();

#if CONFIG_USE_SBRK
    char *low_water = (char *) sbrk(0);
#endif

    using namespace XL;
    source_names noSpecificContext;
    Main sources(argc, argv);
    EnterBasics();
    sources.SetupCompiler();
    int rc = sources.LoadContextFiles(noSpecificContext);
    if (rc)
        return rc;
    rc = sources.LoadFiles();

    if (!rc && Options::options->doDiff)
        rc = sources.Diff();
    else if (!rc && !Options::options->parseOnly)
        rc = sources.Run(true);

    if (!rc && sources.HadErrors())
        rc = 1;

    sources.compiler->Dump();

#if CONFIG_USE_SBRK
    record(main, "Total memory usage %ldK\n",
           long ((char *) sbrk(1) - low_water) / 1024);
#endif

    return rc;
}