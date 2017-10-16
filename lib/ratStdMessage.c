/*
 * ratStdMessage.c --
 *
 *	This file contains code which implements standard c-client messages.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratStdFolder.h"

/*
 * The ClientData for each bodypart entity
 */
typedef struct StdBodyInfo {
    char *section;
} StdBodyInfo;

/*
 * The number of message entities created. This is used to create new
 * unique command names.
 */
static int numStdMessages = 0;

#ifdef MEM_DEBUG
static char *mem_header = NULL;
#endif /* MEM_DEBUG */


/*
 *----------------------------------------------------------------------
 *
 * RatStdMessagesInit --
 *
 *      Initializes the given MessageProcInfo entry for a c-client message
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
RatStdMessagesInit(MessageProcInfo *messageProcInfoPtr)
{
    messageProcInfoPtr->getHeadersProc = Std_GetHeadersProc;
    messageProcInfoPtr->getEnvelopeProc = Std_GetEnvelopeProc;
    messageProcInfoPtr->getInfoProc = Std_GetInfoProc;
    messageProcInfoPtr->createBodyProc = Std_CreateBodyProc;
    messageProcInfoPtr->fetchTextProc = Std_FetchTextProc;
    messageProcInfoPtr->envelopeProc = Std_EnvelopeProc;
    messageProcInfoPtr->msgDeleteProc = Std_MsgDeleteProc;
    messageProcInfoPtr->makeChildrenProc = Std_MakeChildrenProc;
    messageProcInfoPtr->fetchBodyProc = Std_FetchBodyProc;
    messageProcInfoPtr->bodyDeleteProc = Std_BodyDeleteProc;
    messageProcInfoPtr->getInternalDateProc = Std_GetInternalDateProc;
    messageProcInfoPtr->dbinfoGetProc = NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * RatStdMessageCreate --
 *
 *      Creates a std message entity
 *
 * Results:
 *	The name of the new message entity.
 *
 * Side effects:
 *	The message's long cache entry is locked until the message is
 *	deleted.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatStdMessageCreate(Tcl_Interp *interp, RatFolderInfoPtr folderInfoPtr,
		    MAILSTREAM *stream, int msgNo)
{
    MessageInfo *msgPtr = (MessageInfo*)folderInfoPtr->privatePtr[msgNo];
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;

    stdMsgPtr->envPtr =
	mail_fetchstructure_full(stream, msgNo+1, &stdMsgPtr->bodyPtr, NIL);
    stdMsgPtr->eltPtr = mail_elt(stream, msgNo+1);
    stdMsgPtr->eltPtr->lockcount++;
    stdMsgPtr->spec = cpystr(stream->mailbox);
    sprintf(msgPtr->name, "RatStdMsg%d", numStdMessages++);
    Tcl_CreateObjCommand(interp, msgPtr->name, RatMessageCmd,
			 (ClientData)msgPtr, NULL);
    return msgPtr->name;
}


/*
 *----------------------------------------------------------------------
 *
 * RatStdEasyCopyingOK --
 *
 *      Check if we can lets c-client handle the copying of this message
 *
 * Results:
 *	A boolean which says if it is OK or not.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
RatStdEasyCopyingOK(Tcl_Interp *interp, MessageInfo *msgPtr, Tcl_Obj *defPtr)
{
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    Tcl_Obj **objv;
    int objc;

    Tcl_ListObjGetElements(interp, defPtr, &objc, &objv);

    switch (stdMsgPtr->type) {
    case RAT_DIS:
	return 0;
    case RAT_MBX:
	return 0;
    case RAT_UNIX:
	return 0;
    case RAT_MH:
	return !strcasecmp(Tcl_GetString(objv[1]), "mh");
    case RAT_POP:
	return 0;
    case RAT_IMAP:
	if (strcasecmp(Tcl_GetString(objv[1]), "imap")) {
	    return 0;
	}
	return !strcmp(stdMsgPtr->spec, RatGetFolderSpec(interp, defPtr));
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * RatStdMessageCopy --
 *
 *      Copy a message to another c-client folder.
 *
 * Results:
 *	A boolean which says if it went OK or not.
 *
 * Side effects:
 *	The destination folder is modified.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatStdMessageCopy (Tcl_Interp *interp, MessageInfo *msgPtr, char *destination)
{
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    int flagged = stdMsgPtr->eltPtr->flagged;
    int deleted = stdMsgPtr->eltPtr->deleted;
    char *cPtr, seq[16];
    int r = TCL_ERROR;

    sprintf(seq, "%d", msgPtr->msgNo+1);
    if (flagged) {
	mail_clearflag(stdMsgPtr->stream, seq,
		       flag_name[RAT_FLAGGED].imap_name);
    }
    if (deleted) {
	mail_clearflag(stdMsgPtr->stream, seq,
		       flag_name[RAT_DELETED].imap_name);
    }
    switch (stdMsgPtr->type) {
	case RAT_UNIX:	/* fallthrough */
	case RAT_MBX:	/* fallthrough */
	case RAT_DIS:	/* fallthrough */
	case RAT_MH:	/* fallthrough */
	case RAT_POP:	/* fallthrough */
	    if (T == mail_copy_full(stdMsgPtr->stream, seq, destination, 0)) {
		r = TCL_OK;
	    }
	    break;
	case RAT_IMAP:
	    cPtr = strchr(destination, '}');
	    if (cPtr && mail_copy_full(stdMsgPtr->stream, seq, &cPtr[1], 0)) {
		r = TCL_OK;
	    }
	    break;
    }
    if (flagged) {
	mail_setflag(stdMsgPtr->stream, seq, flag_name[RAT_FLAGGED].imap_name);
    }
    if (deleted) {
	mail_setflag(stdMsgPtr->stream, seq, flag_name[RAT_DELETED].imap_name);
    }
    return r;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_GetHeadersProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

