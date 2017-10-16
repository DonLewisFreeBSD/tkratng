/*
 * ratDSN.c --
 *
 *	This file handles the delivery status notifications.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"
#include <unistd.h>

typedef struct {
    char *envid;		/* Original envelope ID                      */
    Tcl_Obj *msgFields;		/* Contains tcl list of msg fields           */
    int numRecipients;		/* Number of recipients mentioned in this DSN*/
    char **actionPtrPtr;	/* The action                                */
    char **recTypePtrPtr;	/* Type of recipient addresses               */
    char **recipientPtrPtr;	/* Recipient addresses                       */
    Tcl_Obj **rListPtrPtr;      /* Recipient fields                          */
} RatDeliveryStatus;

/*
 * Static data
 */
static Tcl_HashTable seenTable;

/*
 * Local functions
 */
static Tcl_Channel OpenIndex(Tcl_Interp *interp, char *mode);
static Tcl_ObjCmdProc RatDSNList;
static Tcl_ObjCmdProc RatDSNGet;
static int RatDSNExpire(Tcl_Interp *interp, Tcl_Obj *lineObj);
static char *RatParseDSNLine(char *buf, Tcl_Obj **name, Tcl_Obj **value,
			     int *length);
static RatDeliveryStatus *RatParseDS(Tcl_Interp *interp, Tcl_Obj *body);
static void RatFreeDeliveryStatus(RatDeliveryStatus *statusPtr);


/*
 *----------------------------------------------------------------------
 *
 * RatDSNInit --
 *
 *      Initializes the DSN system. That is adds the apropriate commands
 *	to the interpreter.
 *
 * Results:
 *	A standard tcl resutl.
 *
 * Side effects:
 *	Commands are created in the interpreter.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDSNInit(Tcl_Interp *interp)
{
    Tcl_InitHashTable(&seenTable, TCL_STRING_KEYS);
    Tcl_CreateObjCommand(interp, "RatDSNList", RatDSNList, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDSNGet", RatDSNGet, NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNStartMessage --
 *
 *      Start recording a new DSN message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new current message is initialized.
 *
 *
 *----------------------------------------------------------------------
 */

