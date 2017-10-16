/*
 * ratSMTP.c --
 *
 *	This file contains basic support for sending messages via SMTP.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"

/*
 * Each channel has one of these structures. The channel handler is actually
 * just a pointer to this structure.
 */
typedef struct SMTPChannelPriv {
    Tcl_Channel channel;
    unsigned int mime : 1;	/* True if the peer supports 8 bit mime */
    unsigned int dsn : 1;	/* True if the peer supports DSN */
} SMTPChannelPriv;

/*
 * Linked list of cached channels
 */
typedef struct ChannelCache {
    SMTPChannelPriv *chPtr;
    char *host;
    int port;
    struct ChannelCache *next;
} ChannelCache;
static ChannelCache *channelCache = NULL;
 
/*
 * Local functions
 */
static char *RatTimedGets(Tcl_Interp *interp, Tcl_Channel channel,int timeout);
static int RatSendCommand(Tcl_Interp *interp, Tcl_Channel channel, char *cmd);
static int RatSendRcpt(Tcl_Interp *interp, Tcl_Channel channel,
	ADDRESS *adrPtr, DSNhandle handle, int verbose);


/*
 *----------------------------------------------------------------------
 *
 * RatTimedGets --
 *
 *      A gets with timeout. the channel must be in nonblocking mode.
 *
 * Results:
 *	Returns a pointer to a static area containing the read string.
 *
 * Side effects:
 *	The previous result is overwritten
 *
 *
 *----------------------------------------------------------------------
 */

