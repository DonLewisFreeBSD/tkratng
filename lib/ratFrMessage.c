/*
 * ratFrMessage.c --
 *
 *	This file contains code which implements free messages.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"
#include "ratPGP.h"

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

/*
 * Used when doing output to internal string
 */
typedef struct {
    unsigned int used;
    unsigned int allocated;
    char *data;
} DynamicString;

static ENVELOPE* RatFrCreateEnvelope(Tcl_Interp *interp, char *role,
				     Tcl_Obj *envelope_data,
 				     Tcl_DString *extraHeaders);
static void RatFrCreateBody(BODY *b, Tcl_Interp *interp, char *role,
			    Tcl_Obj *body_data, Tcl_DString *extraHeaders);
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
static long RatStringSoutr(void *stream_x, char *string);


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
    messageProcInfoPtr->dbinfoGetProc = NULL;
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
 * RatCreateMessageCmd --
 *
 *      Creates a free message entity from tcl
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

int
RatCreateMessageCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		    Tcl_Obj *const objv[])
{
    FrMessageInfo *frMsgPtr=(FrMessageInfo*)ckalloc(sizeof(FrMessageInfo));
    MessageInfo *msgPtr=(MessageInfo*)ckalloc(sizeof(MessageInfo));
    DynamicString ds;
    ENVELOPE *env;
    BODY *body;
    char *msgData = NULL;
    Tcl_Obj **aobjv;
    int i, aobjc, len;
    Tcl_DString extraHeaders;

    if (3 != objc
	|| TCL_OK != Tcl_ListObjGetElements(interp, objv[2], &aobjc, &aobjv)
	|| 2 != aobjc) {
	Tcl_AppendResult(interp, "bad args: should be \"",
		Tcl_GetString(objv[0]), " role {envelope body}\"", NULL);
	return TCL_ERROR;
    }
    Tcl_DStringInit(&extraHeaders);
    Tcl_DStringAppend(&extraHeaders, "Status: R\r\n", -1);
    env = RatFrCreateEnvelope(interp, Tcl_GetString(objv[1]), aobjv[0],
			      &extraHeaders);
    body = mail_newbody();
    RatFrCreateBody(body, interp, Tcl_GetString(objv[1]),
		    aobjv[1], &extraHeaders);
    rfc822_encode_body_8bit(env, body);

    msgPtr->folderInfoPtr = NULL;
    msgPtr->type = RAT_FREE_MESSAGE;
    msgPtr->bodyInfoPtr = NULL;
    msgPtr->msgNo = 0;
    msgPtr->fromMe = RAT_ISME_UNKOWN;
    msgPtr->toMe = RAT_ISME_UNKOWN;
    msgPtr->clientData = (ClientData)frMsgPtr;
    for (i=0; i<sizeof(msgPtr->info)/sizeof(*msgPtr->info); i++) {
	msgPtr->info[i] = NULL;
    }
    frMsgPtr->msgData = msgData;
    frMsgPtr->messagePtr = mail_newmsg();
    frMsgPtr->messagePtr->env = env;
    frMsgPtr->messagePtr->body = body;
    frMsgPtr->from = NULL;

    len = RatHeaderSize(env, body) + Tcl_DStringLength(&extraHeaders);
    frMsgPtr->headers = (char*)ckalloc(len);
    rfc822_header(frMsgPtr->headers, env, body);
    frMsgPtr->headers[strlen(frMsgPtr->headers)-2] = '\0';
    strlcat(frMsgPtr->headers, Tcl_DStringValue(&extraHeaders), len);

    ds.used = ds.allocated = 0;
    ds.data = NULL;
    rfc822_output_body(body, RatStringSoutr, (void*)&ds);
    if (NULL != ds.data) {
	/* Skip the last two which are extra \r\n added by c-client */
	ds.data[ds.used-2] = '\0';
    } else {
	ds.data = cpystr("");
    }
    frMsgPtr->bodyData = (unsigned char*)ds.data;
    
    sprintf(msgPtr->name, "RatFrMsg%d", numFrMessages++);
    Tcl_CreateObjCommand(interp, msgPtr->name, RatMessageCmd,
			 (ClientData)msgPtr, NULL);
    Tcl_SetResult(interp, msgPtr->name, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatFrCreateEnvelope --
 *
 *      Creates an envelope structure from a list
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
static ENVELOPE*
RatFrCreateEnvelope(Tcl_Interp *interp, char *role, Tcl_Obj *envelope_data,
		    Tcl_DString *ehp)
{
    ENVELOPE *e = mail_newenvelope();
    Tcl_Obj **objv, **ev;
    int objc, ec, i;
    char host[1024], buf[8192];

    if (TCL_OK != Tcl_ListObjGetElements(interp, envelope_data, &objc, &objv)){
	return e;
    }
    strlcpy(host, RatGetCurrent(interp, RAT_HOST, role), sizeof(host));
    for (i=0; i<objc; i++) {
	if (TCL_OK != Tcl_ListObjGetElements(interp, objv[i], &ec, &ev)
	    || 2 != ec) {
	    continue;
	}
	if (!strcasecmp(Tcl_GetString(ev[0]), "date")) {
	    e->date = (unsigned char*)cpystr(Tcl_GetString(ev[1]));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "subject")) {
	    e->subject = cpystr(RatEncodeHeaderLine(interp, ev[1], 7));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "in_reply_to")) {
	    e->in_reply_to = cpystr(Tcl_GetString(ev[1]));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "message_id")) {
	    e->message_id = cpystr(Tcl_GetString(ev[1]));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "newsgroups")) {
	    e->newsgroups = cpystr(Tcl_GetString(ev[1]));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "followup_to")) {
	    e->followup_to = cpystr(Tcl_GetString(ev[1]));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "references")) {
	    e->references = cpystr(Tcl_GetString(ev[1]));
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "from")) {
	    strlcpy(buf, Tcl_GetString(ev[1]), sizeof(buf));
	    rfc822_parse_adrlist(&e->from, buf, host);
            RatEncodeAddresses(interp, e->from);
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "sender")) {
	    strlcpy(buf, Tcl_GetString(ev[1]), sizeof(buf));
	    rfc822_parse_adrlist(&e->sender, buf, host);
            RatEncodeAddresses(interp, e->sender);
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "reply_to")) {
	    strlcpy(buf, Tcl_GetString(ev[1]), sizeof(buf));
	    rfc822_parse_adrlist(&e->reply_to, buf, host);
            RatEncodeAddresses(interp, e->reply_to);
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "to")) {
	    strlcpy(buf, Tcl_GetString(ev[1]), sizeof(buf));
	    rfc822_parse_adrlist(&e->to, buf, host);
            RatEncodeAddresses(interp, e->to);
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "cc")) {
	    strlcpy(buf, Tcl_GetString(ev[1]), sizeof(buf));
	    rfc822_parse_adrlist(&e->cc, buf, host);
            RatEncodeAddresses(interp, e->cc);
	} else if (!strcasecmp(Tcl_GetString(ev[0]), "bcc")) {
	    strlcpy(buf, Tcl_GetString(ev[1]), sizeof(buf));
	    rfc822_parse_adrlist(&e->bcc, buf, host);
            RatEncodeAddresses(interp, e->bcc);
	} else if (!strncmp(Tcl_GetString(ev[0]), "X-", 2)) {
	    Tcl_DStringAppend(ehp, Tcl_GetString(ev[0]), -1);
	    Tcl_DStringAppend(ehp, ": ", 2);
	    Tcl_DStringAppend(ehp, Tcl_GetString(ev[1]), -1);
	    Tcl_DStringAppend(ehp, "\r\n", 2);
	} else {
	    /* Perhaps handle this error? */
	    fprintf(stderr, "Env: unknown envelope header '%s'\n",
                    Tcl_GetString(ev[0]));
	}
    }
    return e;
}

