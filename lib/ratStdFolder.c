/*
 * ratStdFolder.c --
 *
 *      This file contains code which implements standard c-client folders.
 *	This means ONLY filefolders at this moment.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratStdFolder.h"

/*
 * We use this structure to keep a list of open connections
 */

typedef struct Connection {
    MAILSTREAM *stream;		/* Handler to c-client entity */
    int *errorFlagPtr;		/* Address of flag to set on hard errors */
    int refcount;		/* references count */
    int closing;		/* True if this connection is unused and
				   waiting to be closed */
    int isnet;                  /* Nonnull if this is a network conn */
    Tcl_TimerToken token;	/* Timer token for closing timer */
    struct Connection *next;	/* Struct linkage */
    FolderHandlers *handlers;	/* Event handlers */
} Connection;
FolderHandlers **globHD;

/*
 * Remember if we must initialize the package
 */
static int initialize = 1;

/*
 * List of open connections
 */
static Connection *connListPtr = NULL;

/*
 * The values below are used to catch calls to mm_log. That is when you
 * want to handle the message internally.
 */
static RatLogLevel logLevel;
static char *logMessage = NULL;
int logIgnore = 0;

/*
 * These variables are used by mm_login
 */
static char loginPassword[MAILTMPLEN];
static char loginSpec[MAILTMPLEN];
static int loginStore;

/*
 * This is used to build a list of found mailboxes when listing
 */
typedef struct Mailbox {
    char *name;			/* The en component of name the mailbox name */
    char *folder;      		/* The mailbox name */
    long attributes;		/* The attributes from c-client */
    int delimiter;		/* The delimiter in the folder names */
    struct Mailbox *next;	/* Pointer to the next mailbox on this level */
    struct Mailbox *child;	/* Pointer to subfolders */
} Mailbox;
static Mailbox *mailboxListPtr = NULL;
static char *mailboxSearchBase = NULL;
static char lastDelimiter[2] = {'\0', '\0'};

/*
 * Used to store search results
 */
long *searchResultPtr = NULL;
int searchResultSize = 0;
int searchResultNum = 0;

/*
 * Used to store status results
 */
MAILSTATUS stdStatus;

/*
 * File handler of debugging file
 */
static FILE *debugFile = NULL;

/*
 * Procedures private to this module.
 */
static RatInitProc Std_InitProc;
static RatCloseProc Std_CloseProc;
static RatUpdateProc Std_UpdateProc;
static RatInsertProc Std_InsertProc;
static RatSetFlagProc Std_SetFlagProc;
static RatGetFlagProc Std_GetFlagProc;
static Tcl_TimerProc CloseConnection;
static Tcl_ObjCmdProc StdImportCmd;
static Connection *FindConn(MAILSTREAM *stream);
static void StdImportBuildResult(Tcl_Interp *interp, Mailbox *mPtr,
				 int *lastId, int id,
				 int templatec, Tcl_Obj **templatev);
static HandleExists Std_HandleExists;
static HandleExpunged Std_HandleExpunged;
static RatStdFolderType Std_GetType(const char *spec);


/*
 *----------------------------------------------------------------------
 *
 * RatStdFolderInit --
 *
 *      Initializes the file folder command.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	The C-client library is initialized and the apropriate mail drivers
 *	are linked.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatStdFolderInit(Tcl_Interp *interp)
{
    /* Link imap code */
#include "../imap/c-client/linkage.c"

    Tcl_CreateObjCommand(interp, "RatImport", StdImportCmd, NULL, NULL);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_StreamOpen --
 *
 *      Opens a standard c-client mailstream. This function handles
 *	caching of passwords and connections.
 *
 * Results:
 *	The mail stream.
 *
 * Side effects:
 *	The caches may be modified.
 *
 *----------------------------------------------------------------------
 */

