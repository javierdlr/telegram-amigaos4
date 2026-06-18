/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include "tg_app.h"

#if defined(__MORPHOS__) || defined(__MORPHOS)
#include <stdio.h>   /* fflush */
#include <unistd.h>  /* _exit */
#endif

int main(int argc, char **argv)
{
    int rc;

    rc = tg_app_run(argc, argv);
#if defined(__MORPHOS__) || defined(__MORPHOS)
    /* MorphOS is built -noixemul and does not own SocketBase, so the C-runtime
       auto-CloseLibrary's bsdsocket.library in its epilogue AFTER main returns.
       On a GUI exit with a just-closed MTProto connection, that deferred library
       teardown HARD-FREEZES the whole machine (the socket is already closed by
       tg_gui_session_close; the stack still considers it half-open on the slow
       link). Skip the runtime epilogue entirely with _exit -- flush first, since
       _exit runs no atexit/stdio flush. The only thing not cleanly released is
       the bsdsocket handle, which the OS reaps on task death: a benign leak vs a
       frozen box. Other lanes (OS3/OS4/AROS) own SocketBase and return normally. */
    fflush(0);
    _exit(rc);
#endif
    return rc;
}
