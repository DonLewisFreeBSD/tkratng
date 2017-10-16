
/*
 * bltWindow.c --
 *
 *	This module implements additional window functionality for 
 *	the BLT toolkit, such as transparent Tk windows, 
 *	and reparenting Tk windows.
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
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
 */

#include "tk.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "blt.h"

struct TkIdStack;
struct TkErrorHandler;
struct TkSelectionInfo;
struct TkClipboardTarget;

typedef struct TkWindow TkWindow;
typedef struct TkMainInfo TkMainInfo;
typedef struct TkEventHandler TkEventHandler;
typedef struct TkSelHandler TkSelHandler;
typedef struct TkWinInfo TkWinInfo;
typedef struct TkClassProcs TkClassProcs;
typedef struct TkWindowPrivate TkWindowPrivate;
typedef struct TkGrabEvent TkGrabEvent;
typedef struct TkColormap TkColormap;
typedef struct TkStressedCmap TkStressedCmap;

/*
 * This defines whether we should try to use XIM over-the-spot style
 * input.  Allow users to override it.  It is a much more elegant use
 * of XIM, but uses a bit more memory.
 */

#ifndef TK_XIM_SPOT
#   define TK_XIM_SPOT	1
#endif

/*
 * One of the following structures is maintained for each display
 * containing a window managed by Tk.  In part, the structure is 
 * used to store thread-specific data, since each thread will have 
 * its own TkDisplay structure.
 */

