/*
 * bltBusy.c --
 *
 *
 * This file has been changed to fit into the tkrat distribution.
 * I have among other things changed the semantics of the release command
 * so that it ignores errors.
 *
 *	maf@dtek.chalmers.se
 *
 *
 *	This module implements busy windows for the BLT toolkit.
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 *
 *	The "busy" command was created by George Howlett.  
 */

#include "tcl.h"
#include "tk.h"

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <stdlib.h>
#include "blt.h"

#ifndef CONST84
#   define CONST84
#endif

#define TRUE    1
#define FALSE   0


#ifndef TK_REPARENTED
#define TK_REPARENTED 0
#endif

typedef struct {
    Display *display;		/* Display of busy window */
    Tcl_Interp *interp;		/* Interpreter where "busy" command was 
				 * created. It's used to key the
				 * searches in the window hierarchy. See the
				 * "windows" command. */

    Tk_Window busy;		/* Busy window: Transparent window used 
				 * to block delivery of events to windows
				 * underneath it. */

    Tk_Window parent;		/* Parent window of the busy
				 * window. It may be the reference
				 * window (if the reference is a
				 * toplevel) or a mutual ancestor of
				 * the reference window */

    Tk_Window tkwin;		/* Reference window of the busy window. 
				 * It is used to manage the size and 
				 * position of the busy window. */

    int x, y;			/* Position of the reference window */

    int width, height;		/* Size of the reference window. Retained to 
				 * know if the reference window has been 
				 * reconfigured to a new size. */

    int isBusy;			/* Indicates whether the transparent
				 * window should be displayed. This
				 * can be different from what
				 * Tk_IsMapped says because the a
				 * sibling reference window may be
				 * unmapped, forcing the busy window
				 * to be also hidden. */

    int menuBar;		/* Menu bar flag. */
    Tk_Cursor cursor;		/* Cursor for the busy window. */

} Busy;

#ifdef WIN32
#define DEF_BUSY_CURSOR "wait"
#else 
#define DEF_BUSY_CURSOR "watch"
#endif

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_CURSOR, "-cursor", "busyCursor", "BusyCursor",
	DEF_BUSY_CURSOR, Tk_Offset(Busy, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static int initialized = 0;	/* If non-zero, indicates to
				 * initialize the hash table */
static Tcl_HashTable busyTable;	/* Hash table of busy window
				 * structures keyed by the address of
				 * the reference Tk window */

static void BusyGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));
static void BusyCustodyProc _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin));

static Tk_GeomMgr busyMgrInfo =
{
    "busy",			/* Name of geometry manager used by winfo */
    BusyGeometryProc,		/* Procedure to for new geometry requests */
    BusyCustodyProc,		/* Procedure when window is taken away */
};

/* Forward declarations */
static void DestroyBusy _ANSI_ARGS_((char* dataPtr));
static void BusyEventProc _ANSI_ARGS_((ClientData clientData, 
	XEvent *eventPtr));

#ifdef __STDC__
static Tk_EventProc BusyEventProc;
static Tk_EventProc RefWinEventProc;
static Tcl_CmdProc BusyCmd;
#endif


/*
 *----------------------------------------------------------------------
 *
 * BusyEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for events on
 *	the busy window itself.  We're only concerned with destroy
 *	events.
 *
 *	It might be necessary (someday) to watch resize events.  Right
 *	now, I don't think there's any point in it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When a busy window is destroyed, all internal structures
 * 	associated with it released at the next idle point.
 *
 *----------------------------------------------------------------------
 */
