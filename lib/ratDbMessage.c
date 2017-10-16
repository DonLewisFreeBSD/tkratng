/*
 * ratDbMessage.c --
 *
 *	This file contains code which implements dbase messages.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"

/*
 * The ClientData for each message entity
 */
typedef struct DbMessageInfo {
    int index;
    char *buffer;
    MESSAGE *messagePtr;
} DbMessageInfo;

/*
 * The number of message entities created. This is used to create new
 * unique command names.
 */
static int numDbMessages = 0;

static RatGetHeadersProc Db_GetHeadersProc;
static RatGetEnvelopeProc Db_GetEnvelopeProc;
static RatInfoProc Db_GetInfoProc;
static RatCreateBodyProc Db_CreateBodyProc;
static RatFetchTextProc Db_FetchTextProc;
static RatEnvelopeProc Db_EnvelopeProc;
static RatMsgDeleteProc Db_MsgDeleteProc;
static RatMakeChildrenProc Db_MakeChildrenProc;
static RatFetchBodyProc Db_FetchBodyProc;
static RatBodyDeleteProc Db_BodyDeleteProc;
static RatGetInternalDateProc Db_GetInternalDateProc;
static RatMsgDbInfoProc Db_MsgDbInfoGetProc;


/*
 *----------------------------------------------------------------------
 *
 * RatDbMessagesInit --
 *
 *      Initializes the given MessageProcInfo entry for a dbase message
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given MessageProcInfo is initialized.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatDbMessagesInit(MessageProcInfo *messageProcInfoPtr)
{
    messageProcInfoPtr->getHeadersProc = Db_GetHeadersProc;
    messageProcInfoPtr->getEnvelopeProc = Db_GetEnvelopeProc;
    messageProcInfoPtr->getInfoProc = Db_GetInfoProc;
    messageProcInfoPtr->createBodyProc = Db_CreateBodyProc;
    messageProcInfoPtr->fetchTextProc = Db_FetchTextProc;
    messageProcInfoPtr->envelopeProc = Db_EnvelopeProc;
    messageProcInfoPtr->msgDeleteProc = Db_MsgDeleteProc;
    messageProcInfoPtr->makeChildrenProc = Db_MakeChildrenProc;
    messageProcInfoPtr->fetchBodyProc = Db_FetchBodyProc;
    messageProcInfoPtr->bodyDeleteProc = Db_BodyDeleteProc;
    messageProcInfoPtr->getInternalDateProc = Db_GetInternalDateProc;
    messageProcInfoPtr->dbinfoGetProc = Db_MsgDbInfoGetProc;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbMessageCreate --
 *
 *      Creates a dbase message entity
 *
 * Results:
 *	The name of the new message entity.
 *
 * Side effects:
 *	None.
 *	deleted.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatDbMessageCreate(Tcl_Interp *interp, RatFolderInfoPtr infoPtr, int index,
	int dbIndex)
{
    DbMessageInfo *dbMsgPtr=(DbMessageInfo*)ckalloc(sizeof(DbMessageInfo));
    MessageInfo *msgPtr=(MessageInfo*)ckalloc(sizeof(MessageInfo));
    int i;

    msgPtr->folderInfoPtr = infoPtr;
    msgPtr->type = RAT_DBASE_MESSAGE;
    msgPtr->bodyInfoPtr = NULL;
    msgPtr->msgNo = index;
    msgPtr->fromMe = RAT_ISME_UNKOWN;
    msgPtr->toMe = RAT_ISME_UNKOWN;
    msgPtr->clientData = (ClientData)dbMsgPtr;
    for (i=0; i<sizeof(msgPtr->info)/sizeof(*msgPtr->info); i++) {
	msgPtr->info[i] = NULL;
    }
    dbMsgPtr->index = dbIndex;
    dbMsgPtr->messagePtr = RatDbGetMessage(interp, dbIndex, &dbMsgPtr->buffer);
    sprintf(msgPtr->name, "RatDbMsg%d", numDbMessages++);
    Tcl_CreateObjCommand(interp, msgPtr->name, RatMessageCmd,
	    (ClientData) msgPtr, NULL);
    return msgPtr->name;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_GetHeadersProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Db_GetHeadersProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    DbMessageInfo *dbMsgPtr=(DbMessageInfo*)msgPtr->clientData;
    return RatDbGetHeaders(interp, dbMsgPtr->index);
}


/*
 *----------------------------------------------------------------------
 *
 * Db_GetEnvelopeProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Db_GetEnvelopeProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    static char buf[1024];
    DbMessageInfo *dbMsgPtr=(DbMessageInfo*)msgPtr->clientData;
    RatDbEntry *entryPtr = RatDbGetEntry(dbMsgPtr->index);
    struct tm *tmPtr;
    time_t date;

    date = atoi(entryPtr->content[DATE]);
    tmPtr = gmtime(&date);
    snprintf(buf, sizeof(buf), "From %s %s %s %2d %02d:%02d GMT %04d\n",
	    entryPtr->content[FROM], dayName[tmPtr->tm_wday],
	    monthName[tmPtr->tm_mon], tmPtr->tm_mday, tmPtr->tm_hour,
	    tmPtr->tm_min, tmPtr->tm_year+1900);
    return buf;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_CreateBodyProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static BodyInfo*
Db_CreateBodyProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    DbMessageInfo *dbMsgPtr = (DbMessageInfo*)msgPtr->clientData;
    msgPtr->bodyInfoPtr = CreateBodyInfo(interp, msgPtr,
					 dbMsgPtr->messagePtr->body);
    return msgPtr->bodyInfoPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_FetchTextProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Db_FetchTextProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    DbMessageInfo *dbMsgPtr = (DbMessageInfo*)msgPtr->clientData;
    return RatDbGetText(interp, dbMsgPtr->index);
}


/*
 *----------------------------------------------------------------------
 *
 * Db_EnvelopeProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static ENVELOPE*
Db_EnvelopeProc(MessageInfo *msgPtr)
{
    return ((DbMessageInfo*)msgPtr->clientData)->messagePtr->env;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_MsgDeleteProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static void
Db_MsgDeleteProc(MessageInfo *msgPtr)
{
    DbMessageInfo *dbMsgPtr = (DbMessageInfo*)msgPtr->clientData;
    mail_free_body(&dbMsgPtr->messagePtr->body);
    mail_free_envelope(&dbMsgPtr->messagePtr->env);
    ckfree(dbMsgPtr->messagePtr);
    ckfree(dbMsgPtr->buffer);
    ckfree(dbMsgPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Db_MakeChildrenProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static void
Db_MakeChildrenProc(Tcl_Interp *interp, BodyInfo *bodyInfoPtr)
{
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    BodyInfo *partInfoPtr, **partInfoPtrPtr;
    PART *partPtr;

    if (!bodyInfoPtr->firstbornPtr) {
	partInfoPtrPtr = &bodyInfoPtr->firstbornPtr;
	for (partPtr = bodyPtr->nested.part; partPtr;
		partPtr = partPtr->next) {
	    partInfoPtr = CreateBodyInfo(interp, bodyInfoPtr->msgPtr,
					 &partPtr->body);
	    *partInfoPtrPtr = partInfoPtr;
	    partInfoPtrPtr = &partInfoPtr->nextPtr;
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Db_FetchBodyProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Db_FetchBodyProc(BodyInfo *bodyInfoPtr, unsigned long *lengthPtr)
{
    if (bodyInfoPtr->decodedTextPtr) {
	*lengthPtr = Tcl_DStringLength(bodyInfoPtr->decodedTextPtr);
	return Tcl_DStringValue(bodyInfoPtr->decodedTextPtr);
    }
    *lengthPtr = bodyInfoPtr->bodyPtr->contents.text.size;
    return (char*)bodyInfoPtr->bodyPtr->contents.text.data;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_BodyDeleteProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static void
Db_BodyDeleteProc(BodyInfo *bodyInfoPtr)
{
    /* Nothing to do */
}


