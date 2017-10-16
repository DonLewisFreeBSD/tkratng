/*
 * blt.h --
 *
 *      Declarations of stuff in teh blt-files.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#if TK_MINOR_VERSION < 4 && TK_MAJOR_VERSION == 8
/*
 * These definitions are public in tk8.4
 */
typedef Window (Tk_ClassCreateProc) _ANSI_ARGS_((Tk_Window tkwin,
        Window parent, ClientData instanceData));
typedef void (Tk_ClassGeometryProc) _ANSI_ARGS_((ClientData instanceData));
typedef void (Tk_ClassModalProc) _ANSI_ARGS_((Tk_Window tkwin,
        XEvent *eventPtr));
typedef struct Tk_ClassProcs {
    Tk_ClassCreateProc *createProc;
    Tk_ClassGeometryProc *geometryProc;
    Tk_ClassModalProc *modalProc;
    unsigned int size; /* Not present in 8.3 but needed in 8.4 */
} Tk_ClassProcs;

#define Tk_SetClassProcs(a,b,c) TkSetClassProcs(a,b,c)

void TkSetClassProcs(Tk_Window tkwin, Tk_ClassProcs *procs,
		     ClientData instanceData);
#endif /* version < 8.4 */
