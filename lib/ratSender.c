/* 
 * ratSender.c --
 *
 *	This is the subprocess which handles the actual sending of messages.
 *
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"
#include "ratPGP.h"

/*
 * This is a list of outstanding commands
 */
typedef struct CmdList {
    char *cmd;
    struct CmdList *next;
} CmdList;
static CmdList *cmdList = NULL;

/*
 * Create bodypart s procedure
 */
static BODY *RatCreateBody(Tcl_Interp *interp, char *handler, ENVELOPE *env,
			   int *errorFlag, Tcl_Obj *files, const char *role);
static int RatSenderSend(Tcl_Interp *interp, const char *prefix,
	Tcl_Obj *usedArraysPtr, Tcl_Obj *files, int *hardError);
static void RatSenderStanddown(Tcl_Interp *interp);
static int RatParseParameter(Tcl_Interp *interp, const char *src,
	PARAMETER **dstPtrPtr);


/*
 *----------------------------------------------------------------------
 *
 * RatSender --
 *
 *	This routine runs the sending process.
 *
 * Results:
 *	None, this routine never returns
 *
 * Side effects:
 *      Messages may be sent
 *
 *
 *----------------------------------------------------------------------
 */

void
RatSender(Tcl_Interp *interp)
{
    Tcl_DString result;
    CONST84 char **sendlistArgv, **argv;
    char *buf;
    CmdList *cmdPtr;
    int sendlistArgc, objc, i, s, buflen, hardError = 0, argc;
    Tcl_Obj *usedArraysPtr, **objv, *filesPtr;

    /*
     * Clear cached passwords
     */
    ClearPGPPass(NULL);

    Tcl_DStringInit(&result);
    buflen = 1024;
    buf = (char*)ckalloc(buflen);

    while (1) {
	if (cmdList) {
	    cmdPtr = cmdList;
	    strlcpy(buf, cmdList->cmd, buflen);
	    cmdList = cmdList->next;
	    ckfree(cmdPtr->cmd);
	    ckfree(cmdPtr);
	} else {
	    i = 0;
	    while (buf[buflen-2] = '\0',
		    fgets(buf+i, buflen-i, stdin)
		    && buflen-i-1 == strlen(buf+i)
		    && buf[buflen-2] != '\n') {
		i = buflen-1;
		buflen += 1024;
		buf = ckrealloc(buf, buflen);
	    }
	    if (feof(stdin)) {
		exit(0);
	    }
	}
	
	if (!strncmp(buf, "SEND", 4)) {
	    (void)Tcl_SplitList(interp, buf, &sendlistArgc, &sendlistArgv);
	    for (s=1; s<sendlistArgc && !hardError; s++) {
		(void)Tcl_SplitList(interp, sendlistArgv[s], &argc, &argv);
		usedArraysPtr = Tcl_NewObj();
		filesPtr = Tcl_NewObj();
		Tcl_DStringSetLength(&result, 0);
		if (TCL_OK == RatSenderSend(interp, argv[1], usedArraysPtr,
			filesPtr, &hardError)){
		    Tcl_DStringAppendElement(&result, "SENT");
		    Tcl_DStringAppendElement(&result, argv[0]);
		    Tcl_ListObjGetElements(interp, filesPtr, &objc, &objv);
		    for (i=0; i<objc; i++) {
			(void)unlink(Tcl_GetString(objv[i]));
		    }
		} else {
		    Tcl_DStringAppendElement(&result, "FAILED");
		    Tcl_DStringAppendElement(&result, argv[0]);
		    Tcl_DStringAppendElement(&result, argv[1]);
		    Tcl_DStringAppendElement(&result,
			    Tcl_GetStringResult(interp));
		    sprintf(buf, "%d", hardError);
		    Tcl_DStringAppendElement(&result, buf);
		}
		ckfree(argv);
		Tcl_ListObjGetElements(interp, usedArraysPtr, &objc, &objv);
		for (i=0; i < objc; i++) {
		    (void)Tcl_UnsetVar(interp, Tcl_GetString(objv[i]),
				       TCL_GLOBAL_ONLY);
		}
		Tcl_DecrRefCount(usedArraysPtr);
		Tcl_DecrRefCount(filesPtr);
		for (i=Tcl_DStringLength(&result)-1; i>=0; i--) {
		    if ('\n' == Tcl_DStringValue(&result)[i]) {
			Tcl_DStringValue(&result)[i] = ' ';
		    }
		}
		fwrite(Tcl_DStringValue(&result), Tcl_DStringLength(&result)+1,
		       1, stdout);
		fflush(stdout);
	    }
	    ckfree(sendlistArgv);
	    RatSenderStanddown(interp);
	} else if (!strncmp(buf, "RSET", 4)) {
	    hardError = 0;
	} else {
	    exit(0);
	}
    }
    /* Notreached */
    exit(0);
}


