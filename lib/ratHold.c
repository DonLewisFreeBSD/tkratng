/* 
 * ratHold.c --
 *
 *	Manages different holds of messages.
 *
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <pwd.h>
#include <signal.h>
#include "rat.h"
#include <locale.h>


/*
 * Parts of the message and body handlers which are saved in the hold.
 */
static char *holdMessageParts[] = {"remail", "date", "from",
	"reply_to", "subject", "to", "cc", "bcc", "in_reply_to", "message_id",
	"save_to", "request_dsn", "role", "other_tags", (char *) NULL};
static char *holdBodyParts[] = {"type", "subtype", "encoding", "parameter",
	"id", "description", "charset", "filename", "removeFile", "pgp_sign",
	"pgp_encrypt", "disp_type", "disp_parm", (char *) NULL};

/*
 * Keeps track of number of held and deferred messages
 */
static int numHeld, numDeferred;

/*
 * Save bodyparts during hold
 */
static int RatHoldBody(Tcl_Interp *interp, FILE *fPtr, char *baseName,
	char *handler, char **listValue, int *listValueSize, int intId);

/*
 *----------------------------------------------------------------------
 *
 * RatHold --
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

int
RatHold(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    static Tcl_Obj *fileListPtr = NULL;
    char buf[1024];
    const char *holdDir;
    Tcl_Obj *oPtr;
    int index;

    if (objc < 2) goto usage;
    if (NULL == (holdDir = RatGetPathOption(interp, "hold_dir"))
	|| (0 != mkdir(holdDir, DIRMODE) && EEXIST != errno)) {
	Tcl_AppendResult(interp, "error creating directory \"", holdDir,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    if (!strcmp(Tcl_GetString(objv[1]), "insert")) {
	if (objc != 4) goto usage;
	return RatHoldInsert(interp, holdDir, Tcl_GetString(objv[2]),
			     Tcl_GetString(objv[3]));

    } else if (!strcmp(Tcl_GetString(objv[1]), "list")) {

	if (fileListPtr) {
	    Tcl_DecrRefCount(fileListPtr);
	}
	fileListPtr = Tcl_NewObj();
	return RatHoldList(interp, holdDir, fileListPtr);

    } else if (!strcmp(Tcl_GetString(objv[1]), "extract")) {
	if (objc != 3
	    || TCL_OK != Tcl_GetIntFromObj(interp, objv[2], &index)) {
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
 * RatHoldBody --
 *
 *	Saves a bodypart into the file specified by fPtr. The handler of
 *	the bodypart is passed in the handler argument. We also get a
 *	listValue buffer and the length of it which we may use when
 *	converting list elements. The last argument is an internal counter
 *	which use is explained below. The baseName argument is a string
 *	which contains the first part of the name of any new files. The
 *	new value of intId is returned.
 *
 *	The goal of this function is to generate code which recreates
 *	the structure we are passed, but with another set of handlers.
 *	The handlers are on the form holdX, where X is a number. To get
 *	unique X'es we use the holdId variable which is incremented
 *	every time we need a new handler. When we enter this routine
 *	holdId has a good value. The problem is that this bodypart
 *	must include references to all its children, and we don't know
 *	which id's the children are going to get (they may create their
 *	own children). This is solved by storing the name of the reference
 *	variable in another variable holdRefX where the number X is taken
 *	from the intId argument. Then we can add code so that each child
 *	adds itself to the reference list.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The size of the space allocated in listValue may be increased,
 *	this is mirrored in the listValueSize argument.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatHoldBody(Tcl_Interp *interp, FILE *fPtr, char *baseName, char *handler,
	    char **listValue, int *listValueSize, int intId)
{
    int i, newSize, flags, objc;
    const char *value;
    Tcl_Obj *vPtr, **objv;

    /*
     * Write standard part
     */
    fprintf(fPtr, "global hold${holdId}\n");
    for (i=0; holdBodyParts[i]; i++) {
	if ((value = Tcl_GetVar2(interp, handler, holdBodyParts[i],
		TCL_GLOBAL_ONLY))) {
	    if ((newSize = Tcl_ScanElement(value, &flags)) > *listValueSize) {
		*listValueSize = newSize+1;
		*listValue = (char*) ckrealloc(*listValue, *listValueSize);
	    }
	    Tcl_ConvertElement(value, *listValue, flags);
	    fprintf(fPtr, "set hold${holdId}(%s) %s\n", holdBodyParts[i],
		    *listValue);
	}
    }
    /*
     * Handle children (if any)
     */
    if ((vPtr = Tcl_GetVar2Ex(interp, handler, "children", TCL_GLOBAL_ONLY))) {
	int myId = intId;

	Tcl_ListObjGetElements(interp, vPtr, &objc, &objv);
	fprintf(fPtr, "set holdRef%d hold${holdId}(children)\n", intId);
	fprintf(fPtr, "incr holdId\n");
	for (i=0; i<objc; i++) {
	    fprintf(fPtr, "lappend $holdRef%d hold${holdId}\n", myId);
	    intId = RatHoldBody(interp, fPtr, baseName, Tcl_GetString(objv[i]),
				listValue, listValueSize, intId+1);
	    if (0 > intId) {
		return -1;
	    }
	}
	fprintf(fPtr, "unset holdRef%d\n", myId);
    } else {
	fprintf(fPtr, "incr holdId\n");
    }

    return intId;
}

