/* 
 * ratHold.c --
 *
 *	Manages different holds of messages.
 *
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <pwd.h>
#include <signal.h>
#include "rat.h"
#include <locale.h>

static int RatoldAppInit(Tcl_Interp *interp);
static Tcl_ObjCmdProc RatHoldCmd;
static int RatHoldList(Tcl_Interp *interp, const char *dir,
		       Tcl_Obj *fileListPtr);
static int RatHoldExtract(Tcl_Interp *interp, const char *prefix,
			  Tcl_Obj *usedArraysPtr, Tcl_Obj *filesPtr);

int Ratold_Init(Tcl_Interp *interp)
{
    RatoldAppInit(interp);
    return Tcl_PkgProvide(interp, "ratatosk_old", VERSION);
}
int Ratold_SafeInit(Tcl_Interp *interp)
{
    RatoldAppInit(interp);
    return Tcl_PkgProvide(interp, "ratatosk_old", VERSION);
}


/*
 *----------------------------------------------------------------------
 *
 * RatAppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in the result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

static int
RatoldAppInit(Tcl_Interp *interp)
{
    Tcl_CreateObjCommand(interp, "RatHold", RatHoldCmd, NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatHoldCmd --
 *
 *	See ../doc/interface
 *
 * Results:
 *      A standard tcl result.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatHoldCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    static Tcl_Obj *fileListPtr = NULL;
    char buf[1024];
    const char *holdDir;
    Tcl_Obj *oPtr;
    int index;

    if (objc < 3) goto usage;
    holdDir = RatTranslateFileName(interp, Tcl_GetString(objv[1]));

    if (!strcmp(Tcl_GetString(objv[2]), "list")) {

	if (fileListPtr) {
	    Tcl_DecrRefCount(fileListPtr);
	}
	fileListPtr = Tcl_NewObj();
	return RatHoldList(interp, holdDir, fileListPtr);

    } else if (!strcmp(Tcl_GetString(objv[2]), "extract")) {
	if (objc != 4
	    || TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &index)) {
	    goto usage;
	} else if (!fileListPtr) {
	    Tcl_SetResult(interp,"You must list the content of the hold first",
			  TCL_STATIC);
	    return TCL_ERROR;
	} else {
	    Tcl_ListObjIndex(interp, fileListPtr, index, &oPtr);
	    snprintf(buf, sizeof(buf), "%s/%s", holdDir, Tcl_GetString(oPtr));
	    return RatHoldExtract(interp, buf, NULL, NULL);
	}
    }

 usage:
    Tcl_AppendResult(interp, "Usage error of \"", Tcl_GetString(objv[0]), "\"",
		     (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RatHoldList --
 *
 *	List the content of the specified hold directory.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	The fileListPtr argument will be used to hold the return list
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatHoldList(Tcl_Interp *interp, const char *dir, Tcl_Obj *fileListPtr)
{
    struct dirent *direntPtr;
    char buf[1024];
    DIR *dirPtr;
    FILE *fPtr;
    Tcl_Obj *oPtr = Tcl_NewObj();
    int l;

    if (NULL == (dirPtr = opendir(dir))) {
	snprintf(buf, sizeof(buf), "Failed to open %s: %s",
		dir, Tcl_PosixError(interp));
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_ERROR;
    }
    while (0 != (direntPtr = readdir(dirPtr))) {
	l = strlen(direntPtr->d_name);
	if (   'd' == direntPtr->d_name[l-4]
	    && 'e' == direntPtr->d_name[l-3]
	    && 's' == direntPtr->d_name[l-2]
	    && 'c' == direntPtr->d_name[l-1]) {
	    snprintf(buf, sizeof(buf), "%s/%s", dir, direntPtr->d_name);
	    fPtr = fopen(buf, "r");
	    fgets(buf, sizeof(buf), fPtr);
	    fclose(fPtr);
	    buf[strlen(buf)-1] = '\0';
	    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewStringObj(buf, -1));
	    snprintf(buf, sizeof(buf), direntPtr->d_name);
	    if (fileListPtr) {
		Tcl_ListObjAppendElement(interp, fileListPtr,
					 Tcl_NewStringObj(buf, strlen(buf)-5));
	    }
	}
    }
    closedir(dirPtr);
    Tcl_SetObjResult(interp, oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatHoldExtract --
 *
 *	Extract a specified held message.
 *
 * Results:
 *      A standard Tcl result. The handler of the new message will be left
 *	in the result area.
 *
 * Side effects:
 *	The content of holdId will be modified.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatHoldExtract(Tcl_Interp *interp, const char *prefix,
	       Tcl_Obj *usedArraysPtr, Tcl_Obj *filesPtr)
{
    static int holdId = 0;
    Tcl_Channel ch;
    char buf[1024], *cPtr;
    int i, oldId;
    Tcl_Obj *fPtr = Tcl_NewObj(), *oPtr;

    /*
     * We start by reading the file. This is complicated by the fact that
     * the file is encoded in utf-8.
     */
    if (NULL == (ch = Tcl_OpenFileChannel(interp, (char*)prefix, "r", 0))) {
	return TCL_ERROR;
    }
    Tcl_SetChannelOption(interp, ch, "-encoding", "utf-8");
    i = Tcl_Seek(ch, 0, SEEK_END);
    Tcl_Seek(ch, 0, SEEK_SET);
    Tcl_ReadChars(ch, fPtr, i, 0);
    Tcl_Close(interp, ch);

    /*
     * Now we should eval the data, first we set the holdId variable
     * so that the file can generate unique handlers, after the read
     * we remember a new value of holdId. Then we get the right entry
     * in the list of files.
     */
    oldId = holdId;
    sprintf(buf, "%d", holdId);
    Tcl_SetVar(interp, "holdId", buf, 0);
    Tcl_IncrRefCount(fPtr);
    Tcl_EvalObjEx(interp, fPtr, TCL_EVAL_DIRECT);
    Tcl_DecrRefCount(fPtr);
    sprintf(buf, "hold%d", holdId);
    if (NULL == Tcl_GetVar2Ex(interp, buf, "role", 0)) {
	oPtr = Tcl_GetVar2Ex(interp, "option", "default_role",TCL_GLOBAL_ONLY);
	Tcl_SetVar2Ex(interp, buf, "role", oPtr, 0);
    }
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    oPtr = Tcl_GetVar2Ex(interp, "holdId", NULL, 0);
    Tcl_GetIntFromObj(interp, oPtr, &holdId);
    if (usedArraysPtr) {
	for (i=oldId; i<holdId; i++) {
	    sprintf(buf, "hold%d", i);
	    Tcl_ListObjAppendElement(interp, usedArraysPtr,
				     Tcl_NewStringObj(buf, -1));
	}
    }
    snprintf(buf, sizeof(buf), "%s.desc", prefix);
    if (filesPtr) {
	Tcl_ListObjAppendElement(interp, filesPtr,Tcl_NewStringObj(prefix,-1));
	Tcl_ListObjAppendElement(interp, filesPtr, Tcl_NewStringObj(buf, -1));
    } else {
	unlink(prefix);
	unlink(buf);
    }
    fflush(stderr);

    strlcpy(buf, prefix, sizeof(buf));
    if ((cPtr = strrchr(buf, '/'))) *cPtr = '\0';
    return TCL_OK;
}