/*
 *----------------------------------------------------------------------
 *
 * RatSenderSend --
 *
 *	Send a specified message
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *      A message is sent.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatSenderSend(Tcl_Interp *interp, const char *prefix, Tcl_Obj *usedArraysPtr,
	Tcl_Obj *filesPtr, int *hardError)
{
    int listObjc, i, errorFlag = 0, requestDSN, verbose, listArgc;
    char *tmp, buf[1024], *handler, *header = NULL, host[1024], *s;
    const char *role, *ctmp, *saveTo;
    CONST84 char **listArgv;
    SMTPChannel smtpChannel = NULL;
    Tcl_Obj *oPtr, *rPtr, **listObjv;
    Tcl_DString ds;
    ENVELOPE *env;
    BODY *body;

    /*
     * Extract the message
     */
    if (TCL_OK != RatHoldExtract(interp, prefix, usedArraysPtr, filesPtr)) {
	return TCL_ERROR;
    }
    handler = cpystr(Tcl_GetStringResult(interp));
    role = Tcl_GetVar2(interp, handler, "role", 0);

    /*
     * Check hostname to use for unqualified addresses
     */
    strlcpy(host, RatGetCurrent(interp, RAT_HOST, role), sizeof(host));

    /*
     * Construct the headers
     */
    if (!(oPtr = Tcl_GetVar2Ex(interp, handler, "request_dsn",TCL_GLOBAL_ONLY))
	|| TCL_OK != Tcl_GetBooleanFromObj(interp, oPtr, &requestDSN)) {
	requestDSN = 0;
    }
    env = mail_newenvelope ();
    RatGenerateAddresses(interp, role, handler, &env->from, &env->sender);
    buf[0] = '\0';
    tmp = cpystr(Tcl_GetVar2(interp, handler, "to", TCL_GLOBAL_ONLY));
    rfc822_parse_adrlist(&env->to, tmp, host);
    ckfree(tmp);
    RatEncodeAddresses(interp, env->to);
    env->remail = cpystr(Tcl_GetVar2(interp,handler,"remail",TCL_GLOBAL_ONLY));
    if (NULL == (env->date =
		 (char*)Tcl_GetVar2(interp,handler, "date",TCL_GLOBAL_ONLY))) {
	rfc822_date(buf);
	env->date = buf;
    }
    env->date = cpystr(env->date);
    if ((ctmp = Tcl_GetVar2(interp, handler, "reply_to", TCL_GLOBAL_ONLY))
	    && !RatIsEmpty(ctmp)) {
	tmp = cpystr(ctmp);
	rfc822_parse_adrlist(&env->reply_to, tmp, host);
	ckfree(tmp);
	RatEncodeAddresses(interp, env->reply_to);
    }
    oPtr = Tcl_GetVar2Ex(interp, handler, "subject", TCL_GLOBAL_ONLY);
    if (oPtr && 0 < Tcl_GetCharLength(oPtr)) {
	env->subject = cpystr(RatEncodeHeaderLine(interp, oPtr, 9));
    }
    if ((ctmp = Tcl_GetVar2(interp, handler, "cc", TCL_GLOBAL_ONLY))
	    && !RatIsEmpty(ctmp)) {
	tmp = cpystr(ctmp);
	rfc822_parse_adrlist(&env->cc, tmp, host);
	ckfree(tmp);
	RatEncodeAddresses(interp, env->cc);
    }
    if ((ctmp = Tcl_GetVar2(interp, handler, "bcc", TCL_GLOBAL_ONLY))
	    && !RatIsEmpty(ctmp)) {
	tmp = cpystr(ctmp);
	rfc822_parse_adrlist(&env->bcc, tmp, host);
	ckfree(tmp);
	RatEncodeAddresses(interp, env->bcc);
    }
    env->in_reply_to = cpystr(Tcl_GetVar2(interp, handler, "in_reply_to",
	    TCL_GLOBAL_ONLY));
    env->message_id = cpystr(Tcl_GetVar2(interp, handler, "message_id",
	    TCL_GLOBAL_ONLY));
    env->newsgroups = NULL;

    /*
     * Construct the body
     */
    if ((ctmp = Tcl_GetVar2(interp, handler, "body", TCL_GLOBAL_ONLY))) {
	body = RatCreateBody(interp, (char*)ctmp, env, &errorFlag, filesPtr,
			     role);
    } else {
	body = mail_newbody ();
    }
    if (errorFlag) {
	goto error;
    }

    /*
     * Send the message
     */
    header = (char*)ckalloc(RatHeaderSize(env, body));
    oPtr = Tcl_GetVar2Ex(interp, "option", "smtp_verbose", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &verbose);
    if (1 == verbose) {
	RatLogF(interp, RAT_PARSE, "sending_message", RATLOG_EXPLICIT);
    }
    snprintf(buf, sizeof(buf), "%s,sendprot", role);
    ctmp = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
    if (ctmp && !strcmp(ctmp, "smtp")) {
	int reuseChannel;

	oPtr = Tcl_GetVar2Ex(interp, "option", "smtp_reuse", TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &reuseChannel);
	listObjc = 0;
	snprintf(buf, sizeof(buf), "%s,smtp_hosts", role);
	if ((oPtr = Tcl_GetVar2Ex(interp, "option", buf, TCL_GLOBAL_ONLY))) {
	    Tcl_ListObjGetElements(interp, oPtr, &listObjc, &listObjv);
	}
	if (0 == listObjc) {
	    Tcl_SetResult(interp, 
			  "Configuration error; no valid SMTP hosts given",
		          TCL_STATIC);
	    *hardError = 1;
	    goto error;
	}
	for (i=0; !smtpChannel && i<listObjc; i++) {
	    smtpChannel = RatSMTPOpen(interp, Tcl_GetString(listObjv[i]),
				      verbose, role);
	}
	if (!smtpChannel) {
	    Tcl_SetResult(interp, "No valid SMTP hosts", TCL_STATIC);
	    *hardError = 1;
	    goto error;
	}
	if (TCL_OK != RatSMTPSend(interp, smtpChannel, env, body, requestDSN,
		verbose)) {
	    rPtr = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(rPtr);
	    RatSMTPClose(interp, smtpChannel, verbose);
	    Tcl_SetObjResult(interp, rPtr);
	    Tcl_DecrRefCount(rPtr);
	    goto error;
	}
	if (!reuseChannel) {
	    RatSMTPClose(interp, smtpChannel, verbose);
	}

    } else if (ctmp && !strcmp(ctmp, "prog")) {
	Tcl_Channel channel;
	ADDRESS *adrPtr;

	snprintf(buf, sizeof(buf), "%s,sendprog_8bit", role);
	oPtr = Tcl_GetVar2Ex(interp, "option", buf, TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &i);
	snprintf(buf, sizeof(buf), "%s,sendprog", role);
	if (NULL == (ctmp = RatGetPathOption(interp, buf))) {
	    Tcl_SetResult(interp, "Invalid send program", TCL_STATIC);
	    *hardError = 1;
	    goto error;
	}
	Tcl_DStringInit(&ds);
	Tcl_DStringAppendElement(&ds, ctmp);
	Tcl_DStringAppendElement(&ds, "-i");
	for (adrPtr = env->to; adrPtr; adrPtr = adrPtr->next) {
	    if (RatAddressSize(adrPtr, 0) > sizeof(buf)-1) {
		Tcl_SetResult(interp, "Ridiculously long address", TCL_STATIC);
		*hardError = 1;
		goto error;
	    } else {
		buf[0] = '\0';
		rfc822_address(buf, adrPtr);
		Tcl_DStringAppendElement(&ds, buf);
	    }
	}
	for (adrPtr = env->cc; adrPtr; adrPtr = adrPtr->next) {
	    if (RatAddressSize(adrPtr, 0) > sizeof(buf)-1) {
		Tcl_SetResult(interp, "Ridiculously long address", TCL_STATIC);
		*hardError = 1;
		goto error;
	    } else {
		buf[0] = '\0';
		rfc822_address(buf, adrPtr);
		Tcl_DStringAppendElement(&ds, buf);
	    }
	}
	for (adrPtr = env->bcc; adrPtr; adrPtr = adrPtr->next) {
	    if (RatAddressSize(adrPtr, 0) > sizeof(buf)-1) {
		Tcl_SetResult(interp, "Ridiculously long address", TCL_STATIC);
		*hardError = 1;
		goto error;
	    } else {
		buf[0] = '\0';
		rfc822_address(buf, adrPtr);
		Tcl_DStringAppendElement(&ds, buf);
	    }
	}

	Tcl_ResetResult(interp);
	if (TCL_OK != Tcl_SplitList(interp, Tcl_DStringValue(&ds),
		    &listArgc, &listArgv)
		|| (NULL == (channel = Tcl_OpenCommandChannel(interp, listArgc,
		    listArgv, TCL_STDIN|TCL_STDOUT|TCL_STDERR)))) {
	    rPtr = Tcl_NewStringObj("Failed to run send program: ", -1);
	    Tcl_AppendToObj(rPtr, Tcl_GetStringResult(interp), -1);
	    Tcl_SetObjResult(interp, rPtr);
	    goto error;
	} else {
	    Tcl_DStringFree(&ds);
	    ckfree(listArgv);
	    rfc822_output(header, env, body, RatTclPutsSendmail, channel, i);
	    Tcl_Close(interp, channel);
	}
    } else {
	snprintf(buf, sizeof(buf), "Invalid send protocol '%s'",
		tmp ? tmp : "<NULL>");
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	*hardError = 1;
	goto error;
    }
    if (verbose) {
	RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
    }

    saveTo = Tcl_GetVar2(interp, handler, "save_to", TCL_GLOBAL_ONLY);
    if (saveTo && *saveTo) {
	MESSAGECACHE elt;
	Tcl_DString saveArg, ds;
	Tcl_Channel channel;
	char saveFile[1024];
	struct tm *tmPtr;
	time_t now;
	int perm;

	Tcl_DStringInit(&saveArg);
	Tcl_DStringInit(&ds);
	ctmp = Tcl_GetVar(interp, "rat_tmp", TCL_GLOBAL_ONLY);
	RatGenId(NULL, interp, 0, NULL);
	snprintf(saveFile, sizeof(saveFile), "%s/rat.%s",
		 ctmp, Tcl_GetStringResult(interp));

	oPtr = Tcl_GetVar2Ex(interp, "option", "permissions", TCL_GLOBAL_ONLY);
	Tcl_GetIntFromObj(interp, oPtr, &perm);
	if (NULL == (channel=Tcl_OpenFileChannel(interp,saveFile,"a",perm))){
	    Tcl_AppendResult(interp, "Failed to save copy of message: ",
		    Tcl_PosixError(interp), (char*) NULL);
	    goto error;
	}
	rfc822_output(header, env, body, RatTclPuts, channel, 1);
	Tcl_Close(interp, channel);

	Tcl_DStringAppendElement(&saveArg, saveFile);
	Tcl_DStringAppendElement(&saveArg, saveTo);
	Tcl_DStringSetLength(&ds, RatAddressSize(env->to, 1));
	Tcl_DStringValue(&ds)[0] = '\0';
	rfc822_write_address(Tcl_DStringValue(&ds), env->to);
	Tcl_DStringSetLength(&ds, strlen(Tcl_DStringValue(&ds)));
	Tcl_DStringAppendElement(&saveArg,
		RatDecodeHeader(interp, Tcl_DStringValue(&ds), 1));
	if (env->from) {
	    Tcl_DStringSetLength(&ds, RatAddressSize(env->from, 1));
	    Tcl_DStringValue(&ds)[0] = '\0';
	    rfc822_write_address(Tcl_DStringValue(&ds), env->from);
	    Tcl_DStringSetLength(&ds, strlen(Tcl_DStringValue(&ds)));
	} else {
	    s = RatGetCurrent(interp, RAT_MAILBOX, role);
	    Tcl_DStringSetLength(&ds, strlen(s) + strlen(host)+2);
	    sprintf(Tcl_DStringValue(&ds), "%s@%s", s, host);
	}
	Tcl_DStringAppendElement(&saveArg,
		RatDecodeHeader(interp, Tcl_DStringValue(&ds), 1));
	Tcl_DStringSetLength(&ds, RatAddressSize(env->cc, 1));
	Tcl_DStringValue(&ds)[0] = '\0';
	rfc822_write_address(Tcl_DStringValue(&ds), env->cc);
	Tcl_DStringSetLength(&ds, strlen(Tcl_DStringValue(&ds)));
	Tcl_DStringAppendElement(&saveArg,
		RatDecodeHeader(interp, Tcl_DStringValue(&ds), 1));
	Tcl_DStringAppendElement(&saveArg, env->message_id);
	Tcl_DStringAppendElement(&saveArg,
		(env->in_reply_to ? env->in_reply_to : env->references));
	Tcl_DStringAppendElement(&saveArg,
		RatDecodeHeader(interp, env->subject, 0));
	Tcl_DStringAppendElement(&saveArg, flag_name[RAT_SEEN].imap_name);
	now = time(NULL);
	tmPtr = gmtime(&now);
	elt.day = tmPtr->tm_mday;
	elt.month = tmPtr->tm_mon+1;
	elt.year = tmPtr->tm_year+1900-BASEYEAR;
	elt.hours = tmPtr->tm_hour;
	elt.minutes = tmPtr->tm_min;
	elt.seconds = tmPtr->tm_sec;
	elt.zoccident = 0;
	elt.zhours = 0;
	elt.zminutes = 0;
	Tcl_DStringAppendElement(&saveArg, mail_date(buf, &elt));
	fprintf(stdout, "SAVE %s", Tcl_DStringValue(&saveArg));
	fputc('\0', stdout);
	fflush(stdout);
	Tcl_DStringFree(&saveArg);
    }

    mail_free_envelope(&env);
    mail_free_body(&body);
    ckfree(header);
    return TCL_OK;

