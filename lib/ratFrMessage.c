/*
 * ratFrMessage.c --
 *
 *	This file contains code which implements free messages.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"

/*
 * The ClientData for each message entity
 */
typedef struct FrMessageInfo {
    MESSAGE *messagePtr;
    char *from;
    char *headers;
    char *msgData;
    unsigned char *bodyData;
} FrMessageInfo;

/*
 * The ClientData for each bodypart entity
 */
typedef struct FrBodyInfo {
    unsigned char *text;
} FrBodyInfo;

/*
 * The number of message entities created. This is used to create new
 * unique command names.
 */
static int numFrMessages = 0;

static RatGetHeadersProc Fr_GetHeadersProc;
static RatGetEnvelopeProc Fr_GetEnvelopeProc;
static RatFetchTextProc Fr_FetchTextProc;
static RatEnvelopeProc Fr_EnvelopeProc;
static RatMsgDeleteProc Fr_MsgDeleteProc;
static RatMakeChildrenProc Fr_MakeChildrenProc;
static RatFetchBodyProc Fr_FetchBodyProc;
static RatBodyDeleteProc Fr_BodyDeleteProc;
static RatInfoProc Fr_GetInfoProc;
static RatGetInternalDateProc Fr_GetInternalDateProc;


/*
 *----------------------------------------------------------------------
 *
 * RatFrMessagesInit --
 *
 *      Initializes the given MessageProcInfo entry for a free message
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
RatFrMessagesInit(MessageProcInfo *messageProcInfoPtr)
{
    messageProcInfoPtr->getHeadersProc = Fr_GetHeadersProc;
    messageProcInfoPtr->getEnvelopeProc = Fr_GetEnvelopeProc;
    messageProcInfoPtr->getInfoProc = Fr_GetInfoProc;
    messageProcInfoPtr->createBodyProc = Fr_CreateBodyProc;
    messageProcInfoPtr->fetchTextProc = Fr_FetchTextProc;
    messageProcInfoPtr->envelopeProc = Fr_EnvelopeProc;
    messageProcInfoPtr->msgDeleteProc = Fr_MsgDeleteProc;
    messageProcInfoPtr->makeChildrenProc = Fr_MakeChildrenProc;
    messageProcInfoPtr->fetchBodyProc = Fr_FetchBodyProc;
    messageProcInfoPtr->bodyDeleteProc = Fr_BodyDeleteProc;
    messageProcInfoPtr->getInternalDateProc = Fr_GetInternalDateProc;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFrMessageCreate --
 *
 *      Creates a free message entity
 *
 * Results:
 *	The name of the new message entity.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatFrMessageCreate(Tcl_Interp *interp, char *data, int length,
		   MessageInfo **msgPtrPtr)
{
    FrMessageInfo *frMsgPtr=(FrMessageInfo*)ckalloc(sizeof(FrMessageInfo));
    MessageInfo *msgPtr=(MessageInfo*)ckalloc(sizeof(MessageInfo));
    char *msgData, *cPtr;
    int headerLength, j, fromLength;

    for (headerLength = 0; data[headerLength]; headerLength++) {
	if (data[headerLength] == '\n' && data[headerLength+1] == '\n') {
	    headerLength++;
	    break;
	}
	if (data[headerLength]=='\r' && data[headerLength+1]=='\n'
		&& data[headerLength+2]=='\r' && data[headerLength+3]=='\n') {
	    headerLength += 2;
	    break;
	}
    }

    msgData = (char*)ckalloc(length+1);
    memcpy(msgData, data, length);
    msgData[length] = '\0';

    msgPtr->folderInfoPtr = NULL;
    msgPtr->type = RAT_FREE_MESSAGE;
    msgPtr->bodyInfoPtr = NULL;
    msgPtr->msgNo = 0;
    msgPtr->fromMe = RAT_ISME_UNKOWN;
    msgPtr->toMe = RAT_ISME_UNKOWN;
    msgPtr->clientData = (ClientData)frMsgPtr;
    for (j=0; j<sizeof(msgPtr->info)/sizeof(*msgPtr->info); j++) {
	msgPtr->info[j] = NULL;
    }
    frMsgPtr->msgData = msgData;
    frMsgPtr->messagePtr = RatParseMsg(interp, (unsigned char*)msgData);
    frMsgPtr->bodyData = frMsgPtr->messagePtr->text.text.data +
			 frMsgPtr->messagePtr->text.offset;
    frMsgPtr->headers = (char*)ckalloc(headerLength+1);
    strlcpy(frMsgPtr->headers, data, headerLength+1);
    if (!strncmp("From ", data, 5) && (cPtr = strchr(data, '\n'))) {
	fromLength = cPtr-data;
	frMsgPtr->from = (char*)ckalloc(fromLength+1);
	strlcpy(frMsgPtr->from, frMsgPtr->headers, fromLength);
    } else {
	frMsgPtr->from = NULL;
    }

    if (msgPtrPtr) {
	*msgPtrPtr = msgPtr;
    }

    sprintf(msgPtr->name, "RatFrMsg%d", numFrMessages++);
    Tcl_CreateObjCommand(interp, msgPtr->name, RatMessageCmd,
	    (ClientData)msgPtr, NULL);
    return msgPtr->name;
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_GetHeadersProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Fr_GetHeadersProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    return frMsgPtr->headers;
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_GetEnvelopeProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Fr_GetEnvelopeProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    static char buf[1024];
    MESSAGECACHE elt;
    ADDRESS *adrPtr;
    time_t date;
    struct tm tm, *tmPtr;

    if (frMsgPtr->messagePtr->env->return_path) {
	adrPtr = frMsgPtr->messagePtr->env->sender;
    } else if (frMsgPtr->messagePtr->env->sender) {
	adrPtr = frMsgPtr->messagePtr->env->sender;
    } else {
	adrPtr = frMsgPtr->messagePtr->env->from;
    }
    if (!strcmp(Tcl_GetHostName(), adrPtr->host)) {
	snprintf(buf, sizeof(buf), "From %s", adrPtr->mailbox);
    } else {
	strlcpy(buf, "From ", sizeof(buf));
	if (RatAddressSize(adrPtr, 0) > sizeof(buf)-32) {
	    snprintf(buf+5, sizeof(buf)-5, "ridiculously@long.address");
	} else {
	    rfc822_write_address_full(buf+5, adrPtr, NULL);
	}
    }
    if (T == mail_parse_date(&elt, frMsgPtr->messagePtr->env->date)) {
	tm.tm_sec = elt.seconds;
	tm.tm_min = elt.minutes;
	tm.tm_hour = elt.hours;
	tm.tm_mday = elt.day;
	tm.tm_mon = elt.month - 1;
	tm.tm_year = elt.year+69;
	tm.tm_wday = 0;
	tm.tm_yday = 0;
	tm.tm_isdst = -1;
	date = (int)mktime(&tm);
    } else {
	date = 0;
    }
    tmPtr = gmtime(&date);
    snprintf(buf + strlen(buf), sizeof(buf)-strlen(buf),
	    " %s %s %2d %02d:%02d GMT %04d\n", dayName[tmPtr->tm_wday],
	    monthName[tmPtr->tm_mon], tmPtr->tm_mday, tmPtr->tm_hour,
	    tmPtr->tm_min, tmPtr->tm_year+1900);
    return buf;
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_CreateBodyProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

BodyInfo*
Fr_CreateBodyProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    FrBodyInfo *frBodyInfoPtr = (FrBodyInfo*)ckalloc(sizeof(FrBodyInfo));
    msgPtr->bodyInfoPtr = CreateBodyInfo(msgPtr);

    msgPtr->bodyInfoPtr->bodyPtr = frMsgPtr->messagePtr->body;
    msgPtr->bodyInfoPtr->clientData = (ClientData)frBodyInfoPtr;
    frBodyInfoPtr->text = frMsgPtr->bodyData;
    return msgPtr->bodyInfoPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_FetchTextProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Fr_FetchTextProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    return (char*)frMsgPtr->bodyData;
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_EnvelopeProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static ENVELOPE*
Fr_EnvelopeProc(MessageInfo *msgPtr)
{
    return ((FrMessageInfo*)msgPtr->clientData)->messagePtr->env;
}

/*
 *----------------------------------------------------------------------
 *
 * Fr_MsgDeleteProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static void
Fr_MsgDeleteProc(MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    mail_free_envelope(&frMsgPtr->messagePtr->env);
    mail_free_body(&frMsgPtr->messagePtr->body);
    ckfree(frMsgPtr->messagePtr);
    ckfree(frMsgPtr->headers);
    ckfree(frMsgPtr->msgData);
    ckfree(frMsgPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_MakeChildrenProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static void
Fr_MakeChildrenProc(Tcl_Interp *interp, BodyInfo *bodyInfoPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)bodyInfoPtr->msgPtr->clientData;
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    BodyInfo *partInfoPtr, **partInfoPtrPtr;
    FrBodyInfo *frPartInfoPtr;
    PART *partPtr;

    if (!bodyInfoPtr->firstbornPtr) {
	partInfoPtrPtr = &bodyInfoPtr->firstbornPtr;
	for (partPtr = bodyPtr->nested.part; partPtr;
		partPtr = partPtr->next) {
	    frPartInfoPtr = (FrBodyInfo*)ckalloc(sizeof(FrBodyInfo));
	    partInfoPtr = CreateBodyInfo(bodyInfoPtr->msgPtr);
	    *partInfoPtrPtr = partInfoPtr;
	    partInfoPtr->bodyPtr = &partPtr->body;
	    partInfoPtrPtr = &partInfoPtr->nextPtr;
	    partInfoPtr->clientData = (ClientData)frPartInfoPtr;
	    frPartInfoPtr->text = frMsgPtr->bodyData +
				  partPtr->body.contents.offset;
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_FetchBodyProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static char*
Fr_FetchBodyProc(BodyInfo *bodyInfoPtr, unsigned long *lengthPtr)
{
    FrBodyInfo *frBodyInfoPtr = (FrBodyInfo*)bodyInfoPtr->clientData;

    if (bodyInfoPtr->decodedTextPtr) {
	*lengthPtr = Tcl_DStringLength(bodyInfoPtr->decodedTextPtr);
	return Tcl_DStringValue(bodyInfoPtr->decodedTextPtr);
    }
    *lengthPtr = bodyInfoPtr->bodyPtr->contents.text.size;
    return (char*)frBodyInfoPtr->text;
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_BodyDeleteProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static void
Fr_BodyDeleteProc(BodyInfo *bodyInfoPtr)
{
    FrBodyInfo *frBodyInfoPtr = (FrBodyInfo*)bodyInfoPtr->clientData;
    ckfree(frBodyInfoPtr);
}



/*
 *----------------------------------------------------------------------
 *
 * Fr_GetInternalDateProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static MESSAGECACHE*
Fr_GetInternalDateProc(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    static MESSAGECACHE elt;

    if (frMsgPtr->from) {
	return RatParseFrom(frMsgPtr->from);
    } else {
	mail_parse_date(&elt, frMsgPtr->messagePtr->env->date);
	return &elt;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Fr_GetInfoProc --
 *
 *      See ratFolder.h
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
Fr_GetInfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index)
{
    static char buf[128];
    MessageInfo *msgPtr = (MessageInfo*)clientData;
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    MESSAGECACHE elt;
    Tcl_Obj *oPtr = NULL;
    char *cPtr;
    int i, f;

    if (msgPtr->info[type]) {
	return msgPtr->info[type];
    }

    switch (type) {
	case RAT_FOLDER_SUBJECT:	/* fallthrough */
	case RAT_FOLDER_CANONSUBJECT:	/* fallthrough */
	case RAT_FOLDER_NAME:		/* fallthrough */
	case RAT_FOLDER_MAIL_REAL:	/* fallthrough */
	case RAT_FOLDER_MAIL:		/* fallthrough */
	case RAT_FOLDER_NAME_RECIPIENT:	/* fallthrough */
	case RAT_FOLDER_MAIL_RECIPIENT:	/* fallthrough */
	case RAT_FOLDER_TYPE:		/* fallthrough */
	case RAT_FOLDER_TO:		/* fallthrough */
	case RAT_FOLDER_FROM:		/* fallthrough */
	case RAT_FOLDER_SENDER:		/* fallthrough */
	case RAT_FOLDER_CC:		/* fallthrough */
	case RAT_FOLDER_REPLY_TO:	/* fallthrough */
	case RAT_FOLDER_MSGID:		/* fallthrough */
	case RAT_FOLDER_REF:		/* fallthrough */
	case RAT_FOLDER_PARAMETERS:
	    return RatGetMsgInfo(interp, type,msgPtr,frMsgPtr->messagePtr->env,
		    frMsgPtr->messagePtr->body, NULL, 0);
	case RAT_FOLDER_SIZE:		/* fallthrough */
	case RAT_FOLDER_SIZE_F:
	    return RatGetMsgInfo(interp, type, msgPtr, NULL, NULL, NULL,
		    frMsgPtr->messagePtr->header.text.size +
		    frMsgPtr->messagePtr->text.text.size);
	case RAT_FOLDER_DATE_F:	/* fallthrough */
	case RAT_FOLDER_DATE_N:	/* fallthrough */
	case RAT_FOLDER_DATE_IMAP4:
	    if (T != mail_parse_date(&elt, frMsgPtr->messagePtr->env->date)) {
		rfc822_date(buf);
		mail_parse_date(&elt, buf);
	    }
	    return RatGetMsgInfo(interp, type, msgPtr,
		    frMsgPtr->messagePtr->env, NULL, &elt, 0);
	case RAT_FOLDER_STATUS:
	    cPtr = frMsgPtr->headers;
	    do {
		if (!strncasecmp(cPtr, "status:", 7)) {
		    int seen, deleted, marked, answered;
		    ADDRESS *addressPtr;

		    seen = deleted = marked = answered = 0;
		    for (i=7; cPtr[i]; i++) {
			switch (cPtr[i]) {
			case 'R': seen = 1;	break;
			case 'D': deleted = 1;	break;
			case 'F': marked = 1;	break;
			case 'A': answered = 1;	break;
			}
		    }
		    if (RAT_ISME_UNKOWN == msgPtr->toMe) {
			msgPtr->toMe = RAT_ISME_NO;
			for (addressPtr = frMsgPtr->messagePtr->env->to;
				addressPtr;
				addressPtr = addressPtr->next) {
			    if (RatAddressIsMe(interp, addressPtr, 1)) {
				msgPtr->toMe = RAT_ISME_YES;
				break;
			    }
			}
		    }
		    i = 0;
		    if (!seen) {
			buf[i++] = 'N';
		    }
		    if (deleted) {
			buf[i++] = 'D';
		    }
		    if (marked) {
			buf[i++] = 'F';
		    }
		    if (answered) {
			buf[i++] = 'A';
		    }
		    if (RAT_ISME_YES == msgPtr->toMe) {
			buf[i++] = '+';
		    } else {
			buf[i++] = ' ';
		    }
		    buf[i] = '\0';
		    oPtr = Tcl_NewStringObj(buf, -1);
		    break;
		}
	    } while ((cPtr = strchr(cPtr, '\n')) && cPtr++ && *cPtr);
	    break;
	case RAT_FOLDER_FLAGS:
	    cPtr = frMsgPtr->headers;
	    buf[0] = '\0';
	    do {
		if (!strncasecmp(cPtr, "status:", 7)) {
		    for (i=7; cPtr[i] != '\n' && cPtr[i]; i++) {
			for (f=0; flag_name[f].imap_name; f++) {
			    if (flag_name[f].unix_char == cPtr[i]) {
				strlcat(buf, " ", sizeof(buf));
				strlcat(buf, flag_name[f].imap_name,
					sizeof(buf));
				break;
			    }
			}
		    }
		    if (*buf) {
			oPtr = Tcl_NewStringObj(buf+1, -1);
		    } else {
			oPtr = Tcl_NewObj();
		    }
		    break;
		}
	    } while ((cPtr = strchr(cPtr, '\n')) && cPtr++ && *cPtr);
	    break;
	case RAT_FOLDER_UNIXFLAGS:
	    cPtr = frMsgPtr->headers;
	    buf[0] = '\0';
	    do {
		if (!strncasecmp(cPtr, "status:", 7)) {
		    for (cPtr += 7; isspace(*cPtr); cPtr++);
		    oPtr = Tcl_NewStringObj(cPtr, -1);
		    break;
		}
	    } while ((cPtr = strchr(cPtr, '\n')) && cPtr++ && *cPtr);
	    break;
	case RAT_FOLDER_INDEX:
	    oPtr = Tcl_NewIntObj(1);
	    break;
	case RAT_FOLDER_THREADING:
	    oPtr = Tcl_NewObj();
	    break;
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
