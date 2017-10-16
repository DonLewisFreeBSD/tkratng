/* 
 * ratBusy.c --
 *
 *	Interface to the blt_busy stuff.
 *
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"

static int busyCount = 0;
static Tcl_Obj *childrenPtr = NULL;
static Tcl_Obj *winfoCmdPtr = NULL;
static Tcl_Obj *updateCmdPtr = NULL;
static Tcl_Obj *balloonCmdPtr = NULL;
static Tcl_Obj *trueObjPtr = NULL;
static Tcl_Obj *falseObjPtr = NULL;


/*
 *----------------------------------------------------------------------
 *
 * RatSetBusy --
 *
 *      Makes the interface busy by changing the cursor etc.
 *	This function can be called multiple times and it will stack.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatSetBusy(Tcl_Interp *interp)
{
    int objc, i;
    Tcl_Obj **objv, *argv[2];
    char buf[1024];

    if (0 < busyCount++) {
	return;
    }
    if (NULL == balloonCmdPtr) {
	balloonCmdPtr = Tcl_NewStringObj("rat_balloon::SetIgnore", -1);
	Tcl_IncrRefCount(balloonCmdPtr);
	trueObjPtr = Tcl_NewBooleanObj(1);
	Tcl_IncrRefCount(trueObjPtr);
	falseObjPtr = Tcl_NewBooleanObj(0);
	Tcl_IncrRefCount(falseObjPtr);
    }
    argv[0] = balloonCmdPtr;
    argv[1] = trueObjPtr;
    i = Tcl_EvalObjv(interp, 2, argv, 0);
    if (NULL == winfoCmdPtr) {
	winfoCmdPtr = Tcl_NewStringObj("winfo children .", -1);
	Tcl_IncrRefCount(winfoCmdPtr);
	updateCmdPtr = Tcl_NewStringObj("update idletasks", -1);
	Tcl_IncrRefCount(updateCmdPtr);
    }
    if (TCL_OK == Tcl_EvalObjEx(interp, winfoCmdPtr, 0)) {
	childrenPtr = Tcl_GetObjResult(interp);
    } else {
	childrenPtr = Tcl_NewObj();
    }
    Tcl_IncrRefCount(childrenPtr);

    Tcl_ListObjGetElements(interp, childrenPtr, &objc, &objv);
    for (i=0; i<objc; i++) {
	snprintf(buf, sizeof(buf), "blt_busy hold %s\n",
		 Tcl_GetString(objv[i]));
	if (TCL_OK != Tcl_Eval(interp, buf)) {
	    fprintf(stderr, "blt_busy hold failed: %s\n",
		    Tcl_GetStringResult(interp));
	}
    }
    Tcl_EvalObjEx(interp, updateCmdPtr, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * RatClearBusy --
 *
 *      Unmakes the interface busy by changing the cursor etc.
 *	This function can be called multiple times and it will stack.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatClearBusy(Tcl_Interp *interp)
{
    int objc, i;
    Tcl_Obj **objv, *argv[2];
    char buf[1024];
    
    if (0 < --busyCount) {
	return;
    }

    Tcl_ListObjGetElements(interp, childrenPtr, &objc, &objv);
    for (i=0; i<objc; i++) {
	snprintf(buf, sizeof(buf), "blt_busy release %s\n",
		 Tcl_GetString(objv[i]));
	Tcl_Eval(interp, buf);
    }
    Tcl_DecrRefCount(childrenPtr);
    if (NULL != balloonCmdPtr) {
	argv[0] = balloonCmdPtr;
	argv[1] = falseObjPtr;
	Tcl_EvalObjv(interp, 2, argv, 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatBusyCmd --
 *
 *      Implements the Busy command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatBusyCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		       Tcl_Obj *const objv[])
{
    Tcl_Obj *rPtr;
    int r;
    
    if (objc != 2) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), " cmd",
			 (char*) NULL);
	return TCL_ERROR;
    }

    RatSetBusy(interp);
    r = Tcl_EvalObj(interp, objv[1]);
    rPtr = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(rPtr);
    RatClearBusy(interp);

    Tcl_SetObjResult(interp, rPtr);
    Tcl_DecrRefCount(rPtr);
    return r;
}