error:
    if (verbose) {
	RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
    }
    ckfree(header);
    mail_free_envelope(&env);
    mail_free_body(&body);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * RatSenderStanddown --
 *
 *	Closes the open SMTP channel (if open)
 *
 * Results:
 *	None
 *
 * Side effects:
 *      All open SMTP channels are closed.
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatSenderStanddown(Tcl_Interp *interp)
{
    int verbose;
    Tcl_Obj *oPtr;

    oPtr = Tcl_GetVar2Ex(interp, "option", "smtp_verbose", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &verbose);
    RatSMTPCloseAll(interp, verbose);
}


/*
 *----------------------------------------------------------------------
 *
 * RatCreateBody --
 *
 *	See ../doc/interface
 *
 * Results:
 *      The return value is normally TCL_OK and the result can be found
 *      in the result area. If something goes wrong TCL_ERROR is returned
 *      and an error message will be left in the result area.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */
static BODY*
RatCreateBody(Tcl_Interp *interp, char *handler, ENVELOPE *env,
	      int *errorFlag, Tcl_Obj *filesPtr, const char *role)
{
    BODY *body = mail_newbody();
    CONST84 char *type;
    CONST84 char *encoding;
    CONST84 char *parameter;
    char buf[1024];
    CONST84 char *filename = NULL;
    CONST84 char *children;
    int pgp_sign, pgp_encrypt;
    Tcl_Obj *oPtr;

    if (NULL == (type = Tcl_GetVar2(interp, handler, "type",
	    TCL_GLOBAL_ONLY))) {
	Tcl_SetResult(interp, "Internal error, no bhandler(type)", TCL_STATIC);
	(*errorFlag)++;
	return body;
    }
    if (!strcasecmp(type, "text"))		body->type = TYPETEXT;
    else if (!strcasecmp(type, "multipart"))	body->type = TYPEMULTIPART;
    else if (!strcasecmp(type, "message"))	body->type = TYPEMESSAGE;
    else if (!strcasecmp(type, "application"))	body->type = TYPEAPPLICATION;
    else if (!strcasecmp(type, "audio"))	body->type = TYPEAUDIO;
    else if (!strcasecmp(type, "image"))	body->type = TYPEIMAGE;
    else if (!strcasecmp(type, "video"))	body->type = TYPEVIDEO;
    else					body->type = TYPEOTHER;
    if (NULL == (encoding = Tcl_GetVar2(interp, handler, "encoding",
	    TCL_GLOBAL_ONLY))) {
	body->encoding = ENC7BIT;
    } else {
	if (!strcasecmp(encoding, "7bit"))	    body->encoding = ENC7BIT;
	else if (!strcasecmp(encoding, "8bit"))	    body->encoding = ENC8BIT;
	else if (!strcasecmp(encoding, "binary"))   body->encoding = ENCBINARY;
	else if (!strcasecmp(encoding, "base64"))   body->encoding = ENCBASE64;
	else if (!strcasecmp(encoding, "quoted-printable"))
					body->encoding = ENCQUOTEDPRINTABLE;
	else {
	    snprintf(buf, sizeof(buf), "Unkown encoding %s\n", encoding);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    (*errorFlag)++;
	}
    }
    /*
     * Force all non-text and non-message bodyparts to binary
     * (unless already encoded)
     */
    if (TYPETEXT != body->type && TYPEMESSAGE != body->type
	&& (ENC7BIT == body->encoding || ENC8BIT == body->encoding)) {
	body->encoding = ENCBINARY;
    }

    if (NULL == (body->subtype = (char*)Tcl_GetVar2(interp, handler, "subtype",
						    TCL_GLOBAL_ONLY))) {
	Tcl_SetResult(interp, "Internal error, no bhandler(subtype)",
		      TCL_STATIC);
	(*errorFlag)++;
	return body;
    }
    body->subtype = cpystr(body->subtype);
    if ((parameter = (char*)Tcl_GetVar2(interp, handler, "parameter",
					TCL_GLOBAL_ONLY))){
	(*errorFlag) += RatParseParameter(interp, parameter, &body->parameter);
    }
    if ((parameter =
	 Tcl_GetVar2(interp, handler, "disp_parm", TCL_GLOBAL_ONLY))) {
	(*errorFlag) += RatParseParameter(interp, parameter,
					  &body->disposition.parameter);
    }
    body->disposition.type = cpystr(Tcl_GetVar2(interp, handler,
	    "disp_type", TCL_GLOBAL_ONLY));
    if (body->disposition.type && !strlen(body->disposition.type)) {
	body->disposition.type = NULL;
    }
    body->id = cpystr(Tcl_GetVar2(interp, handler, "id", TCL_GLOBAL_ONLY));
    if (body->id && !strlen(body->id)) {
	body->id = NULL;
    }
    oPtr = Tcl_GetVar2Ex(interp, handler, "description", TCL_GLOBAL_ONLY);
    if (oPtr) {
	body->description = cpystr(RatEncodeHeaderLine(interp,oPtr,13));
    }
    if (body->description && !strlen(body->description)) {
	body->description = NULL;
    }

    if (TYPEMULTIPART == body->type && NULL != (children =
	    Tcl_GetVar2(interp, handler, "children", TCL_GLOBAL_ONLY))) {
	PART **partPtrPtr = &body->nested.part;
	int childrenArgc, i;
	CONST84 char **childrenArgv;
	/* Convert to Tcl_Obj */
	Tcl_SplitList(interp, children, &childrenArgc, &childrenArgv);
	for (i=0; i<childrenArgc; i++) {
	    *partPtrPtr = mail_newbody_part();
	    (*partPtrPtr)->body =
		*RatCreateBody(interp, (char*)childrenArgv[i],
			       env, errorFlag, filesPtr, role);
	    partPtrPtr = &(*partPtrPtr)->next;
	}
    } else if (TYPEMESSAGE == body->type) {
	unsigned char *message;

	if (NULL == (filename = Tcl_GetVar2(interp, handler,"filename",
		TCL_GLOBAL_ONLY))) {
	    Tcl_SetResult(interp, "Internal error, no bhandler(filename)",
			  TCL_STATIC);
	    (*errorFlag)++;
	    return body;
	}
	message = RatReadFile(interp, filename, &body->contents.text.size, 1);
	if (NULL == message) {
	    (*errorFlag)++;
	    return body;
	}
	body->nested.msg = RatParseMsg(interp, message);
	body->contents.text.data = (unsigned char*)message;

    } else {
	unsigned char *data;

	if (NULL == (filename = Tcl_GetVar2(interp, handler, "filename",
					    TCL_GLOBAL_ONLY))) {
	    Tcl_SetResult(interp, "Internal error, no bhandler(filename)",
			  TCL_STATIC);
	    (*errorFlag)++;
	    return body;
	}
	if (ENCBINARY != body->encoding && TYPETEXT == body->type) {
	    data = RatReadFile(interp, filename, &body->contents.text.size,1);
	} else {
	    data = RatReadFile(interp, filename, &body->contents.text.size,0);
	}
	if (NULL == data) {
	    (*errorFlag)++;
	    return body;
	}
	body->contents.text.data = data;
	body->size.bytes = body->contents.text.size;
	
	if (TYPEMULTIPART == body->type) {
	    body->type = TYPEAPPLICATION;
	    ckfree(body->subtype);
	    body->subtype = cpystr("octet-stream");
	}
    }
    if (filename) {
	int removeFile = 0;

	if ((oPtr=Tcl_GetVar2Ex(interp,handler,"removeFile",TCL_GLOBAL_ONLY))){
	    Tcl_GetBooleanFromObj(interp, oPtr, &removeFile);
	}
	if (removeFile && filesPtr) {
	    Tcl_ListObjAppendElement(interp, filesPtr,
				     Tcl_NewStringObj(filename, -1));
	}
    }

    /*
     * Do PGP signing and/or encryption
     */
    if (!(oPtr = Tcl_GetVar2Ex(interp, handler, "pgp_sign",TCL_GLOBAL_ONLY)) ||
         TCL_OK != Tcl_GetBooleanFromObj(interp, oPtr, &pgp_sign)) {
	pgp_sign = 0;
    }
    if (!(oPtr = Tcl_GetVar2Ex(interp, handler,"pgp_encrypt",TCL_GLOBAL_ONLY))
	|| TCL_OK != Tcl_GetBooleanFromObj(interp, oPtr, &pgp_encrypt)) {
	pgp_encrypt = 0;
    }
    if (pgp_sign || pgp_encrypt) {
	if (pgp_encrypt) {
	    body = RatPGPEncrypt(interp, env, body, pgp_sign);
	} else {
	    snprintf(buf, sizeof(buf), "%s,pgp_keyid", role);
	    body = RatPGPSign(interp, env, body,
			      Tcl_GetVar2(interp, "option", buf,
					  TCL_GLOBAL_ONLY));
	}
	if (!body) {
	    Tcl_SetResult(interp, "Failed to do PGP operation", TCL_STATIC);
	    (*errorFlag)++;
	    return body;
	}
    }

    return body;
}