typedef struct TkDisplay {
    Display *display;		/* Xlib's info about display. */
    struct TkDisplay *nextPtr;	/* Next in list of all displays. */
    char *name;			/* Name of display (with any screen
				 * identifier removed).  Malloc-ed. */
    Time lastEventTime;		/* Time of last event received for this
				 * display. */

    /*
     * Information used primarily by tk3d.c:
     */

    int borderInit;             /* 0 means borderTable needs initializing. */
    Tcl_HashTable borderTable;  /* Maps from color name to TkBorder 
				 * structure. */

    /*
     * Information used by tkAtom.c only:
     */

    int atomInit;		/* 0 means stuff below hasn't been
				 * initialized yet. */
    Tcl_HashTable nameTable;	/* Maps from names to Atom's. */
    Tcl_HashTable atomTable;	/* Maps from Atom's back to names. */

    /*
     * Information used primarily by tkBind.c:
     */

    int bindInfoStale;		/* Non-zero means the variables in this
				 * part of the structure are potentially
				 * incorrect and should be recomputed. */
    unsigned int modeModMask;	/* Has one bit set to indicate the modifier
				 * corresponding to "mode shift".  If no
				 * such modifier, than this is zero. */
    unsigned int metaModMask;	/* Has one bit set to indicate the modifier
				 * corresponding to the "Meta" key.  If no
				 * such modifier, then this is zero. */
    unsigned int altModMask;	/* Has one bit set to indicate the modifier
				 * corresponding to the "Meta" key.  If no
				 * such modifier, then this is zero. */
    enum {LU_IGNORE, LU_CAPS, LU_SHIFT} lockUsage;
				/* Indicates how to interpret lock modifier. */
    int numModKeyCodes;		/* Number of entries in modKeyCodes array
				 * below. */
    KeyCode *modKeyCodes;	/* Pointer to an array giving keycodes for
				 * all of the keys that have modifiers
				 * associated with them.  Malloc'ed, but
				 * may be NULL. */

    /*
     * Information used by tkBitmap.c only:
     */
  
    int bitmapInit;             /* 0 means tables above need initializing. */
    int bitmapAutoNumber;       /* Used to number bitmaps. */
    Tcl_HashTable bitmapNameTable;    
                                /* Maps from name of bitmap to the first 
				 * TkBitmap record for that name. */
    Tcl_HashTable bitmapIdTable;/* Maps from bitmap id to the TkBitmap
				 * structure for the bitmap. */
    Tcl_HashTable bitmapDataTable;    
                                /* Used by Tk_GetBitmapFromData to map from
				 * a collection of in-core data about a 
				 * bitmap to a reference giving an auto-
				 * matically-generated name for the bitmap. */

    /*
     * Information used by tkCanvas.c only:
     */

    int numIdSearches;          
    int numSlowSearches;

    /*
     * Used by tkColor.c only:
     */

    int colorInit;              /* 0 means color module needs initializing. */
    TkStressedCmap *stressPtr;	/* First in list of colormaps that have
				 * filled up, so we have to pick an
				 * approximate color. */
    Tcl_HashTable colorNameTable;
                                /* Maps from color name to TkColor structure
				 * for that color. */
    Tcl_HashTable colorValueTable;
                                /* Maps from integer RGB values to TkColor
				 * structures. */

    /*
     * Used by tkCursor.c only:
     */

    int cursorInit;             /* 0 means cursor module need initializing. */
    Tcl_HashTable cursorNameTable;
                                /* Maps from a string name to a cursor to the
				 * TkCursor record for the cursor. */
    Tcl_HashTable cursorDataTable;
                                /* Maps from a collection of in-core data
				 * about a cursor to a TkCursor structure. */
    Tcl_HashTable cursorIdTable;
                                /* Maps from a cursor id to the TkCursor
				 * structure for the cursor. */
    char cursorString[20];      /* Used to store a cursor id string. */
    Font cursorFont;		/* Font to use for standard cursors.
				 * None means font not loaded yet. */

    /*
     * Information used by tkError.c only:
     */

    struct TkErrorHandler *errorPtr;
				/* First in list of error handlers
				 * for this display.  NULL means
				 * no handlers exist at present. */
    int deleteCount;		/* Counts # of handlers deleted since
				 * last time inactive handlers were
				 * garbage-collected.  When this number
				 * gets big, handlers get cleaned up. */

    /*
     * Used by tkEvent.c only:
     */

    struct TkWindowEvent *delayedMotionPtr;
				/* Points to a malloc-ed motion event
				 * whose processing has been delayed in
				 * the hopes that another motion event
				 * will come along right away and we can
				 * merge the two of them together.  NULL
				 * means that there is no delayed motion
				 * event. */

    /*
     * Information used by tkFocus.c only:
     */

    int focusDebug;             /* 1 means collect focus debugging 
				 * statistics. */
    struct TkWindow *implicitWinPtr;
				/* If the focus arrived at a toplevel window
				 * implicitly via an Enter event (rather
				 * than via a FocusIn event), this points
				 * to the toplevel window.  Otherwise it is
				 * NULL. */
    struct TkWindow *focusPtr;	/* Points to the window on this display that
				 * should be receiving keyboard events.  When
				 * multiple applications on the display have
				 * the focus, this will refer to the
				 * innermost window in the innermost
				 * application.  This information isn't used
				 * under Unix or Windows, but it's needed on
				 * the Macintosh. */

    /*
     * Information used by tkGC.c only:
     */
    
    Tcl_HashTable gcValueTable; /* Maps from a GC's values to a TkGC structure
				 * describing a GC with those values. */
    Tcl_HashTable gcIdTable;    /* Maps from a GC to a TkGC. */ 
    int gcInit;                 /* 0 means the tables below need 
				 * initializing. */

    /*
     * Information used by tkGeometry.c only:
     */

    Tcl_HashTable maintainHashTable;
                                /* Hash table that maps from a master's 
				 * Tk_Window token to a list of slaves
				 * managed by that master. */
    int geomInit;    

    /*
     * Information used by tkGet.c only:
     */
  
    Tcl_HashTable uidTable;     /* Stores all Tk_Uids used in a thread. */
    int uidInit;                /* 0 means uidTable needs initializing. */

    /*
     * Information used by tkGrab.c only:
     */

    struct TkWindow *grabWinPtr;
				/* Window in which the pointer is currently
				 * grabbed, or NULL if none. */
    struct TkWindow *eventualGrabWinPtr;
				/* Value that grabWinPtr will have once the
				 * grab event queue (below) has been
				 * completely emptied. */
    struct TkWindow *buttonWinPtr;
				/* Window in which first mouse button was
				 * pressed while grab was in effect, or NULL
				 * if no such press in effect. */
    struct TkWindow *serverWinPtr;
				/* If no application contains the pointer then
				 * this is NULL.  Otherwise it contains the
				 * last window for which we've gotten an
				 * Enter or Leave event from the server (i.e.
				 * the last window known to have contained
				 * the pointer).  Doesn't reflect events
				 * that were synthesized in tkGrab.c. */
    TkGrabEvent *firstGrabEventPtr;
				/* First in list of enter/leave events
				 * synthesized by grab code.  These events
				 * must be processed in order before any other
				 * events are processed.  NULL means no such
				 * events. */
    TkGrabEvent *lastGrabEventPtr;
				/* Last in list of synthesized events, or NULL
				 * if list is empty. */
    int grabFlags;		/* Miscellaneous flag values.  See definitions
				 * in tkGrab.c. */

    /*
     * Information used by tkGrid.c only:
     */

    int gridInit;               /* 0 means table below needs initializing. */
    Tcl_HashTable gridHashTable;/* Maps from Tk_Window tokens to 
				 * corresponding Grid structures. */

    /*
     * Information used by tkImage.c only:
     */

    int imageId;                /* Value used to number image ids. */

    /*
     * Information used by tkMacWinMenu.c only:
     */

    int postCommandGeneration;  

    /*
     * Information used by tkOption.c only.
     */



    /*
     * Information used by tkPack.c only.
     */

    int packInit;              /* 0 means table below needs initializing. */
    Tcl_HashTable packerHashTable;
                               /* Maps from Tk_Window tokens to 
				* corresponding Packer structures. */
    

    /*
     * Information used by tkPlace.c only.
     */

    int placeInit;              /* 0 means tables below need initializing. */
    Tcl_HashTable masterTable;  /* Maps from Tk_Window toke to the Master
				 * structure for the window, if it exists. */
    Tcl_HashTable slaveTable;   /* Maps from Tk_Window toke to the Slave
				 * structure for the window, if it exists. */

    /*
     * Information used by tkSelect.c and tkClipboard.c only:
     */

    struct TkSelectionInfo *selectionInfoPtr;
				/* First in list of selection information
				 * records.  Each entry contains information
				 * about the current owner of a particular
				 * selection on this display. */
    Atom multipleAtom;		/* Atom for MULTIPLE.  None means
				 * selection stuff isn't initialized. */
    Atom incrAtom;		/* Atom for INCR. */
    Atom targetsAtom;		/* Atom for TARGETS. */
    Atom timestampAtom;		/* Atom for TIMESTAMP. */
    Atom textAtom;		/* Atom for TEXT. */
    Atom compoundTextAtom;	/* Atom for COMPOUND_TEXT. */
    Atom applicationAtom;	/* Atom for TK_APPLICATION. */
    Atom windowAtom;		/* Atom for TK_WINDOW. */
    Atom clipboardAtom;		/* Atom for CLIPBOARD. */
#if TK_MINOR_VERSION >= 4
    Atom utf8Atom;
#endif /* 8.4 or later */

    Tk_Window clipWindow;	/* Window used for clipboard ownership and to
				 * retrieve selections between processes. NULL
				 * means clipboard info hasn't been
				 * initialized. */
    int clipboardActive;	/* 1 means we currently own the clipboard
				 * selection, 0 means we don't. */
    struct TkMainInfo *clipboardAppPtr;
				/* Last application that owned clipboard. */
    struct TkClipboardTarget *clipTargetPtr;
				/* First in list of clipboard type information
				 * records.  Each entry contains information
				 * about the buffers for a given selection
				 * target. */

    /*
     * Information used by tkSend.c only:
     */

    Tk_Window commTkwin;	/* Window used for communication
				 * between interpreters during "send"
				 * commands.  NULL means send info hasn't
				 * been initialized yet. */
    Atom commProperty;		/* X's name for comm property. */
    Atom registryProperty;	/* X's name for property containing
				 * registry of interpreter names. */
    Atom appNameProperty;	/* X's name for property used to hold the
				 * application name on each comm window. */

    /*
     * Information used by tkXId.c only:
     */

    struct TkIdStack *idStackPtr;
				/* First in list of chunks of free resource
				 * identifiers, or NULL if there are no free
				 * resources. */
    XID (*defaultAllocProc) _ANSI_ARGS_((Display *display));
				/* Default resource allocator for display. */
    struct TkIdStack *windowStackPtr;
				/* First in list of chunks of window
				 * identifers that can't be reused right
				 * now. */
    int idCleanupScheduled;	/* 1 means a call to WindowIdCleanup has
				 * already been scheduled, 0 means it
				 * hasn't. */

    /*
     * Information used by tkUnixWm.c and tkWinWm.c only:
     */

#if TK_MINOR_VERSION < 4
    int wmTracing;              /* Used to enable or disable tracing in 
				 * this module.  If tracing is enabled, 
				 * then information is printed on
				 * standard output about interesting 
				 * interactions with the window manager. */
#endif /* TK_MINOR_VERSION < 4 */
    struct TkWmInfo *firstWmPtr;  /* Points to first top-level window. */
    struct TkWmInfo *foregroundWmPtr;    
                                /* Points to the foreground window. */

    /*
     * Information maintained by tkWindow.c for use later on by tkXId.c:
     */


    int destroyCount;		/* Number of Tk_DestroyWindow operations
				 * in progress. */
    unsigned long lastDestroyRequest;
				/* Id of most recent XDestroyWindow request;
				 * can re-use ids in windowStackPtr when
				 * server has seen this request and event
				 * queue is empty. */

    /*
     * Information used by tkVisual.c only:
     */

    TkColormap *cmapPtr;	/* First in list of all non-default colormaps
				 * allocated for this display. */

    /*
     * Miscellaneous information:
     */
#ifdef TK_USE_INPUT_METHODS     
    XIM inputMethod;            /* Input method for this display */
#if TK_MINOR_VERSION >= 4
#if TK_XIM_SPOT
    XFontSet inputXfs;		/* XFontSet cached for over-the-spot XIM. */
#endif    
#else /* < 8.4 */
#ifdef I18N_IMPROVE              
    Tcl_Encoding imEncoding;    /* Tcl encoding when the first Tcl
                                 * interp was created.
                                 * For encoding conversion from
                                 * XmbLookupString() to UTF.
                                 */
    XIC lastFocusedIC;          /* The last focused input context on
                                 * the display.
                                 */
#ifdef XNDestroyCallback
    XIMCallback destroyCallback;
#endif /* XNDestroyCallback */
#endif /* I18N_IMPROVE */
#endif /* < 8.4 */
#endif /* TK_USE_INPUT_METHODS */
    Tcl_HashTable winTable;	/* Maps from X window ids to TkWindow ptrs. */

    int refCount;		/* Reference count of how many Tk applications
                                 * are using this display. Used to clean up
                                 * the display when we no longer have any
                                 * Tk applications using it.
                                 */
#if TK_MINOR_VERSION >= 3
    int mouseButtonState;       /* current mouse button state for this
                                 * display */
    int warpInProgress;
    Window warpWindow;
    int warpX;
    int warpY;
    int useInputMethods;        /* Whether to use input methods */
#endif /* 8.3 or later */
#if TK_MINOR_VERSION >= 4
    long deletionEpoch;         /* Incremented by window deletions */
#endif /* 8.4 or later */
} TkDisplay;