MAILSTREAM*
Std_StreamOpen(Tcl_Interp *interp, char *spec, long options,
	       int *errorFlagPtr, FolderHandlers *handlers)
{
    MAILSTREAM *stream = NULL;
    Connection *connPtr = NULL;
    char *host = NULL, *cPtr;
    int len;

    if ('{' == spec[0]) {
	strlcpy(loginSpec, spec, sizeof(loginSpec));
	cPtr = strchr(loginSpec, '}');
	cPtr[1] = '\0';
	len = strchr(spec, '}') - spec;
	if (NULL != (cPtr = strstr(spec, "/debug}"))) {
	    len = cPtr-spec;
	}
	
	for (connPtr = connListPtr; connPtr; connPtr = connPtr->next) {
	    if ((connPtr->closing || options & OP_HALFOPEN)
		&& !strncmp(spec, connPtr->stream->mailbox, len)) {
		break;
	    }
	}
	if (connPtr) {
	    stream = connPtr->stream;
	    connPtr->refcount++;
	    Tcl_DeleteTimerHandler(connPtr->token);
	    if (connPtr->closing) {
		connPtr->handlers = handlers;
		connPtr->errorFlagPtr = errorFlagPtr;
	    }
	    connPtr->closing = 0;
	}
    }
    if (stream && options & OP_HALFOPEN) {
	ckfree(host);
	return stream;
    }
    loginPassword[0] = '\0';
    stream = mail_open(stream, spec, options);
    if (stream && !connPtr) {
	connPtr = (Connection*)ckalloc(sizeof(Connection));
	connPtr->stream = stream;
	connPtr->errorFlagPtr = errorFlagPtr;
	connPtr->refcount = 1;
	connPtr->closing = 0;
	connPtr->handlers = handlers;
	connPtr->next = connListPtr;
        connPtr->token = NULL;
	connPtr->isnet = (('{' == spec[0]) ? 1 : 0);
	connListPtr = connPtr;
	if (loginPassword[0] != '\0') {
	    RatCachePassword(interp, spec, loginPassword, loginStore);
	    memset(loginPassword, 0, strlen(loginPassword));
	}
    }
    if (!stream && '{' == spec[0]) {
	Tcl_Obj *oPtr;
	int n;

	oPtr = Tcl_GetVar2Ex(interp, "ratNetOpenFailures",
			     NULL, TCL_GLOBAL_ONLY);
	Tcl_GetIntFromObj(interp, oPtr, &n);
	Tcl_SetVar2Ex(interp, "ratNetOpenFailures", NULL, Tcl_NewIntObj(++n),
		      TCL_GLOBAL_ONLY);
    }
    if (errorFlagPtr) {
	*errorFlagPtr = 0;
    }
    ckfree(host);
    return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_StreamClose --
 *
 *      Closes a standard c-client mailstream. This function handles
 *	caching of passwords and connections.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The caches may be modified.
 *
 *----------------------------------------------------------------------
 */

void
Std_StreamClose(Tcl_Interp *interp, MAILSTREAM *stream)
{
    Connection *connPtr;
    Tcl_Obj *oPtr;

    for (connPtr = connListPtr;
	    connPtr && stream != connPtr->stream;
	    connPtr = connPtr->next);
    if (connPtr) {
	int timeout, doCache;

	if (--connPtr->refcount) {
	    return;
	}
	oPtr = Tcl_GetVar2Ex(interp, "option", "cache_conn",
			     TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &doCache);
	if (doCache && RAT_IMAP == Std_GetType(connPtr->stream->mailbox)
	    && (!connPtr->errorFlagPtr || 0 == *connPtr->errorFlagPtr)) {
	    oPtr = Tcl_GetVar2Ex(interp, "option", "cache_conn_timeout",
				 TCL_GLOBAL_ONLY);
	    Tcl_GetIntFromObj(interp, oPtr, &timeout);
	    connPtr->closing = 1;
	    if (connPtr->errorFlagPtr) {
		connPtr->errorFlagPtr = NULL;
	    }
	    if (timeout) {
		connPtr->token = Tcl_CreateTimerHandler(timeout*1000,
			CloseConnection, (ClientData)connPtr);
	    } else {
		connPtr->token = NULL;
	    }
	} else {
	    CloseConnection((ClientData)connPtr);
	}
    } else {
	logIgnore++;
	mail_close_full(stream, NIL);
	logIgnore--;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Std_StreamCloseAllCached --
 *
 *      Forces a close of all cached connections
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The caches may be modified.
 *
 *----------------------------------------------------------------------
 */
void
Std_StreamCloseAllCached(Tcl_Interp *interp)
{
    Connection *connPtr, *nextPtr;

    for (connPtr = connListPtr; connPtr; connPtr = nextPtr) {
	nextPtr = connPtr->next;
	if (connPtr->closing) {
	    Tcl_DeleteTimerHandler(connPtr->token);
	    CloseConnection((ClientData)connPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OpenStdFolder --
 *
 *      Opens a standard c-client folder and if it is a filefolder and
 *	is of an incompatible format (unfortunately generated by an older
 *	version of this program) we convert it.
 *
 * Results:
 *	The mail stream.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

MAILSTREAM*
OpenStdFolder(Tcl_Interp *interp, char *spec, void *voidPtr)
{
    MAILSTREAM *stream = NULL;
    RatStdFolderType type;
    StdFolderInfo *stdPtr = (StdFolderInfo*)voidPtr;
    Tcl_DString dsBuf;
    struct stat sbuf;
    int dsBufUse = 0;

    type = Std_GetType(spec);
    if (RAT_UNIX == type) {
	spec = Tcl_UtfToExternalDString(NULL, spec, -1, &dsBuf);
	dsBufUse = 1;
    }
    if ('/' == spec[0] && stat(spec, &sbuf) && ENOENT == errno) {
	int fd;
	fd = open(spec, O_CREAT | O_WRONLY, 0600);
	close(fd);
    }
    logLevel = RAT_BABBLE;
    stream = Std_StreamOpen(interp, spec, 0, (stdPtr ? &stdPtr->error : NULL),
			    (stdPtr ? &stdPtr->handlers : NULL));
    if (logLevel > RAT_WARN) {
	Tcl_SetResult(interp, logMessage, TCL_VOLATILE);
	return NULL;
    }
    if (NIL == stream) {
	Tcl_AppendResult(interp, "Failed to open std mailbox \"",
			 spec, "\"", (char *) NULL);
	return NULL;
    }
    if (!strcmp(stream->dtb->name, "mbx")) {
	type = RAT_MBX;
    }
    if (stdPtr) {
	stdPtr->stream = stream;
	stdPtr->referenceCount = 1;
	stdPtr->exists = stream->nmsgs;
	stdPtr->type = type;
    }
    if (dsBufUse) {
	Tcl_DStringFree(&dsBuf);
    }
    return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * RatStdFolderCreate --
 *
 *      Creates a std folder entity.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	A std folder is created.
 *
 *
 *----------------------------------------------------------------------
 */

RatFolderInfo*
RatStdFolderCreate(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    RatFolderInfo *infoPtr;
    StdFolderInfo *stdPtr;
    MAILSTREAM *stream = NULL;
    char buf[32];
    Tcl_Obj *oPtr;
    char *spec;
    int i;

    /*
     * Now it is time to initialize things
     */
    if (initialize) {
        char *role, *domain;
        
	role = Tcl_GetVar2(interp, "option", "default_role",TCL_GLOBAL_ONLY);
        domain = RatGetCurrent(interp, RAT_HOST, role);
	env_parameters(SET_LOCALHOST, (void*)domain);
	initialize = 0;
    }

    stdPtr = (StdFolderInfo *) ckalloc(sizeof(*stdPtr));
    stdPtr->handlers.state = (void*)stdPtr;
    stdPtr->handlers.exists = Std_HandleExists;
    stdPtr->handlers.expunged = Std_HandleExpunged;

    if (NULL == (spec = RatGetFolderSpec(interp, defPtr))
	|| NULL == (stream = OpenStdFolder(interp, spec, stdPtr))) {
	ckfree(stdPtr);
	return NULL;
    }

    infoPtr = (RatFolderInfo *) ckalloc(sizeof(*infoPtr)); 

    infoPtr->type = "std";
    Tcl_ListObjIndex(interp, defPtr, 0, &oPtr);
    infoPtr->name = cpystr(Tcl_GetString(oPtr));
    infoPtr->size = -1;
    infoPtr->number = stream->nmsgs;
    infoPtr->recent = stream->recent;
    infoPtr->unseen = 0;
    if (stream->nmsgs) {
	sprintf(buf, "1:%ld", stream->nmsgs);
	mail_fetchfast_full(stream, buf, NIL);
	for (i = 1; i <= stream->nmsgs; i++)
	    if (!mail_elt (stream,i)->seen) infoPtr->unseen++; 
    }
    infoPtr->initProc = Std_InitProc;
    infoPtr->finalProc = NULL;
    infoPtr->closeProc = Std_CloseProc;
    infoPtr->updateProc = Std_UpdateProc;
    infoPtr->insertProc = Std_InsertProc;
    infoPtr->setFlagProc = Std_SetFlagProc;
    infoPtr->getFlagProc = Std_GetFlagProc;
    infoPtr->infoProc = Std_InfoProc;
    infoPtr->setInfoProc = Std_SetInfoProc;
    infoPtr->createProc = Std_CreateProc;
    infoPtr->syncProc = NULL;
    infoPtr->private = (ClientData) stdPtr;

    return infoPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_InitProc --
 *
 *      See the documentation for initProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for initProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static void
Std_InitProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index)
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;
    MessageInfo *msgPtr;
    int i, j, start, end;

    if (-1 == index) {
       start = 0;
       end = infoPtr->number;
    } else {
       start = index;
       end = start+1;
    }
    for (i=start; i<end; i++) {
	msgPtr = (MessageInfo*)ckalloc(sizeof(MessageInfo));
	msgPtr->folderInfoPtr = infoPtr;
	msgPtr->name[0] = '\0';
	msgPtr->type = RAT_CCLIENT_MESSAGE;
	msgPtr->bodyInfoPtr = NULL;
	msgPtr->msgNo = i;
	msgPtr->fromMe = RAT_ISME_UNKOWN;
	msgPtr->toMe = RAT_ISME_UNKOWN;
	msgPtr->clientData = NULL;
	for (j=0; j<RAT_FOLDER_END; j++) {
	    msgPtr->info[j] = NULL;
	}
	infoPtr->privatePtr[i] = (ClientData)msgPtr;
    }
    RatStdMsgStructInit(infoPtr, interp, index, stdPtr->stream, stdPtr->type);
}

/*
 *----------------------------------------------------------------------
 *
 * CloseStdFolder --
 *
 *      See the documentation for closeProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for closeProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
void
CloseStdFolder(Tcl_Interp *interp, MAILSTREAM *stream)
{
    Std_StreamClose(interp, stream);
}


/*
 *----------------------------------------------------------------------
 *
 * Std_CloseProc --
 *
 *      See the documentation for closeProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for closeProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Std_CloseProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int expunge)
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;
    MessageInfo *msgPtr;
    int i, j;

    if (stdPtr->stream) {
	if (expunge) {
	    logIgnore++;
	    mail_expunge(stdPtr->stream);
	    logIgnore--;
	}
	Std_StreamClose(interp, stdPtr->stream);
    }
    if (0 == --stdPtr->referenceCount) {
	for (i=0; i<infoPtr->number; i++) {
	    if (NULL == infoPtr->msgCmdPtr[i]) {
		msgPtr = (MessageInfo*)infoPtr->privatePtr[i];
		if (msgPtr) {
		    for (j=0; j<RAT_FOLDER_END; j++) {
			if (msgPtr->info[j]) {
			    Tcl_DecrRefCount(msgPtr->info[j]);
			    msgPtr->info[j] = NULL;
			}
		    }
		    ckfree(msgPtr->clientData);
		    ckfree(infoPtr->privatePtr[i]);
		}
	    }
	}
	ckfree(stdPtr);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_UpdateProc --
 *
 *      See the documentation for updateProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for updateProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Std_UpdateProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp,RatUpdateType mode)
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;
    int numNew = 0, oldExists, i;
    char sequence[16];

    if (RAT_SYNC == mode) {
	MESSAGECACHE *cachePtr;
	MessageInfo *msgPtr;
	int i, offset = 0;

	if (infoPtr->number) {
	    for (i=0; i<infoPtr->number; i++) {
		cachePtr = mail_elt(stdPtr->stream, i+1);
		if (cachePtr->deleted) {
		    if (-1 != infoPtr->size) {
			infoPtr->size -= cachePtr->rfc822_size;
		    }
		    if (infoPtr->msgCmdPtr[i]) {
			RatMessageDelete(interp, infoPtr->msgCmdPtr[i]);
		    }
		    offset++;
		} else if (offset) {
		    infoPtr->msgCmdPtr[i-offset] = infoPtr->msgCmdPtr[i];
		    infoPtr->privatePtr[i-offset] = infoPtr->privatePtr[i];
		    if (infoPtr->privatePtr[i]) {
			msgPtr = (MessageInfo*)infoPtr->privatePtr[i];
			msgPtr->msgNo = i - offset;
		    }
		}
	    }
	    for (i=infoPtr->number-offset; i<infoPtr->number; i++) {
		infoPtr->msgCmdPtr[i] = NULL;
		infoPtr->privatePtr[i] = NULL;
	    }
	}
	mail_expunge(stdPtr->stream);
	numNew = stdPtr->exists - (infoPtr->number - offset);

    } else if (RAT_CHECKPOINT == mode) {
	oldExists = infoPtr->number;
	mail_check(stdPtr->stream);
	numNew = stdPtr->exists-oldExists;
    } else {
	oldExists = infoPtr->number;
	if (T != mail_ping(stdPtr->stream)) {
	    char buf[1024];
	    stdPtr->stream = NIL;
	    snprintf(buf, sizeof(buf), "%s close 1", infoPtr->cmdName);
	    Tcl_GlobalEval(interp, buf);
	    Tcl_SetResult(interp, "Lost contact with mailbox", TCL_STATIC);
	    Tcl_SetErrorCode(interp, "C_CLIENT", "streamdied", NULL);
	    return -1;
	}
	numNew = stdPtr->exists-oldExists;
    }
    if (numNew) {
	sprintf(sequence, "%d:%d", stdPtr->exists-numNew+1, stdPtr->exists);
	mail_fetchfast_full(stdPtr->stream, sequence, NIL);
    }
    infoPtr->number = stdPtr->exists;
    infoPtr->recent = stdPtr->stream->recent;
    for (i = 1,infoPtr->unseen=0; i <= stdPtr->stream->nmsgs; i++) {
	if (!mail_elt(stdPtr->stream,i)->seen) infoPtr->unseen++;
    }
    return numNew;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_InsertProc --
 *
 *      See the documentation for insertProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for insertProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Std_InsertProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int argc,
	char *argv[])
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;
    char flags[128], date[128];
    Tcl_CmdInfo cmdInfo;
    Tcl_DString ds;
    STRING string;
    int i;

    if (NIL == stdPtr->stream) {
	Tcl_AppendResult(interp, "Failed to open std mailbox \"",
		argv[2], "\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_DStringInit(&ds);
    for (i=0; i<argc; i++) {
 	Tcl_GetCommandInfo(interp, argv[i], &cmdInfo);
	RatMessageGet(interp, (MessageInfo*)cmdInfo.objClientData,
		      &ds, flags, sizeof(flags), date, sizeof(date));
	INIT(&string,mail_string,Tcl_DStringValue(&ds),Tcl_DStringLength(&ds));
	RatPurgeFlags(flags, 1);
	if (!mail_append_full(stdPtr->stream, stdPtr->stream->mailbox,
			      flags, date, &string)){
	    Tcl_SetResult(interp, "mail_append failed", TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_DStringSetLength(&ds, 0);
	if (!stdPtr->exists) {
	    if (T != mail_ping(stdPtr->stream)) {
		char buf[1024];
		Tcl_DStringFree(&ds);
		snprintf(buf, sizeof(buf), "%s close", infoPtr->cmdName);
		Tcl_GlobalEval(interp, buf);
		Tcl_SetResult(interp, "Mailbox stream died", TCL_STATIC);
		Tcl_SetErrorCode(interp, "C_CLIENT", "streamdied", NULL);
		return TCL_ERROR;
	    }
	}
    }
    Tcl_DStringFree(&ds);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_SetFlagProc --
 *
 *      See the documentation for setFlagProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for setFlagProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Std_SetFlagProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index,
	RatFlag flag, int value)
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;
    MessageInfo *msgPtr = (MessageInfo*)infoPtr->privatePtr[index];
    MESSAGECACHE *cachePtr;
    char sequence[8];
    int wasseen;

    if (!stdPtr->stream
	|| stdPtr->stream->rdonly) {
	return TCL_OK;
    }

    if (msgPtr->info[RAT_FOLDER_STATUS]) {
	Tcl_DecrRefCount(msgPtr->info[RAT_FOLDER_STATUS]);
	msgPtr->info[RAT_FOLDER_STATUS] = NULL;
    }

    cachePtr = mail_elt(stdPtr->stream, index+1);
    wasseen = cachePtr->seen;
    sprintf(sequence, "%d", index+1);
    if (value) {
	mail_setflag_full(stdPtr->stream, sequence,
			  flag_name[flag].imap_name, NIL);
    } else {
	mail_clearflag_full(stdPtr->stream, sequence,
			    flag_name[flag].imap_name, NIL);
    }
    (void)mail_fetchenvelope(stdPtr->stream, index+1);
    cachePtr = mail_elt(stdPtr->stream, index+1);
    switch (flag) {
	case RAT_SEEN:	   
		if (wasseen != value) {
		    if (wasseen) {
			infoPtr->unseen++;
		    } else {
			infoPtr->unseen--;
		    }
		}
		cachePtr->seen = value; break;
		break;
	case RAT_DELETED:  cachePtr->deleted = value; break;
	case RAT_FLAGGED:  cachePtr->flagged = value; break;
	case RAT_ANSWERED: cachePtr->answered = value; break;
	case RAT_DRAFT:	   cachePtr->draft = value; break;
	case RAT_RECENT:   cachePtr->recent = value; break;
    }
    infoPtr->recent = stdPtr->stream->recent;
    if (logLevel > RAT_WARN) {
	Tcl_SetResult(interp, logMessage, TCL_VOLATILE);
	return TCL_ERROR;
    } else {
	return TCL_OK;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Std_GetFlagProc --
 *
 *      See the documentation for getFlagProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for getFlagProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Std_GetFlagProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index,
	RatFlag flag)
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;
    MESSAGECACHE *cachePtr;
    char sequence[8];
    int value = 0;

    if (!stdPtr->stream) return 0;

    sprintf(sequence, "%d", index+1);
    logLevel = RAT_BABBLE;
    (void)mail_fetchstructure_full(stdPtr->stream, index+1, NIL, NIL);
    cachePtr = mail_elt(stdPtr->stream, index+1);
    switch (flag) {
    case RAT_SEEN:	value = cachePtr->seen; break;
    case RAT_DELETED:	value = cachePtr->deleted; break;
    case RAT_FLAGGED:	value = cachePtr->flagged; break;
    case RAT_ANSWERED:	value = cachePtr->answered; break;
    case RAT_DRAFT:	value = cachePtr->draft; break;
    case RAT_RECENT:	value = cachePtr->recent; break;
    }
    return value;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_InfoProc --
 *
 *      See the documentation for infoProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for infoProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
Std_InfoProc(Tcl_Interp *interp, ClientData clientData, RatFolderInfoType type,
	int index)
{
    RatFolderInfo *infoPtr = (RatFolderInfo*)clientData;

    return Std_GetInfoProc(interp, (ClientData)infoPtr->privatePtr[index],
	    type, 0);
}


/*
 *----------------------------------------------------------------------
 *
 * Std_SetInfoProc --
 *
 *      See the documentation for setInfoProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for setInfoProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */

void
Std_SetInfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index, Tcl_Obj *oPtr)
{
    RatFolderInfo *infoPtr = (RatFolderInfo*)clientData;
    MessageInfo *msgPtr = (MessageInfo*)infoPtr->privatePtr[index];

    if (msgPtr->info[type]) {
	Tcl_DecrRefCount(msgPtr->info[type]);
    }
    msgPtr->info[type] = oPtr;
    if (oPtr) {
	Tcl_IncrRefCount(oPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Std_CreateProc --
 *
 *      See the documentation for createProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for createProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
char*
Std_CreateProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index)
{
    StdFolderInfo *stdPtr = (StdFolderInfo *) infoPtr->private;

    return RatStdMessageCreate(interp, infoPtr, stdPtr->stream, index);
}


/*
 *----------------------------------------------------------------------
 *
 * StdImportCmd --
 *
 *      Import folders (via mm_list)
 *
 * Results:
 *	The folders found are returned as a list
 *
 * Side effects:
 *	RatLogin may be called.
 *
 *
 *----------------------------------------------------------------------
 */
static int
StdImportCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *oPtr, **iobjv, **bobjv, *origPtr;
    int iobjc, bobjc, subscribed, i, lastId, id;
    char *spec, path[1024], buf[1024];
    MAILSTREAM *stream = NULL;

    /*
     * Check arguments
     *  check that we got one
     *  check that it is is a folder definition id and that it
     *    points to a correct import-folder.
     */
    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " id\"", (char *) NULL);
	return TCL_ERROR;
    }
    origPtr = Tcl_GetVar2Ex(interp, "vFolderDef", Tcl_GetString(objv[1]),
			 TCL_GLOBAL_ONLY);

    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &id)
	|| NULL == origPtr
	|| TCL_OK != Tcl_ListObjGetElements(interp, origPtr, &iobjc, &iobjv)
	|| 6 != iobjc
	|| strcmp("import", Tcl_GetString(iobjv[1]))
	|| TCL_OK != Tcl_ListObjGetElements(interp, iobjv[3], &bobjc, &bobjv)){
	Tcl_AppendResult(interp, "Bad folder id specified \"",
		Tcl_GetString(objv[1]), "\"", (char *) NULL);
	return TCL_ERROR;
    }

    spec = RatGetFolderSpec(interp, iobjv[3]);
    logIgnore++;
    stream = Std_StreamOpen(interp, spec, OP_HALFOPEN, NULL, NULL);
    logIgnore--;

    /*
     * See if we only want subscribed folders
     */
    Tcl_ListObjLength(interp, iobjv[2], &i);
    for (subscribed = 0, i -= 2; i>=0; i -= 2) {
	Tcl_ListObjIndex(interp, iobjv[2], i, &oPtr);
	if (!strcmp("subscribed", Tcl_GetString(oPtr))) {
	    Tcl_ListObjIndex(interp, iobjv[2], i+1, &oPtr);
	    Tcl_GetIntFromObj(interp, oPtr, &subscribed);
	    break;
	}
    }

    /*
     * Run search
     * This builds a list of all found folders in mailboxListPtr
     * First we run a dummy-search to get the hierarchy delimiter
     */
    if ((mailboxSearchBase = strchr(spec, '}'))) {
	mailboxSearchBase++;
    } else {
	mailboxSearchBase = spec;
    }
    mail_list(stream, "", spec);
    strlcpy(buf, spec, sizeof(buf));
    if (*mailboxSearchBase
	&& lastDelimiter[0] != mailboxSearchBase[strlen(mailboxSearchBase)-1]
	&& lastDelimiter[0] != Tcl_GetString(iobjv[4])[0]) {
	strlcat(buf, lastDelimiter, sizeof(buf));
    }
    strlcat(buf, Tcl_GetString(iobjv[4]), sizeof(buf));
    if (subscribed) {
	mail_lsub(stream, "", buf);
    } else {
	mail_list(stream, "", buf);
    }
    if (stream) {
	Std_StreamClose(interp, stream);
    }

    /*
     * Compare list in mailboxListPtr with already existing list
     */
    if ('{' == spec[0]) {
	strlcpy(path, strchr(spec, '}')+1, sizeof(path));
    } else {
	strlcpy(path, spec, sizeof(path));
    }
    if ('*' == path[strlen(path)-1] || '%' == path[strlen(path)-1]) {
	path[strlen(path)-1] = '\0';
    }
    if (mailboxListPtr && path[strlen(path)-1] == mailboxListPtr->delimiter) {
	path[strlen(path)-1] = '\0';
    }
    snprintf(buf, sizeof(buf),
	     "lindex [lsort -integer [array names vFolderDef]] end");
    Tcl_GlobalEval(interp, buf);
    lastId = atoi(Tcl_GetStringResult(interp));
    StdImportBuildResult(interp, mailboxListPtr, &lastId, id,
			 bobjc, bobjv);

    
    mailboxListPtr = NULL;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StdImportBuildResult --
 *
 *      Recursive function which parses the import result into folders
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The vFolderDef array may be modified
 *
 *
 *----------------------------------------------------------------------
 */

typedef struct {
    int id;
    int objc;
    Tcl_Obj **objv;
} Mbox;

static void
StdImportBuildResult(Tcl_Interp *interp, Mailbox *mPtr, int *lastId,
		     int id, int templatec, Tcl_Obj **templatev)
{
    Tcl_Obj **objv, *oPtr, *lPtr, *iPtr, *idList;
    Mailbox *nPtr;
    Mbox *mbox_in, *mbox_out = NULL;
    int i, num_mbox_in, mbox_out_alloc = 0, count, changed = 0,
	disconnected = 0, listpos;
    char buf[32];

    if (!strcmp("dis", Tcl_GetString(templatev[1]))) {
	disconnected = 1;
    }

    /*
     * Split list of ids and create list of definitions
     */
    snprintf(buf, sizeof(buf), "%d", id);
    oPtr = Tcl_GetVar2Ex(interp, "vFolderDef", buf, TCL_GLOBAL_ONLY);
    Tcl_ListObjIndex(interp, oPtr, 1, &iPtr);
    if (!strcmp("struct", Tcl_GetString(iPtr))) {
	listpos = 3;
    } else {
	listpos = 5;
    }
    Tcl_ListObjIndex(interp, oPtr, listpos, &idList);
    Tcl_ListObjGetElements(interp, idList, &num_mbox_in, &objv);
    mbox_in = (Mbox*)ckalloc(sizeof(Mbox)*num_mbox_in);
    for (i=0; i<num_mbox_in; i++) {
	Tcl_GetIntFromObj(interp, objv[i], &mbox_in[i].id);
	oPtr = Tcl_GetVar2Ex(interp, "vFolderDef", Tcl_GetString(objv[i]),
			     TCL_GLOBAL_ONLY);
	Tcl_ListObjGetElements(interp, oPtr,&mbox_in[i].objc,&mbox_in[i].objv);
    }

    /*
     * Loop over found mailboxes on this level.
     * For each one start by locating it in the list.
     * If found, then move to outlist
     * Finally if it is a struct, then check list of contained folders
     */
    for (count = 0; mPtr; mPtr = nPtr) {
	if (count+2 >= mbox_out_alloc) {
	    mbox_out_alloc += 100;
	    mbox_out = (Mbox*)ckrealloc(mbox_out, mbox_out_alloc*sizeof(Mbox));
	}
	for (i=0; i<num_mbox_in; i++) {
	    if (mbox_in[i].id != 0
		&& !strcmp(mPtr->name, Tcl_GetString(mbox_in[i].objv[0]))
		&& ((!strcmp("struct", Tcl_GetString(mbox_in[i].objv[1]))
		     && 0 == (LATT_NOINFERIORS & mPtr->attributes))
		    || ((strcmp("struct", Tcl_GetString(mbox_in[i].objv[1]))
			 && 0 != (LATT_NOINFERIORS & mPtr->attributes))))) {
		break;
	    }
	}
	if (i == num_mbox_in) { /* Not found => new */
	    changed = 1;
	    if (0 == (mPtr->attributes & LATT_NOSELECT)) {
		/* Create ordinary folder */
		mbox_out[count].id = ++(*lastId);
		lPtr = Tcl_NewObj();
		Tcl_ListObjAppendElement(interp, lPtr,
					 Tcl_NewStringObj(mPtr->name, -1));
		Tcl_ListObjAppendElement(interp, lPtr, templatev[1]);
		Tcl_ListObjAppendElement(interp, lPtr, templatev[2]);
		if (5 == templatec) {
		    Tcl_ListObjAppendElement(interp, lPtr, templatev[3]);
		}
		Tcl_ListObjAppendElement(interp, lPtr,
					 Tcl_NewStringObj(mPtr->folder, -1));
		snprintf(buf, sizeof(buf), "%d", mbox_out[count].id);
		Tcl_SetVar2Ex(interp, "vFolderDef", buf, lPtr,TCL_GLOBAL_ONLY);
		Tcl_ListObjGetElements(interp, lPtr, &mbox_out[count].objc,
				       &mbox_out[count].objv);
		if (disconnected) {
		    RatDisManageFolder(interp, RAT_MGMT_CREATE, lPtr);
		}
		count++;
	    }
	    if (0 == (mPtr->attributes & LATT_NOINFERIORS)) {
		/* Create struct */
		mbox_out[count].id = ++(*lastId);
		lPtr = Tcl_NewObj();
		Tcl_ListObjAppendElement(interp, lPtr,
					 Tcl_NewStringObj(mPtr->name, -1));
		Tcl_ListObjAppendElement(interp, lPtr,
					 Tcl_NewStringObj("struct", 6));
		Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewObj());
		Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewObj());
		snprintf(buf, sizeof(buf), "%d", mbox_out[count].id);
		Tcl_SetVar2Ex(interp, "vFolderDef", buf, lPtr,TCL_GLOBAL_ONLY);
		Tcl_ListObjGetElements(interp, lPtr, &mbox_out[count].objc,
				       &mbox_out[count].objv);
		count++;
	    }
	} else { /* Found => old */
	    mbox_out[count].id = mbox_in[i].id;
	    mbox_out[count].objc = mbox_in[i].objc;
	    mbox_out[count].objv = mbox_in[i].objv;
	    mbox_in[i].id = 0;
	    count++;
	}
	if (mPtr->child) {
	    StdImportBuildResult(interp, mPtr->child, lastId,
				 mbox_out[count-1].id, templatec, templatev);
	}
	nPtr = mPtr->next;
	ckfree(mPtr);
    }
    for (i=0; i<num_mbox_in; i++) {
	if (0 != mbox_in[i].id) {
	    changed = 1;
	    snprintf(buf, sizeof(buf), "%d", mbox_in[i].id);
	    Tcl_UnsetVar2(interp, "vFolderDef", buf, TCL_GLOBAL_ONLY);
	}
    }
    /* Build result */
    if (changed) {
	oPtr = Tcl_NewObj();
	for (i=0; i<count; i++) {
	    Tcl_ListObjAppendElement(interp, oPtr,
				     Tcl_NewIntObj(mbox_out[i].id));
	}
	/* Change vFolderDef */
	snprintf(buf, sizeof(buf), "%d", id);
	lPtr = Tcl_GetVar2Ex(interp, "vFolderDef", buf, TCL_GLOBAL_ONLY);
	lPtr = Tcl_DuplicateObj(lPtr);
	Tcl_ListObjReplace(interp, lPtr, listpos, 1, 1, &oPtr);
	Tcl_SetVar2Ex(interp, "vFolderDef", buf, lPtr, TCL_GLOBAL_ONLY);
    } else {
	oPtr = idList;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * CloseConnection --
 *
 *      Closes a connection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The connection list is modified.
 *
 *
 *----------------------------------------------------------------------
 */
static void
CloseConnection(ClientData clientData)
{
    Connection **connPtrPtr, *connPtr = (Connection*)clientData;

    Tcl_DeleteTimerHandler(connPtr->token);
    logIgnore++;
    mail_close_full(connPtr->stream, NIL);
    logIgnore--;
    for (connPtrPtr = &connListPtr; *connPtrPtr != connPtr;
	    connPtrPtr = &(*connPtrPtr)->next);
    *connPtrPtr = connPtr->next;
    ckfree(connPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * AppendToIMAP --
 *
 *      Append the given message to an IMAP folder
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The specified folder will be modified
 *
 *
 *----------------------------------------------------------------------
 */

void
AppendToIMAP(Tcl_Interp *interp, const char *mailboxSpec, const char *flags,
	     const char *date, const char *msg, int length)
{
    char *mailbox;
    MAILSTREAM *stream;
    STRING msgString;
    int error;

    mailbox = RatLindex(interp, mailboxSpec, 0);
    if (NULL == (stream = Std_StreamOpen(interp, mailbox, 0, &error, NULL))) {
	return;
    }

    INIT(&msgString, mail_string, (char*)msg, length);
    mail_append_full(stream, (char*)mailbox, (char*)flags, (char*)date,
		     &msgString);

    Std_StreamClose(interp, stream);
}

/*
 *----------------------------------------------------------------------
 *
 * FindConn --
 *
 *      Find the connection pointer for the stream
 *
 * Results:
 *	A connection pointer (or NULL)
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */

static Connection*
FindConn(MAILSTREAM *stream)
{
    Connection *connPtr;

    for (connPtr=connListPtr;
	    connPtr && connPtr->stream != stream;
	    connPtr=connPtr->next);
    return connPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Handle* --
 *
 *      Handle events from mailbox
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */

static void
Std_HandleExists(void *state, unsigned long nmsg)
{ 
    StdFolderInfo *stdPtr = (StdFolderInfo *) state;
    stdPtr->exists = nmsg;
}   

static void
Std_HandleExpunged(void *state, unsigned long index)
{ 
    StdFolderInfo *stdPtr = (StdFolderInfo *) state;
    stdPtr->exists--;
}   

/*
 *----------------------------------------------------------------------
 *
 * Std_GetType --
 *
 *      Determines the type of folder from a mailbox stream name
 *
 * Results:
 *	The type
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
static RatStdFolderType
Std_GetType(const char *spec)
{
    const char *c;
    RatStdFolderType type;

    if ('{' == spec[0]) {
	type = RAT_IMAP;
	for (c=spec+1; *c != '}'; c++) {
	    if ('/' == c[0]
		&& 'p' == c[1] && 'o' == c[2] && 'p' == c[3] && '3' == c[4]) {
		type = RAT_POP;
		break;
	    }
	}
    } else if ('#' == spec[0] && 'm' == spec[1] && 'h' == spec[2]) {
	type = RAT_MH;
    } else {
	type = RAT_UNIX;
    }
    return type;
}

/*
 *----------------------------------------------------------------------
 *
 * RatStdManageFolder --
 *
 *      Create or delete folders on disk ro remote server
 *
 * Results:
 *	A standard tcl result
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
int
RatStdManageFolder(Tcl_Interp *interp, RatManagementAction op, Tcl_Obj *fPtr)
{
    MAILSTREAM *stream;
    struct stat sbuf;
    char *spec;
    Tcl_Obj *oPtr;
    int result, error;

    spec = RatGetFolderSpec(interp, fPtr);
    if ('{' == spec[0]) {
	stream = Std_StreamOpen(interp, spec, OP_HALFOPEN, &error, NULL);
	if (!stream) {
	    Tcl_SetResult(interp,"Failed to open stream to server",TCL_STATIC);
	    return TCL_ERROR;
	}
    } else {
	stream = NULL;
    }
    if (op == RAT_MGMT_CREATE) {
	if ('/' == spec[0]) {
	    /*
	     * Since this is a file folder we check if the file already
	     * exists, and do nothing if that is the case. This is to
	     * avoid getting an error message
	     */
	    if (0 == stat(spec, &sbuf)) {
		return TCL_OK;
	    }
	}
	result = mail_create(stream, spec);
    } else {
	logIgnore++;
	(void)mail_delete(stream, spec);
	logIgnore--;
	result = 1;
    }
    if (stream) {
	Std_StreamClose(interp, stream);
    }
    Tcl_ListObjIndex(interp, fPtr, 1, &oPtr);
    if (result && !strcmp("dis", Tcl_GetString(oPtr))) {
	RatDisManageFolder(interp, op, fPtr);
    }

    if (result) {
	return TCL_OK;
    } else {
	Tcl_SetResult(interp, "Failed to create folder", TCL_STATIC);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatStdCheckNet --
 *
 *      Check if we have any network connections which are active
 *      and if not go offline.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
void RatStdCheckNet(Tcl_Interp *interp)
{
    Connection *connPtr;
    char buf[64];
    int existsnetok = 0;

    for (connPtr = connListPtr; connPtr; connPtr = connPtr->next) {
	if (connPtr->isnet
	    && (!connPtr->errorFlagPtr || 0 == *connPtr->errorFlagPtr)) {
	    existsnetok = 1;
	}
    }
    if (0 == existsnetok) {
	strlcpy(buf, "SetOnlineStatus 0", sizeof(buf));
	Tcl_Eval(interp, buf);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * mm_*
 *
 *	The functions below are called from the C-client library. They
 *	are docuemnted in Internal.DOC.
 *
 *----------------------------------------------------------------------
 */
void mm_searched (MAILSTREAM *stream,unsigned long number)
{
    if (searchResultSize == searchResultNum) {
	searchResultSize += 1024;
	searchResultPtr = (long*)ckrealloc(searchResultPtr,
		searchResultSize*sizeof(long));
    }
    searchResultPtr[searchResultNum++] = number;
}


void mm_exists (MAILSTREAM *stream, unsigned long nmsgs)
{
    Connection *connPtr = FindConn(stream);

    if (connPtr && connPtr->handlers && connPtr->handlers->exists) {
	(*connPtr->handlers->exists)(connPtr->handlers->state, nmsgs);
    }
}


void mm_expunged (MAILSTREAM *stream, unsigned long index)
{
    Connection *connPtr = FindConn(stream);

    if (connPtr && connPtr->handlers && connPtr->handlers->expunged) {
	(*connPtr->handlers->expunged)(connPtr->handlers->state, index);
    }
}


void mm_mailbox (char *string)
{
}


void mm_bboard (char *string)
{
}


void mm_notify (MAILSTREAM *stream,char *string,long errflg)
{
    if (errflg == BYE) {
	Connection *connPtr = FindConn(stream);
	if (connPtr && connPtr->errorFlagPtr) {
	    *connPtr->errorFlagPtr = 1;
	}
    }
}


void mm_log (char *string,long errflg)
{
    switch(errflg) {
    case NIL:	logLevel = RAT_BABBLE; break;
    case PARSE:	logLevel = RAT_PARSE; break;
    case WARN:	logLevel = RAT_WARN; break;
    case BYE:	logLevel = RAT_FATAL; break;
    case ERROR:	/* fallthrough */
    default:	logLevel = RAT_ERROR; break;
    }

    ckfree(logMessage);
    logMessage = cpystr(string);

    if (logIgnore) {
	return;
    }

    RatLog(timerInterp, logLevel, string, RATLOG_NOWAIT);
}


void mm_dlog (char *string)
{
    CONST84 char *filename;

    if (!debugFile
	&& NULL != (filename = RatGetPathOption(timerInterp, "debug_file"))) {
	debugFile = fopen(filename, "a");
	if (debugFile) {
	    fchmod(fileno(debugFile), 0600);
	}
    }
    
    if (debugFile) {
	fprintf(debugFile, "%s\n", string);
	fflush(debugFile);
    }
    RatLog(timerInterp, RAT_BABBLE, string, RATLOG_TIME);
}


void mm_login (NETMBX *mbPtr, char *user, char *pwd, long trial)
{
    char *pw;
    int objc;
    Tcl_Obj *oPtr, **objv;

    /*
     * Check for cached entry
     */
    if ((pw = RatGetCachedPassword(timerInterp, loginSpec))) {
	strlcpy(user, mbPtr->user, MAILTMPLEN);
	strlcpy(pwd, pw, MAILTMPLEN);
	return;
    }
    oPtr = Tcl_NewObj();
    Tcl_ListObjAppendElement(timerInterp, oPtr,
			     Tcl_NewStringObj("RatLogin", -1));
    Tcl_ListObjAppendElement(timerInterp, oPtr,
			     Tcl_NewStringObj(mbPtr->host, -1));
    Tcl_ListObjAppendElement(timerInterp, oPtr,
			     Tcl_NewLongObj(trial));
    Tcl_ListObjAppendElement(timerInterp, oPtr,
			     Tcl_NewStringObj(mbPtr->user, -1));
    Tcl_ListObjAppendElement(timerInterp, oPtr,
			     Tcl_NewStringObj(mbPtr->service,-1));
    Tcl_ListObjAppendElement(timerInterp, oPtr, Tcl_NewLongObj(mbPtr->port));
    if (TCL_OK != Tcl_EvalObj(timerInterp, oPtr)
	|| NULL == (oPtr = Tcl_GetObjResult(timerInterp))
	|| TCL_OK != Tcl_ListObjGetElements(timerInterp, oPtr, &objc, &objv)
	|| 3 != objc) {
	pwd[0] = '\0';
	return;
    }
    strlcpy(user, Tcl_GetString(objv[0]), MAILTMPLEN);
    strlcpy(pwd, Tcl_GetString(objv[1]), MAILTMPLEN);
    if ('\0' != user[0]) {
	strlcpy(loginPassword, Tcl_GetString(objv[1]), MAILTMPLEN);
	Tcl_GetBooleanFromObj(timerInterp, objv[2], &loginStore);
    } else {
	/* User pressed cancel */
	loginStore = 0;
	logIgnore++;
    }
}


void mm_critical (MAILSTREAM *stream)
{
}


void mm_nocritical (MAILSTREAM *stream)
{
}


long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
    char buf[64];

    sprintf(buf, "Disk error: %ld", errcode);
    RatLog(timerInterp, RAT_FATAL, buf, RATLOG_TIME);
    return 1;
}


void mm_fatal (char *string)
{
    RatLog(timerInterp, RAT_FATAL, string, RATLOG_TIME);
}

void mm_flags (MAILSTREAM *stream,unsigned long number)
{
}


void
mm_list(MAILSTREAM *stream, int delimiter, char *spec, long attributes)
{
    Mailbox **mPtrPtr = &mailboxListPtr, *nPtr;
    char *name, *folder, *s, *e;
    int do_decode = 0;
    Tcl_DString *encoded;

    lastDelimiter[0] = delimiter;
    if ('{' == spec[0]) {
	for (s=spec; *s && 0 == (*s & 0x80); s++);
	if (!*s) {
	    do_decode = 1;
	}
    }
    /*
     * Create new Mailbox structure
     */
    if ((folder = strchr(spec, '}'))) {
	folder++;
    } else {
	folder = spec;
    }
    if (delimiter && (NULL != (name = strrchr(folder, delimiter)))) {
	name++;
    } else {
	name = folder;
    }
    if (!*name && !(attributes & LATT_NOSELECT)) {
	return;
    }

    /*
     * First find the right level
     */
    if (!strncmp(mailboxSearchBase, folder, strlen(mailboxSearchBase))) {
	s = folder+strlen(mailboxSearchBase);
    } else {
	s = folder;
    }
    for (; delimiter && (e = strchr(s, delimiter));
	 *e = delimiter, s = e+1) {
	*e = '\0';
	if (!strlen(s)) {
	    continue;
	}
	while (*mPtrPtr && 0 > strcmp((*mPtrPtr)->name, s)) {
	    mPtrPtr = &(*mPtrPtr)->next;
	}
	if (!*mPtrPtr || strcmp((*mPtrPtr)->name, s)) {
	    nPtr = (Mailbox*)ckalloc(sizeof(Mailbox)+strlen(s)*3+1);
	    nPtr->name = (char*)nPtr+sizeof(Mailbox);
	    strcpy(nPtr->name, (do_decode ? RatMutf7toUtf8(s) : s));
	    nPtr->folder = NULL;
	    nPtr->attributes = LATT_NOSELECT;
	    nPtr->next = *mPtrPtr;
	    nPtr->child = NULL;
	    *mPtrPtr = nPtr;
	    mPtrPtr = &nPtr->child;
	} else {
	    mPtrPtr = &(*mPtrPtr)->child;
	}
    }

    if (attributes & LATT_NOSELECT) {
	return;
    }

    /*
     * Find location and link it
     */
    while (*mPtrPtr && 0 > strcmp((*mPtrPtr)->name, name)) {
	mPtrPtr = &(*mPtrPtr)->next;
    }

    /*
     * Ignore duplicates
     */
    encoded = RatEncodeQP(folder);
    if (*mPtrPtr && (*mPtrPtr)->folder
	&& !strcmp((*mPtrPtr)->folder, Tcl_DStringValue(encoded))
	&& (*mPtrPtr)->attributes == attributes) {
	Tcl_DStringFree(encoded);
	ckfree(encoded);
	return;
    }

    /*
     * Create actual folder entry
     */
    nPtr = (Mailbox*)ckalloc(
	sizeof(Mailbox) + strlen(name)*3 + Tcl_DStringLength(encoded) + 2);
    nPtr->name = (char*)nPtr+sizeof(Mailbox);
    strcpy(nPtr->name, (do_decode ? RatMutf7toUtf8(name) : name));
    nPtr->folder = nPtr->name+strlen(nPtr->name)+1;
    strcpy(nPtr->folder, Tcl_DStringValue(encoded));
    nPtr->attributes = attributes;
    nPtr->delimiter = delimiter;
    nPtr->next = *mPtrPtr;
    nPtr->child = NULL;
    *mPtrPtr = nPtr;
    Tcl_DStringFree(encoded);
    ckfree(encoded);
}


void
mm_lsub (MAILSTREAM *stream, int delimiter, char *name, long attributes)
{
    mm_list(stream, delimiter, name, attributes | LATT_NOINFERIORS);
}


void mm_status (MAILSTREAM *stream, char *mailbox, MAILSTATUS *status)
{
    memcpy(&stdStatus, status, sizeof(MAILSTATUS));
}

#ifdef MEM_DEBUG
void ratStdFolderCleanup()
{
    ckfree(logMessage);
}
#endif /* MEM_DEBUG */