DSNhandle
RatDSNStartMessage(Tcl_Interp *interp, const char *id, const char *subject)
{
    Tcl_DString *dsPtr = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));
    unsigned char buf[32], *header, *cPtr;
    time_t seconds;

    Tcl_DStringInit(dsPtr);
    Tcl_DStringAppendElement(dsPtr, id);
    seconds = time(NULL);
    sprintf(buf, "%d", (int)seconds);
    Tcl_DStringAppendElement(dsPtr, buf);
    header = RatDecodeHeader(interp, subject, 0);
    for (cPtr = header; *cPtr; cPtr++) {
	if (*cPtr < 32) {
	    *cPtr = ' ';
	}
    }
    Tcl_DStringAppendElement(dsPtr, header);
    Tcl_DStringStartSublist(dsPtr);

    return (DSNhandle)dsPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNAddRecipient --
 *
 *      Add a recipient to the currently recording DSN message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current message is modified.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatDSNAddRecipient(Tcl_Interp *interp, DSNhandle handle, char *recipient)
{
    Tcl_DString *dsPtr = (Tcl_DString*)handle;

    Tcl_DStringStartSublist(dsPtr);
    Tcl_DStringAppendElement(dsPtr, "none");
    Tcl_DStringAppendElement(dsPtr, recipient);
    Tcl_DStringAppendElement(dsPtr, "");
    Tcl_DStringEndSublist(dsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNAbort --
 *
 *	Aborts the composition of the indicated DSN message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The handle becomes invalid.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatDSNAbort(Tcl_Interp *interp, DSNhandle handle)
{
    Tcl_DString *dsPtr = (Tcl_DString*)handle;

    if (dsPtr) {
	Tcl_DStringFree(dsPtr);
	ckfree(dsPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNFinish --
 *
 *      Moves the message under construction to the list of outstanding
 *	DSN's.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current message is cleared.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatDSNFinish(Tcl_Interp *interp, DSNhandle handle)
{
    Tcl_Channel channel = OpenIndex(interp, "a");
    Tcl_DString *dsPtr = (Tcl_DString*)handle;

    if (!channel) {
	Tcl_BackgroundError(interp);
	return;
    }
    Tcl_DStringEndSublist(dsPtr);
    Tcl_Write(channel, Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr));
    Tcl_Write(channel, "\n", 1);
    Tcl_Close(interp, channel);
    Tcl_DStringFree(dsPtr);
    ckfree(dsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNHandle --
 *
 *      Handle an incoming DSN.
 *
 * Results:
 *	Returns true if the given DSN matched one of those in our list.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDSNHandle (Tcl_Interp *interp, char *msg)
{
    int i, j, new, match = 0, changed = 0, objc, rfound, perm, robjc, srobjc;
    RatDeliveryStatus *statusPtr;
    Tcl_HashEntry *entryPtr;
    Tcl_Channel channel;
    char buf[1024], id[1024], *oldId, *msgFile = NULL, *file;
    const char *dir;
    Tcl_CmdInfo cmdInfo;
    Tcl_Obj *oPtr, *linePtr, *l1Ptr, *l2Ptr, *l3Ptr, *l4Ptr, **objv, **robjv,
	**srobjv;

    /*
     * Avoid processing the same DSN twice
     */
    entryPtr = Tcl_CreateHashEntry(&seenTable, msg, &new);
    if (!new) {
	return (int)Tcl_GetHashValue(entryPtr);
    }
    Tcl_SetHashValue(entryPtr, 0);

    snprintf(buf, sizeof(buf), "[lindex [[%s body] children] 1] data 0", msg);
    if (TCL_OK != Tcl_Eval(interp, buf)) {
	return 0;
    }
    statusPtr = RatParseDS(interp, Tcl_GetObjResult(interp));
    if (!statusPtr->envid) {
	RatFreeDeliveryStatus(statusPtr);
	return 0;
    }

    if (NULL == (channel = OpenIndex(interp, "r"))) {
	RatFreeDeliveryStatus(statusPtr);
	return 0;
    }
    l1Ptr = Tcl_NewObj();
    oPtr = Tcl_GetVar2Ex(interp, "option", "permissions", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &perm);
    dir = RatGetPathOption(interp, "dsn_directory");
    while (linePtr = Tcl_NewObj(), -1 != Tcl_GetsObj(channel, linePtr)) {
	/* Join lines until we have a valid list */
	while (0 != Tcl_ListObjLength(interp, linePtr, &i)
	       && -1 != Tcl_GetsObj(channel, linePtr));
	if (i != 4) {
	    /* If the list does not have 4 elements it is invalid */
	    continue;
	}
	if (RatDSNExpire(interp, linePtr)) {
	    /*
	     * This DSN has expired so we should remove all associated files
	     */
	    Tcl_ListObjIndex(interp, linePtr, 0, &oPtr);
	    snprintf(buf, sizeof(buf), "%s/%s", dir, Tcl_GetString(oPtr));
	    (void)unlink(buf);
	    Tcl_ListObjLength(interp, linePtr, &i);
	    Tcl_ListObjIndex(interp, linePtr, i-1, &oPtr);
	    Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);
	    for (i=0; i < objc; i++) {
		Tcl_ListObjIndex(interp, objv[i], 2, &oPtr);
		file = Tcl_GetString(oPtr);;
		if (strlen(file)) {
		    snprintf(buf, sizeof(buf), "%s/%s", dir, file);
		    (void)unlink(buf);
		}
	    }
	    changed++;
	    continue;
	}
	Tcl_ListObjIndex(interp, linePtr, 0, &oPtr);
	if (strcmp(Tcl_GetString(oPtr), statusPtr->envid)) {
	    Tcl_ListObjAppendElement(interp, l1Ptr, linePtr);
	    continue;
	}
	changed++;
	match = 1;
	l2Ptr = Tcl_NewObj();
	for (i=0; i<3; i++) {
	    Tcl_ListObjIndex(interp, linePtr, i, &oPtr);
	    Tcl_ListObjAppendElement(interp, l2Ptr, oPtr);
	}
	l3Ptr = Tcl_NewObj();
	Tcl_ListObjLength(interp, linePtr, &i);
	Tcl_ListObjIndex(interp, linePtr, i-1, &oPtr);
	Tcl_ListObjGetElements(interp, oPtr, &robjc, &robjv);
	for (i=0; i<robjc; i++) {
	    for (j=rfound=0; !rfound && j<statusPtr->numRecipients; j++) {
		Tcl_ListObjGetElements(interp, robjv[i], &srobjc, &srobjv);
		if (statusPtr->recTypePtrPtr[j]
			&& statusPtr->actionPtrPtr[j]
			&& !strcasecmp(statusPtr->recTypePtrPtr[j], "rfc822")
			&& !strcmp(statusPtr->recipientPtrPtr[j],
				   Tcl_GetString(srobjv[1]))
			&& strcmp(statusPtr->actionPtrPtr[j],
				  Tcl_GetString(srobjv[0]))) {
		    /*
		     * This DSN matched this recipient
		     * We start by saving the DSN message;
		     * then we add it to the index file.
		     * Finally we notify the user.
		     */
		    rfound = 1;
		    oldId = Tcl_GetString(srobjv[2]);
		    RatGenId(NULL, interp, 0, NULL);
		    strlcpy(id, Tcl_GetStringResult(interp), sizeof(id));
		    if (strlen(oldId)) {
			snprintf(buf, sizeof(buf), "%s/%s", dir, oldId);
			(void)unlink(buf);
		    }

		    snprintf(buf, sizeof(buf), "%s/%s", dir, id);
		    if (!msgFile) {
			Tcl_DString msgDS;
			Tcl_Channel msgCh;

			msgFile = cpystr(buf);
			Tcl_DStringInit(&msgDS);
			Tcl_GetCommandInfo(interp, msg, &cmdInfo);
			RatMessageGet(interp,
				      (MessageInfo*)cmdInfo.objClientData,
				      &msgDS, NULL, 0, NULL, 0);
			msgCh = Tcl_OpenFileChannel(interp, msgFile, "w",perm);
			Tcl_Write(msgCh, Tcl_DStringValue(&msgDS),
				Tcl_DStringLength(&msgDS));
			Tcl_Close(interp, msgCh);
			Tcl_DStringFree(&msgDS);
		    } else {
			link(msgFile, buf);
		    }
		    l4Ptr = Tcl_NewObj();
		    oPtr = Tcl_NewStringObj(statusPtr->actionPtrPtr[j], -1);
		    Tcl_ListObjAppendElement(interp, l4Ptr, oPtr);
		    oPtr = Tcl_NewStringObj(statusPtr->recipientPtrPtr[j], -1);
		    Tcl_ListObjAppendElement(interp, l4Ptr, oPtr);
		    oPtr = Tcl_NewStringObj(id, -1);
		    Tcl_ListObjAppendElement(interp, l4Ptr, oPtr);
		    Tcl_ListObjAppendElement(interp, l3Ptr, l4Ptr);
		    Tcl_ListObjIndex(interp, linePtr, 2, &oPtr);
		    Tcl_VarEval(interp, "RatDSNRecieve {",
				Tcl_GetString(oPtr), "} {",
				statusPtr->actionPtrPtr[j], "} {",
				statusPtr->recipientPtrPtr[j], "} {", id,
				"}",NULL);
		}
	    }
	    if (!rfound) {
		Tcl_ListObjAppendElement(interp, l3Ptr, robjv[i]);
	    }
	}
	Tcl_ListObjAppendElement(interp, l2Ptr, l3Ptr);
	Tcl_ListObjAppendElement(interp, l1Ptr, l2Ptr);
    }
    Tcl_Close(interp, channel);
    RatFreeDeliveryStatus(statusPtr);
    if (changed) {
	if (NULL == (channel = OpenIndex(interp, "w"))) {
	    return 0;
	}
	Tcl_ListObjGetElements(interp, l1Ptr, &objc, &objv);
	for (i=0; i<objc; i++) {
	    Tcl_WriteObj(channel, objv[i]);
	    Tcl_Write(channel, "\n", 1);
	}
	Tcl_Close(interp, channel);
    }
    Tcl_DecrRefCount(l1Ptr);
    Tcl_DecrRefCount(linePtr);
    Tcl_SetHashValue(entryPtr, match);
    ckfree(msgFile);
    return match;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNExtract --
 *
 *      Extract the DSN data from a dsn body part
 *
 * Results:
 *	A standard tcl result and the requested data in the result area
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDSNExtract (Tcl_Interp *interp, Tcl_Obj *body)
{
    RatDeliveryStatus *sPtr = RatParseDS(interp, body);
    Tcl_Obj *rPtr, *oPtr;
    int i;

    rPtr = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, rPtr, sPtr->msgFields);
    oPtr = Tcl_NewObj();
    for (i=0; i<sPtr->numRecipients; i++) {
	Tcl_ListObjAppendElement(interp, oPtr, sPtr->rListPtrPtr[i]);
    }
    Tcl_ListObjAppendElement(interp, rPtr, oPtr);
    Tcl_SetObjResult(interp, rPtr);
    RatFreeDeliveryStatus(sPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenIndex --
 *
 *      Opens the DSN indexfile.
 *
 * Results:
 *	A Tcl channel handle. If an error occurs NULL is returned
 *	and an error message is left in the result area.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static Tcl_Channel
OpenIndex(Tcl_Interp *interp, char *mode)
{
    char buf[1024];
    const char *dir;
    struct stat sbuf;
    Tcl_Channel channel;
    int perm;
    Tcl_Obj *oPtr;

    oPtr = Tcl_GetVar2Ex(interp, "option", "permissions", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &perm);
    dir = RatGetPathOption(interp, "dsn_directory");
    if (stat(dir, &sbuf)) {
	if (mkdir(dir, perm|0100)) {
	    Tcl_AppendResult(interp, "Failed to create directory \"",
		    dir, "\" :", Tcl_PosixError(interp), NULL);
	    return NULL;
	}
    } else if (!S_ISDIR(sbuf.st_mode)) {
	Tcl_AppendResult(interp, "This is no directory \"", dir, "\"", NULL);
	return NULL;
    }
    snprintf(buf, sizeof(buf), "%s/index", dir);

    if (NULL == (channel = Tcl_OpenFileChannel(interp, buf, mode, perm))) {
	return NULL;
    }
    Tcl_SetChannelOption(interp, channel, "-encoding", "utf-8");
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNList --
 *
 *      List the currently known DSN's
 *
 * Results:
 *	See ../doc/interface
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatDSNList (ClientData clientData, Tcl_Interp *interp, int objc,
	    Tcl_Obj *const objv[])
{
    Tcl_Channel channel = OpenIndex(interp, "r");
    Tcl_Obj *oPtr, *rPtr;

    if (!channel) {
	Tcl_ResetResult(interp);
	return TCL_OK;
    }

    rPtr = Tcl_NewObj();
    while (oPtr = Tcl_NewObj(), -1 != Tcl_GetsObj(channel, oPtr)) {
	if (!RatDSNExpire(interp, oPtr)) {
	    Tcl_ListObjAppendElement(interp, rPtr, oPtr);
	} else {
	    Tcl_DecrRefCount(oPtr);
	}
    }
    Tcl_DecrRefCount(oPtr);
    if (!Tcl_Eof(channel)) {
	Tcl_Close(interp, channel);
	Tcl_DecrRefCount(rPtr);
	return TCL_ERROR;
    }
    Tcl_Close(interp, channel);
    Tcl_SetObjResult(interp, rPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNGet --
 *
 *      Get information about a DSN.
 *
 * Results:
 *	See ../doc/interface
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatDSNGet (ClientData clientData, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    char buf[1024], *msg, *data;
    const char *dir;
    RatDeliveryStatus *statusPtr;
    Tcl_Channel channel;
    int i, len;
    Tcl_Obj *rPtr, *oPtr;

    if (objc != 3 && objc != 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]), " what id ?recipient?\"",
			 (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(Tcl_GetString(objv[1]), "msg")
	&& strcmp(Tcl_GetString(objv[1]), "report")) {
	Tcl_AppendResult(interp, "Illegal 'what' argument; should be ",
		"'msg' or 'report'.", (char*) NULL);
	return TCL_ERROR;
    }
    if (!strlen(Tcl_GetString(objv[2]))) {
	Tcl_SetResult(interp, "Empty 'id' argument.", TCL_STATIC);
	return TCL_ERROR;
    }

    dir = RatGetPathOption(interp, "dsn_directory");
    snprintf(buf, sizeof(buf), "%s/%s", dir, Tcl_GetString(objv[2]));
    if (NULL == (channel = Tcl_OpenFileChannel(interp, buf, "r", 0))) {
        return TCL_ERROR;
    }
    len = Tcl_Seek(channel, 0, SEEK_END);
    data = (char*) ckalloc(len+1);
    Tcl_Seek(channel, 0, SEEK_SET);
    len = Tcl_Read(channel, data, len);
    data[len] = '\0';
    Tcl_Close(interp, channel);
    msg = RatFrMessageCreate(interp, data, len, NULL);
    ckfree(data);

    if (!strcmp(Tcl_GetString(objv[1]), "msg")) {
	Tcl_SetResult(interp, msg, TCL_VOLATILE);
    } else {
	snprintf(buf,sizeof(buf),"[lindex [[%s body] children] 1] data 0",msg);
	if (TCL_OK != Tcl_Eval(interp, buf)) {
	    return TCL_ERROR;
	}
	statusPtr = RatParseDS(interp, Tcl_GetObjResult(interp));
	rPtr = Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, rPtr, statusPtr->msgFields);
	oPtr = Tcl_NewObj();
	for (i=0; i < statusPtr->numRecipients; i++) {
	    if (!strcmp(statusPtr->recipientPtrPtr[i],
			Tcl_GetString(objv[3]))){
		Tcl_ListObjAppendElement(interp, oPtr,
			Tcl_NewStringObj(statusPtr->recipientPtrPtr[i], -1));
		break;
	    }
	}
	Tcl_ListObjAppendElement(interp, rPtr, oPtr);
	Tcl_SetObjResult(interp, rPtr);
	RatFreeDeliveryStatus(statusPtr);
	RatMessageDelete(interp, msg);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSNExpire --
 *
 *      Check if a given DSN line has expired.
 *
 * Results:
 *	Returns true if it has expired.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatDSNExpire(Tcl_Interp *interp, Tcl_Obj *linePtr)
{
    Tcl_Obj *oPtr;
    long intime;
    int days;

    oPtr = Tcl_GetVar2Ex(interp, "option", "dsn_expiration", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &days);
    Tcl_ListObjIndex(interp, linePtr, 1, &oPtr);
    Tcl_GetLongFromObj(interp, oPtr, &intime);
    return (intime+days*24*60*60 < time(NULL));
}

/*
 *----------------------------------------------------------------------
 *
 * RatParseDSNLine --
 *
 *      Extract the next line of DSN information from a message/delivery-status
 *	message.
 *
 * Results:
 *	The name and value pointers objects are filled in.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static char*
RatParseDSNLine(char *line, Tcl_Obj **name, Tcl_Obj **value, int *length)
{
    char *s, *e, *n;

    *name = *value = NULL;

    /* Find start of name */
    for (s=line; ' ' == *s || '\t' == *s; s++);
    /* Find end of name */
    for (e=s; *e && ':' != *e && '\r' != *e && '\n' != *e; e++);
    if (':' != *e) {
	goto bad;
    }
    n = e+1;
    for (e--; isspace(e[-1]) && e>s; e--);
    *name = Tcl_NewStringObj(s, e-s+1);

    /* Find start of value */
    for (s=n; ' ' == *s || '\t' == *s; s++);
    /* Find end of value */
    for (e=s; (e=strstr(e, "\r\n")) && (' ' == e[2] || '\t' == e[2]); e++);
    if (NULL == e) {
	n = e = s+strlen(s);
    } else {
	n = e+2;
    }
    *value = Tcl_NewStringObj(s, e-s);
    *length -= n-line;
    return n;

 bad:
    if (*name) {
	Tcl_DecrRefCount(*name);
	*name = NULL;
    }
    for (e=line; (e=strstr(e, "\r\n")) && (' '==e[2] || '\t'==e[2]); e++);
    if (e) {
	*length -= e-line;
	return e;
    } else {
	*length -= strlen(line);
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatParseDS --
 *
 *	Parse a message/delivery-status body.
 *
 * Results:
 *	A pointer to a RatDeliveryStatus structure. It is the callers
 *	responsibility to free this pointer later with a call to
 *	RatFreeDeliveryStatus().
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static RatDeliveryStatus*
RatParseDS(Tcl_Interp *interp, Tcl_Obj *bPtr)
{
    RatDeliveryStatus *sPtr = (RatDeliveryStatus*)ckalloc(sizeof(*sPtr));
    char *cPtr, *body;
    int allocated, i, length;
    Tcl_Obj *ov[2];

    body = Tcl_GetStringFromObj(bPtr, &length);

    /*
     * Parse the per message fields.
     */
    sPtr->envid = NULL;
    while (strchr(" \t\015\012", *body)) {
       body++;
       length--;
    }
    sPtr->msgFields = Tcl_NewObj();
    Tcl_IncrRefCount(sPtr->msgFields);
    while (length > 0 && body) {
	body = RatParseDSNLine(body, &ov[0], &ov[1], &length);
	if (!ov[0]) {
	    break;
	}
	Tcl_ListObjAppendElement(interp, sPtr->msgFields,
				 Tcl_NewListObj(2, ov));
	if (!strcasecmp("original-envelope-id", Tcl_GetString(ov[0]))) {
	    sPtr->envid = cpystr(Tcl_GetString(ov[1]));
	}
	/* Added by Lou Ruppert to account for odd DSN implementations */
	if (!strcasecmp("arrival-date", Tcl_GetString(ov[0]))
	    && '\n' != *body
	    && '\r' != *body) {
	    break;
	}
    }

    /*
     * Parse the per recipient fields
     */
    sPtr->numRecipients = 0;
    sPtr->actionPtrPtr = NULL;
    sPtr->recTypePtrPtr = NULL;
    sPtr->recipientPtrPtr = NULL;
    sPtr->rListPtrPtr = NULL;
    allocated = 0;
    while (length > 0 && body) {
	while (isspace(*body) && length > 0) {
	    body++;
	    length--;
	}
	if (!*body) {
	    break;
	}
	if (allocated <= sPtr->numRecipients) {
	    allocated += 32;
	    sPtr->actionPtrPtr = (char**)ckrealloc(sPtr->actionPtrPtr, 
		    allocated*sizeof(char*));
	    sPtr->recTypePtrPtr = (char**)ckrealloc(sPtr->recTypePtrPtr, 
		    allocated*sizeof(char*));
	    sPtr->recipientPtrPtr = (char**)ckrealloc(sPtr->recipientPtrPtr, 
		    allocated*sizeof(char*));
	    sPtr->rListPtrPtr = (Tcl_Obj**)ckrealloc(sPtr->rListPtrPtr, 
		    allocated*sizeof(Tcl_DString));
	}
	i = sPtr->numRecipients++;
	sPtr->actionPtrPtr[i] = NULL;
	sPtr->recTypePtrPtr[i] = NULL;
	sPtr->recipientPtrPtr[i] = NULL;
	sPtr->rListPtrPtr[i] = Tcl_NewObj();
	Tcl_IncrRefCount(sPtr->rListPtrPtr[i]);
	while (length > 0) {
	    body = RatParseDSNLine(body, &ov[0], &ov[1], &length);
	    if (!ov[0]) {
		break;
	    }
	    Tcl_ListObjAppendElement(interp, sPtr->rListPtrPtr[i],
				     Tcl_NewListObj(2, ov));
	    if (!strcasecmp("original-recipient", Tcl_GetString(ov[0]))) {
		sPtr->recTypePtrPtr[i] = cpystr(Tcl_GetString(ov[1]));
		if (NULL != (cPtr = strchr(sPtr->recTypePtrPtr[i], ';'))) {
		    *cPtr++ = '\0';
		}
		sPtr->recipientPtrPtr[i] = cPtr;
	    }
	    if (!strcasecmp("action", Tcl_GetString(ov[0]))) {
		sPtr->actionPtrPtr[i] = cpystr(Tcl_GetString(ov[1]));
	    }
	}
	if (!sPtr->actionPtrPtr[i]) {
	    sPtr->numRecipients--;
	    sPtr->recTypePtrPtr[i] = NULL;
	    sPtr->recipientPtrPtr[i] = NULL;
	}
    }

    return sPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatFreeDeliveryStatus --
 *
 *	Free a RatDeliveryStatus structure.
 *
 * Results:
 *	A pointer to a RatDeliveryStatus structure. It is the callers
 *	responsibility to free this pointer later with a call to
 *	RatFreeDeliveryStatus().
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatFreeDeliveryStatus(RatDeliveryStatus *statusPtr)
{
    int i;

    ckfree(statusPtr->envid);
    Tcl_DecrRefCount(statusPtr->msgFields);
    if (statusPtr->numRecipients) {
	for (i=0; i<statusPtr->numRecipients; i++) {
	    ckfree(statusPtr->actionPtrPtr[i]);
	    ckfree(statusPtr->recTypePtrPtr[i]);
	    Tcl_DecrRefCount(statusPtr->rListPtrPtr[i]);
	}
	ckfree(statusPtr->actionPtrPtr);
	ckfree(statusPtr->recTypePtrPtr);
	ckfree(statusPtr->recipientPtrPtr);
	ckfree(statusPtr->rListPtrPtr);
    }

    ckfree(statusPtr);
}