struct TkWindow {
    Display *display;
    TkDisplay *dispPtr;	
    int screenNum;
    Visual *visual;
    int depth;
    Window window;
    struct TkWindow *childList;
    struct TkWindow *lastChildPtr;
    struct TkWindow *parentPtr;
    struct TkWindow *nextPtr;
    TkMainInfo *mainPtr;
    char *pathName;
    Tk_Uid nameUid;
    Tk_Uid classUid;
    XWindowChanges changes;
    unsigned int dirtyChanges;
    XSetWindowAttributes atts;
    unsigned long dirtyAtts;
    unsigned int flags;
    TkEventHandler *handlerList;
#ifdef TK_USE_INPUT_METHODS
    XIC inputContext;
#endif /* TK_USE_INPUT_METHODS */
    ClientData *tagPtr;
    int numTags;
    int optionLevel;	
    TkSelHandler *selHandlerList;
    Tk_GeomMgr *geomMgrPtr;
    ClientData geomData;
    int reqWidth, reqHeight;
    int internalBorderLeft;
    TkWinInfo *wmInfoPtr;
    TkClassProcs *classProcsPtr;
    ClientData instanceData;
    TkWindowPrivate *privatePtr;
};


Window
Blt_GetParent(display, window)
    Display *display;
    Window window;
{
    Window root, parent;
    Window *dummy;
    unsigned int count;

    if (XQueryTree(display, window, &root, &parent, &dummy, &count) > 0) {
	XFree(dummy);
	return parent;
    }
    return None;
}