/*
 *----------------------------------------------------------------------
 *
 * Db_GetInternalDateProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static MESSAGECACHE*
Db_GetInternalDateProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    DbMessageInfo *dbMsgPtr = (DbMessageInfo*)msgPtr->clientData;
    char *from;

    from = RatDbGetFrom(interp, dbMsgPtr->index);
    return RatParseFrom(from);
}

/*
 *----------------------------------------------------------------------
 *
 * Db_GetInfoProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
Db_GetInfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int notused)
{
    MessageInfo *msgPtr = (MessageInfo*)clientData;
    MESSAGE *messagePtr = ((DbMessageInfo*)msgPtr->clientData)->messagePtr;
    ADDRESS *adrPtr;
    Tcl_Obj *oPtr;

    switch (type) {
	case RAT_FOLDER_SUBJECT:	/* fallthrough */
	case RAT_FOLDER_CANONSUBJECT:	/* fallthrough */
	case RAT_FOLDER_NAME:		/* fallthrough */
	case RAT_FOLDER_ANAME:		/* fallthrough */
	case RAT_FOLDER_MAIL_REAL:	/* fallthrough */
	case RAT_FOLDER_MAIL:		/* fallthrough */
	case RAT_FOLDER_NAME_RECIPIENT:	/* fallthrough */
	case RAT_FOLDER_MAIL_RECIPIENT:	/* fallthrough */
	case RAT_FOLDER_SIZE:		/* fallthrough */
	case RAT_FOLDER_SIZE_F:		/* fallthrough */
	case RAT_FOLDER_TYPE:		/* fallthrough */
	case RAT_FOLDER_PARAMETERS:	/* fallthrough */
	case RAT_FOLDER_DATE_F:		/* fallthrough */
	case RAT_FOLDER_DATE_N:		/* fallthrough */
	case RAT_FOLDER_DATE_IMAP4:	/* fallthrough */
	case RAT_FOLDER_TO:		/* fallthrough */
	case RAT_FOLDER_FROM:		/* fallthrough */
	case RAT_FOLDER_STATUS:		/* fallthrough */
	case RAT_FOLDER_FLAGS:		/* fallthrough */
	case RAT_FOLDER_UNIXFLAGS:	/* fallthrough */
	case RAT_FOLDER_MSGID:		/* fallthrough */
	case RAT_FOLDER_REF:		/* fallthrough */
	case RAT_FOLDER_UID:		/* fallthrough */
	case RAT_FOLDER_THREADING:	/* fallthrough */
	case RAT_FOLDER_INDEX:
	    return Db_InfoProcInt(interp, msgPtr->folderInfoPtr,
		    type, msgPtr->msgNo);
	case RAT_FOLDER_SENDER:		/* fallthrough */
	case RAT_FOLDER_CC:		/* fallthrough */
	case RAT_FOLDER_REPLY_TO:
	    if (type == RAT_FOLDER_SENDER) {
		adrPtr = messagePtr->env->sender;
	    } else if (type == RAT_FOLDER_CC) {
		adrPtr = messagePtr->env->cc;
	    } else {
		adrPtr = messagePtr->env->reply_to;
	    }
	    oPtr = Tcl_NewStringObj("", 0);
	    Tcl_SetObjLength(oPtr, RatAddressSize(adrPtr, 1));
	    Tcl_GetString(oPtr)[0] = '\0';
	    rfc822_write_address(Tcl_GetString(oPtr), adrPtr);
	    Tcl_SetObjLength(oPtr, strlen(Tcl_GetString(oPtr)));
	    return oPtr;
	case RAT_FOLDER_END:
	    break;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Db_DbinfoGetProc --
 *
 *      Handle the dbinfo_get command. See ../doc/interface for details
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
static Tcl_Obj*
Db_MsgDbInfoGetProc(MessageInfo *msgPtr)
{
    DbMessageInfo *dbMsgPtr = (DbMessageInfo*)msgPtr->clientData;
    Tcl_Obj *robjv[3];
    RatDbEntry *entry;
    int len;
    
    entry = RatDbGetEntry(dbMsgPtr->index);

    len = strlen(entry->content[KEYWORDS]);
    if (len && '{' == entry->content[KEYWORDS][0]
        && '}' == entry->content[KEYWORDS][len-1]) {
        robjv[0] = Tcl_NewStringObj(entry->content[KEYWORDS]+1, len-2);
    } else {
        robjv[0] = Tcl_NewStringObj(entry->content[KEYWORDS], len);
    }
    robjv[1] = Tcl_NewLongObj(atol(entry->content[EX_TIME]));
    robjv[2] = Tcl_NewStringObj(entry->content[EX_TYPE], -1);
    return Tcl_NewListObj(3, robjv);
}
