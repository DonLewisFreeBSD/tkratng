/*
 * ratFolder.c --
 *
 *      This file contains basic support code for the folder commands. Each
 *      folder type is created using an unique command. This command returns
 *      a folder handler, which when invoked calls the RatFolderCmd()
 *      procedure with a pointer to a RatFolderInfo structure as clientData.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notices is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"

/*
 * This structure is used to hold the data while sorting
 */
typedef struct SortData {
    char *msgid;
    char *ref;
    char *subject;
    char *sender;
    time_t date;
	long size;
    struct SortData *nextPtr;
    int prev, next, child, parent;
    Tcl_Obj *tPtr;
} SortData;

/* 
 * Global list of folders
 */
RatFolderInfo *ratFolderList = NULL;

/*
 * The number of folders opened. This is used when making
 * the folder entities.
 */
static int numFolders = 0;

/*
 * Sort order names
 */
struct {
    SortOrder order;
    int reverse;
    char *name;
} sortNames[] = {
    {SORT_THREADED,	0, "threaded"},
    {SORT_SUBJDATE,	0, "subject"},
    {SORT_SENDERDATE,	0, "sender"},
    {SORT_SUBJECT,	0, "subjectonly"},
    {SORT_SENDER,	0, "senderonly"},
    {SORT_DATE,		0, "date"},
    {SORT_NONE,		0, "folder"},
    {SORT_NONE,		1, "reverseFolder"},
    {SORT_DATE,		1, "reverseDate"},
    {SORT_SIZE,		0, "size"},
    {SORT_SIZE,		1, "reverseSize"},
    {0,			0, NULL}
};

/*
 * Flag names
 * The entries in this list must be synchronized with the enum type RatFlag
 * defined in RatFolder.h
 */
flag_name_t flag_name[] = {
    { "\\Seen",	    "seen", 'R' },
    { "\\Deleted",  "deleted", 'D' },
    { "\\Flagged",  "flagged", 'F' },
    { "\\Answered", "answered", 'A' },
    { "\\Draft",    "draft", 'T' },
    { "\\Recent",   "recent", '\0' },
    { NULL, NULL, '\0'}
};

/*
 * Global variable used to controll the sorting functions
 */
static SortData *baseSortDataPtr;

/*
 * Global id to handle folder list updates
 */
static int folderChangeId = 0;

/*
 * Possible flag values for remote hosts
 */
#ifdef HAVE_OPENSSL
static char *cClientFlags[] = {
    "/notls", "/ssl", "/novalidate-cert", "/secure", NULL
};
#endif /* HAVE_OPENSSL */

/*
 * Fluff to remove when canonalizing headers (case insensitive)
 */
static char *subjectFluff[] = {
    "re: ", "re ", "fwd: ", "fwd ", "ans: ", "ans ", "sv: ", "sv ", NULL
};

static Tcl_ObjCmdProc RatOpenFolderCmd;
static Tcl_ObjCmdProc RatGetOpenHandlerCmd;
static Tcl_ObjCmdProc RatFolderCmd;
static void RatFolderSort(Tcl_Interp *interp, RatFolderInfo *infoPtr);
static int RatFolderSortCompareDate(const void *arg1, const void *arg2);
static int RatFolderSortCompareSize(const void *arg1, const void *arg2);
static int RatFolderSortCompareSubject(const void *arg1, const void *arg2);
static int RatFolderSortCompareSender(const void *arg1, const void *arg2);
static int IsChild(SortData *dataPtr, int child, int parent);
static int RatFolderSortLinearize(int *p, int n, SortData *dataPtr, int first,
	int depth);
static Tcl_ObjCmdProc RatCreateFolderCmd;
static RatFlag RatFlagNameToInt(const char *name);
static char *RatGetIdentDef(Tcl_Interp *interp, Tcl_Obj *defPtr);

/*
 *----------------------------------------------------------------------
 *
 * RatFolderInit --
 *
 *      Initializes the folder commands.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	The folder creation commands are created in interp. And the
 *	Std folder is initialized.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatFolderInit(Tcl_Interp *interp)
{
    RatInitMessages();
    if (TCL_OK != RatStdFolderInit(interp)) {
	return TCL_ERROR;
    }
    if (TCL_OK != RatDbFolderInit(interp)) {
	return TCL_ERROR;
    }
    if (TCL_OK != RatDisFolderInit(interp)) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, "RatOpenFolder", RatOpenFolderCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGetOpenHandler", RatGetOpenHandlerCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatParseExp", RatParseExpCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGetExp", RatGetExpCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatFreeExp", RatFreeExpCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCreateFolder", RatCreateFolderCmd,
			 (void*)RAT_MGMT_CREATE, NULL);
    Tcl_CreateObjCommand(interp, "RatCheckFolder", RatCreateFolderCmd,
			 (void*)RAT_MGMT_CHECK, NULL);
    Tcl_CreateObjCommand(interp, "RatDeleteFolder", RatCreateFolderCmd,
			 (void*)RAT_MGMT_DELETE, NULL);
    Tcl_CreateObjCommand(interp, "RatSubscribeFolder", RatCreateFolderCmd,
			 (void*)RAT_MGMT_SUBSCRIBE, NULL);
    Tcl_CreateObjCommand(interp, "RatUnSubscribeFolder", RatCreateFolderCmd,
			 (void*)RAT_MGMT_UNSUBSCRIBE, NULL);

    RatFolderUpdateTime((ClientData)interp);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatGetOpenFolder --
 *
 *      Search the list of open folders for a folder matching the given
 *	definition.
 *
 * Results:
 *	If a match is found then that folders reference count is incremented
 *	and the infoPtr is returned. Otherwise NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

RatFolderInfo*
RatGetOpenFolder(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    RatFolderInfo *infoPtr;
    char *def = RatGetIdentDef(interp, defPtr);

    for (infoPtr = ratFolderList;
	 infoPtr && strcmp(infoPtr->ident_def, def);
	 infoPtr = infoPtr->nextPtr);
    if (infoPtr) {
	infoPtr->refCount++;
    }
    return infoPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatOpenFolder --
 *
 *      See the INTERFACE specification
 *
 * Results:
 *      The return value is normally TCL_OK and a foilder handle is left
 *	in the result area; if something goes wrong TCL_ERROR is returned
 *	and an error message will be left in the result area.
 *
 * Side effects:
 *	The folder creation commands are created in interp.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatOpenFolderCmd(ClientData clientData, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[])
{
    RatFolderInfo *infoPtr;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " folderdef\"", (char *) NULL);
	return TCL_ERROR;
    }

    infoPtr = RatOpenFolder(interp, objv[1]);
    if (NULL == infoPtr) {
	Tcl_AppendResult(interp, ": Failed to create folder", NULL);
	return TCL_ERROR;
    } else {
	Tcl_SetResult(interp, infoPtr->cmdName, TCL_VOLATILE);
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatOpenFolder --
 *
 *      See the INTERFACE specification
 *
 * Results:
 *      The return value is normally TCL_OK and a foilder handle is left
 *	in the result area; if something goes wrong TCL_ERROR is returned
 *	and an error message will be left in the result area.
 *
 * Side effects:
 *	The folder creation commands are created in interp.
 *
 *
 *----------------------------------------------------------------------
 */