/*
 *----------------------------------------------------------------------
 *
 * DoConfigureNotify --
 *
 *	Generate a ConfigureNotify event describing the current
 *	configuration of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An event is generated and processed by Tk_HandleEvent.
 *
 *----------------------------------------------------------------------
 */
static void
DoConfigureNotify(winPtr)
    Tk_FakeWin *winPtr;		/* Window whose configuration was just changed. */
{
    XEvent event;

    event.type = ConfigureNotify;
    event.xconfigure.serial = LastKnownRequestProcessed(winPtr->display);
    event.xconfigure.send_event = False;
    event.xconfigure.display = winPtr->display;
    event.xconfigure.event = winPtr->window;
    event.xconfigure.window = winPtr->window;
    event.xconfigure.x = winPtr->changes.x;
    event.xconfigure.y = winPtr->changes.y;
    event.xconfigure.width = winPtr->changes.width;
    event.xconfigure.height = winPtr->changes.height;
    event.xconfigure.border_width = winPtr->changes.border_width;
    if (winPtr->changes.stack_mode == Above) {
	event.xconfigure.above = winPtr->changes.sibling;
    } else {
	event.xconfigure.above = None;
    }
    event.xconfigure.override_redirect = winPtr->atts.override_redirect;
    Tk_HandleEvent(&event);
}

