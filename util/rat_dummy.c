/* 
 * ratDummy.c --
 *
 *	Provides dummy routines so blt_busy can be linked into tclsh
 *
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <tk.h>
#include "blt.h"

#ifndef CONST84
#   define CONST84
#endif

#if (TCL_MAJOR_VERSION >= 8) && (TCL_MINOR_VERSION >= 5)
#   define CONST85 CONST
#else
#   define CONST85
#endif

void
Tk_UnmapWindow(Tk_Window tkwin) {}

int
Tk_ConfigureValue(Tcl_Interp * interp, Tk_Window tkwin,
	Tk_ConfigSpec * specs, char * widgRec, CONST84 char * argvName, int flags)
{
    return 0;
}

void
Tk_CreateEventHandler(Tk_Window token, unsigned long mask,
		      Tk_EventProc * proc, ClientData clientData) {}

Tk_Window
Tk_CreateWindow(Tcl_Interp * interp, Tk_Window parent, CONST84 char * name,
		CONST84 char * screenName)
{
    return 0;
}

Status
XQueryTree(Display *display, Window w, Window *root_return,
	   Window *parent_return, Window **children_return,
	   unsigned int *nchildren_return)
{
    return 0;
}

Tk_Window
Tk_MainWindow(Tcl_Interp * interp)
{
    return 0;
}

int
Tk_RestackWindow(Tk_Window tkwin, int aboveBelow, Tk_Window other)
{
    return 0;
}

int
Tk_ConfigureInfo (Tcl_Interp * interp, Tk_Window tkwin, Tk_ConfigSpec * specs,
		  char * widgRec, CONST84 char * argvName, int flags)
{
    return 0;
}

int
Tk_ConfigureWidget (Tcl_Interp * interp, Tk_Window tkwin,
		    Tk_ConfigSpec * specs, int argc, CONST84 char ** argv,
		    char * widgRec, int flags)
{
    return 0;
}

void
Tk_DefineCursor (Tk_Window window, Tk_Cursor cursor) {}

void
Tk_DeleteEventHandler (Tk_Window token, unsigned long mask,
		       Tk_EventProc * proc, ClientData clientData) {}

void
Tk_DestroyWindow (Tk_Window tkwin) {}

void
Tk_FreeOptions (Tk_ConfigSpec * specs, char * widgRec, Display * display,
		int needFlags) {}

void
Tk_MakeWindowExist (Tk_Window tkwin) {}

void
Tk_ManageGeometry (Tk_Window tkwin, CONST85 Tk_GeomMgr * mgrPtr,
		   ClientData clientData) {}

void
Tk_MapWindow (Tk_Window tkwin) {}

void
Tk_MoveResizeWindow (Tk_Window tkwin, int x, int y, int width,
		     int height) {}

Tk_Window
Tk_NameToWindow (Tcl_Interp * interp, CONST84 char * pathName,
		 Tk_Window tkwin)
{
    return 0;
}

void
Tk_SetClass (Tk_Window tkwin, CONST84 char * className) {}

void
Tk_UndefineCursor (Tk_Window window) {}

void
Tk_HandleEvent (XEvent * eventPtr) {}

int
XFree(void *data)
{
    return 0;
}

int
XMapWindow(Display *display, Window w)
{
    return 0;
}

int
XConfigureWindow(Display *display, Window w, unsigned int value_mask,
	XWindowChanges *values)
{
    return 0;
}

extern
Window XCreateWindow(Display *display, Window parent, int x, int y, 
		     unsigned int width, unsigned int height,
		     unsigned int border_width, int depth, unsigned int class,
		     Visual *visual, unsigned long valuemask,
		     XSetWindowAttributes *attributes)
{
    return 0;
}

Tk_PhotoHandle
Tk_FindPhoto (Tcl_Interp *interp, CONST84 char *imageName)
{
    return NULL;
}

int
Tk_PhotoGetImage(Tk_PhotoHandle hd, Tk_PhotoImageBlock *blockPtr)
{
    return 0;
}

int
Rat_dummy_Init(Tcl_Interp *interp)
{
    return TCL_OK;
}

void
Tk_SetClassProcs(Tk_Window tkwin, Tk_ClassProcs *procs,
		ClientData instanceData) {}