RatFolderInfo*
RatOpenFolder(Tcl_Interp *interp, Tcl_Obj *def)
{
    RatFolderInfo *infoPtr;
    int i, fobjc, lobjc;
    CONST84 char *sortName = NULL;
    Tcl_Obj **fobjv, **lobjv, *role = NULL;

    /*
     * Check open folders
     */
    if ((infoPtr = RatGetOpenFolder(interp, def))) {
	return infoPtr;
    }
    Tcl_ListObjGetElements(interp, def, &fobjc, &fobjv);

    if (!strcmp(Tcl_GetString(fobjv[1]), "dbase")) {
	infoPtr = RatDbFolderCreate(interp, def);
    } else if (!strcmp(Tcl_GetString(fobjv[1]), "dis")) {
	infoPtr = RatDisFolderCreate(interp, def);
    } else {
	infoPtr = RatStdFolderCreate(interp, def);
    }
    if (NULL == infoPtr) {
	return NULL;
    }
    Tcl_ListObjGetElements(interp, fobjv[2], &lobjc, &lobjv);
    for (i=0; i < lobjc; i+=2) {
	if (!strcmp("sort", Tcl_GetString(lobjv[i]))) {
	    sortName = Tcl_GetString(lobjv[i+1]);
	}
	if (!strcmp("role", Tcl_GetString(lobjv[i]))) {
	    role = lobjv[i+1];
	}
    }
    infoPtr->ident_def = cpystr(RatGetIdentDef(interp, def));
    ckfree(infoPtr->name);
    infoPtr->name = cpystr(Tcl_GetString(fobjv[0]));
    infoPtr->refCount = 1;
    if (!sortName || !strcmp("default", sortName)) {
	sortName = Tcl_GetVar2(interp, "option","folder_sort",TCL_GLOBAL_ONLY);
    }
    for (i=0; sortNames[i].name && strcmp(sortNames[i].name, sortName); i++);
    if (sortNames[i].name) {
	infoPtr->sortOrder = sortNames[i].order;
	infoPtr->reverse = sortNames[i].reverse;
    } else {
	infoPtr->sortOrder = SORT_NONE;
	infoPtr->reverse = 0;
    }
    if (!role || !strcmp("default", Tcl_GetString(role))) {
	role = Tcl_NewObj();
    }
    infoPtr->role = role;
    Tcl_IncrRefCount(infoPtr->role);
    infoPtr->sortOrderChanged = 0;
    infoPtr->cmdName = ckalloc(16);
    infoPtr->allocated = infoPtr->number;
    infoPtr->msgCmdPtr = (char **) ckalloc(infoPtr->allocated*sizeof(char*));
    infoPtr->privatePtr = (ClientData**)ckalloc(
	    infoPtr->allocated*sizeof(ClientData));
    for (i=0; i<infoPtr->allocated; i++) {
	infoPtr->msgCmdPtr[i] = (char *) NULL;
	infoPtr->privatePtr[i] = (ClientData*) NULL;
    }
    (*infoPtr->initProc)(infoPtr, interp, -1);
    infoPtr->presentationOrder = (int*)ckalloc(infoPtr->allocated*sizeof(int));
    infoPtr->flagsChanged = 0;
    infoPtr->nextPtr = ratFolderList;
    if (infoPtr->finalProc) {
	(*infoPtr->finalProc)(infoPtr, interp);
    }
    ratFolderList = infoPtr;
    RatFolderSort(interp, infoPtr);
    sprintf(infoPtr->cmdName, "RatFolder%d", numFolders++);
    Tcl_CreateObjCommand(interp, infoPtr->cmdName, RatFolderCmd,
    	    (ClientData) infoPtr, (Tcl_CmdDeleteProc *) NULL);
    Tcl_SetVar2Ex(interp, "folderExists", infoPtr->cmdName,
	    Tcl_NewIntObj(infoPtr->number), TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "folderRecent", infoPtr->cmdName,
	    Tcl_NewIntObj(infoPtr->recent), TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "folderUnseen", infoPtr->cmdName,
	    Tcl_NewIntObj(infoPtr->unseen), TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "folderChanged", infoPtr->cmdName,
	    Tcl_NewIntObj(++folderChangeId), TCL_GLOBAL_ONLY);
    return infoPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetOpenHandlerCmd --
 *
 *      See the INTERFACE specification
 *
 * Results:
 *      The return value is normally TCL_OK. If there is an open folder
 *      for the given definition then the handler to that folder is
 *      stored in the result area. Otherwise an empty string is resturned.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatGetOpenHandlerCmd(ClientData clientData, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[])
{
    RatFolderInfo *infoPtr;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " folderdef\"", (char *) NULL);
	return TCL_ERROR;
    }

    if ((infoPtr = RatGetOpenFolder(interp, objv[1]))) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(infoPtr->cmdName, -1));
    } else {
	Tcl_ResetResult(interp);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatFolderCmd --
 *
 *      Main folder entity procedure. This procedure implements the
 *	folder commands mentioned in ../doc/interface. In order to make
 *	this a tad easier it uses the procedures defined in the
 *	RatFolderInfo structure :-)
 *
 * Results:
 *      Depends on the input :-)
 *
 * Side effects:
 *	The specified folder may be modified.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatFolderCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	     Tcl_Obj *const objv[])
{
    RatFolderInfo *infoPtr = (RatFolderInfo*) clientData;
    Tcl_Obj *oPtr;
    int r;

    if (objc < 2) goto usage;

    if (!strcmp(Tcl_GetString(objv[1]), "update")) {
	RatUpdateType mode;

	if (objc != 3) goto usage;
	if (!strcmp(Tcl_GetString(objv[2]), "update")) {
	    mode = RAT_UPDATE;
	} else if (!strcmp("checkpoint", Tcl_GetString(objv[2]))) {
	    if (!infoPtr->flagsChanged) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
		return TCL_OK;
	    }
	    mode = RAT_CHECKPOINT;
	    infoPtr->flagsChanged = 0;
	} else if (!strcmp(Tcl_GetString(objv[2]), "sync")) {
	    mode = RAT_SYNC;
	} else {
	    goto usage;
	}
	return RatUpdateFolder(interp, infoPtr, mode);

    } else if (!strcmp(Tcl_GetString(objv[1]), "close")) {
	int force = 0;

	if (objc != 2 &&
		(objc != 3
		 || TCL_OK != Tcl_GetBooleanFromObj(interp,objv[2],&force))) {
	    goto usage;
	}
	r = RatFolderClose(interp, infoPtr, force);
	return r;

    } else if (!strcmp(Tcl_GetString(objv[1]), "setName")) {
	if (objc != 3) goto usage;
	ckfree(infoPtr->name);
	infoPtr->name = (char *) ckalloc(strlen(Tcl_GetString(objv[2]))+1);
	strcpy(infoPtr->name, Tcl_GetString(objv[2]));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "info")) {
	int infoArgc;
	CONST84 char *infoArgv[3];
	char numberBuf[16], sizeBuf[16];
	char *list;

	if (objc != 2) goto usage;
	sprintf(numberBuf, "%d", infoPtr->number);
	sprintf(sizeBuf, "%d", infoPtr->size);
	infoArgv[0] = infoPtr->name;
	infoArgv[1] = numberBuf;
	infoArgv[2] = sizeBuf;
	infoArgc = 3;
	list = Tcl_Merge(infoArgc, infoArgv);
	Tcl_SetResult(interp, list, TCL_DYNAMIC);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "list")) {
	ListExpression *exprPtr;
	Tcl_Obj *oPtr, *rPtr;
	int i;

	if (objc != 3) goto usage;
	if (NULL == (exprPtr = RatParseList(Tcl_GetString(objv[2]), NULL))) {
	    Tcl_SetResult(interp, "Illegal list format", TCL_STATIC);
	    goto error;
	}

	rPtr = Tcl_NewObj();
	for (i=0; i < infoPtr->number; i++) {
	    oPtr = RatDoList(interp, exprPtr, infoPtr->infoProc,
		    (ClientData)infoPtr, infoPtr->presentationOrder[i]);
	    Tcl_ListObjAppendElement(interp, rPtr, oPtr);
	}
	RatFreeListExpression(exprPtr);
	Tcl_SetObjResult(interp, rPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "get")) {
	int index;
	char *name;

	if (objc != 3
	    || TCL_OK != Tcl_GetIntFromObj(interp, objv[2],&index)) goto usage;
	if (index < 0 || index >= infoPtr->number) {
	    Tcl_SetResult(interp, "Index is out of bounds", TCL_STATIC);
	    goto error;
	}
	name = RatFolderCmdGet(interp, infoPtr, index);
	Tcl_SetResult(interp, name, TCL_VOLATILE);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "setFlag")) {
	int *ilist, value, i, ic;
	RatFlag flag;
	Tcl_Obj **iv;

	if (objc != 5
	    || TCL_OK != Tcl_GetBooleanFromObj(interp, objv[4], &value)) {
	    goto usage;
	}
	Tcl_ListObjGetElements(interp, objv[2], &ic, &iv);
	ilist = (int*)ckalloc(ic*sizeof(int));
	for (i=0; i<ic; i++) {
	    if (TCL_OK != Tcl_GetIntFromObj(interp, iv[i], &ilist[i])
		|| ilist[i] < 0 || ilist[i] >= infoPtr->number) {
		Tcl_SetResult(interp, "Bad index", TCL_STATIC);
		ckfree(ilist);
		goto error;
	    }
	}
	flag = RatFlagNameToInt(Tcl_GetString(objv[3]));
	RatFolderCmdSetFlag(interp, infoPtr, ilist, ic, flag, value);
	return TCL_OK;
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "getFlag")) {
	int index;
	RatFlag flag;

	if (objc != 4
	    || TCL_OK != Tcl_GetIntFromObj(interp, objv[2],&index)) goto usage;
	if (index < 0 || index >= infoPtr->number) {
	    Tcl_SetResult(interp, "Index is out of bounds", TCL_STATIC);
	    goto error;
	}
	flag = RatFlagNameToInt(Tcl_GetString(objv[3]));
	Tcl_SetObjResult(interp, Tcl_NewIntObj(
		(*infoPtr->getFlagProc)(infoPtr, interp,
		infoPtr->presentationOrder[index],flag)));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "flagged")) {
	RatFlag flag;
	int i, v;

	if (objc != 4
	    || TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &v)) goto usage;
	flag = RatFlagNameToInt(Tcl_GetString(objv[2]));
	oPtr = Tcl_NewObj();
	Tcl_ResetResult(interp);
	for (i=0; i<infoPtr->number; i++) {
	    if ((*infoPtr->getFlagProc)(infoPtr, interp,
		    infoPtr->presentationOrder[i], flag) == v) {
		Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(i));
	    }
	}
	Tcl_SetObjResult(interp, oPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "insert")) {
	Tcl_CmdInfo cmdInfo;
	char **msgv;
	int i;

	if (objc < 3) goto usage;

	for(i=2; i<objc; i++) {
	    if (0 == Tcl_GetCommandInfo(interp,Tcl_GetString(objv[i]),&cmdInfo)
		    || NULL == cmdInfo.objClientData) {
		Tcl_AppendResult(interp, "error \"", Tcl_GetString(objv[i]),
			"\" is not a valid message command", (char *) NULL);
		goto error;
	    }
	}
	msgv = (char**)ckalloc((objc-2)*sizeof(char*));
	for (i=2; i<objc; i++) {
	    msgv[i-2] = Tcl_GetString(objv[i]);
	}

	r = RatFolderInsert(interp, infoPtr, objc-2, msgv);
	ckfree(msgv);
	return r;

    } else if (!strcmp(Tcl_GetString(objv[1]), "type")) {
	Tcl_SetResult(interp, infoPtr->type, TCL_STATIC);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "find")) {
	int msgNo, i;

	if (objc != 3) goto usage;

	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	for (msgNo=0; msgNo < infoPtr->number; msgNo++) {
	    if (infoPtr->msgCmdPtr[msgNo]
	    	    && !strcmp(infoPtr->msgCmdPtr[msgNo],
			       Tcl_GetString(objv[2]))) {
		for (i=0; msgNo != infoPtr->presentationOrder[i]; i++);
		Tcl_SetObjResult(interp, Tcl_NewIntObj(i));
		break;
	    }
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "match")) {
	int i, expId;

	if (objc != 3
	    || TCL_OK != Tcl_GetIntFromObj(interp, objv[2],&expId)) goto usage;

	oPtr = Tcl_NewObj();
	for (i=0; i<infoPtr->number; i++) {
	    if (RatExpMatch(interp, expId, infoPtr->infoProc,
		    (ClientData)infoPtr, infoPtr->presentationOrder[i])) {
		Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(i));
	    }
	}
	Tcl_SetObjResult(interp, oPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "setSortOrder")) {
	CONST84 char *name = Tcl_GetString(objv[2]);
	int i, j;

	if (objc != 3) goto usage;

	if (!strcmp("default", name)) {
	    name = Tcl_GetVar2(interp, "option","folder_sort",TCL_GLOBAL_ONLY);
	}

	for (i=0; sortNames[i].name && strcmp(sortNames[i].name, name); i++);
	if (sortNames[i].name) {
	    /*
	     * If the old sort order was threaded and the new one is not then
	     * we should clear the threading info
	     */
	    if (infoPtr->sortOrder == SORT_THREADED
		    && sortNames[i].order != SORT_THREADED) {
		for (j=0; j<infoPtr->number; j++) {
		    (*infoPtr->setInfoProc)(interp,(ClientData)infoPtr,
			    RAT_FOLDER_THREADING, j, NULL);
		}
	    }
	    infoPtr->sortOrder = sortNames[i].order;
	    infoPtr->reverse = sortNames[i].reverse;
	    infoPtr->sortOrderChanged = 1;
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, "No such sort order", TCL_STATIC);
	    goto error;
	}

    } else if (!strcmp(Tcl_GetString(objv[1]), "getSortOrder")) {
	int i;

	for (i=0; sortNames[i].name; i++) {
	    if (infoPtr->sortOrder == sortNames[i].order
		    && infoPtr->reverse == sortNames[i].reverse) {
		Tcl_SetResult(interp, sortNames[i].name, TCL_STATIC);
	    }
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "netsync")) {
	if (!infoPtr->syncProc) {
	    Tcl_AppendResult(interp, "Operation unsupported on this folder",
		    (char *) NULL);
	    goto error;
	}
	r = (*infoPtr->syncProc)(infoPtr, interp);
	return r;

    } else if (!strcmp(Tcl_GetString(objv[1]), "refcount")) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(infoPtr->refCount));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "role")) {
	Tcl_SetObjResult(interp, infoPtr->role);
	return TCL_OK;
    }

 usage:
    Tcl_AppendResult(interp, "Illegal usage of \"", Tcl_GetString(objv[0]),
		     "\"", (char *) NULL);
 error:
    return TCL_ERROR;

}