static char*
RatTimedGets(Tcl_Interp *interp, Tcl_Channel channel, int timeout)
{
    static Tcl_DString ds;
    static int dsInit = 0;

    if (!dsInit) {
	dsInit = 1;
	Tcl_DStringInit(&ds);
    } else {
	Tcl_DStringSetLength(&ds, 0);
    }

    Tcl_SetChannelOption(interp, channel, "-blocking", "0");
    while (-1 == Tcl_Gets(channel, &ds)) {
	if (Tcl_InputBlocked(channel) && timeout) {
	    sleep(1),
	    timeout--;
	} else {
	    Tcl_SetChannelOption(interp, channel, "-blocking", "1");
	    return NULL;
	}
    }
    Tcl_SetChannelOption(interp, channel, "-blocking", "1");
    return Tcl_DStringValue(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendCommand --
 *
 *      Send a command to the SMTP peer, wait for and parse the result.
 *
 * Results:
 *	A standard Tcl result and an eventual error messages in 
 *	the result area.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatSendCommand(Tcl_Interp *interp, Tcl_Channel channel, char *cmd)
{
    int result, timeout;
    Tcl_Obj *oPtr;
    char *reply;

    Tcl_Write(channel, cmd, -1);
    if ('\n' != cmd[strlen(cmd)-1]) {
	Tcl_Write(channel, "\r\n", -1);
    }
    oPtr = Tcl_GetVar2Ex(interp, "option", "smtp_timeout", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &timeout);
    do {
	reply = RatTimedGets(interp, channel, timeout);
	if (reply) {
	    if ('2' == reply[0] || '3' == reply[0]) {
		result = TCL_OK;
	    } else {
		Tcl_SetResult(interp, reply, TCL_VOLATILE);
		result = TCL_ERROR;
	    }
	} else {
	    Tcl_SetResult(interp, "Timeout from SMTP server", TCL_STATIC);
	    result = TCL_ERROR;
	}
    } while (TCL_OK == result && '-' == reply[4]);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendRcpt --
 *
 *      Send the RCPT TO statements to the SMTP peer.
 *
 * Results:
 *	Returns the number of failed addresses. The result string of interp
 *	will contain more information in case of failure.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatSendRcpt(Tcl_Interp *interp, Tcl_Channel channel, ADDRESS *adrPtr,
	DSNhandle handle, int verbose)
{
    char buf[2048], adr[1024];
    unsigned char *cPtr;
    int failures = 0, i;

    for (; adrPtr; adrPtr = adrPtr->next) {
	if (RatAddressSize(adrPtr, 0) > sizeof(adr)) {
	    RatLogF(interp, RAT_WARN, "ridiculously_long", RATLOG_TIME);
	    failures++;
	}
	adr[0] = '\0';
	rfc822_address(adr, adrPtr);
	snprintf(buf, sizeof(buf), "RCPT TO:<%s>", adr);
	if (handle) {
	    RatDSNAddRecipient(interp, handle,  adr);
	    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
		    " NOTIFY=SUCCESS,FAILURE,DELAY");
	    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
		    " ORCPT=rfc822;");
	    for (i = strlen(buf),cPtr = (unsigned char*)adr; *cPtr; cPtr++) {
		if (*cPtr < 33 || *cPtr > 126 || *cPtr == '+' || *cPtr == '='){
		    buf[i++] = '+';
		    buf[i++] = alphabetHEX[*cPtr>>4];
		    buf[i++] = alphabetHEX[*cPtr&0xf];
		} else {
		    buf[i++] = *cPtr;
		}
	    }
	    buf[i] = '\0';
	}
	if (3 == verbose) {
	    RatLogF(interp, RAT_PARSE, "send_rcpt", RATLOG_EXPLICIT, adr);
	}
	if (TCL_OK != RatSendCommand(interp, channel, buf)) {
	    failures++;
	}
    }
    return failures;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSMTPOpen --
 *
 *      Open an SMTP channel.
 *
 * Results:
 *	Returns a channel handler.
 *
 * Side effects:
 *	A new channel is created.
 *
 *
 *----------------------------------------------------------------------
 */

SMTPChannel
RatSMTPOpen (Tcl_Interp *interp, char *host, int verbose, const char *role)
{
    SMTPChannelPriv *chPtr;
    char *reply, buf[1024], *cPtr, *ch;
    int port, timeout;
    ChannelCache *cachePtr;
    Tcl_Obj *oPtr;

    strlcpy(buf, host, sizeof(buf));
    if ((cPtr = strchr(buf, ':'))) {
	*cPtr++ = '\0';
	port = atoi(cPtr);
    } else {
	port = 25;	/* The default SMTP port */
    }

    for (cachePtr = channelCache; cachePtr; cachePtr = cachePtr->next) {
	if (!strcmp(cachePtr->host, buf) && cachePtr->port == port) {
	    if (TCL_OK == RatSendCommand(interp, cachePtr->chPtr->channel,
					 "RSET")) {
		return cachePtr->chPtr;
	    }
	    break;
	}
    }

    if (verbose > 1) {
	RatLogF(interp, RAT_PARSE, "opening_connection", RATLOG_EXPLICIT);
    }
    chPtr = (SMTPChannelPriv*)ckalloc(sizeof(SMTPChannelPriv));
    chPtr->mime = chPtr->dsn = 0;
    /*
     * Your compiler may complain that there are too many arguments to
     * Tcl_OpenTcpClient() below. This is a symtom of you having a prerelease
     * of tcl7.5 installed. In this case you must upgrade to the real
     * releases of tcl7.5/tk4.1 and reconfigure tkrat (this MUST be done).
     * After rerunning configure you may build tkrat.
     */
    if (NULL == (chPtr->channel =Tcl_OpenTcpClient(interp,port,buf,NULL,0,0))){
	ckfree(chPtr);
	return NULL;
    }
    Tcl_SetChannelOption(interp, chPtr->channel, "-buffering", "line");
    Tcl_SetChannelOption(interp, chPtr->channel, "-translation", "binary");

    /*
     * Get initial greeting
     */
    if (verbose > 1) {
	RatLogF(interp, RAT_PARSE, "wait_greeting", RATLOG_EXPLICIT);
    }
    oPtr = Tcl_GetVar2Ex(interp, "option", "smtp_timeout", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &timeout);
    do {
	reply = RatTimedGets(interp, chPtr->channel, timeout);
	if (!reply || '2' != reply[0]) {
	    Tcl_Close(interp, chPtr->channel);
	    ckfree(chPtr);
	    return NULL;
	}
    } while (strncmp("220 ", reply, 4));

    /*
     * Send EHLO (HELO) and get capabilities
     */
    if (verbose > 1) {
	RatLogF(interp, RAT_PARSE, "get_capabilities", RATLOG_EXPLICIT);
    }
    ch = RatGetCurrent(interp, RAT_HOST, role);
    snprintf(buf, sizeof(buf), "EHLO %s\r\n", ch);
    Tcl_Write(chPtr->channel, buf, -1);
    reply = RatTimedGets(interp, chPtr->channel, timeout);
    if (!reply || '2' != reply[0]) {
	snprintf(buf, sizeof(buf), "HELO %s\r\n", ch);
	Tcl_Write(chPtr->channel, buf, -1);
	reply = RatTimedGets(interp, chPtr->channel, timeout);
    }
    while (reply) {
	if (!reply) {
	    Tcl_Close(interp, chPtr->channel);
	    ckfree(chPtr);
	    return NULL;
	}
	if (!strncmp("8BITMIME", &reply[4], 8)) {
	    chPtr->mime = 1;
	} else if (!strncmp("DSN", &reply[4], 3)) {
	    chPtr->dsn = 1;
	}
	if (!strncmp("250 ", reply, 4)) {
	    break;
	}
	reply = RatTimedGets(interp, chPtr->channel, timeout);
    }

    if (verbose > 1) {
	RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
    }

    cachePtr = (ChannelCache*)ckalloc(sizeof(ChannelCache)+strlen(host)+1);
    cachePtr->chPtr = chPtr;
    cachePtr->host = (char*)cachePtr + sizeof(*cachePtr);
    strlcpy(cachePtr->host, host, strlen(host));
    cachePtr->port = port;
    cachePtr->next = channelCache;
    channelCache = cachePtr;
    
    return (SMTPChannel)chPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSMTPClose --
 *
 *      Close an SMTP channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	THe chanel is closed.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatSMTPClose (Tcl_Interp *interp, SMTPChannel channel, int verbose)
{
    SMTPChannelPriv *chPtr = (SMTPChannelPriv*)channel;
    ChannelCache **c1Ptr, *c2Ptr;

    /*
     * Close connection
     */
    if (verbose > 1) {
	RatLogF(interp, RAT_PARSE, "closing", RATLOG_EXPLICIT);
    }
    Tcl_Write(chPtr->channel, "QUIT\r\n", -1);
    Tcl_Close(interp, chPtr->channel);
    if (verbose > 1) {
	RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
    }
    ckfree(chPtr);

    /*
     * Clear cache
     */
    for (c1Ptr = &channelCache; *c1Ptr && (*c1Ptr)->chPtr != chPtr;
	 c1Ptr = &(*c1Ptr)->next);
    if (*c1Ptr) {
	c2Ptr = (*c1Ptr)->next;
	ckfree(*c1Ptr);
	*c1Ptr = c2Ptr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatSMTPClose --
 *
 *      Close an SMTP channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	THe chanel is closed.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatSMTPCloseAll (Tcl_Interp *interp, int verbose)
{
    while (channelCache) {
	RatSMTPClose(interp, channelCache->chPtr, verbose);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatSMTPSend --
 *
 *      Send a message with SMTP over the specified channel.
 *
 * Results:
 *	A standard Tcl result and an eventual error messages in 
 *	the result area.
 *
 * Side effects:
 *	The DSN structures may be updated.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatSMTPSend (Tcl_Interp *interp, SMTPChannel channel, ENVELOPE *envPtr,
	BODY *bodyPtr, int doDSN, int verbose)
{
    SMTPChannelPriv *chPtr = (SMTPChannelPriv*)channel;
    char buf[1024], *header;
    int failures = 0;
    DSNhandle handle = NULL;

    /*
     * Check input and reset stream to known status.
     */
    if (!(envPtr->to || envPtr->cc || envPtr->bcc)) {
	Tcl_SetResult(interp, "No recipients specified", TCL_STATIC);
	goto abort;
    }
    if (TCL_OK != RatSendCommand(interp, chPtr->channel, "RSET")) {
	goto abort;
    }

    /*
     * Check if we should request DSN
     */
    if (doDSN && !chPtr->dsn) {
	RatLogF(interp, RAT_WARN, "no_dsn", RATLOG_TIME);
	doDSN = 0;
    }

    /*
     * Send envelope information.
     */
    if (verbose > 1) {
	if (verbose == 2) {
	    RatLogF(interp, RAT_PARSE, "send_envelope", RATLOG_EXPLICIT);
	} else {
	    RatLogF(interp, RAT_PARSE, "send_from", RATLOG_EXPLICIT);
	}
    }
    if (RatAddressSize(envPtr->from, 0) > sizeof(buf)-128) {
	RatLogF(interp, RAT_WARN, "ridiculously_long", RATLOG_TIME);
	goto abort;
    }
    snprintf(buf, sizeof(buf), "MAIL FROM:<");
    rfc822_address(buf, envPtr->from);
    strlcat(buf, ">", sizeof(buf));
    if (chPtr->mime) {
	strlcat(buf, " BODY=8BITMIME", sizeof(buf));
    }
    if (doDSN) {
	RatGenId(NULL, interp, 0, NULL);
	handle = RatDSNStartMessage(interp, Tcl_GetStringResult(interp),
				    envPtr->subject);
	strlcat(buf, " ENVID=", sizeof(buf));
	strlcat(buf, Tcl_GetStringResult(interp), sizeof(buf));
    }
    if (TCL_OK != RatSendCommand(interp, chPtr->channel, buf)) {
	goto abort;
    }
    failures += RatSendRcpt(interp, chPtr->channel,envPtr->to,handle,verbose);
    failures += RatSendRcpt(interp, chPtr->channel,envPtr->cc,handle,verbose);
    failures += RatSendRcpt(interp, chPtr->channel,envPtr->bcc,handle,verbose);
    if (failures) {
	goto abort;
    }

    /*
     * Send message data
     */
    if (verbose > 1) {
	RatLogF(interp, RAT_PARSE, "send_data", RATLOG_EXPLICIT);
    }
    if (TCL_OK != RatSendCommand(interp, chPtr->channel, "DATA")) {
	goto abort;
    }
    header = (char*)ckalloc(RatHeaderSize(envPtr, bodyPtr));
    rfc822_output(header, envPtr, bodyPtr, RatTclPutsSMTP, chPtr->channel,
	    chPtr->mime);
    ckfree(header);
    if (verbose > 1) {
	RatLogF(interp, RAT_PARSE, "wait_ack", RATLOG_EXPLICIT);
    }
    if (TCL_OK != RatSendCommand(interp, chPtr->channel, ".")) {
	goto abort;
    }
    if (handle) {
	RatDSNFinish(interp, handle);
    }

    return TCL_OK;

 abort:
    RatDSNAbort(interp, handle);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSMTPSupportDSN --
 *
 *      Check if a host supports DSN.
 *
 * Results:
 *	TCL_OK and the result area will contain "1" if the host supports
 *	DSN and "0" otherwise.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatSMTPSupportDSN(ClientData dummy, Tcl_Interp *interp, int objc,
		  Tcl_Obj *const objv[])
{
    SMTPChannelPriv *chPrivPtr;
    SMTPChannel channel;
    int verbose, result;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]), " hostname\"",
			 (char *) NULL);
	return TCL_ERROR;
    }

    Tcl_GetIntFromObj(interp,
		      Tcl_GetVar2Ex(interp, "option", "smtp_verbose",
				    TCL_GLOBAL_ONLY),
		      &verbose);
    channel = RatSMTPOpen(interp, Tcl_GetString(objv[1]), verbose, "");
    if (channel) {
	chPrivPtr = (SMTPChannelPriv*)channel;
	result = chPrivPtr->dsn;
	RatSMTPClose(interp, channel, verbose);
    } else {
	result = 0;
    }
    if (verbose) {
	RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(result));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatTclPutsSMTP --
 *
 *	Like RatTclPuts but also escapes sigle dots with double dots.
 *
 * Results:
 *      Always returns 1L.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

long
RatTclPutsSMTP(void *stream_x, char *string)
{
    Tcl_Channel channel = (Tcl_Channel)stream_x;
    char *cPtr, *srcPtr;

    if ('.' == string[0]) {
	Tcl_Write(channel, ".", 1);
    }

    for (srcPtr = string; *srcPtr;) {
	if (!srcPtr[0] || !srcPtr[1] || !srcPtr[2]) {
	    break;
	}
	for (cPtr = srcPtr; cPtr[2]; cPtr++) {
	    if ('\r' == cPtr[0] && '\n' == cPtr[1] && '.' == cPtr[2]) {
		break;
	    }
	}
	if (cPtr[2]) {
	    if (-1 == Tcl_Write(channel, srcPtr, cPtr-srcPtr+3)
		    || -1 == Tcl_Write(channel, ".", 1)) {
		return 0;
	    }
	    srcPtr = cPtr+3;
	} else {
	    break;
	}
    }
    if (-1 == Tcl_Write(channel, srcPtr, -1)) {
	return 0;
    } else {
	return 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatTclPutsSendmail --
 *
 *	Like RatTclPutsSMTP but also escapes changes line endings to
 *	put \n
 *
 * Results:
 *      Always returns 1L.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

long
RatTclPutsSendmail(void *stream_x, char *string)
{
    Tcl_Channel channel = (Tcl_Channel)stream_x;
    char *cPtr, *srcPtr;
    int add;

    for (srcPtr = string; *srcPtr;) {
	if (!srcPtr[0] || !srcPtr[1]) {
	    break;
	}
	add = 1;
	for (cPtr = srcPtr; cPtr[1]; cPtr++) {
	    if ('\r' == cPtr[0] && '\n' == cPtr[1]) {
		cPtr--;
		add = 2;
		break;
	    }
	}
	if (-1 == Tcl_Write(channel, srcPtr, (cPtr+1)-srcPtr)) {
	    return 0;
	}
	srcPtr = cPtr+add;
    }
    if (*srcPtr && -1 == Tcl_Write(channel, srcPtr, -1)) {
	return 0;
    } else {
	return 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatHeaderSize --
 *
 *	Calculate size of header
 *
 * Results:
 *      Maximum size of header
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int RatHeaderLineSize(char *name, ENVELOPE *env, char *text);
static int RatHeaderAddressSize(char *name, ENVELOPE *env, ADDRESS *adr);

static int
RatHeaderLineSize(char *name, ENVELOPE *env, char *text)
{
    if (text) {
	return (env->remail ? 7 : 0) + strlen(name) + 2 + strlen(text) + 2;
    } else {
	return 0;
    }
}

static int
RatHeaderAddressSize(char *name, ENVELOPE *env, ADDRESS *adr)
{
    if (adr) {
	return (env->remail?7:0) + strlen(name) + 2 + RatAddressSize(adr, 1)+2;
    } else {
	return 0;
    }
}

size_t
RatHeaderSize(ENVELOPE *env,BODY *body)
{
    size_t len = 0;

    if (env->remail) len += strlen(env->remail);
    len += RatHeaderLineSize("Newsgroups", env, env->newsgroups);
    len += RatHeaderLineSize("Date", env, env->date); 
    len += RatHeaderAddressSize("From", env, env->from);
    len += RatHeaderAddressSize("Sender", env, env->sender);
    len += RatHeaderAddressSize("Reply-To", env, env->reply_to);
    len += RatHeaderLineSize("Subject", env, env->subject);
    if (env->bcc && !(env->to || env->cc)) {
	len += strlen("To: undisclosed recipients: ;\015\012");
    }
    len += RatHeaderAddressSize("To", env, env->to);
    len += RatHeaderAddressSize("cc", env, env->cc);
    len += RatHeaderLineSize("In-Reply-To", env, env->in_reply_to);
    len += RatHeaderLineSize("Message-ID", env, env->message_id);
    len += RatHeaderLineSize("Followup-to", env, env->followup_to);
    len += RatHeaderLineSize("References", env, env->references);
    if (body && !env->remail) {   /* not if remail or no body structure */
	/*
	 * TODO: Fix this correctly
	 * Here we assume that the body headers will never become longer
	 * than 8192 bytes
	 */
        len += 8192;
    } 
    len += 2;
    return len;
}