static void
BusyEventProc(clientData, eventPtr)
    ClientData clientData;	/* Busy window record */
    XEvent *eventPtr;		/* Event which triggered call to routine */
{
    Busy *busyPtr = (Busy *)clientData;

    if (eventPtr->type == DestroyNotify) {
	busyPtr->busy = NULL;
	Tk_EventuallyFree((ClientData)busyPtr, DestroyBusy);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * BusyCustodyProc --
 *
 *	This procedure is invoked when the busy window has been stolen
 *	by another geometry manager.  The information and memory
 *	associated with the busy window is released. I don't know why
 *	anyone would try to pack a busy window, but this should keep
 *	everything sane, if it is.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Busy structure is freed at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the slave window */
    Tk_Window tkwin;		/* Not used */
{
    Busy *busyPtr = (Busy *)clientData;

    Tk_DeleteEventHandler(busyPtr->busy, StructureNotifyMask, BusyEventProc,
	(ClientData)busyPtr);
    if (busyPtr->busy != NULL) {
	Tk_UnmapWindow(busyPtr->busy);
	busyPtr->busy = NULL;
    }
    busyPtr->isBusy = FALSE;
    Tk_EventuallyFree((ClientData)busyPtr, DestroyBusy);
}

/*
 * ----------------------------------------------------------------------------
 *
 * BusyGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for busy
 *	windows.  Busy windows never request geometry, so it's
 *	unlikely that this routine will ever be called.  The routine
 *	exists simply as a place holder for the GeomProc in the
 *	Geometry Manager structure.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about window that got new
				 * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * window. */
{
    /* Should never get here */
}

/*
 * ------------------------------------------------------------------
 *
 * RefWinEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for the
 *	following events on the reference window.  If the reference and
 *	parent windows are the same, only the first event is
 *	important.
 *
 *	   1) ConfigureNotify  - The reference window has been resized or
 *				 moved.  Move and resize the busy window
 *				 to be the same size and position of the
 *				 reference window.
 *
 *	   2) DestroyNotify    - The reference window was destroyed. Destroy
 *				 the busy window and the free resources
 *				 used.
 *
 *	   3) MapNotify	       - The reference window was (re)shown. Map the
 *				 busy window again.
 *
 *	   4) UnmapNotify      - The reference window was hidden. Unmap the
 *				 busy window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the reference window gets deleted, internal structures get
 *	cleaned up.  When it gets resized, the busy window is resized
 *	accordingly. If it's displayed, the busy window is displayed. And
 *	when it's hidden, the busy window is unmapped.
 *
 * -------------------------------------------------------------------
 */
static void
RefWinEventProc(clientData, eventPtr)
    ClientData clientData;	/* Busy window record */
    register XEvent *eventPtr;	/* Event which triggered call to routine */
{
    register Busy *busyPtr = (Busy *)clientData;

    switch (eventPtr->type) {
    case DestroyNotify:

	/*
	 * Arrange for the busy structure to be removed at a proper time.
	 */

	Tk_EventuallyFree((ClientData)busyPtr, DestroyBusy);
	break;

    case ConfigureNotify:
	if ((busyPtr->width != Tk_Width(busyPtr->tkwin)) ||
	    (busyPtr->height != Tk_Height(busyPtr->tkwin)) ||
	    (busyPtr->x != Tk_X(busyPtr->tkwin)) ||
	    (busyPtr->y != Tk_Y(busyPtr->tkwin))) {
	    int x, y;

	    busyPtr->width = Tk_Width(busyPtr->tkwin);
	    busyPtr->height = Tk_Height(busyPtr->tkwin);
	    busyPtr->x = Tk_X(busyPtr->tkwin);
	    busyPtr->y = Tk_Y(busyPtr->tkwin);

	    x = y = 0;
	    if (busyPtr->parent != busyPtr->tkwin) {
		Tk_Window ancestor;

		for (ancestor = busyPtr->tkwin; ancestor != busyPtr->parent;
		    ancestor = Tk_Parent(ancestor)) {
		    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
		    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
		}
	    }
#ifdef DEBUG
	    PurifyPrintf("2 busyPtr->width=%d, busyPtr->height=%d\n", busyPtr->width, busyPtr->height);
#endif
	    if (busyPtr->busy != NULL) {
		Tk_MoveResizeWindow(busyPtr->busy, x, y, busyPtr->width,
		    busyPtr->height);
	    }
	}
	break;

    case MapNotify:
	if ((busyPtr->parent != busyPtr->tkwin) && (busyPtr->isBusy)) {
	    Tk_MapWindow(busyPtr->busy);
	}
	break;

    case UnmapNotify:
	if (busyPtr->parent != busyPtr->tkwin) {
	    Tk_UnmapWindow(busyPtr->busy);
	}
	break;
    }
}

/*
 * ------------------------------------------------------------------
 *
 * ConfigureBusy --
 *
 *	This procedure is called from the Tk event dispatcher. It
 * 	releases X resources and memory used by the busy window and
 *	updates the internal hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and resources are released and the Tk event handler
 *	is removed.
 *
 * -------------------------------------------------------------------
 */
static int
ConfigureBusy(interp, busyPtr, argc, argv)
    Tcl_Interp *interp;
    Busy *busyPtr;
    int argc;
    char **argv;
{
    Tk_Cursor oldCursor;

    oldCursor = busyPtr->cursor;
    if (Tk_ConfigureWidget(interp, busyPtr->tkwin, configSpecs, argc,
			   (CONST84 char**)argv, (char*)busyPtr, 0)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (busyPtr->cursor != oldCursor) {
	if (busyPtr->cursor == None) {
	    Tk_UndefineCursor(busyPtr->busy);
	} else {
	    Tk_DefineCursor(busyPtr->busy, busyPtr->cursor);
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * CreateBusy --
 *
 *	Creates a child transparent window that obscures its parent
 *	window thereby effectively blocking device events.  The size
 *	and position of the busy window is exactly that of the reference
 *	window.
 *
 *	We want to create sibling to the window to be blocked.  If the
 *	busy window is a child of the window to be blocked, Enter/Leave
 *	events can sneak through.  Futhermore under WIN32, messages of
 *	transparent windows are sent directly to the parent.  The only
 *	exception to this are toplevels, since we can't make a sibling.
 *	Fortunately, toplevel windows rarely receive events that need
 *	blocking.
 *
 * Results:
 *	Returns a pointer to the new busy window structure.
 *
 * Side effects:
 *	When the busy window is eventually displayed, it will screen
 *	device events (in the area of the reference window) from reaching
 *	its parent window and its children.  User feed back can be
 *	achieved by changing the cursor.
 *
 * -------------------------------------------------------------------
 */
static Busy *
CreateBusy(interp, tkwin)
    Tcl_Interp *interp;		/* Interpreter to report error to */
    Tk_Window tkwin;		/* Window hosting the busy window */
{
    Busy *busyPtr;
    int length;
    char *fmt, *name;
    Tk_Window busy;
    Window parentWin;
    Tk_Window parent;
    Tk_FakeWin *winPtr;
    int x, y;

    busyPtr = (Busy *)calloc(1, sizeof(Busy));
    x = y = 0;
    length = strlen(Tk_Name(tkwin));
    name = (char *)ckalloc(length + 6);
    if (Tk_IsTopLevel(tkwin)) {
	fmt = "_Busy";		/* Child */
	parent = tkwin;
    } else {
	Tk_Window ancestor;

	fmt = "%s_Busy";	/* Sibling */
	parent = Tk_Parent(tkwin);
	for (ancestor = tkwin; ancestor != parent;
	    ancestor = Tk_Parent(ancestor)) {
	    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
	    if (Tk_IsTopLevel(ancestor)) {
		break;
	    }
	}
    }
    sprintf(name, fmt, Tk_Name(tkwin));
    busy = Tk_CreateWindow(interp, parent, name, (char *)NULL);
    ckfree((char *)name);

    if (busy == NULL) {
	return NULL;
    }
    Tk_MakeWindowExist(tkwin);
    busyPtr->display = Tk_Display(tkwin);
    busyPtr->tkwin = tkwin;
    busyPtr->parent = parent;
    busyPtr->interp = interp;
    busyPtr->width = Tk_Width(tkwin);
    busyPtr->height = Tk_Height(tkwin);
    busyPtr->x = Tk_X(tkwin);
    busyPtr->y = Tk_Y(tkwin);
    busyPtr->cursor = None;
    busyPtr->busy = busy;
    Tk_SetClass(busy, "Busy");
#if (TK_MAJOR_VERSION >= 8)
    Blt_SetWindowInstanceData(busy, (ClientData)busyPtr);
#endif
    winPtr = (Tk_FakeWin *)tkwin;
    if (winPtr->flags & TK_REPARENTED) {
	/* 
	 * This works around a bug in the implementation of menubars
	 * for non-MacIntosh window systems (Win32 and X11).  Tk
	 * doesn't reset the pointers to the parent window when the
	 * menu is reparented (winPtr->parentPtr points to the
	 * wrong window). We get around this by determining the parent
	 * via the native API calls. 
	 */
#ifdef WIN32
	{
	    HWND hWnd;
	    RECT region;

	    hWnd = GetParent(Tk_GetHWND(Tk_WindowId(tkwin)));
	    parentWin = (Window)hWnd;
	    if (GetWindowRect(hWnd, &region)) {
		busyPtr->width = region.right - region.left;
		busyPtr->height = region.bottom - region.top;
#ifdef WINDEBUG
		PurifyPrintf("0. busyPtr->width=%d, busyPtr->height=%d\n", 
	busyPtr->width, busyPtr->height);
#endif
	    }
	}
#else
	parentWin = Blt_GetParent(Tk_Display(tkwin), Tk_WindowId(tkwin));
#endif
    } else {
	parentWin = Tk_WindowId(parent);
#ifdef WIN32
	parentWin = (Window)Tk_GetHWND(parentWin);
#endif
    }

    Blt_MakeTransparentWindowExist(busy, parentWin);

#ifdef WINDEBUG
    PurifyPrintf("1. busyPtr->width=%d, busyPtr->height=%d\n", 
	busyPtr->width, busyPtr->height);
#endif
    Tk_MoveResizeWindow(busy, x, y, busyPtr->width, busyPtr->height);
    Tk_RestackWindow(busy, Above, (Tk_Window)NULL);

    /*
     * Only worry if the busy window is destroyed.
     */
    Tk_CreateEventHandler(busy, StructureNotifyMask, BusyEventProc, 
	(ClientData)busyPtr);

    /*
     * Indicate that the busy window's geometry is being managed.
     * This will also notify us if the busy window is ever packed.
     */
    Tk_ManageGeometry(busy, &busyMgrInfo, (ClientData)busyPtr);

    if (busyPtr->cursor != None) {
	Tk_DefineCursor(busy, busyPtr->cursor);
    }

    /* Track the reference window to see if it is resized or destroyed.  */
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, RefWinEventProc,
	(ClientData)busyPtr);
    return (busyPtr);
}

/*
 * ------------------------------------------------------------------
 *
 * DestroyBusy --
 *
 *	This procedure is called from the Tk event dispatcher. It
 *	releases X resources and memory used by the busy window and
 *	updates the internal hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and resources are released and the Tk event handler
 *	is removed.
 *
 * -------------------------------------------------------------------
 */
static void
DestroyBusy(char *dataPtr)
{
    Busy *busyPtr = (Busy *)dataPtr;
    Tcl_HashEntry *hPtr;

    Tk_FreeOptions(configSpecs, (char *)busyPtr, busyPtr->display, 0);
    hPtr = Tcl_FindHashEntry(&busyTable, (char *)busyPtr->tkwin);
    if (hPtr != NULL) {
	Tcl_DeleteHashEntry(hPtr);
    }
    Tk_DeleteEventHandler(busyPtr->tkwin, StructureNotifyMask,
	RefWinEventProc, (ClientData)busyPtr);
    if (busyPtr->busy != NULL) {
	Tk_DeleteEventHandler(busyPtr->busy, StructureNotifyMask,
	    BusyEventProc, (ClientData)busyPtr);
	Tk_ManageGeometry(busyPtr->busy, (Tk_GeomMgr *) NULL,
	    (ClientData)busyPtr);
	Tk_DestroyWindow(busyPtr->busy);
    }
    free((char *)busyPtr);
}

/*
 * ------------------------------------------------------------------
 *
 * GetBusy --
 *
 *	Returns the busy window structure associated with the reference
 *	window, keyed by its path name.  The clientData argument is
 *	the main window of the interpreter, used to search for the
 *	reference window in its own window hierarchy.
 *
 * Results:
 *	If path name represents a reference window with a busy window, a
 *	pointer to the busy window structure is returned. Otherwise,
 *	NULL is returned and an error message is left in
 *	interp->result.
 *
 * -------------------------------------------------------------------
 */
static int
GetBusy(clientData, interp, pathName, busyPtrPtr)
    ClientData clientData;	/* Window used to reference search  */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    char *pathName;		/* Path name of parent window */
    Busy **busyPtrPtr;		/* Will contain address of busy window if
				 * found. */
{
    Tcl_HashEntry *hPtr;
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Tcl_FindHashEntry(&busyTable, (char *)tkwin);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't find busy window \"", pathName, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    *busyPtrPtr = ((Busy *)Tcl_GetHashValue(hPtr));
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * HoldBusy --
 *
 *	Creates (if necessary) and maps a busy window, thereby
 *	preventing device events from being be received by the parent
 *	window and its children.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, it is unmapped and TCL_OK is returned. Otherwise,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	The busy window is created and displayed, blocking events from
 *	the parent window and its children.
 *
 * -------------------------------------------------------------------
 */
static int
HoldBusy(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Window name and option pairs */
{
    Tk_Window tkwin;
    Tcl_HashEntry *hPtr;
    Busy *busyPtr;
    int isNew;
    int result;

    tkwin = Tk_NameToWindow(interp, argv[0], Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Tcl_CreateHashEntry(&busyTable, (char *)tkwin, &isNew);
    if (isNew) {
	busyPtr = (Busy *)CreateBusy(interp, tkwin);
	if (busyPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetHashValue(hPtr, (char *)busyPtr);
    } else {
	busyPtr = (Busy *)Tcl_GetHashValue(hPtr);

	/*
	 * Raise and re-map the busy window whenever hold is reasserted.
	 */

	Tk_RestackWindow(busyPtr->busy, Above, (Tk_Window)NULL);
    }

    /*
     * Don't map the busy window unless the reference window is also displayed
     */
    if (Tk_IsMapped(busyPtr->tkwin)) {
	Tk_MapWindow(busyPtr->busy);
    } else {
	Tk_UnmapWindow(busyPtr->busy);
    }
    busyPtr->isBusy = TRUE;
    Tk_Preserve((ClientData)busyPtr);
    result = ConfigureBusy(interp, busyPtr, argc - 1, argv + 1);
    Tk_Release((ClientData)busyPtr);
#ifdef WIN32
    { 
	POINT point;
	/* 
	 * Under Win32, cursors aren't associated with windows.  Tk
	 * fakes this by watching Motion events on its windows.  So Tk
	 * will automatically change the cursor when the pointer
	 * enters the Busy window.  But Windows doesn't immediately
	 * change the cursor; it waits for the cursor position to
	 * change or a system call.  We need to change the cursor
	 * before the application starts processing, so set the cursor
	 * position redundantly back to the current position.  
	 */
	GetCursorPos(&point);
	SetCursorPos(point.x, point.y);
    }
#endif
    return result;
}

/*
 * ------------------------------------------------------------------
 *
 * StatusOp --
 *
 *	Returns the status of the busy window; whether it's blocking
 *	events or not.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, the status is returned via interp->result and TCL_OK
 *	is returned. Otherwise, TCL_ERROR is returned and an error
 *	message is left in interp->result.
 *
 * -------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StatusOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of interpreter */
    Tcl_Interp *interp;		/* Interpreter to report error to */
    int argc;			/* not used */
    char **argv;
{
    Busy *busyPtr;

    if (GetBusy(clientData, interp, argv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tk_Preserve((ClientData)busyPtr);
    Tcl_SetResult(interp, busyPtr->isBusy ? "1" : "0", TCL_STATIC);
    Tk_Release((ClientData)busyPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * ForgetOp --
 *
 *	Destroys the busy window associated with the reference window and
 *	arranges for internal resources to the released when they're
 *	not being used anymore.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, it is destroyed and TCL_OK is returned. Otherwise,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	The busy window is removed.  Other related memory and resources
 *	are eventually released by the Tk dispatcher.
 *
 * -------------------------------------------------------------------
 */
static int
ForgetOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Busy *busyPtr;
    register int i;
    Tcl_HashEntry *hPtr;
    Tk_Window tkwin;
    Tk_Window mainWindow;

    mainWindow = Tk_MainWindow(interp);
    for (i = 2; i < argc; i++) {
	tkwin = Tk_NameToWindow(interp, argv[i], mainWindow);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&busyTable, (char *)tkwin);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find busy window \"", argv[i],
		"\"", (char *)NULL);
	    return TCL_ERROR;
	}
	busyPtr = (Busy *)Tcl_GetHashValue(hPtr);

	/* Unmap the window even though it will be soon destroyed */
	if (busyPtr->busy != NULL) {
	    Tk_UnmapWindow(busyPtr->busy);
	}
	busyPtr->isBusy = FALSE;

	Tk_EventuallyFree((ClientData)busyPtr, DestroyBusy);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * ReleaseOp --
 *
 *	Unmaps the busy window, thereby permitting device events
 *	to be received by the parent window and its children.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, it is unmapped and TCL_OK is returned. Otherwise,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	The busy window is hidden, allowing the parent window and
 *	its children to receive events again.
 *
 * -------------------------------------------------------------------
 */
static int
ReleaseOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Busy *busyPtr;
    int i;

    for (i = 2; i < argc; i++) {
	if (GetBusy(clientData, interp, argv[i], &busyPtr) != TCL_OK) {
	    continue;
	}
	Tk_Preserve((ClientData)busyPtr);
	if (busyPtr->busy != NULL) {
	    Tk_UnmapWindow(busyPtr->busy);
	}
	busyPtr->isBusy = FALSE;
	Tk_Release((ClientData)busyPtr);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * WindowsOp --
 *
 *	Reports the names of all widgets with busy windows attached to
 *	them, matching a given pattern.  If no pattern is given, all
 *	busy widgets are listed.
 *
 * Results:
 *	Returns a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy
 *	or not.
 *
 * -------------------------------------------------------------------
 */
static int
WindowsOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Busy *busyPtr;

    for (hPtr = Tcl_FirstHashEntry(&busyTable, &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	busyPtr = (Busy *)Tcl_GetHashValue(hPtr);
	if (busyPtr->interp == interp) {
	    if ((argc != 3) ||
		(Tcl_StringMatch(Tk_PathName(busyPtr->tkwin), argv[2]))) {
		Tcl_AppendElement(interp, Tk_PathName(busyPtr->tkwin));
	    }
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * IsBusyOp --
 *
 *	Reports the names of all widgets with busy windows attached to
 *	them, matching a given pattern.  If no pattern is given, all
 *	busy widgets are listed.
 *
 * Results:
 *	Returns a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy
 *	or not.
 *
 * -------------------------------------------------------------------
 */
static int
IsBusyOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Busy *busyPtr;

    for (hPtr = Tcl_FirstHashEntry(&busyTable, &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	busyPtr = (Busy *)Tcl_GetHashValue(hPtr);
	if (busyPtr->interp == interp) {
	    if ((busyPtr->isBusy) && ((argc != 3) ||
		    (Tcl_StringMatch(Tk_PathName(busyPtr->tkwin), argv[2])))) {
		Tcl_AppendElement(interp, Tk_PathName(busyPtr->tkwin));
	    }
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * HoldOp --
 *
 *	Creates (if necessary) and maps a busy window, thereby
 *	preventing device events from being be received by the parent
 *      window and its children. The argument vector may contain
 *	option-value pairs of configuration options to be set.
 *
 * Results:
 *	Returns a standard TCL result.
 *
 * Side effects:
 *	The busy window is created and displayed, blocking events from the
 *	parent window and its children.
 *
 * -------------------------------------------------------------------
 */
static int
HoldOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Window name and option pairs */
{
    register int i, count;

    if ((argv[1][0] == 'h') && (strcmp(argv[1], "hold") == 0)) {	
	argc--, argv++;		/* Command used "hold" keyword */
    }
    for (i = 1; i < argc; i++) {
	/*
	 * Find the end of the option-value pairs for this window.
	 */
	for (count = i + 1; count < argc; count += 2) {
	    if (argv[count][0] != '-') {
		break;
	    }
	}
	if (count > argc) {
	    count = argc;
	}
	if (HoldBusy(clientData, interp, count - i, argv + i) != TCL_OK) {
	    return TCL_ERROR;
	}
	i = count;
    }
    return TCL_OK;
}

/* ARGSUSED*/
static int
CgetOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Widget pathname and option switch */
{
    Busy *busyPtr;
    int result;

    if (GetBusy(clientData, interp, argv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tk_Preserve((ClientData)busyPtr);
    result = Tk_ConfigureValue(interp, busyPtr->tkwin, configSpecs,
	(char *)busyPtr, argv[3], 0);
    Tk_Release((ClientData)busyPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	This procedure is called to process an argv/argc list in order
 *	to configure (or reconfigure) a busy window.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information get set for busyPtr; old resources
 *	get freed, if there were any.  The busy window destroyed and
 *	recreated in a new parent window.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Reference window path name and options */
{
    Busy *busyPtr;
    int result;

    if (GetBusy(clientData, interp, argv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tk_Preserve((ClientData)busyPtr);
    if (argc == 3) {
	result = Tk_ConfigureInfo(interp, busyPtr->tkwin, configSpecs,
	    (char *)busyPtr, (char *)NULL, 0);
    } else if (argc == 4) {
	result = Tk_ConfigureInfo(interp, busyPtr->tkwin, configSpecs,
	    (char *)busyPtr, argv[3], 0);
    } else {
	result = ConfigureBusy(interp, busyPtr, argc - 3, argv + 3);
    }
    Tk_Release((ClientData)busyPtr);
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Busy Sub-command specification:
 *
 *	- Name of the sub-command.
 *	- Minimum number of characters needed to unambiguously
 *        recognize the sub-command.
 *	- Pointer to the function to be called for the sub-command.
 *	- Minimum number of arguments accepted.
 *	- Maximum number of arguments accepted.
 *	- String to be displayed for usage (arguments only).
 *
 *--------------------------------------------------------------
 *
static Blt_OpSpec busyOps[] =
{
    {"cget", 2, (Blt_Operation)CgetOp, 4, 4, "window option",},
    {"configure", 2, (Blt_Operation)ConfigureOp, 3, 0,
	"window ?options?...",},
    {"forget", 1, (Blt_Operation)ForgetOp, 2, 0, "?window?...",},
    {"hold", 3, (Blt_Operation)HoldOp, 3, 0,
	"window ?options?... ?window options?...",},
    {"isbusy", 1, (Blt_Operation)IsBusyOp, 2, 3, "?pattern?",},
    {"release", 1, (Blt_Operation)ReleaseOp, 2, 0, "?window?...",},
    {"status", 1, (Blt_Operation)StatusOp, 3, 3, "window",},
    {"windows", 1, (Blt_Operation)WindowsOp, 2, 3, "?pattern?",},
};
static int numBusyOps = sizeof(busyOps) / sizeof(Blt_OpSpec);*/

/*
 *----------------------------------------------------------------------
 *
 * BusyCmd --
 *
 *	This procedure is invoked to process the "busy" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
static int
BusyCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter associated with command */
    int argc;
    CONST84 char **argv;
{
    int result;

    if (!initialized) {
	Tcl_InitHashTable(&busyTable, TCL_ONE_WORD_KEYS);
	initialized = 1;
    }
    if ((argc > 1) && (argv[1][0] == '.')) {
	return (HoldOp(clientData, interp, argc, argv));
    }
    if (!strcmp(argv[1], "cget") && argc == 4) {
	result = CgetOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "configure") && argc >= 3) {
	result = ConfigureOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "forget") && argc >= 2) {
	result = ForgetOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "hold") && argc >= 3) {
	result = HoldOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "isbusy") && argc >= 2 && argc <= 3) {
	result = IsBusyOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "release") && argc >= 2) {
	result = ReleaseOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "status") && argc == 3) {
	result = StatusOp(clientData, interp, argc, argv);
    } else if (!strcmp(argv[1], "windows") && argc >= 2 && argc <= 3) {
	result = WindowsOp(clientData, interp, argc, argv);
    } else {
	return TCL_ERROR;
    }
    return result;
}

int
Blt_busy_Init(Tcl_Interp *interp)
{
    Tcl_CreateCommand(interp, "blt_busy", BusyCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "blt_busy", BUSYLIB_VERSION);
}