/*
 *----------------------------------------------------------------------
 *
 * RatFolderCmd* --
 *
 *      Implement various folder commands
 *
 * Results:
 *	Varies
 *
 * Side effects:
 *	Varies
 *
 *
 *----------------------------------------------------------------------
 */
char*
RatFolderCmdGet(Tcl_Interp *interp, RatFolderInfo *infoPtr, int index)
{
    if (NULL == infoPtr->msgCmdPtr[infoPtr->presentationOrder[index]]) {
	infoPtr->msgCmdPtr[infoPtr->presentationOrder[index]] =
	    (*infoPtr->createProc)(infoPtr, interp,
				   infoPtr->presentationOrder[index]);
    }
    return infoPtr->msgCmdPtr[infoPtr->presentationOrder[index]];
}

void
RatFolderCmdSetFlag(Tcl_Interp *interp, RatFolderInfo *infoPtr, int *ilist,
		    int count, RatFlag flag, int value)
{
    int recent, unseen, i;

    for (i=0; i<count; i++) {
	ilist[i] = infoPtr->presentationOrder[ilist[i]];
    }

    recent = infoPtr->recent;
    unseen = infoPtr->unseen;
    (*infoPtr->setFlagProc)(infoPtr, interp, ilist, count, flag,value);
    infoPtr->flagsChanged = 1;
    if (infoPtr->recent != recent) {
	Tcl_SetVar2Ex(interp, "folderRecent", infoPtr->cmdName,
		      Tcl_NewIntObj(infoPtr->recent), TCL_GLOBAL_ONLY);
    }
    if (infoPtr->unseen != unseen) {
	Tcl_SetVar2Ex(interp, "folderUnseen", infoPtr->cmdName,
		      Tcl_NewIntObj(infoPtr->unseen),
		      TCL_GLOBAL_ONLY);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatFolderSort --
 *
 *      Sorts the folder according to the users wishes. The user may
 *	communicates their will via the folder_sort variable. Currently
 *	The following methods are implemented:
 *	  subjectonly		- Alphabetically on subject
 *	  sender		- Alphabetically on sender name
 *	  folder		- Sorts in native folder order
 *	  reverseFolder		- The reverse of the above
 *	  date			- By date sent
 *	  reverseDate		- By reverse date sent
 *	  subject		- Group messages with the same subject
 *				  and sort the groups by the earliest date
 *				  in each group.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The presentation order member of the RatFolderInfo structure
 *	is initialized. The size of the folder is updated.
 *
 * TODO, convert to Tcl_Obj
 *
 *----------------------------------------------------------------------
 */

static void
RatFolderSort(Tcl_Interp *interp, RatFolderInfo *infoPtr)
{
    int i, j, k, pi, pj, numParm, *tmpPtr, first, last,
	*p=infoPtr->presentationOrder,
	needDate=0, needSubject=0, needSender=0, needIds = 0, needSize = 0,
	*uniqList, uniqListUsed, *subList, *lengthList, newEntry;
    Tcl_HashTable uniqTable;
    Tcl_HashEntry *uniqEntry;
    SortData *dataPtr, *dPtr;
    Tcl_Obj *oPtr;

    if (0 == infoPtr->number) {
	return;
    }
    
    switch(infoPtr->sortOrder) {
        case SORT_NONE:
	    break;
        case SORT_SUBJECT:
	    needSubject = 1;
	    break;
        case SORT_THREADED:
	    needIds = 1;
	    needDate = 1;
	    needSubject = 1;
	    break;
        case SORT_SUBJDATE:
	    needDate = 1;
	    needSubject = 1;
	    break;
        case SORT_SENDER:
	    needSender = 1;
	    break;
        case SORT_SENDERDATE:
	    needDate = 1;
	    needSender = 1;
	    break;
        case SORT_DATE:
	    needDate = 1;
	    break;
        case SORT_SIZE:
            needSize = 1;
            break;
    }

    dataPtr = (SortData*)ckalloc(infoPtr->number*sizeof(*dataPtr));
    infoPtr->size = 0;
    for (i=0; i<infoPtr->number; i++) {
	infoPtr->presentationOrder[i] = i;
	oPtr = (*infoPtr->infoProc)(interp, (ClientData)infoPtr,
				    RAT_FOLDER_TYPE, i);
	if (oPtr && !strcasecmp(Tcl_GetString(oPtr), "multipart/report")) {
	    oPtr = (*infoPtr->infoProc)(interp, (ClientData)infoPtr,
		RAT_FOLDER_PARAMETERS, i);
	    if (!oPtr) {
		continue;
	    }
	    Tcl_ListObjLength(interp, oPtr, &numParm);
	}
	if ((oPtr = (*infoPtr->infoProc)(interp, (ClientData)infoPtr,
					RAT_FOLDER_SIZE,i))) {
	    Tcl_GetIntFromObj(interp, oPtr, &j);
	    infoPtr->size += j;
	}
	if (needSubject) {
	    oPtr = (*infoPtr->infoProc)(interp, (ClientData)infoPtr,
		    RAT_FOLDER_CANONSUBJECT, i);
	    dataPtr[i].subject = Tcl_GetString(oPtr);
	}
	if (needSender) {
	    oPtr = (*infoPtr->infoProc)(interp,(ClientData)infoPtr,
		    RAT_FOLDER_ANAME, i);
	    dataPtr[i].sender = Tcl_GetString(oPtr);
	    dataPtr[i].sender = cpystr(dataPtr[i].sender);
	    lcase((unsigned char*)dataPtr[i].sender);
	}
	if (needDate) {
	    long myLong;
	    oPtr = (*infoPtr->infoProc)(interp,(ClientData)infoPtr,
		    RAT_FOLDER_DATE_N, i);
	    Tcl_GetLongFromObj(interp, oPtr, &myLong);
	    dataPtr[i].date = (time_t) myLong;
	}
	if (needSize) {
	    oPtr = (*infoPtr->infoProc)(interp,(ClientData)infoPtr,
		    RAT_FOLDER_SIZE, i);
	    Tcl_GetLongFromObj(interp, oPtr, &dataPtr[i].size);
	}
	if (needIds) {
	    oPtr = (*infoPtr->infoProc)(interp,(ClientData)infoPtr,
		    RAT_FOLDER_MSGID, i);
	    if (oPtr) {
		dataPtr[i].msgid = Tcl_GetString(oPtr);
	    } else {
		dataPtr[i].msgid = "";
	    }
	    oPtr = (*infoPtr->infoProc)(interp,(ClientData)infoPtr,
		    RAT_FOLDER_REF, i);
	    if (oPtr) {
		dataPtr[i].ref = Tcl_GetString(oPtr);
	    } else {
		dataPtr[i].ref = "";
	    }
	}
    }

    baseSortDataPtr = dataPtr;
    switch (infoPtr->sortOrder) {
    case SORT_NONE:
	for (i=0; i<infoPtr->number; i++) {
	    p[i] = i;
	}
	break;
    case SORT_THREADED:
	for (i=0; i<infoPtr->number; i++) {
	    p[i] = i;
	}
	qsort((void*)p, infoPtr->number, sizeof(int),RatFolderSortCompareDate);
	/*for (i=0; i<infoPtr->number; i++) {
	    printf("Msg: %d (really %d)\n", i, p[i]);
	    printf(" Subj: %s\n", dataPtr[p[i]].subject);
	    printf("MsgId: <%s>\n", dataPtr[p[i]].msgid);
	    printf("  Ref: <%s>\n", (dataPtr[p[i]].ref ?
	    dataPtr[p[i]].ref : "(NULL)"));
	    }*/
	/*
	 * Start by sorting on hard references
	 */
	dataPtr[p[0]].prev = -1;
	dataPtr[p[0]].next = -1;
	dataPtr[p[0]].child = -1;
	dataPtr[p[0]].parent = -1;
	dataPtr[p[0]].tPtr = NULL;
	first = last = p[0];
	for (i=1; i<infoPtr->number; i++) {
	    pi = p[i];
	    dataPtr[pi].tPtr = NULL;
	    dataPtr[pi].child = -1;
	    dataPtr[pi].prev = last;
	    dataPtr[last].next = pi;
	    dataPtr[pi].next = -1;
	    dataPtr[pi].child = -1;
	    dataPtr[pi].parent = -1;
	    last = pi;

	    /* Find any replies to the current message */
	    for (j=i-1; j >= 0 && *dataPtr[pi].msgid; j--) {
		pj = p[j];
		if (!strcmp(dataPtr[pj].ref, dataPtr[pi].msgid)
		    && -1 == dataPtr[pj].parent) {
		    /* Here 'pj' is considered a reply to 'pi' */
		    if (dataPtr[pj].prev != -1) {
			dataPtr[dataPtr[pj].prev].next = dataPtr[pj].next;
		    }
		    if (dataPtr[pj].next != -1) {
			dataPtr[dataPtr[pj].next].prev = dataPtr[pj].prev;
		    }
		    if (first == pj) first = dataPtr[pj].next;
		    if (dataPtr[pi].child != -1) {
			dataPtr[dataPtr[pi].child].prev = pj;
			dataPtr[pj].next = dataPtr[pi].child;
		    } else {
			dataPtr[pj].next = -1;
		    }
		    dataPtr[pi].child = pj;
		    dataPtr[pj].prev = -1;
		    dataPtr[pj].parent = pi;
		}
	    }
	    /* Find message which the current message is a reply to */
	    for (j=0; j<i
		    && -1 == dataPtr[pi].parent
		    && *dataPtr[pi].ref; j++) {
		pj = p[j];
		if (!strcmp(dataPtr[pj].msgid, dataPtr[pi].ref)
			 && !IsChild(dataPtr, pj, pi)) {
		    /* Here 'p' is considered a reply to 'pj' */
		    if (first == pi) {
			if (-1 == dataPtr[first].next) {
			    first = pj;
			} else {
			    first = dataPtr[first].next;
			}
		    }
		    if (last == pi) {
			if (-1 == dataPtr[last].prev) {
			    last = pj;
			} else {
			    last = dataPtr[last].prev;
			}
		    }
		    if (dataPtr[pi].next != -1) {
			dataPtr[dataPtr[pi].next].prev = dataPtr[pi].prev;
		    }
		    if (dataPtr[pi].prev != -1) {
			dataPtr[dataPtr[pi].prev].next = dataPtr[pi].next;
		    }
		    if (-1 != dataPtr[pj].child) {
			for (pj = dataPtr[pj].child; -1 != dataPtr[pj].next;
				pj = dataPtr[pj].next);
			dataPtr[pi].prev = pj;
			dataPtr[pj].next = pi;
		    } else {
			dataPtr[pj].child = pi;
			dataPtr[pi].prev = -1;
		    }
		    dataPtr[pi].parent = pj;
		    dataPtr[pi].next = -1;
		    break;
		}
	    }
	}
	/*
	 * Now we have a number of 'trees' linked by hard references.
	 * Here we try to link the top nodes in all trees by subject.
	 */
	for (i=1; i<infoPtr->number; i++) {
	    pi = p[i];
	    if (-1 != dataPtr[pi].parent) continue;

	    for (j=i-1; j>=0; j--) {
		pj = p[j];
		if (!strcmp(dataPtr[pi].subject, dataPtr[pj].subject)
			&& !IsChild(dataPtr, pj, pi)) {
		    /*
		     * 'pi' is later in the same thread as 'pj'
		     * First we remove it from the list.
		     */
		    dataPtr[dataPtr[pi].prev].next = dataPtr[pi].next;
		    if (dataPtr[pi].next != -1) {
			dataPtr[dataPtr[pi].next].prev = dataPtr[pi].prev;
		    }

		    /*
		     * If the parent to 'pj' also has the same subject
		     * then we add this message after 'pj', otherwise
		     * we add it under 'pj'
		     */
		    for (k=pj; -1 != dataPtr[k].prev; k = dataPtr[k].prev);
		    if (-1 != dataPtr[k].parent && !strcmp(dataPtr[pi].subject,
			    dataPtr[dataPtr[k].parent].subject)) {
			dataPtr[pi].prev = pj;
			dataPtr[pi].next = dataPtr[pj].next;
			if (dataPtr[pj].next != -1) {
			    dataPtr[dataPtr[pj].next].prev = pi;
			}
			dataPtr[pj].next = pi;
		    } else {
			dataPtr[pi].parent = pj;
			dataPtr[pi].prev = -1;
			dataPtr[pi].next = dataPtr[pj].child;
			if (-1 != dataPtr[pj].child) {
			    dataPtr[dataPtr[pj].child].prev = pi;
			    dataPtr[dataPtr[pj].child].parent = -1;
			}
			dataPtr[pj].child = pi;
		    }
		    break;
		}
	    }
	}
	/*printf("First: %d\n", first);
	for (i=0; i<infoPtr->number; i++) {
	    printf("%d:\tprev: %2d  next: %2d  parent: %2d  child: %2d\n",
		    i, dataPtr[i].prev, dataPtr[i].next, dataPtr[i].parent,
		    dataPtr[i].child);
		    }*/
	RatFolderSortLinearize(p, infoPtr->number, dataPtr, first, 0);
	for (i=0; i<infoPtr->number; i++) {
	    (*infoPtr->setInfoProc)(interp,(ClientData)infoPtr,
		    RAT_FOLDER_THREADING, i, dataPtr[i].tPtr);
	}
	break;
    case SORT_SUBJDATE:
    case SORT_SENDERDATE:
	/*
	 * This algorithm is complicated:
	 * - First we build a list of unique subjects in uniqList. Each entry
	 *   in this list contains the index of the first message with this
	 *   subject. The messages are linked with the nextPtr field in
	 *   the SortData structs.
	 * - Then we sort each found subject. This is done by placing the
	 *   indexes of the messages in subList. And sort that. When it
	 *   is sorted we rebuild the subject chains via the nextPtr;
	 * - After that we sort the first message in each subject. This is done
	 *   by reusing the uniqList. We replace each entry in it with a
	 *   pointer to the first entry in the set. Actually we do this in
	 *   the preceding step. Then we sort this list.
	 * - Finally we build to result array.
	 */
	uniqList = (int*)ckalloc(2*infoPtr->number*sizeof(*uniqList));
	Tcl_InitHashTable(&uniqTable, TCL_STRING_KEYS);
	subList = &uniqList[infoPtr->number];
	lengthList = &uniqList[2*infoPtr->number];
	for (i=uniqListUsed=0; i<infoPtr->number; i++) {
	    uniqEntry = Tcl_CreateHashEntry(&uniqTable,
				          infoPtr->sortOrder == SORT_SUBJDATE ?
					  dataPtr[i].subject :
					  dataPtr[i].sender, &newEntry);
	    if (newEntry) {
		dataPtr[i].nextPtr = NULL;
		uniqList[uniqListUsed++] = i;
		Tcl_SetHashValue(uniqEntry, &dataPtr[i]);
	    } else {
		dPtr = Tcl_GetHashValue(uniqEntry);
		dataPtr[i].nextPtr = dPtr->nextPtr;
		dPtr->nextPtr = &dataPtr[i];
	    }
	}
	Tcl_DeleteHashTable(&uniqTable);
	for (i=0; i<uniqListUsed; i++) {
	    if (NULL != dataPtr[uniqList[i]].nextPtr) {
		for (j = 0, dPtr = &dataPtr[uniqList[i]]; dPtr;
			dPtr = dPtr->nextPtr) {
		    subList[j++] = dPtr-dataPtr;
		}
		qsort((void*)subList, j, sizeof(int),RatFolderSortCompareDate);
		for (k=0; k<j-1; k++) {
		    dataPtr[subList[k]].nextPtr = &dataPtr[subList[k+1]];
		}
		dataPtr[subList[k]].nextPtr = NULL;
		uniqList[i] = subList[0];
	    }
	}
	qsort((void*)uniqList, uniqListUsed, sizeof(int),
		RatFolderSortCompareDate);
	for (i=k=0; i<uniqListUsed; i++) {
	    for (dPtr = &dataPtr[uniqList[i]]; dPtr; dPtr = dPtr->nextPtr) {
		p[k++] = dPtr-baseSortDataPtr;
	    }
	}
	ckfree(uniqList);
	break;
    case SORT_SENDER:
	for (i=0; i<infoPtr->number; i++) {
	    p[i] = i;
	}
	qsort((void*)p,infoPtr->number,sizeof(int),RatFolderSortCompareSender);
	break;
    case SORT_SUBJECT:
	for (i=0; i<infoPtr->number; i++) {
	    p[i] = i;
	}
	qsort((void*)p, infoPtr->number, sizeof(int),
	      RatFolderSortCompareSubject);
	break;
    case SORT_DATE:
	for (i=0; i<infoPtr->number; i++) {
	    p[i] = i;
	}
	qsort((void*)p, infoPtr->number, sizeof(int),RatFolderSortCompareDate);
	break;
    case SORT_SIZE:
	for (i=0; i<infoPtr->number; i++) {
	    p[i] = i;
	}
	qsort((void*)p, infoPtr->number, sizeof(int),RatFolderSortCompareSize);
    }

    if (infoPtr->reverse) {
	tmpPtr = (int*)ckalloc(infoPtr->number*sizeof(int));
	for (i=infoPtr->number-1, j=0; i >= 0; i--) {
	    tmpPtr[j++] = p[i];
	}
	memcpy(p, tmpPtr, j*sizeof(int));
	ckfree(tmpPtr);
    } else {
	for (i=j=0; i < infoPtr->number; i++) {
	    p[j++] = p[i];
	}
    }

    /*
     * Cleanup dataPtr
     */
    for (i=0; i<infoPtr->number; i++) {
	if (needSender) {
	    ckfree(dataPtr[i].sender);
	}
    }

    ckfree(dataPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatFolderSortCompare* --
 *
 *	This is the comparison functions used by RatFolderSort. They
 *	expect to get pointers to integers as argumens. The integers
 *	pointed at are indexes into a list of SortData structs which can be
 *	found at the address in baseSortDataPtr.
 *
 * Results:
 *	An integers describing the order of the compared objects.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatFolderSortCompareDate(const void *arg1, const void *arg2)
{
    return baseSortDataPtr[*((int*)arg1)].date
	    - baseSortDataPtr[*((int*)arg2)].date;
}

static int
RatFolderSortCompareSubject(const void *arg1, const void *arg2)
{
    return strcmp(baseSortDataPtr[*((int*)arg1)].subject,
		  baseSortDataPtr[*((int*)arg2)].subject);
}

static int
RatFolderSortCompareSender(const void *arg1, const void *arg2)
{
    return strcmp(baseSortDataPtr[*((int*)arg1)].sender,
		  baseSortDataPtr[*((int*)arg2)].sender);
}

static int
RatFolderSortCompareSize(const void *arg1, const void *arg2)
{
    return baseSortDataPtr[*((int*)arg1)].size
	    - baseSortDataPtr[*((int*)arg2)].size;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFolderCanonalizeSubject --
 *
 * Copy a subject line and remove certain constructs (the re:).
 *
 * Results:
 *	A new object reference
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatFolderCanonalizeSubject(const char *s)
{
    Tcl_Obj *nPtr = Tcl_NewStringObj("", 0);
    const char *e;
    int i, len;

    if (s) {
	/*
	 * We first try to find the start of the actual text, i.e. without any
	 * leading fluff (Re: etc) and whitespaces. If the text starts with
         * [sometext] then also look for fluff immediately after it.
         * Then we find how long the text is (ignore trailing whitespaces)
	 */
	e = s+strlen(s)-1;
	while (*s) {
	    while (*s && isspace((int)*s)) s++;
            for (i=0; subjectFluff[i]; i++) {
                if (!strncasecmp(subjectFluff[i], s, strlen(subjectFluff[i]))){
                    break;
                }
            }
            if (subjectFluff[i]) {
                s += strlen(subjectFluff[i]);
            } else if (*s == '[' && (e = strchr(s+1, ']'))) {
                Tcl_AppendToObj(nPtr, (CONST84 char*)s, e-s+1);
                s = e+1;
            } else {
                break;
            }
	}
        for (len = strlen(s)-1; len > 0 && isspace((int)s[len]); len--);
	Tcl_AppendToObj(nPtr, (CONST84 char*)s, len+1);
	len = Tcl_UtfToLower(Tcl_GetString(nPtr));
	Tcl_SetObjLength(nPtr, len);
    }
    return nPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * IsChild --
 *
 * 	See if one message has the other as one of its ancestors.
 *
 * Results:
 *	A non-zero value if the child is related to the parent
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static int
IsChild(SortData *dataPtr, int child, int parent)
{
    int i;

    for (i=child; i > -1 && i != parent;) {
	if (-1 != dataPtr[i].parent) {
	    i = dataPtr[i].parent;
	} else {
	    i = dataPtr[i].prev;
	}
    }
    return (i == parent);
}

/*
 *----------------------------------------------------------------------
 *
 * RatFolderSortLinearize --
 *
 * Linearizes the linked list of messages. The list is also sorted.
 *
 * Results:
 *	Number of elements added to p
 *
 * Side effects:
 *	Modifies the p array.
 *
 *
 *----------------------------------------------------------------------
 */
static int
RatFolderSortLinearize(int *p, int n, SortData *dataPtr, int first, int depth)
{
    int *s = (int*)ckalloc(sizeof(int)*n);
    int i, j, k, o, ns;
    char *c;

    for (i=first, ns=0; -1 != i; i = dataPtr[i].next) {
	s[ns++] = i;
    }
    qsort((void*)s, ns, sizeof(int), RatFolderSortCompareDate);
    for (i=j=0; i<ns; i++) {
	if (depth) {
	    dataPtr[s[i]].tPtr = Tcl_NewObj();
	    for (k=0; k<depth-1; k++) {
		Tcl_AppendToObj(dataPtr[s[i]].tPtr, " ", 1);
	    }
	    Tcl_AppendToObj(dataPtr[s[i]].tPtr, "+", 1);
	}
	p[j++] = s[i];
	if (-1 != dataPtr[s[i]].child) {
	    o = j;
	    j += RatFolderSortLinearize(&p[j], n-ns, dataPtr,
		    dataPtr[s[i]].child, depth+1);
	    if (i < ns-1 && depth) {
		while (o<j) {
		    c = Tcl_GetStringFromObj(dataPtr[p[o++]].tPtr, NULL);
		    c[depth-1] = '|';
		}
	    }
	}
    }

    ckfree(s);
    return j;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetMsgInfo --
 *
 * Gets info from message structure and formats it somewhat. None of the
 * informations items needs both the messagePtr and the eltPtr. The following
 * table describes which needs which:
 *
 *  Info			msgPtr	envPtr	bodyPtr	eltPtr	size
 *  RAT_FOLDER_SUBJECT		-	needed	-	-	-
 *  RAT_FOLDER_CANONSUBJECT	-	needed	-	-	-
 *  RAT_FOLDER_NAME		needed	needed	-	-	-
 *  RAT_FOLDER_ANAME		needed	needed	-	-	-
 *  RAT_FOLDER_MAIL_REAL	-	needed	-	-	-
 *  RAT_FOLDER_MAIL		needed	needed	-	-	-
 *  RAT_FOLDER_NAME_RECIPIENT	needed	needed	-	-	-
 *  RAT_FOLDER_MAIL_RECIPIENT	needed	needed	-	-	-
 *  RAT_FOLDER_SIZE		-	-	-	-	needed
 *  RAT_FOLDER_SIZE_F		-	-	-	-	needed
 *  RAT_FOLDER_DATE_F		-	needed	-	needed	-
 *  RAT_FOLDER_DATE_N		-	needed	-	needed	-
 *  RAT_FOLDER_DATE_IMAP4	-	-	-	needed	-
 *  RAT_FOLDER_STATUS		    [not supported ]
 *  RAT_FOLDER_TYPE		-	-	needed	-	-
 *  RAT_FOLDER_PARAMETERS	-	-	needed	-	-
 *  RAT_FOLDER_FLAGS		-	-	-	needed	-
 *  RAT_FOLDER_UNIXFLAGS	-	-	-	needed	-
 *  RAT_FOLDER_MSGID		-	needed	-	-	-
 *  RAT_FOLDER_REF		-	needed	-	-	-
 *  RAT_FOLDER_INDEX		    [not supported ]
 *  RAT_FOLDER_THREADING	    [not supported ]
 *  RAT_FOLDER_UID       	    [not supported ]
 *
 * Results:
 *	A pointer to a string which is valid at least until the next call
 *	to this structure.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatGetMsgInfo(Tcl_Interp *interp, RatFolderInfoType type, MessageInfo *msgPtr,
	ENVELOPE *envPtr, BODY *bodyPtr, MESSAGECACHE *eltPtr, int size)
{
    Tcl_Obj *oPtr = NULL, *pPtr[2];
    time_t time, zonediff;
    MESSAGECACHE dateElt, *dateEltPtr;
    PARAMETER *parmPtr;
    ADDRESS *adrPtr;
    struct tm tm, *tm2;
    char buf[1024], *s;

    switch (type) {
	case RAT_FOLDER_SUBJECT:
	    oPtr = Tcl_NewStringObj(
		    RatDecodeHeader(interp,envPtr->subject,0), -1);
	    break;
	case RAT_FOLDER_CANONSUBJECT:
	    oPtr = RatFolderCanonalizeSubject(
		    RatDecodeHeader(interp,envPtr->subject,0));
	    break;
	case RAT_FOLDER_MAIL_REAL:
	    for (adrPtr = envPtr->from; adrPtr; adrPtr = adrPtr->next) {
		if (adrPtr->mailbox && adrPtr->host) {
		    break;
		}
	    }
	    if (!adrPtr) {
		oPtr = Tcl_NewStringObj(NULL, 0);
	    } else {
		oPtr = Tcl_NewStringObj(RatAddressMail(adrPtr), -1);
	    }
	    break;
        case RAT_FOLDER_ANAME: /* fallthrough */
	case RAT_FOLDER_NAME:
	    if (!envPtr->from) {
		oPtr = Tcl_NewObj();
	    } else if (type != RAT_FOLDER_ANAME
                       && (RAT_ISME_YES == msgPtr->fromMe
                           || (RAT_ISME_UNKOWN == msgPtr->fromMe
                               && RatAddressIsMe(interp, envPtr->from, 1)))) {
		msgPtr->fromMe = RAT_ISME_YES;
		if (envPtr->to && envPtr->to->personal) {
		    oPtr = Tcl_GetVar2Ex(interp, "t", "to", TCL_GLOBAL_ONLY);
		    if (Tcl_IsShared(oPtr)) {
			oPtr = Tcl_DuplicateObj(oPtr);
		    }
		    Tcl_AppendToObj(oPtr, ": ", 2);
		    Tcl_AppendToObj(oPtr,
				    RatDecodeHeader(interp,
						    envPtr->to->personal, 0),
				    -1);
		    break;
		}
	    } else {
                if (type != RAT_FOLDER_ANAME) {
                    msgPtr->fromMe = RAT_ISME_NO;
                }
		if (envPtr->from->personal) {
		    oPtr = Tcl_NewStringObj(RatDecodeHeader(interp,
			    envPtr->from->personal, 0), -1);
		    break;
		}
	    }
	    /* fallthrough */
	case RAT_FOLDER_MAIL:
	    oPtr = Tcl_NewObj();
	    if (type != RAT_FOLDER_ANAME
                && (RAT_ISME_YES == msgPtr->fromMe
		    || (RAT_ISME_UNKOWN == msgPtr->fromMe
			&& RatAddressIsMe(interp, envPtr->from, 1)))) {
		msgPtr->fromMe = RAT_ISME_YES;
		adrPtr = envPtr->to;
		Tcl_AppendObjToObj(oPtr, Tcl_GetVar2Ex(interp, "t", "to",
			TCL_GLOBAL_ONLY));
		Tcl_AppendToObj(oPtr, ": ", 2);
	    } else {
                if (type != RAT_FOLDER_ANAME) {
                    msgPtr->fromMe = RAT_ISME_NO;
                }
		adrPtr = envPtr->from;
	    }
	    for (; adrPtr; adrPtr = adrPtr->next) {
		if (adrPtr->mailbox && adrPtr->host) {
		    break;
		}
	    }
	    if (!adrPtr) {
		Tcl_IncrRefCount(oPtr);
		Tcl_DecrRefCount(oPtr);
		oPtr = Tcl_NewObj();
	    } else {
		Tcl_AppendToObj(oPtr, RatAddressMail(adrPtr), -1);
	    }
	    break;
	case RAT_FOLDER_NAME_RECIPIENT:
	    if (!envPtr->to) {
		oPtr = Tcl_NewObj();
		break;
	    }
	    msgPtr->toMe = RAT_ISME_NO;
	    if (envPtr->to->personal) {
		oPtr = Tcl_NewStringObj(
		    RatDecodeHeader(interp, envPtr->to->personal, 0), -1);
		break;
	    }
	    /* fallthrough */
	case RAT_FOLDER_MAIL_RECIPIENT:
	    oPtr = Tcl_NewObj();
	    adrPtr = envPtr->to;
	    for (; adrPtr; adrPtr = adrPtr->next) {
		if (adrPtr->mailbox && adrPtr->host) {
		    break;
		}
	    }
	    if (!adrPtr) {
		Tcl_IncrRefCount(oPtr);
		Tcl_DecrRefCount(oPtr);
		oPtr = Tcl_NewObj();
	    } else {
		Tcl_AppendToObj(oPtr, RatAddressMail(adrPtr), -1);
	    }
	    break;
	case RAT_FOLDER_SIZE:
	    oPtr = Tcl_NewIntObj(size);
	    break;
	case RAT_FOLDER_SIZE_F:
	    oPtr = RatMangleNumber(size);
	    break;
	case RAT_FOLDER_DATE_F:
            /* fallthrough */
	case RAT_FOLDER_DATE_N:
	    if (envPtr->date && T == mail_parse_date(&dateElt, envPtr->date)) {
		dateEltPtr = &dateElt;
	    } else {
		dateEltPtr = eltPtr;
	    }
	    tm.tm_sec = dateEltPtr->seconds;
	    tm.tm_min = dateEltPtr->minutes;
	    tm.tm_hour = dateEltPtr->hours;
	    tm.tm_mday = dateEltPtr->day;
	    tm.tm_mon = dateEltPtr->month - 1;
	    tm.tm_year = dateEltPtr->year+70;
	    tm.tm_wday = 0;
	    tm.tm_yday = 0;
	    tm.tm_isdst = -1;
            /* time represents the time teh message was sent, without
             * the time zone factor. So when rendered in gmt it gives
             * correct date/time. */
            time = mktime(&tm);
            if (RAT_FOLDER_DATE_F == type) {
                tm2 = gmtime(&time);
                oPtr = RatFormatDate(interp, tm2);
            } else {
                /* To get the real time of sending we must add the
                 * time zone offset. */
                zonediff = (dateEltPtr->zhours*60+dateEltPtr->zminutes)*60;
                if (!dateEltPtr->zoccident) {
                    zonediff *= -1;
                }
                time += zonediff;
                oPtr = Tcl_NewObj();
                Tcl_SetLongObj(oPtr, time);
            }
	    break;
	case RAT_FOLDER_DATE_IMAP4:
            dateEltPtr = eltPtr;
	    mail_date(buf, dateEltPtr); 
	    oPtr = Tcl_NewStringObj(buf, -1);
	    break;
	case RAT_FOLDER_TYPE:
	    oPtr = Tcl_NewObj();
	    Tcl_AppendStringsToObj(oPtr, body_types[bodyPtr->type], "/",
		    bodyPtr->subtype, NULL);
	    break;
	case RAT_FOLDER_PARAMETERS:
	    oPtr = Tcl_NewObj();
	    for (parmPtr = bodyPtr->parameter; parmPtr;
		    parmPtr = parmPtr->next) {
		pPtr[0] = Tcl_NewStringObj(parmPtr->attribute, -1);
		pPtr[1] = Tcl_NewStringObj(parmPtr->value, -1);
		Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewListObj(2,pPtr));
	    }
	    break;
	case RAT_FOLDER_TO:
	    oPtr = Tcl_NewStringObj("", 0);
	    Tcl_SetObjLength(oPtr, RatAddressSize(envPtr->to, 1));
	    Tcl_GetString(oPtr)[0] = '\0';
	    rfc822_write_address(Tcl_GetString(oPtr), envPtr->to);
	    Tcl_SetObjLength(oPtr, strlen(Tcl_GetString(oPtr)));
	    break;
	case RAT_FOLDER_FROM:
	    oPtr = Tcl_NewStringObj("", 0);
	    Tcl_SetObjLength(oPtr, RatAddressSize(envPtr->from, 1));
	    Tcl_GetString(oPtr)[0] = '\0';
	    rfc822_write_address(Tcl_GetString(oPtr), envPtr->from);
	    Tcl_SetObjLength(oPtr, strlen(Tcl_GetString(oPtr)));
	    break;
	case RAT_FOLDER_SENDER:
	    oPtr = Tcl_NewStringObj("", 0);
	    Tcl_SetObjLength(oPtr, RatAddressSize(envPtr->sender, 1));
	    Tcl_GetString(oPtr)[0] = '\0';
	    rfc822_write_address(Tcl_GetString(oPtr), envPtr->sender);
	    Tcl_SetObjLength(oPtr, strlen(Tcl_GetString(oPtr)));
	    break;
	case RAT_FOLDER_CC:
	    oPtr = Tcl_NewStringObj("", 0);
	    Tcl_SetObjLength(oPtr, RatAddressSize(envPtr->cc, 1));
	    Tcl_GetString(oPtr)[0] = '\0';
	    rfc822_write_address(Tcl_GetString(oPtr), envPtr->cc);
	    Tcl_SetObjLength(oPtr, strlen(Tcl_GetString(oPtr)));
	    break;
	case RAT_FOLDER_REPLY_TO:
	    oPtr = Tcl_NewStringObj("", 0);
	    Tcl_SetObjLength(oPtr, RatAddressSize(envPtr->reply_to, 1));
	    Tcl_GetString(oPtr)[0] = '\0';
	    rfc822_write_address(Tcl_GetString(oPtr), envPtr->reply_to);
	    Tcl_SetObjLength(oPtr, strlen(Tcl_GetString(oPtr)));
	    break;
	case RAT_FOLDER_FLAGS:
	    oPtr = Tcl_NewStringObj(MsgFlags(eltPtr), -1);
	    break;
	case RAT_FOLDER_UNIXFLAGS:
	    s = buf;
	    if (eltPtr->seen)	  *s++ = 'R';
	    if (eltPtr->deleted)  *s++ = 'D';
	    if (eltPtr->flagged)  *s++ = 'F';
	    if (eltPtr->answered) *s++ = 'A';
	    oPtr = Tcl_NewStringObj(buf, s-buf);
	    break;
	case RAT_FOLDER_MSGID:
	    oPtr = RatExtractRef(envPtr->message_id);
	    if (NULL == oPtr) {
		oPtr = Tcl_NewObj();
	    }
	    break;
	case RAT_FOLDER_REF:
	    oPtr = RatExtractRef(envPtr->in_reply_to);
	    if (NULL == oPtr) {
		oPtr = RatExtractRef(envPtr->references);
	    }
	    if (NULL == oPtr) {
		oPtr = Tcl_NewObj();
	    }
	    break;
	case RAT_FOLDER_STATUS:	   /*fallthrough */
	case RAT_FOLDER_INDEX:	   /*fallthrough */
	case RAT_FOLDER_THREADING: /*fallthrough */
	case RAT_FOLDER_UID:       /*fallthrough */
	case RAT_FOLDER_END:
	    oPtr = Tcl_NewObj();
	    break;
    }
    msgPtr->info[type] = oPtr;
    Tcl_IncrRefCount(oPtr);
    return oPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * MsgFlags --
 *
 *	Returns the flags of a message
 *
 * Results:
 *	A poiter to a static area containing the flags
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
char*
MsgFlags(MESSAGECACHE *eltPtr)
{
    static Tcl_DString ds;
    static int initialized = 0;

    if (!initialized) {
	Tcl_DStringInit(&ds);
	initialized = 1;
    }

    Tcl_DStringSetLength(&ds, 0);
    if (eltPtr->seen) {
	Tcl_DStringAppend(&ds, flag_name[RAT_SEEN].imap_name, -1);
    }
    if (eltPtr->deleted) {
	if (Tcl_DStringLength(&ds)) {
	    Tcl_DStringAppend(&ds, " ",1);
	}
	Tcl_DStringAppend(&ds, flag_name[RAT_DELETED].imap_name, -1);
    }
    if (eltPtr->flagged) {
	if (Tcl_DStringLength(&ds)) {
	    Tcl_DStringAppend(&ds, " ",1);
	}
	Tcl_DStringAppend(&ds, flag_name[RAT_FLAGGED].imap_name, -1);
    }
    if (eltPtr->answered) {
	if (Tcl_DStringLength(&ds)) {
	    Tcl_DStringAppend(&ds, " ",1);
	}
	Tcl_DStringAppend(&ds, flag_name[RAT_ANSWERED].imap_name, -1);
    }
    if (eltPtr->draft) {
	if (Tcl_DStringLength(&ds)) {
	    Tcl_DStringAppend(&ds, " ",1);
	}
	Tcl_DStringAppend(&ds, flag_name[RAT_DRAFT].imap_name, -1);
    }
    if (eltPtr->recent) {
	if (Tcl_DStringLength(&ds)) {
	    Tcl_DStringAppend(&ds, " ",1);
	}
	Tcl_DStringAppend(&ds, flag_name[RAT_RECENT].imap_name, -1);
    }

    return Tcl_DStringValue(&ds);
}


/*
 *----------------------------------------------------------------------
 *
 * RatParseFrom --
 *
 *	Parse the time in a 'From ' line. See ../imap/src/osdep/unix/unix.h
 *	for details on how this line may look like.
 *
 * Results:
 *	A poiter to a static area containing a MESSAGECACHE.
 *	The only valid fields in this are the time-fields
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */

MESSAGECACHE*
RatParseFrom(const char *from)
{
    static MESSAGECACHE elt;
    const char *cPtr;
    int i=0, found;

    /*
     * Start by finding the weekday name, if it is followed by one
     * space and a month-spec, then we assume we have found the date.
     */
    for (cPtr = from+5, found=0; cPtr && !found; cPtr = strchr(cPtr, ' ')) {
	for (i=0; i<7 && strncmp(cPtr+1, dayName[i], 3); i++);
	if (i < 7) {
	    for (i=0; i<12; i++) {
		if (!strncmp(cPtr+5, monthName[i], 3)) {
		    found = 1;
		    break;
		}
	    }
	}
    }
    if (!found) {
	return NULL;
    }
    elt.month = i+1;
    for (cPtr+=8; isspace(*cPtr) && *cPtr; cPtr++);
    if (!*cPtr) return NULL;
    elt.day = atoi(cPtr);
    for (cPtr++; !isspace(*cPtr) && *cPtr; cPtr++);
    for (cPtr++; isspace(*cPtr) && *cPtr; cPtr++);
    if (!*cPtr) return NULL;
    elt.hours = atoi(cPtr);
    for (cPtr++; ':' != *cPtr && *cPtr; cPtr++);
    elt.minutes = atoi(cPtr+1);
    for (cPtr++; isdigit(*cPtr) && *cPtr; cPtr++);
    if (!*cPtr) return NULL;
    if (':' == *cPtr) {
	elt.seconds = atoi(cPtr+1);
	for (cPtr++; isdigit(*cPtr) && *cPtr; cPtr++);
    } else {
	elt.seconds = 0;
    }
    while (1) {
	for (cPtr++; isspace(*cPtr) && *cPtr; cPtr++);
	if (isdigit(cPtr[0]) && isdigit(cPtr[1])
		&& isdigit(cPtr[2]) && isdigit(cPtr[3])){
	    elt.year = atoi(cPtr)-BASEYEAR;
	    break;
	} else {
	    for (cPtr++; !isspace(*cPtr) && *cPtr; cPtr++);
	}
	if (!*cPtr) return NULL;
    }
    elt.zoccident = 0;
    elt.zhours = 0;
    elt.zminutes = 0;
    return &elt;
}


/*
 *----------------------------------------------------------------------
 *
 * RatUpdateFolder --
 *
 *	Updates a folder
 *
 * Results:
 *	The number of new messages is left in the tcl result-buffer.
 *	A standard tcl-result is returned.
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */

int
RatUpdateFolder(Tcl_Interp *interp, RatFolderInfo *infoPtr, RatUpdateType mode)
{
    int i, numNew, oldNumber, delta;

    oldNumber = infoPtr->number;
    numNew = (*infoPtr->updateProc)(infoPtr, interp, mode);
    if (numNew < 0) {
	return TCL_ERROR;
    } else if (numNew
	       || oldNumber != infoPtr->number
	       || infoPtr->sortOrderChanged) {
	if (infoPtr->number > infoPtr->allocated) {
	    infoPtr->allocated = infoPtr->number;
	    infoPtr->msgCmdPtr = (char **) ckrealloc(infoPtr->msgCmdPtr,
		    infoPtr->allocated*sizeof(char*));
	    infoPtr->privatePtr = (ClientData**)ckrealloc(infoPtr->privatePtr,
		    infoPtr->allocated*sizeof(ClientData*));
	    infoPtr->presentationOrder = (int *) ckrealloc(
		    infoPtr->presentationOrder,
		    infoPtr->allocated*sizeof(int));
	}
	for (i=infoPtr->number-numNew; i<infoPtr->number; i++) {
	    infoPtr->msgCmdPtr[i] = (char *) NULL;
	    infoPtr->privatePtr[i] = (ClientData*) NULL;
	    (*infoPtr->initProc)(infoPtr, interp, i);
	}
	RatFolderSort(interp, infoPtr);
	infoPtr->sortOrderChanged = 0;
    }
    delta = infoPtr->number - oldNumber;
    Tcl_SetObjResult(interp, Tcl_NewIntObj((delta>0 ? delta : 0)));
    if (delta) {
	Tcl_SetVar2Ex(interp, "folderExists", infoPtr->cmdName,
		Tcl_NewIntObj(infoPtr->number), TCL_GLOBAL_ONLY);
	Tcl_SetVar2Ex(interp, "folderRecent", infoPtr->cmdName,
		Tcl_NewIntObj(infoPtr->recent), TCL_GLOBAL_ONLY);
	Tcl_SetVar2Ex(interp, "folderUnseen", infoPtr->cmdName,
		Tcl_NewIntObj(infoPtr->unseen), TCL_GLOBAL_ONLY);
	Tcl_SetVar2Ex(interp, "folderChanged", infoPtr->cmdName,
		Tcl_NewIntObj(++folderChangeId), TCL_GLOBAL_ONLY);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFolderUpdateTime --
 *
 *	Updates a folder
 *
 * Results:
 *	The number of new messages is left in the tcl result-buffer.
 *	A standard tcl-result is returned.
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */

void
RatFolderUpdateTime(ClientData clientData)
{
    static Tcl_TimerToken timer_token = NULL;
    Tcl_Interp *interp = (Tcl_Interp*)clientData;
    RatFolderInfo *infoPtr, *nextPtr;
    Tcl_Obj *oPtr;
    int interval;

    if (timer_token) {
	Tcl_DeleteTimerHandler(timer_token);
    }

    RatSetBusy(timerInterp);
    for (infoPtr = ratFolderList; infoPtr; infoPtr = nextPtr) {
        nextPtr = infoPtr->nextPtr;
	RatUpdateFolder(interp, infoPtr, RAT_UPDATE);
    }
    RatClearBusy(interp);

    oPtr = Tcl_GetVar2Ex(interp, "option", "watcher_time", TCL_GLOBAL_ONLY);
    if (!oPtr || TCL_OK != Tcl_GetIntFromObj(interp, oPtr, &interval)) {
	interval = 30;
    } else if (interval > 1000000) {
	interval = 1000000;
    }
    timer_token = Tcl_CreateTimerHandler(interval*1000, RatFolderUpdateTime,
					 (ClientData)interp);
}


/*
 *----------------------------------------------------------------------
 *
 * RatFolderClose --
 *
 *	Closes a folder
 *
 * Results:
 *	A standard tcl result
 *
 * Side effects:
 *	Many, all associated with cleaning up from the folder
 *
 *
 *----------------------------------------------------------------------
 */

int
RatFolderClose(Tcl_Interp *interp, RatFolderInfo *infoPtr, int force)
{
    RatFolderInfo **rfiPtrPtr;
    int i, ret, expunge;
    char buf[1024];
    Tcl_Obj *oPtr;

    oPtr = Tcl_GetVar2Ex(interp, "option", "expunge_on_close",TCL_GLOBAL_ONLY);
    Tcl_GetBooleanFromObj(interp, oPtr, &expunge);

    if (infoPtr->refCount-- != 1 && !force) {
	if (expunge) {
	    RatUpdateFolder(interp, infoPtr, RAT_SYNC);
	}
	return TCL_OK;
    }

    snprintf(buf, sizeof(buf),
             "foreach f [array names folderWindowList] {"
             "    if {$folderWindowList($f) == \"%s\"} {"
             "        FolderWindowClear $f"
             "    }"
             "}", infoPtr->cmdName);
    Tcl_GlobalEval(interp, buf);

    for (rfiPtrPtr = &ratFolderList; infoPtr != *rfiPtrPtr;
	    rfiPtrPtr = &(*rfiPtrPtr)->nextPtr);
    *rfiPtrPtr = infoPtr->nextPtr;
    ckfree(infoPtr->name);
    ckfree(infoPtr->ident_def);

    ret = (*infoPtr->closeProc)(infoPtr, interp, expunge);
    for(i=0; i < infoPtr->number; i++) {
	if (NULL != infoPtr->msgCmdPtr[i]) {
	    (void)RatMessageDelete(interp, infoPtr->msgCmdPtr[i]);
	    infoPtr->msgCmdPtr[i] = 0;
	}
    }
    Tcl_UnsetVar2(interp, "folderExists", infoPtr->cmdName,TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "folderUnseen", infoPtr->cmdName,TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "folderChanged", infoPtr->cmdName,TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "vFolderWatch", infoPtr->cmdName,TCL_GLOBAL_ONLY);
    Tcl_UnsetVar(interp, infoPtr->cmdName, TCL_GLOBAL_ONLY);
    (void)Tcl_DeleteCommand(interp, infoPtr->cmdName);
    ckfree(infoPtr->cmdName);
    ckfree(infoPtr->msgCmdPtr);
    ckfree(infoPtr->privatePtr);
    ckfree(infoPtr->presentationOrder);
    ckfree(infoPtr);
    return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFolderInsert --
 *
 *	Insert messages into a folder
 *
 * Results:
 *	A standard tcl result
 *
 * Side effects:
 *	Messages gets added
 *
 *
 *----------------------------------------------------------------------
 */

int
RatFolderInsert(Tcl_Interp *interp, RatFolderInfo *infoPtr,int num,char **msgs)
{
    int result;

    result = (*infoPtr->insertProc)(infoPtr, interp, num, msgs);
    RatUpdateFolder(interp, infoPtr, RAT_UPDATE);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetFolderSpec --
 *
 *      Return the mailbox spec for the given folder definition
 *
 * Results:
 *	A pointer to a static area of memeory where the spec is stored.
 *	This area will be overwritten by the next call.
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
char*
RatGetFolderSpec(Tcl_Interp *interp, Tcl_Obj *def)
{
    static Tcl_DString ds;
    static int initialized = 0;
    Tcl_Obj *oPtr, **objv, **fobjv, **mobjv, **sobjv;
    int objc, fobjc, mobjc, sobjc, port, i, j;
    char buf[64], *type, *c, *file;

    if (0 == initialized) {
	Tcl_DStringInit(&ds);
    } else {
	Tcl_DStringSetLength(&ds, 0);
    }

    Tcl_ListObjGetElements(interp, def, &objc, &objv);
    if (objc < 4) {
	return NULL;
    }
    type = Tcl_GetString(objv[1]);
    if (!strcmp(type, "file")) {
	file = cpystr(RatTranslateFileName(interp, Tcl_GetString(objv[3])));
	if (NULL == file) {
	    Tcl_DStringAppend(&ds, "invalid_file_specified", -1);
	} else {
	    RatDecodeQP((unsigned char*)file);
	    Tcl_DStringAppend(&ds, file, -1);
	    c = Tcl_GetString(objv[3]);
	    if ('/' == c[strlen(c)-1]) {
		Tcl_DStringAppend(&ds, "/", 1);
	    }
	}
    } else if (!strcmp(type, "mh")) {
	Tcl_DStringAppend(&ds, "#mh/", 4);
	file = cpystr(Tcl_GetString(objv[3]));
	RatDecodeQP((unsigned char*)file);
	Tcl_DStringAppend(&ds, file, -1);
	ckfree(file);
    } else if (!strcmp(type, "dbase")) {
	if (objc < 6) {
	    return NULL;
	}
	Tcl_DStringAppend(&ds, Tcl_GetString(objv[3]), -1);
	Tcl_DStringAppend(&ds, Tcl_GetString(objv[4]), -1);
	Tcl_DStringAppend(&ds, Tcl_GetString(objv[5]), -1);
    } else if (!strcmp(type, "imap")
	       || !strcmp(type, "pop3")
	       || !strcmp(type, "dis")) {
	oPtr = Tcl_GetVar2Ex(interp, "mailServer", Tcl_GetString(objv[3]),
			     TCL_GLOBAL_ONLY);
	if (!oPtr) {
	    return NULL;
	}
	Tcl_ListObjGetElements(interp, oPtr, &mobjc, &mobjv);
	Tcl_DStringAppend(&ds, "{", 1);
	Tcl_DStringAppend(&ds, Tcl_GetString(mobjv[0]),
			  Tcl_GetCharLength(mobjv[0]));
	if (TCL_OK == Tcl_GetIntFromObj(interp, mobjv[1], &port) && port!=0) {
	    snprintf(buf, sizeof(buf), ":%d", port);
	    Tcl_DStringAppend(&ds, buf, -1);
	}
	if (!strcmp(type, "pop3")) {
	    Tcl_DStringAppend(&ds, "/pop3", 5);
	} else {
	    Tcl_DStringAppend(&ds, "/imap", 5);
	}
	Tcl_ListObjGetElements(interp, mobjv[2], &fobjc, &fobjv);
#ifdef HAVE_OPENSSL
	/*
	 * These flags must be in a specific order to match strings generated
	 * by c-client. Also only include them if we have SSL available.
	 */
	for (i=0; cClientFlags[i]; i++) {
	    for (j=0; j<fobjc; j++) {
		if (!strcmp(cClientFlags[i]+1, Tcl_GetString(fobjv[j]))) {
		    Tcl_DStringAppend(&ds, cClientFlags[i], -1);
		    break;
		}
	    }
	}
#endif /* HAVE_OPENSSL */
	for (i=0; i<fobjc; i++) {
	    Tcl_ListObjGetElements(interp, fobjv[i], &sobjc, &sobjv);
	    if (2 == sobjc && !strcmp("ssh-cmd", Tcl_GetString(sobjv[0]))) {
		tcp_parameters(SET_SSHCOMMAND, Tcl_GetString(sobjv[1]));
	    }
	}
	Tcl_DStringAppend(&ds, "/user=\"", 7);
	Tcl_DStringAppend(&ds, Tcl_GetString(mobjv[3]),
			  Tcl_GetCharLength(mobjv[3]));
	Tcl_DStringAppend(&ds, "\"", 1);
	for (j=0; j<fobjc; j++) {
	    if (!strcmp("debug", Tcl_GetString(fobjv[j]))) {
		Tcl_DStringAppend(&ds, "/debug", 6);
		break;
	    }
	}

	Tcl_DStringAppend(&ds, "}", 1);
	if (strcmp(type, "pop3")) {
	    file = cpystr(Tcl_GetString(objv[4]));
	    RatDecodeQP((unsigned char*)file);
	    Tcl_DStringAppend(&ds, file, -1);
	    ckfree(file);
	}
    }
    return Tcl_DStringValue(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * RatCreateFolderCmd --
 *
 *      See the INTERFACE specification for RatCreateFolder and
 *	RatDeleteFolder
 *
 * Results:
 *      A normal tcl result.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatCreateFolderCmd(ClientData op, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[])
{
    int fobjc, def, mbx;
    Tcl_Obj **fobjv;

    if ((objc != 2 && objc != 3)
	|| (objc == 3 && strcmp("-mbx", Tcl_GetString(objv[1])))) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " ?-mbx? folderdef\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (3 == objc) {
	mbx = 1;
	def = 2;
    } else {
	mbx = 0;
	def = 1;
    }
    Tcl_ListObjGetElements(interp, objv[def], &fobjc, &fobjv);
    if (fobjc < 4) {
	Tcl_AppendResult(interp, "Argument \"", Tcl_GetString(objv[def]),
			 "\" is not a valid vfolderdef.",
			 (char*)NULL);
	return TCL_ERROR;
    }
    if (!strcmp(Tcl_GetString(fobjv[1]), "dbase")) {
	return TCL_OK;
    } else {
	return RatStdManageFolder(interp, (RatManagementAction)op, mbx,
				  objv[def]);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatFlagNameToInt --
 *
 *      Convert flag name to integer
 *
 * Results:
 *      The RatFlag integer
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static RatFlag
RatFlagNameToInt(const char *name)
{
    int f;
    
    for (f=0; flag_name[f].tkrat_name; f++) {
	if (!strcmp(name, flag_name[f].tkrat_name)) {
	    return f;
	}
    }
    /* Not reached unless in error */
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatExtractRef --
 *
 *      Extract and references. This will extract the last message-id
 *	found in the given text. Also all whitespace ion that message-id
 *	will be removed.
 *
 * Results:
 *      Returns the extracted reference as a tcl object. Or NULL if no
 *	reference was found.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj*
RatExtractRef(CONST84 char *text)
{
    CONST84 char *s, *e, *ls, *le;
    Tcl_Obj *oPtr;
    int quoted = 0;
    
    if (NULL == text) {
	return NULL;
    }

    le = s = text;
    ls = NULL;
    while (s && *s
	   && (s = RatFindCharInHeader(le, '<'))
	   && (e = RatFindCharInHeader(s+1, '>'))) {
	ls = s+1;
	le = e;
    }
    if (ls) {
	oPtr = Tcl_NewObj();
	for (s=ls; s<le; s++) {
	    if ('\\' == *s) {
		Tcl_AppendToObj(oPtr, ++s, 1);
	    } else if ('"' == *s) {
		if (quoted) {
		    quoted = 0;
		} else {
		    quoted = 1;
		}
	    } else {
		Tcl_AppendToObj(oPtr, s, 1);
	    }
	}
	return oPtr;
    } else {
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetIdentDef --
 *
 *      Get the identifying part of a definition. This part is used
 *      to locate previously open instances of the same folder.
 *
 * Results:
 *      Returns a pointer to a static buffer which contains the def.
 *      The buffer will be valid until the next call.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static char*
RatGetIdentDef(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    static Tcl_DString ds;
    static int initialized = 0;
    Tcl_Obj **objv;
    int i, objc;

    if (!initialized) {
	Tcl_DStringInit(&ds);
	initialized = 1;
    } else {
	Tcl_DStringSetLength(&ds, 0);
    }

    Tcl_ListObjGetElements(interp, defPtr, &objc, &objv);
    Tcl_DStringAppendElement(&ds, Tcl_GetString(objv[1]));
    for (i=3; i<objc; i++) {
	Tcl_DStringAppendElement(&ds, Tcl_GetString(objv[i]));
    }
    return Tcl_DStringValue(&ds);
}