/*
 *----------------------------------------------------------------------
 *
 * RatParseParameter --
 *
 *	Splits a parameter list into elements
 *
 * Results:
 *      number of encountered errors
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatParseParameter(Tcl_Interp *interp, const char *src, PARAMETER **dstPtrPtr)
{
    int parmListArgc, parmArgc, i, errcount = 0;
    CONST84 char **parmListArgv, **parmArgv;
    Tcl_Obj *oPtr;
    char buf[1024];

    /* Convert to Tcl_Obj? */
    Tcl_SplitList(interp, src, &parmListArgc, &parmListArgv);
    for (i=0; i<parmListArgc; i++) {
	if (TCL_ERROR == Tcl_SplitList(interp, parmListArgv[i], &parmArgc,
		&parmArgv) || 2 != parmArgc) {
	    snprintf(buf, sizeof(buf), "Illegal parameter: %s",
		    parmListArgv[i]);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    errcount++;
	} else {
	    *dstPtrPtr = mail_newbody_parameter();
	    (*dstPtrPtr)->attribute = cpystr(parmArgv[0]);
	    oPtr = Tcl_NewStringObj(parmArgv[1], -1);
	    (*dstPtrPtr)->value = cpystr(RatEncodeHeaderLine(interp, oPtr, 0));
	    Tcl_DecrRefCount(oPtr);
	    dstPtrPtr = &(*dstPtrPtr)->next;
	}
	ckfree(parmArgv);
    }
    ckfree(parmListArgv);
    return errcount;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendPGPCommand --
 *
 *	Sends a PGP command to the master and then waits for a reply.
 *
 * Results:
 *      A pointer to a static buffer containing the command.
 *
 * Side effects:
 *      Other commands that arrive while we are waiting are placed in cmdList
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatSendPGPCommand(char *cmd)
{
    static char buf[1024];
    CmdList **cmdPtrPtr;

    fwrite(cmd, strlen(cmd)+1, 1, stdout);
    fflush(stdout);
    for (cmdPtrPtr = &cmdList; *cmdPtrPtr; cmdPtrPtr = &(*cmdPtrPtr)->next);
    while(1) {
	fgets(buf, sizeof(buf), stdin);
	if (feof(stdin)) {
	    exit(0);
	}
	buf[strlen(buf)-1] = '\0';
	if (strncmp("PGP ", buf, 4)) {
	    *cmdPtrPtr = (CmdList*)ckalloc(sizeof(CmdList));
	    (*cmdPtrPtr)->cmd = cpystr(buf);
	    (*cmdPtrPtr)->next = NULL;
	    cmdPtrPtr = &(*cmdPtrPtr)->next;
	} else {
	    return buf+4;
	}
    }
}