/*
 *--------------------------------------------------------------
 *
 * Blt_MakeTransparentWindowExist --
 *
 *	Similar to Tk_MakeWindowExist but instead creates a
 *	transparent window to block for user events from sibling
 *	windows.
 *
 *	Differences from Tk_MakeWindowExist.
 *
 *	1. This is always a "busy" window. There's never a
 *	   platform-specific class procedure to execute instead.
 *	2. The window is transparent and never will contain children,
 *	   so colormap information is irrelevant.
 *
 * Results:
 *	None.
 *
 * Side effects: 
 *	When the procedure returns, the internal window associated
 *	with tkwin is guaranteed to exist.  This may require the
 *	window's ancestors to be created too.
 *
 *-------------------------------------------------------------- 
 */
void
Blt_MakeTransparentWindowExist(tkwin, parent)
    Tk_Window tkwin;		/* Token for window. */
    Window parent;		/* Reference window. */
{
    struct TkWindow *winPtr = (TkWindow *)tkwin;
    struct TkWindow *winPtr2;
    Tcl_HashEntry *hPtr;
    int notUsed;
    TkDisplay *dispPtr;

    if (winPtr->window != None) {
	return;			/* Window already exists. */
    }
#ifdef notdef
    if ((winPtr->parentPtr == NULL) || (winPtr->flags & TK_TOP_LEVEL)) {
	parent = XRootWindow(winPtr->display, winPtr->screenNum);
	/* TODO: Make the entire screen busy */
    } else {
	if (Tk_WindowId(winPtr->parentPtr) == None) {
	    Tk_MakeWindowExist((Tk_Window)winPtr->parentPtr);
	}
    }
#endif

    /* Create a transparent window and put it on top.  */
    /*
     * Ignore the important events while the window is mapped.
     */
    winPtr->atts.do_not_propagate_mask = (KeyPressMask | KeyReleaseMask |
	ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    winPtr->atts.event_mask = (KeyPressMask | KeyReleaseMask |
	ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    winPtr->changes.border_width = 0;
    winPtr->depth = 0;
    winPtr->window = XCreateWindow(winPtr->display, parent,
	winPtr->changes.x, winPtr->changes.y,
	(unsigned)winPtr->changes.width,	/* width */
	(unsigned)winPtr->changes.height,	/* height */
	0,			/* border_width */
	0,			/* depth */
	InputOnly,		/* class */
	CopyFromParent,		/* visual */
	(CWDontPropagate | CWEventMask),	/* valuemask */
	&(winPtr->atts) /* attributes */ );

    dispPtr = winPtr->dispPtr;
    hPtr = Tcl_CreateHashEntry(&(dispPtr->winTable), (char *)winPtr->window,
	&notUsed);
    Tcl_SetHashValue(hPtr, winPtr);

    winPtr->dirtyAtts = 0;
    winPtr->dirtyChanges = 0;
#ifdef TK_USE_INPUT_METHODS
    winPtr->inputContext = NULL;
#endif /* TK_USE_INPUT_METHODS */

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	/*
	 * If any siblings higher up in the stacking order have already
	 * been created then move this window to its rightful position
	 * in the stacking order.
	 *
	 * NOTE: this code ignores any changes anyone might have made
	 * to the sibling and stack_mode field of the window's attributes,
	 * so it really isn't safe for these to be manipulated except
	 * by calling Tk_RestackWindow.
	 */

	for (winPtr2 = winPtr->nextPtr; winPtr2 != NULL; 
	     winPtr2 = winPtr2->nextPtr) {
	    if ((winPtr2->window != None) && !(winPtr2->flags & TK_TOP_LEVEL)) {
		XWindowChanges changes;
		changes.sibling = winPtr2->window;
		changes.stack_mode = Below;
		XConfigureWindow(winPtr->display, winPtr->window,
		    CWSibling | CWStackMode, &changes);
		break;
	    }
	}
    }

    /*
     * Issue a ConfigureNotify event if there were deferred configuration
     * changes (but skip it if the window is being deleted;  the
     * ConfigureNotify event could cause problems if we're being called
     * from Tk_DestroyWindow under some conditions).
     */

    if ((winPtr->flags & TK_NEED_CONFIG_NOTIFY)
	&& !(winPtr->flags & TK_ALREADY_DEAD)) {
	winPtr->flags &= ~TK_NEED_CONFIG_NOTIFY;
	DoConfigureNotify((Tk_FakeWin *) tkwin);
    }
}

void
Blt_SetWindowInstanceData(tkwin, instanceData)
    Tk_Window tkwin;
    ClientData instanceData;
{
    struct TkWindow *winPtr = (struct TkWindow *)tkwin;

    winPtr->instanceData = instanceData;
}

ClientData 
Blt_GetWindowInstanceData(tkwin)
    Tk_Window tkwin;
{
    struct TkWindow *winPtr = (struct TkWindow *)tkwin;

    return winPtr->instanceData;
}

void
Blt_DeleteWindowInstanceData(tkwin)
    Tk_Window tkwin;
{
}
