/*
 * blt.h --
 *
 *      Declarations of stuff in teh blt-files.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

extern void
Blt_SetWindowInstanceData(Tk_Window tkwin, ClientData instanceData);

extern Window
Blt_GetParent(Display *display, Window window);

extern void
Blt_MakeTransparentWindowExist(Tk_Window tkwin, Window parent);