/*
 *----------------------------------------------------------------------
 *
 * RatFrCreateBody --
 *
 *      Creates the body structure from a body entity
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
static void
RatFrCreateBody(BODY *b, Tcl_Interp *interp, char *role, Tcl_Obj *body_data,
		Tcl_DString *ehp)
{
    DynamicString ds;
    Tcl_DString tds;
    PARAMETER *p;
    Tcl_Obj **objv, **av, **pv;
    int objc, ac, pc, i;
    char *charset = "us-ascii";

    if (TCL_OK != Tcl_ListObjGetElements(interp, body_data, &objc, &objv)
	|| 8 != objc) {
	return;
    }
    /* Content-type & parameters */
    for (i=0;
	 i < TYPEMAX-1 && body_types[i] &&
             strcasecmp(body_types[i], Tcl_GetString(objv[0]));
	 i++);
    b->type = i;
    b->subtype = (char*)ucase((unsigned char*)cpystr(Tcl_GetString(objv[1])));
    if (TCL_OK == Tcl_ListObjGetElements(interp, objv[2], &ac, &av)) {
	for (i=ac-1; i>=0; i--) {
	    if (TCL_OK != Tcl_ListObjGetElements(interp, av[i], &pc, &pv)) {
		continue;
	    }
	    p = mail_newbody_parameter();
	    p->attribute = (char*)ucase((unsigned char*)
                                        cpystr(Tcl_GetString(pv[0])));
	    p->value = cpystr(Tcl_GetString(pv[1]));
	    p->next = b->parameter;
	    b->parameter = p;
	    if (!strcmp("CHARSET", p->attribute)) {
		charset = p->value;
	    }
	}
	RatEncodeParameters(interp, b->parameter);
    }
    
    for (i=0; i < ENCMAX-1
	     && body_encodings[i]
	     && strcasecmp(body_encodings[i],Tcl_GetString(objv[3])); i++);
    b->encoding = i;

    /* Disposition & parameters */
    if (strlen(Tcl_GetString(objv[4]))) {
	b->disposition.type =
            (char*)ucase((unsigned char*)cpystr(Tcl_GetString(objv[4])));
	if (TCL_OK == Tcl_ListObjGetElements(interp, objv[5], &ac, &av)) {
	    for (i=0; i<ac; i++) {
		if (TCL_OK != Tcl_ListObjGetElements(interp, av[i], &pc,&pv)) {
		    continue;
		}
		p = mail_newbody_parameter();
		p->attribute =
                    (char*)ucase((unsigned char*)cpystr(Tcl_GetString(pv[0])));
		p->value = cpystr(Tcl_GetString(pv[1]));
		p->next = b->disposition.parameter;
		b->disposition.parameter = p;
	    }
	}
	RatEncodeParameters(interp, b->disposition.parameter);
    }

    /* Headers */
    if (TCL_OK == Tcl_ListObjGetElements(interp, objv[6], &ac, &av)) {
	for (i=0; i<ac; i++) {
	    if (TCL_OK != Tcl_ListObjGetElements(interp, av[i], &pc, &pv)
		|| 2 != pc) {
		continue;
	    }
	    if (!strcasecmp(Tcl_GetString(pv[0]), "content_id")) {
		b->id = cpystr(RatEncodeHeaderLine(interp, pv[1], 10));
	    } else if (!strcasecmp(Tcl_GetString(pv[0]),
                                   "content_description")) {
		b->description = cpystr(RatEncodeHeaderLine(interp, pv[1],19));
	    } else if (!strncmp(Tcl_GetString(pv[0]), "X-", 2) && ehp) {
		Tcl_DStringAppend(ehp, Tcl_GetString(pv[0]), -1);
		Tcl_DStringAppend(ehp, ": ", 2);
		Tcl_DStringAppend(ehp, Tcl_GetString(pv[1]), -1);
		Tcl_DStringAppend(ehp, "\r\n", 2);
	    } else {
		fprintf(stderr, "Env: unknown body header '%s'\n",
			Tcl_GetString(pv[0]));
	    }
	}
    }

    if (TYPEMESSAGE == b->type) {
	int len;
	char *tmp;
	
	b->nested.msg = mail_newmsg();

	Tcl_ListObjGetElements(interp, objv[7], &ac, &av);
	if (2 == ac && !strcmp("file", Tcl_GetString(av[0]))) {
	    b->contents.text.data = (unsigned char*)
                RatReadAndCanonify(interp, Tcl_GetString(av[1]),
				   &b->contents.text.size, 0);
	} else {
	    b->nested.msg->env = RatFrCreateEnvelope(interp, role, av[0],NULL);
	    b->nested.msg->body = mail_newbody();
	    RatFrCreateBody(b->nested.msg->body, interp, role, av[1], NULL);
	    ds.used = ds.allocated = 0;
	    ds.data = NULL;
	    len = RatHeaderSize(b->nested.msg->env, b->nested.msg->body);
	    tmp = (char*)ckalloc(len+1);
	    rfc822_output(tmp, b->nested.msg->env, b->nested.msg->body,
			  RatStringSoutr, (void*)&ds, 0);
	    b->contents.text.data = (unsigned char*)ds.data;
	    b->contents.text.size = ds.used;
	}
	
    } else if (TYPEMULTIPART == b->type) {
	PART **p = &b->nested.part;
	
	Tcl_ListObjGetElements(interp, objv[7], &ac, &av);
	for (i=0; i<ac; i++) {
	    *p = mail_newbody_part();
	    RatFrCreateBody(&(*p)->body, interp, role, av[i], NULL);
	    p = &(*p)->next;
	}
	
    } else {
	Tcl_ListObjGetElements(interp, objv[7], &ac, &av);
	if (!strcasecmp(Tcl_GetString(av[0]), "utfblob")) {
	    Tcl_DStringInit(&tds);
	    Tcl_UtfToExternalDString(RatGetEncoding(interp, charset),
				     Tcl_GetString(av[1]), -1, &tds);
	    RatCanonalize(&tds);
	    b->contents.text.data =
                (unsigned char*)cpystr(Tcl_DStringValue(&tds));
	    b->contents.text.size = Tcl_DStringLength(&tds);
	    Tcl_DStringFree(&tds);
	} else if (!strcasecmp(Tcl_GetString(av[0]), "file")) {
	    int canon = 0;
	    if (TYPETEXT == b->type) {
		canon = 1;
	    }
	    b->contents.text.data = (unsigned char*)
		RatReadAndCanonify(interp, Tcl_GetString(av[1]),
				   &b->contents.text.size, canon);
	}
    }
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
    msgPtr->bodyInfoPtr = CreateBodyInfo(interp, msgPtr,
					 frMsgPtr->messagePtr->body);

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
	    partInfoPtr = CreateBodyInfo(interp, bodyInfoPtr->msgPtr,
					 &partPtr->body);
	    *partInfoPtrPtr = partInfoPtr;
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
	case RAT_FOLDER_ANAME:		/* fallthrough */
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
		mail_parse_date(&elt, (unsigned char*)buf);
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
	case RAT_FOLDER_UID:
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