char*
Std_GetHeadersProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    static char *header = NULL;
    static int headerSize = 0;
    unsigned long length;
    char *fetchedHeader = mail_fetchheader_full(stdMsgPtr->stream,
	    msgPtr->msgNo+1, NIL, &length, NIL);

    if (length > 2 && fetchedHeader[length-3] == '\n') {
	length -= 2;
    }

    if (length+64 > headerSize) {
	headerSize = length+64;
	header = (char*)ckrealloc(header, headerSize);
    }
    memmove(header, fetchedHeader, length);
    header[length] = '\0';
    if (stdMsgPtr->eltPtr->seen) {
	strcpy(&header[length], "Status: RO\r\n");
	length += strlen(&header[length]);
    }
    if (stdMsgPtr->eltPtr->answered) {
	strcpy(&header[length], "X-Status: A\r\n");
	length += strlen(&header[length]);
    }

#ifdef MEM_DEBUG
    mem_header = header;
#endif /* MEM_DEBUG */
    return header;
}

/*
 *----------------------------------------------------------------------
 *
 * Std_GetEnvelopeProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

char*
Std_GetEnvelopeProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    static char buf[1024];
    ADDRESS *adrPtr;
    time_t date;
    struct tm tm, *tmPtr;

    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;

    if (stdMsgPtr->envPtr->return_path) {
	adrPtr = stdMsgPtr->envPtr->sender;
    } else if (stdMsgPtr->envPtr->sender) {
	adrPtr = stdMsgPtr->envPtr->sender;
    } else {
	adrPtr = stdMsgPtr->envPtr->from;
    }
    if (adrPtr && RatAddressSize(adrPtr, 0) < sizeof(buf)-6) {
	strlcpy(buf, "From ", sizeof(buf));
	rfc822_address(buf+5, adrPtr);
    } else {
	strlcpy(buf, "From unkown", sizeof(buf));
    }
    tm.tm_sec = stdMsgPtr->eltPtr->seconds;
    tm.tm_min = stdMsgPtr->eltPtr->minutes;
    tm.tm_hour = stdMsgPtr->eltPtr->hours;
    tm.tm_mday = stdMsgPtr->eltPtr->day;
    tm.tm_mon = stdMsgPtr->eltPtr->month - 1;
    tm.tm_year = stdMsgPtr->eltPtr->year+69;
    tm.tm_wday = 0;
    tm.tm_yday = 0;
    tm.tm_isdst = -1;
    date = (int)mktime(&tm);
    tmPtr = gmtime(&date);
    sprintf(buf + strlen(buf), " %s %s %2d %02d:%02d GMT %04d\n",
	    dayName[tmPtr->tm_wday], monthName[tmPtr->tm_mon],
	    tmPtr->tm_mday, tmPtr->tm_hour, tmPtr->tm_min,tmPtr->tm_year+1900);
    return buf;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_CreateBodyProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

BodyInfo*
Std_CreateBodyProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    StdBodyInfo *stdBodyInfoPtr = (StdBodyInfo*)ckalloc(sizeof(StdBodyInfo));
    msgPtr->bodyInfoPtr = CreateBodyInfo(interp, msgPtr, stdMsgPtr->bodyPtr);

    msgPtr->bodyInfoPtr->clientData = (ClientData)stdBodyInfoPtr;
    if (TYPEMULTIPART == msgPtr->bodyInfoPtr->bodyPtr->type) {
        stdBodyInfoPtr->section = NULL;
    } else {
        stdBodyInfoPtr->section = cpystr("1");
    }
    return msgPtr->bodyInfoPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * Std_FetchTextProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

char*
Std_FetchTextProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    return mail_fetchtext_full(stdMsgPtr->stream, msgPtr->msgNo+1, NIL, NIL);
}


/*
 *----------------------------------------------------------------------
 *
 * Std_EnvelopeProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

ENVELOPE*
Std_EnvelopeProc(MessageInfo *msgPtr)
{
    return ((StdMessageInfo*)msgPtr->clientData)->envPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Std_MsgDeleteProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

void
Std_MsgDeleteProc(MessageInfo *msgPtr)
{
    RatFolderInfo *infoPtr = msgPtr->folderInfoPtr;
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;

    infoPtr->privatePtr[msgPtr->msgNo] = NULL;
    stdMsgPtr->eltPtr->lockcount--;
    ckfree(stdMsgPtr->spec);
    ckfree(stdMsgPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Std_MakeChildrenProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

void
Std_MakeChildrenProc(Tcl_Interp *interp, BodyInfo *bodyInfoPtr)
{
    StdBodyInfo *stdBodyInfoPtr = (StdBodyInfo*)bodyInfoPtr->clientData;
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    BodyInfo *partInfoPtr, **partInfoPtrPtr;
    StdBodyInfo *partStdInfoPtr;
    int index = 1;
    PART *partPtr;
    int size;

    if (!bodyInfoPtr->firstbornPtr) {
	partInfoPtrPtr = &bodyInfoPtr->firstbornPtr;
	for (partPtr = bodyPtr->nested.part; partPtr;
		partPtr = partPtr->next) {
	    partInfoPtr = CreateBodyInfo(interp, bodyInfoPtr->msgPtr,
					 &partPtr->body);
	    partStdInfoPtr = (StdBodyInfo*)ckalloc(sizeof(StdBodyInfo));
	    *partInfoPtrPtr = partInfoPtr;
	    partInfoPtrPtr = &partInfoPtr->nextPtr;
	    partInfoPtr->msgPtr = bodyInfoPtr->msgPtr;
	    partInfoPtr->clientData = (ClientData)partStdInfoPtr;
	    if (stdBodyInfoPtr->section) {
		size = strlen(stdBodyInfoPtr->section) + 8;
		partStdInfoPtr->section = (char*)ckalloc(size);
		snprintf(partStdInfoPtr->section, size, "%s.%d",
			 stdBodyInfoPtr->section, index++);
	    } else {
		partStdInfoPtr->section = (char*)ckalloc(8);
		sprintf(partStdInfoPtr->section, "%d", index++);
	    }
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Std_FetchBodyProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

char*
Std_FetchBodyProc(BodyInfo *bodyInfoPtr, unsigned long *lengthPtr)
{
    StdMessageInfo *stdMsgPtr=(StdMessageInfo*)bodyInfoPtr->msgPtr->clientData;

    if (bodyInfoPtr->decodedTextPtr) {
	*lengthPtr = Tcl_DStringLength(bodyInfoPtr->decodedTextPtr);
	return Tcl_DStringValue(bodyInfoPtr->decodedTextPtr);
    }
    return mail_fetchbody_full(stdMsgPtr->stream, bodyInfoPtr->msgPtr->msgNo+1,
	    ((StdBodyInfo*)(bodyInfoPtr->clientData))->section, lengthPtr,NIL);
}


/*
 *----------------------------------------------------------------------
 *
 * Std_BodyDeleteProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

void
Std_BodyDeleteProc(BodyInfo *bodyInfoPtr)
{
    StdBodyInfo *partStdInfoPtr = (StdBodyInfo*)bodyInfoPtr->clientData;
    ckfree(partStdInfoPtr->section);
    ckfree(bodyInfoPtr->clientData);
}


/*
 *----------------------------------------------------------------------
 *
 * Std_GetInternalDateProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

MESSAGECACHE*
Std_GetInternalDateProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    return stdMsgPtr->eltPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Std_GetInfoProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
Std_GetInfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int notused)
{
    Tcl_Obj *oPtr = NULL;
    MessageInfo *msgPtr = (MessageInfo*)clientData;
    StdMessageInfo *stdMsgPtr = (StdMessageInfo*)msgPtr->clientData;
    ADDRESS *addressPtr;
    int i, presIndex;

    if (msgPtr->info[type]) {
	if (type == RAT_FOLDER_INDEX && msgPtr->folderInfoPtr) {
	    Tcl_GetIntFromObj(interp, msgPtr->info[type], &i);
	    if (i < msgPtr->folderInfoPtr->number
		    && msgPtr->folderInfoPtr->privatePtr[
		       msgPtr->folderInfoPtr->presentationOrder[i-1]] ==
		    (ClientData)msgPtr) {
		return msgPtr->info[type];
	    }
	} else {
	    return msgPtr->info[type];
	}
    }

    switch (type) {
	case RAT_FOLDER_SUBJECT:	/* fallthrough */
	case RAT_FOLDER_CANONSUBJECT:	/* fallthrough */
	case RAT_FOLDER_ANAME:		/* fallthrough */
	case RAT_FOLDER_NAME:		/* fallthrough */
	case RAT_FOLDER_MAIL_REAL:	/* fallthrough */
	case RAT_FOLDER_MAIL:		/* fallthrough */
	case RAT_FOLDER_NAME_RECIPIENT:	/* fallthrough */
	case RAT_FOLDER_MAIL_RECIPIENT:	/* fallthrough */
	case RAT_FOLDER_SIZE:		/* fallthrough */
	case RAT_FOLDER_SIZE_F:		/* fallthrough */
	case RAT_FOLDER_DATE_F:		/* fallthrough */
	case RAT_FOLDER_DATE_N:		/* fallthrough */
	case RAT_FOLDER_DATE_IMAP4:	/* fallthrough */
	case RAT_FOLDER_TO:		/* fallthrough */
	case RAT_FOLDER_FROM:		/* fallthrough */
	case RAT_FOLDER_SENDER:		/* fallthrough */
	case RAT_FOLDER_CC:		/* fallthrough */
	case RAT_FOLDER_FLAGS:		/* fallthrough */
	case RAT_FOLDER_UNIXFLAGS:	/* fallthrough */
	case RAT_FOLDER_MSGID:		/* fallthrough */
	case RAT_FOLDER_REF:		/* fallthrough */
	case RAT_FOLDER_THREADING:	/* fallthrough */
	case RAT_FOLDER_REPLY_TO:
	    return RatGetMsgInfo(interp, type, msgPtr, stdMsgPtr->envPtr,
		    NULL, stdMsgPtr->eltPtr, stdMsgPtr->eltPtr->rfc822_size);

	case RAT_FOLDER_PARAMETERS:
	    if (!stdMsgPtr->bodyPtr) {
		stdMsgPtr->envPtr = mail_fetchstructure_full(
			stdMsgPtr->stream, msgPtr->msgNo+1,
			&stdMsgPtr->bodyPtr, NIL);
	    }
	    return RatGetMsgInfo(interp, type, msgPtr, stdMsgPtr->envPtr,
		    stdMsgPtr->bodyPtr, stdMsgPtr->eltPtr,
		    stdMsgPtr->eltPtr->rfc822_size);
	    
	case RAT_FOLDER_TYPE:
	    if (stdMsgPtr->envPtr->optional.subtype) {
		oPtr = Tcl_NewStringObj(
			body_types[stdMsgPtr->envPtr->optional.type], -1);
		Tcl_AppendStringsToObj(oPtr, "/",
				       stdMsgPtr->envPtr->optional.subtype,
				       NULL);
	    } else {
		if (!stdMsgPtr->bodyPtr) {
		    stdMsgPtr->envPtr = mail_fetchstructure_full(
			    stdMsgPtr->stream, msgPtr->msgNo+1,
			    &stdMsgPtr->bodyPtr, NIL);
		}
		oPtr=Tcl_NewStringObj(body_types[stdMsgPtr->bodyPtr->type],-1);
		Tcl_AppendStringsToObj(oPtr, "/",
				       stdMsgPtr->bodyPtr->subtype, NULL);
	    }
	    break;

	case RAT_FOLDER_STATUS:
	    if (RAT_ISME_UNKOWN == msgPtr->toMe) {
		msgPtr->toMe = RAT_ISME_NO;
		for (addressPtr = stdMsgPtr->envPtr->to; addressPtr;
			addressPtr = addressPtr->next) {
		    if (RatAddressIsMe(interp, addressPtr, 1)) {
			msgPtr->toMe = RAT_ISME_YES;
			break;
		    }
		}
	    }
	    oPtr = Tcl_NewStringObj(NULL, 0);
	    if (!stdMsgPtr->eltPtr->seen) {
		Tcl_AppendToObj(oPtr, "N", 1);
	    }
	    if (stdMsgPtr->eltPtr->deleted) {
		Tcl_AppendToObj(oPtr, "D", 1);
	    }
	    if (stdMsgPtr->eltPtr->flagged) {
		Tcl_AppendToObj(oPtr, "F", 1);
	    }
	    if (stdMsgPtr->eltPtr->answered) {
		Tcl_AppendToObj(oPtr, "A", 1);
	    }
	    if (RAT_ISME_YES == msgPtr->toMe) {
		Tcl_AppendToObj(oPtr, "+", 1);
	    } else {
		Tcl_AppendToObj(oPtr, " ", 1);
	    }
	    break;
	case RAT_FOLDER_INDEX:
	    if (msgPtr->folderInfoPtr) {
		for (i=0; i< msgPtr->folderInfoPtr->number; i++) {
		    presIndex = msgPtr->folderInfoPtr->presentationOrder[i];
		    if (msgPtr->folderInfoPtr->privatePtr[presIndex] ==
			    (ClientData)msgPtr){
			oPtr = Tcl_NewIntObj(i+1);
			break;
		    }
		}
	    }
	    break;
	case RAT_FOLDER_UID:
	    oPtr = Tcl_NewIntObj(
		mail_uid(stdMsgPtr->stream, msgPtr->msgNo+1));
	case RAT_FOLDER_END:
	    break;
    }
    if (!oPtr) {
	oPtr = Tcl_NewObj();
    }
    msgPtr->info[type] = oPtr;
    Tcl_IncrRefCount(oPtr);
    return oPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * RatStdMsgStructInit --
 *
 *      Initializes the client data part of the message info structures
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	More data is allocated
 *
 *
 *----------------------------------------------------------------------
 */
void
RatStdMsgStructInit(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index,
	MAILSTREAM *stream, RatStdFolderType type)
{
    StdMessageInfo *stdMsgPtr;
    int i, start, end;
    char seq[32];

    if (-1 == index) {
       start = 0;
       end = infoPtr->number;
       sprintf(seq, "%d:%d", 1, end);
    } else {
       start = index;
       end = start+1;
       sprintf(seq, "%d", end);
    }
    for (i=start; i<end; i++) {
	stdMsgPtr = (StdMessageInfo*)ckalloc(sizeof(StdMessageInfo));
	stdMsgPtr->stream = stream;
	stdMsgPtr->eltPtr = mail_elt(stream, i+1);
	stdMsgPtr->envPtr = mail_fetch_structure(stream, i+1, NIL, NIL);
	stdMsgPtr->bodyPtr = NULL;
	stdMsgPtr->type = type;
        stdMsgPtr->spec = NULL;
	((MessageInfo*)infoPtr->privatePtr[i])->clientData =
		(ClientData)stdMsgPtr;
    }
}

#ifdef MEM_DEBUG
void ratStdMessageCleanup()
{
    ckfree(mem_header);
}
#endif /* MEM_DEBUG */