/*
 *----------------------------------------------------------------------
 *
 * RatHoldInsert --
 *
 *	Inserts a message into the specified hold directory. The directory
 *	will be created if it doesn't exist.
 *
 * Results:
 *      A standard Tcl result. the result area will contain the name of the
 *	newly inserted entry (if everything went ok).
 *
 * Side effects:
 *	The hold on disk may obviously be modified.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatHoldInsert(Tcl_Interp *interp, const char *dir, char *handler,
	      const char *description)
{
    int listValueSize = 0, newSize, flags, i, result = 0;
    char baseName[1024], buf[1024];
    char *listValue = NULL;
    struct stat sbuf;
    const char *value;
    FILE *fPtr;

    i = 0;
    do {
	snprintf(baseName, sizeof(baseName), "%s/%s_%x_%xM",
		dir, Tcl_GetHostName(), (unsigned int)getpid(), i++);
    } while(!stat(baseName, &sbuf));

    /*
     * Write description file
     */
    snprintf(buf, sizeof(buf), "%s.desc", baseName);
    if (NULL == (fPtr = fopen(buf, "w"))) {
	Tcl_AppendResult(interp, "error creating file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    (void)fprintf(fPtr, "%s\n", description);
    (void)fclose(fPtr);

    /*
     * Write main file
     */
    if (NULL == (fPtr = fopen(baseName, "w"))) {
	Tcl_AppendResult(interp, "error creating file \"", baseName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    fprintf(fPtr, "global hold${holdId}\n");
    for (i=0; holdMessageParts[i]; i++) {
	if ((value = Tcl_GetVar2(interp, handler, holdMessageParts[i],
		TCL_GLOBAL_ONLY))) {
	    if ((newSize = Tcl_ScanElement(value, &flags)) > listValueSize){
		listValueSize = newSize+1;
		listValue = (char*) ckrealloc(listValue, listValueSize);
	    }
	    Tcl_ConvertElement(value, listValue, flags);
	    fprintf(fPtr, "set hold${holdId}(%s) %s\n", holdMessageParts[i],
		    listValue);
	}
    }

    snprintf(buf, sizeof(buf), "%s tag ranges noWrap",
	    Tcl_GetVar2(interp, handler, "composeBody", TCL_GLOBAL_ONLY));
    Tcl_Eval(interp, buf);
    if ((newSize = Tcl_ScanElement(Tcl_GetStringResult(interp),
	    &flags)) > listValueSize){
	listValueSize = newSize+1;
	listValue = (char*) ckrealloc(listValue, listValueSize);
    }
    Tcl_ConvertElement(Tcl_GetStringResult(interp), listValue, flags);
    fprintf(fPtr, "set hold${holdId}(tag_range) %s\n", listValue);

    if ((value = Tcl_GetVar2(interp, handler, "body", TCL_GLOBAL_ONLY))) {
	fprintf(fPtr, "set hold${holdId}(body) hold[incr holdId]\n");
	result = RatHoldBody(interp, fPtr, baseName, (char*)value, &listValue, 
			     &listValueSize, 0);
    }
    ckfree(listValue);
    if ( 0 > fprintf(fPtr, "\n") || 0 != fclose(fPtr) || 0 > result) {
	struct dirent *direntPtr;
	DIR *dirPtr;
	char *cPtr;

	(void)fclose(fPtr);
	for (cPtr = baseName+strlen(baseName)-1; *cPtr != '/'; cPtr--);
	*cPtr++ = '\0';
	dirPtr = opendir(dir);
	while (0 != (direntPtr = readdir(dirPtr))) {
	    if (!strncmp(direntPtr->d_name, cPtr, strlen(cPtr))) {
		snprintf(buf, sizeof(buf),"%s/%s", baseName,direntPtr->d_name);
		(void)unlink(buf);
	    }
	}
	closedir(dirPtr);

	Tcl_AppendResult(interp, "error writing files: ",
		Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, baseName, TCL_VOLATILE);
    RatHoldUpdateVars(interp, dir, 1);
    return TCL_OK;
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

int
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

int
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
    RatHoldUpdateVars(interp, buf, -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatHoldInitVars --
 *
 *	Initialize some hold variables
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Some global variables are initialized
 *
 *----------------------------------------------------------------------
 */

void
RatHoldInitVars(Tcl_Interp *interp)
{
    const char *dir;

    if (NULL != (dir = RatGetPathOption(interp, "send_cache"))) {
	RatHoldList(interp, dir, NULL);
	Tcl_ListObjLength(interp, Tcl_GetObjResult(interp), &numDeferred);
    }

    numHeld = 0;
    if (NULL != (dir = RatGetPathOption(interp, "hold_dir"))) {
	if (TCL_OK == RatHoldList(interp, dir, NULL)) {
	    Tcl_ListObjLength(interp, Tcl_GetObjResult(interp), &numHeld);
	}
    }

    Tcl_SetVar2Ex(interp, "numDeferred", NULL, Tcl_NewIntObj(numDeferred),
	    TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "numHeld", NULL, Tcl_NewIntObj(numHeld),
	    TCL_GLOBAL_ONLY);
}

/*
 *----------------------------------------------------------------------
 *
 * RatHoldUpdateVars --
 *
 *	Update the hold variables
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Some global variables may be modified
 *
 *----------------------------------------------------------------------
 */

void
RatHoldUpdateVars(Tcl_Interp *interp, const char *dir, int diff)
{
    const char *senddir, *varname;
    int *intvar;

    dir = cpystr(dir);
    senddir = RatGetPathOption(interp, "send_cache");
    if (senddir && !strcmp(dir, senddir)) {
	varname = "numDeferred";
	intvar = &numDeferred;
    } else {
	varname = "numHeld";
	intvar = &numHeld;
    }
    ckfree(dir);

    *intvar += diff;
    Tcl_SetVar2Ex(interp, (char*)varname, NULL, Tcl_NewIntObj(*intvar),
		  TCL_GLOBAL_ONLY);
}