/*
 *----------------------------------------------------------------------
 *
 * RatStringSoutr --
 *
 *      APpend data to dynamic string
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Affects the passed dynamic string
 *
 *
 *----------------------------------------------------------------------
 */

static long
RatStringSoutr(void *stream_x, char *string)
{
    DynamicString *ds = (DynamicString*)stream_x;
    int len = strlen(string);

    if (ds->used + len > ds->allocated) {
	ds->allocated = ds->used + len + 8192;
	ds->data = (char*)ckrealloc(ds->data, ds->allocated);
    }
    strcpy(ds->data+ds->used, string);
    ds->used += len;
    return(1L);                                 /* T for c-client */
}

/*
 *----------------------------------------------------------------------
 *
 * RatStdMessagePGP --
 *
 *      Do pgp operation on message
 *
 * Results:
 *	A standard tcl result
 *
 * Side effects:
 *	The message is modified and interaction with the user may be
 *      required.
 *
 *
 *----------------------------------------------------------------------
 */
int
RatFrMessagePGP(Tcl_Interp *interp, MessageInfo *msgPtr, int sign,
		int encrypt, char *role, char *signer, Tcl_Obj *rcpts)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    DynamicString ds;
    char *old, *x;
    int r, len;

    if (encrypt) {
	r = RatPGPEncrypt(interp, frMsgPtr->messagePtr->env,
			  &frMsgPtr->messagePtr->body, (sign ? signer : NULL),
			  rcpts);
    } else if (sign) {
	r = RatPGPSign(interp, frMsgPtr->messagePtr->env,
		       &frMsgPtr->messagePtr->body, signer);
    } else {
	return TCL_OK;
    }

    if (TCL_OK == r) {
	len = RatHeaderSize(frMsgPtr->messagePtr->env,
			    frMsgPtr->messagePtr->body);
	/* XXX
	 * Find old X-headers. We assume they are collected at the end
	 * of the headers (which they always are for generated free messages
	 */
	old = frMsgPtr->headers;
	if ((x = strstr(old, "\nX-"))) {
	    x++;
	    len += strlen(x);
	}
	frMsgPtr->headers = (char*)ckalloc(len);
	rfc822_header(frMsgPtr->headers, frMsgPtr->messagePtr->env,
		      frMsgPtr->messagePtr->body);
	frMsgPtr->headers[strlen(frMsgPtr->headers)-2] = '\0';
	if (x) {
	    strlcat(frMsgPtr->headers, x, len);
	}
	ckfree(old);
	
	ds.used = ds.allocated = 0;
	ds.data = NULL;
	rfc822_output_body(frMsgPtr->messagePtr->body, RatStringSoutr,
			   (void*)&ds);
	if (NULL != ds.data) {
	    /* Skip the last two which are extra \r\n added by c-client */
	    ds.data[ds.used-2] = '\0';
	} else {
	    ds.data = cpystr("");
	}
	ckfree(frMsgPtr->bodyData);
	frMsgPtr->bodyData = (unsigned char*)ds.data;
    }
    return r;
}

/*
 *----------------------------------------------------------------------
 *
 * RatFrMessageRemoveInternal --
 *
 *      Remove the internal TkRat header fields
 *
 * Results:
 *	A standard tcl result
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
int
RatFrMessageRemoveInternal(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    FrMessageInfo *frMsgPtr = (FrMessageInfo*)msgPtr->clientData;
    char *s, *e;

    while (NULL != (s = strstr(frMsgPtr->headers, "X-TkRat-Internal"))) {
	if (NULL != (e = strchr(s, '\n'))) {
	    memmove(s, e+1, strlen(e+1)+1);
	} else {
	    *s = '\0';
	}
    }

    return TCL_OK;
}
