/*
 * ratMessage.c --
 *
 *	This file contains code which implements the message entities.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <signal.h>
#include <unistd.h>
#include "ratStdFolder.h"
#include "ratPGP.h"
#include "osdep.h"

/*
 * An array of commands. It contains one entry for each internal message
 * type (as defined by RatMessageType).
 */
static MessageProcInfo *messageProcInfo = NULL;

/*
 * The number of replies created. This is used to create new unique
 * message handlers.
 */
static int numReplies = 0;

/*
 * The number of message entities created. This is used to create new
 * unique command names.
 */
static int numBodies = 0;

static void RatCreateBody(Tcl_Interp *interp, MessageInfo *msgPtr);
static void RatCreateChildren(Tcl_Interp *interp, BodyInfo *bodyInfoPtr);
static void RatBodyDelete(Tcl_Interp *interp, BodyInfo *bodyInfoPtr);
static BodyInfo *RatFindFirstText(BodyInfo *bodyInfoPtr);
static CONST84 char *RatGetCitation(Tcl_Interp *interp, MessageInfo *msgPtr);
static void RatCiteMessage(Tcl_Interp *interp, Tcl_Obj *dstObjPtr, 
			   CONST84 char *src, CONST84 char *myCitation);
static int RatMessageDeleteAttachments(Tcl_Interp *interp, MessageInfo *msgPtr,
                                       Tcl_Obj *attachments);
static int RatDeleteAttachment(Tcl_Interp *interp, MessageInfo *msgPtr,
                               Tcl_DString *ds, Tcl_Obj *spec);
static char* RatFindAttachment(Tcl_Interp *interp, BodyInfo *bodyInfoPtr,
                               char *text, Tcl_Obj *spec, int spec_index,
                               char **boundary);

extern long unix_create (MAILSTREAM *stream,char *mailbox);


/*
 *----------------------------------------------------------------------
 *
 * RatInitMessages --
 *
 *      Initialize the message data structures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The messageProcInfo array is allocated and initialized.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatInitMessages()
{
    messageProcInfo = (MessageProcInfo*)ckalloc(3*sizeof(MessageProcInfo));
    RatStdMessagesInit(&messageProcInfo[RAT_CCLIENT_MESSAGE]);
    RatDbMessagesInit(&messageProcInfo[RAT_DBASE_MESSAGE]);
    RatFrMessagesInit(&messageProcInfo[RAT_FREE_MESSAGE]);
}


/*
 *----------------------------------------------------------------------
 *
 * RatMessageCmd --
 *
 *      Main std mail entity procedure. This routine implements the mail
 *	commands mentioned in ../INTERFACE.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	many.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatMessageCmd(ClientData clientData,Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    MessageInfo *msgPtr = (MessageInfo*) clientData;
    Tcl_Obj *rPtr;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (!strcmp(Tcl_GetString(objv[1]), "headers")) {
	if (objc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " headers\"", (char *) NULL);
	    return TCL_ERROR;
	}
	return RatMessageGetHeader(interp,
		(*messageProcInfo[msgPtr->type].getHeadersProc)(interp,
								msgPtr));

    } else if (!strcmp(Tcl_GetString(objv[1]), "body")) {
	if (!msgPtr->bodyInfoPtr) {
            RatCreateBody(interp, msgPtr);
	}
	Tcl_SetResult(interp, msgPtr->bodyInfoPtr->cmdName, TCL_STATIC);
	return TCL_OK;
    
    } else if (!strcmp(Tcl_GetString(objv[1]), "rawText")) {
	rPtr = Tcl_NewObj();
	Tcl_AppendToObj(rPtr,
	      (*messageProcInfo[msgPtr->type].getHeadersProc)(interp, msgPtr),
	      -1);
	Tcl_AppendToObj(rPtr, "\r\n", 2);
	Tcl_AppendToObj(rPtr,
	      (*messageProcInfo[msgPtr->type].fetchTextProc)(interp, msgPtr),
	      -1);
	Tcl_SetObjResult(interp, rPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "get")) {
	ENVELOPE *env = (*messageProcInfo[msgPtr->type].envelopeProc)(msgPtr);
	int i;
	if (objc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " get fields\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (i=2; i<objc; i++) {
	    if (!strcasecmp(Tcl_GetString(objv[i]), "return_path")) {
		RatInitAddresses(interp, env->return_path);
	    } else if (!strcasecmp(Tcl_GetString(objv[i]), "from")) {
		RatInitAddresses(interp, env->from);
	    } else if (!strcasecmp(Tcl_GetString(objv[i]), "sender")) {
		RatInitAddresses(interp, env->sender);
	    } else if (!strcasecmp(Tcl_GetString(objv[i]), "reply_to")) {
		RatInitAddresses(interp, env->reply_to);
	    } else if (!strcasecmp(Tcl_GetString(objv[i]), "to")) {
		RatInitAddresses(interp, env->to);
	    } else if (!strcasecmp(Tcl_GetString(objv[i]), "cc")) {
		RatInitAddresses(interp, env->cc);
	    } else if (!strcasecmp(Tcl_GetString(objv[i]), "bcc")) {
		RatInitAddresses(interp, env->bcc);
	    } else {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad field \"",Tcl_GetString(objv[i]),
			"\": must be one of return_path, from, sender, ",
			"reply_to, to, cc or bcc", (char*)NULL);
		return TCL_ERROR;
	    }
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "reply")) {
	/*
	 * Construct a reply to a message. We should really handle
	 * different character sets here. /MaF
	 */
	ENVELOPE *env = (*messageProcInfo[msgPtr->type].envelopeProc)(msgPtr);
	char handler[32], *cPtr;
	BodyInfo *bodyInfoPtr;
	unsigned long bodylength;
	ADDRESS *adrPtr;
	char *dataPtr, *role;
	Tcl_DString ds;
	Tcl_Obj *oPtr, *vPtr;

	Tcl_DStringInit(&ds);

	if (objc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " reply to role\"", (char *) NULL);
	    return TCL_ERROR;
	}
        role = Tcl_GetString(objv[3]);
	sprintf(handler, "reply%d", numReplies++);
	if (!strcasecmp(Tcl_GetString(objv[2]), "sender")) {
	    /*
	     * We should construct a reply which should go only to one person.
	     * We look for the address in the following fields:
	     *  Reply-To:, From:. Sender:, From
	     * As sson as an address is found the search stops.
	     */
	    if (env->reply_to) {
		adrPtr = env->reply_to;
	    } else if (env->from) {
		adrPtr = env->from;
	    } else if (env->sender) {
		adrPtr = env->sender;
	    } else {
		adrPtr = env->return_path;
	    }
	    for (;adrPtr; adrPtr = adrPtr->next) {
		RatAddressTranslate(interp, adrPtr);
		if (adrPtr->mailbox) {
		    if (Tcl_DStringLength(&ds)) {
			Tcl_DStringAppend(&ds, ", ", -1);
		    }
		    Tcl_DStringAppend(&ds,
                                      RatAddressFull(interp, adrPtr, role),-1);
		}
	    }
	    Tcl_SetVar2(interp, handler, "to", Tcl_DStringValue(&ds),
		    TCL_GLOBAL_ONLY);
	} else {
	    /*
	     * We should construct a reply which goes to everybody who has
	     * recieved this message. This is done by first collecting all
	     * addresses found in: Reply-To:, From:, Sender:, To: and Cc:.
	     * Then go though this list and eliminate myself and any
	     * duplicates. Now we use the first element of the list as To:
	     * and the rest as Cc:.
	     */
	    ADDRESS **recipientPtrPtr =(ADDRESS**)ckalloc(16*sizeof(ADDRESS*));
	    int numAllocated = 16;
	    int numRecipients = 0;
	    int inList = 0;
	    ADDRESS *to = NULL;
	    int i, j;

#define SCANLIST(x) for (adrPtr = (x); adrPtr; adrPtr = adrPtr->next) { \
			if (numRecipients == numAllocated) { \
			    numAllocated += 16; \
			    recipientPtrPtr = (ADDRESS**)ckrealloc( \
				    recipientPtrPtr, \
				    numAllocated*sizeof(ADDRESS*)); \
			} \
			recipientPtrPtr[numRecipients++] = adrPtr; \
	    	    }

	    SCANLIST(env->reply_to);
	    if (NULL == env->reply_to) {
		SCANLIST(env->from);
	    }
	    SCANLIST(env->to);
	    SCANLIST(env->cc);

	    for (i=0; i<numRecipients; i++) {
		adrPtr = recipientPtrPtr[i];
		if (!adrPtr->host) {
		    inList = (inList)? 0 : 1;
		    continue;
		}
		if (RatAddressIsMe(interp, adrPtr, 1)) {
		    continue;
		}
		RatAddressTranslate(interp, adrPtr);
		for (j=0; j<i; j++) {
		    if (!RatAddressCompare(adrPtr, recipientPtrPtr[j])) {
			break;
		    }
		}
		if (j < i) {
		    continue;
		}
		if (!to) {
		    to = adrPtr;
		} else {
		    if (Tcl_DStringLength(&ds)) {
			Tcl_DStringAppend(&ds, ", ", 2);
		    }
		    Tcl_DStringAppend(&ds,
                                      RatAddressFull(interp, adrPtr, role),-1);
		}
	    }
	    if (Tcl_DStringLength(&ds)) {
		Tcl_SetVar2(interp, handler, "cc", Tcl_DStringValue(&ds),
			TCL_GLOBAL_ONLY);
	    }
	    if (!to && numRecipients) {
		to = recipientPtrPtr[0];
	    }
	    if (to) {
		Tcl_SetVar2(interp, handler, "to",
                            RatAddressFull(interp, to, role), TCL_GLOBAL_ONLY);
	    }
	    ckfree(recipientPtrPtr);
	}
	if (env->subject && *env->subject) {
	    int match;

	    cPtr = RatDecodeHeader(interp, env->subject, 0);
	    Tcl_DStringSetLength(&ds, 0);
	    Tcl_DStringAppendElement(&ds, "regexp");
	    Tcl_DStringAppendElement(&ds, "-nocase");
	    Tcl_DStringAppendElement(&ds,
		    Tcl_GetVar2(interp, "option","re_regexp",TCL_GLOBAL_ONLY));
	    Tcl_DStringAppendElement(&ds, cPtr);
	    Tcl_DStringAppendElement(&ds, "reply_match");
	    if (TCL_OK == Tcl_EvalEx(interp, Tcl_DStringValue(&ds), -1,
				     TCL_EVAL_DIRECT)
		&& NULL != (oPtr = Tcl_GetObjResult(interp))
		&& TCL_OK == Tcl_GetBooleanFromObj(interp, oPtr, &match)
		&& match) {
		CONST84 char *s = Tcl_GetVar(interp, "reply_match", 0);

		if (!strncmp(s, cPtr, strlen(s))) {
		    cPtr += strlen(s);
		    while (isspace(*cPtr)) cPtr++;
		}
	    }
	    oPtr = Tcl_NewStringObj("Re: ", 4);
	    Tcl_AppendToObj(oPtr, cPtr, -1);
	    Tcl_SetVar2Ex(interp, handler,"subject", oPtr,TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar2(interp, handler, "subject",
		    Tcl_GetVar2(interp, "option","no_subject",TCL_GLOBAL_ONLY),
		    TCL_GLOBAL_ONLY);
	}
	if (env->references || env->in_reply_to) {
            Tcl_DStringSetLength(&ds, 0);
            if (env->references) {
                Tcl_DStringAppend(&ds, env->references, -1);
            } else {
                Tcl_DStringAppend(&ds, env->in_reply_to, -1);
            }
	    if (env->message_id) {
	        Tcl_DStringAppend(&ds, " ", 1);
	        Tcl_DStringAppend(&ds, env->message_id, -1);
	    }
	    Tcl_SetVar2(interp, handler, "references", Tcl_DStringValue(&ds),
                        TCL_GLOBAL_ONLY);
	} else if (env->message_id) {
	    Tcl_SetVar2(interp, handler, "references", env->message_id,
                        TCL_GLOBAL_ONLY);
        }
	if (env->message_id) {
	    Tcl_SetVar2(interp, handler, "in_reply_to", env->message_id,
		    TCL_GLOBAL_ONLY);
	}
	bodyInfoPtr = RatFindFirstText(msgPtr->bodyInfoPtr);
	if (bodyInfoPtr && (NULL != (dataPtr =
		(*messageProcInfo[bodyInfoPtr->msgPtr->type].fetchBodyProc)
		(bodyInfoPtr, &bodylength)))) {
	    CONST84 char *alias, *attrFormat, *charset = "us-ascii", *citation;
	    ListExpression *exprPtr;
	    PARAMETER *parameter;
	    Tcl_DString *decBPtr;
	    BODY *bodyPtr;
	    int wrap;

	    bodyPtr = bodyInfoPtr->bodyPtr;

	    for (parameter = bodyPtr->parameter; parameter;
		    parameter = parameter->next) {
		if ( 0 == strcasecmp("charset", parameter->attribute)) {
		    charset = parameter->value;
		}
	    }
	    if ((alias = Tcl_GetVar2(interp, "charsetAlias",
				     (char*)charset, TCL_GLOBAL_ONLY))) {
		charset = alias;
	    }
	    decBPtr = RatDecode(interp, bodyPtr->encoding, dataPtr, bodylength,
		    charset);
	    attrFormat = Tcl_GetVar2(interp, "option", "attribution",
		    TCL_GLOBAL_ONLY);
	    if (attrFormat && strlen(attrFormat)
		    && (exprPtr = RatParseList(attrFormat, NULL))) {
		oPtr = RatDoList(interp, exprPtr,
			messageProcInfo[msgPtr->type].getInfoProc,
			(ClientData)msgPtr, 0);
		RatFreeListExpression(exprPtr);
		Tcl_AppendToObj(oPtr, "\n", 1);
	    } else {
		oPtr = Tcl_NewObj();
	    }
	    Tcl_IncrRefCount(oPtr);

	    citation = RatGetCitation(interp, msgPtr);
	    RatCiteMessage(interp, oPtr, Tcl_DStringValue(decBPtr), citation);
	    Tcl_DStringFree(decBPtr);
	    vPtr = Tcl_GetVar2Ex(interp,"option","wrap_cited",TCL_GLOBAL_ONLY);
	    Tcl_GetBooleanFromObj(interp, vPtr, &wrap);
	    if (wrap) {
		Tcl_Obj *nPtr;
		nPtr = RatWrapMessage(interp, oPtr);
		oPtr = nPtr;
	    }
	    Tcl_SetVar2Ex(interp, handler, "data", oPtr, TCL_GLOBAL_ONLY);
	    Tcl_SetVar2(interp, handler, "data_tags", "Cited noWrap no_spell",
			TCL_GLOBAL_ONLY);
	    Tcl_DecrRefCount(oPtr);
	}
	Tcl_SetResult(interp, handler, TCL_VOLATILE);
	Tcl_DStringFree(&ds);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "copy")) {
	char flags[128], date[128], *spec, *name;
	Tcl_Obj *defPtr, **dobjv, **eobjv, *oPtr;
	MAILSTREAM *stream = NULL;
	struct stat sbuf;
	Tcl_DString ds, specBuf;
	STRING string;
	int dobjc, eobjc, result, i, freeListObjv = 0, specBufUse = 0;
	RatFolderInfo *infoPtr;

	if (objc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0])," copy vfolder_def\"", NULL);
	    return TCL_ERROR;
	}

	defPtr = objv[2];
        infoPtr = RatGetOpenFolder(interp, defPtr);
	if (infoPtr) {
	    name = msgPtr->name;
            result = RatFolderInsert(interp, infoPtr, 1, &name);
	    RatFolderClose(interp, infoPtr, 0);
	    return result;
	} 

	Tcl_ListObjGetElements(interp, defPtr, &dobjc, &dobjv);

	/*
	 * If the destination is dbase then we call RatInsert
	 */
	if (!strcmp("dbase", Tcl_GetString(dobjv[1]))) {
	    Tcl_ListObjGetElements(interp, dobjv[5], &eobjc, &eobjv);
	    for (i=0; i<eobjc-1; i++) {
		if (!strcmp("keywords", Tcl_GetString(eobjv[i]))) {
		    break;
		}
	    }
	    oPtr = Tcl_NewListObj(eobjc-i-1, &eobjv[i+1]);
	    Tcl_IncrRefCount(oPtr);
	    result = RatInsertMsg(interp, msgPtr, Tcl_GetString(oPtr),
		    Tcl_GetString(dobjv[4]), Tcl_GetString(dobjv[3]));
	    Tcl_DecrRefCount(oPtr);
	    return result;
	}

	/*
	 * If the destination is a dynamic folder then we have to do some
	 * magic.
	 */
	if (!strcmp("dynamic", Tcl_GetString(dobjv[1]))) {
	    char *name = NULL;
	    Tcl_Obj *oPtr;

	    oPtr = (*messageProcInfo[msgPtr->type].getInfoProc)(interp,
		    (ClientData)msgPtr, RAT_FOLDER_MAIL_REAL, 0);
	    if (oPtr) {
		name = Tcl_GetString(oPtr);
	    }
	    if (!name) {
		struct passwd *passwdPtr = getpwuid(getuid());
		name = passwdPtr->pw_name;
	    }
	    defPtr = Tcl_NewObj();
	    Tcl_ListObjAppendElement(interp, defPtr, dobjv[0]);
	    Tcl_IncrRefCount(dobjv[0]);
	    Tcl_ListObjAppendElement(interp, defPtr,
				     Tcl_NewStringObj("file", 4));
	    Tcl_ListObjAppendElement(interp, defPtr, dobjv[2]);
	    Tcl_IncrRefCount(dobjv[2]);
	    oPtr = Tcl_DuplicateObj(dobjv[3]);
	    Tcl_AppendToObj(oPtr, "/", 1);
	    for (i=0; name[i] && name[i] != '@'; i++);
	    Tcl_AppendToObj(oPtr, name, i);
	    Tcl_ListObjAppendElement(interp, defPtr, oPtr);
	    freeListObjv = 1;
	    Tcl_ListObjGetElements(interp, defPtr, &dobjc, &dobjv);
	}

	/*
	 * Try to create nonexisting files
	 */
	if (!strcmp("file", Tcl_GetString(dobjv[1]))) {
	    name = RatGetFolderSpec(interp, defPtr);
	    if (!strcmp(name, "INBOX")) {
		name = sysinbox();
	    }
	    if (0 != stat(name, &sbuf)) {
		unix_create(NIL, name);
	    }
	}

	/*
	 * Try the case where both source and destination are c-client
	 * messages of sufficiently same type.
	 */
	if (RAT_CCLIENT_MESSAGE == msgPtr->type
	    && RatStdEasyCopyingOK(interp, msgPtr, defPtr)) {
	    result = RatStdMessageCopy(interp, msgPtr,
				       RatGetFolderSpec(interp, defPtr));
	    goto end_copy;
	}

	/*
	 * Open a folder and get the stream
	 */
	spec = RatGetFolderSpec(interp, defPtr);
	stream = OpenStdFolder(interp, spec, NULL);
	if (stream) {
	    Tcl_DStringInit(&ds);
	    RatMessageGet(interp, msgPtr, &ds, flags, sizeof(flags),
		    date, sizeof(date));
	    if ('\n' != Tcl_DStringValue(&ds)[Tcl_DStringLength(&ds)-1]) {
		Tcl_DStringAppend(&ds, "\r\n", 2);
	    }
	    INIT(&string, mail_string, Tcl_DStringValue(&ds),
		    Tcl_DStringLength(&ds));

	    RatPurgeFlags(flags, 1);
	    if (!mail_append_full(stream, spec, flags, date,
		    &string)){
		CloseStdFolder(interp, stream);
		Tcl_SetResult(interp, "mail_append failed", TCL_STATIC);
		result = TCL_ERROR;
		Tcl_DStringFree(&ds);
		goto end_copy;
	    }
	    if (infoPtr) {
		RatFolderClose(interp, infoPtr, 0);
	    } else {
		CloseStdFolder(interp, stream);
	    }
	    Tcl_DStringFree(&ds);
	    result = TCL_OK;
	} else {
	    result = TCL_ERROR;
	}

end_copy:
	if (specBufUse) {
	    Tcl_DStringFree(&specBuf);
	}
	if (freeListObjv) {
	    Tcl_DecrRefCount(defPtr);
	}
	return result;

    } else if (!strcmp(Tcl_GetString(objv[1]), "list")) {
	ListExpression *exprPtr;
	Tcl_Obj *oPtr;

	if (objc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " list format\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (NULL == (exprPtr = RatParseList(Tcl_GetString(objv[2]), NULL))) {
	    Tcl_SetResult(interp, "Illegal list format", TCL_STATIC);
	    return TCL_ERROR;
	}
	oPtr = RatDoList(interp, exprPtr,
		messageProcInfo[msgPtr->type].getInfoProc,
		(ClientData)msgPtr, 0);
	Tcl_SetObjResult(interp, oPtr);
	RatFreeListExpression(exprPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "pgp")) {
	int sign, encrypt;

	if (objc != 7) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
			     Tcl_GetString(objv[0]),
			     " pgp sign encrypt role signer enc_rcpts\"",
			     (char *) NULL);
	    return TCL_ERROR;
	}
	if (msgPtr->type != RAT_FREE_MESSAGE) {
	    Tcl_SetResult(interp, "pgp only works on internal messages",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_GetBooleanFromObj(interp, objv[2], &sign);
	Tcl_GetBooleanFromObj(interp, objv[3], &encrypt);
	return RatFrMessagePGP(interp, msgPtr, sign, encrypt,
			       Tcl_GetString(objv[4]), Tcl_GetString(objv[5]),
			       objv[6]);
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "remove_internal")) {
	if (msgPtr->type != RAT_FREE_MESSAGE) {
	    Tcl_SetResult(interp, "remove_internal only works on internal "
			  "messages", TCL_STATIC);
	    return TCL_ERROR;
	}
	return RatFrMessageRemoveInternal(interp, msgPtr);
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "duplicate")) {
	char *cmd;	
	Tcl_Obj *oPtr = Tcl_NewObj();
	
	Tcl_AppendToObj(oPtr,
	      (*messageProcInfo[msgPtr->type].getHeadersProc)(interp, msgPtr),
	      -1);
	Tcl_AppendToObj(oPtr, "\r\n", 2);
	Tcl_AppendToObj(oPtr,
	      (*messageProcInfo[msgPtr->type].fetchTextProc)(interp, msgPtr),
	      -1);
	Tcl_IncrRefCount(oPtr);
	cmd = RatFrMessageCreate(interp, Tcl_GetString(oPtr),
				 Tcl_GetCharLength(oPtr), NULL);
	Tcl_DecrRefCount(oPtr);
	
	Tcl_SetResult(interp, cmd, TCL_STATIC);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "delete_attachments")) {
	if (objc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
                             Tcl_GetString(objv[0]), "delete_attachments"
                             " attachments\"", (char*)NULL);
	    return TCL_ERROR;
	}

        /* Create body if needed */
        if (!msgPtr->bodyInfoPtr) {
            RatCreateBody(interp, msgPtr);
        }
        if (msgPtr->bodyInfoPtr->bodyPtr->type != TYPEMULTIPART) {
	    Tcl_AppendResult(interp, "not a multipart message", NULL);
	    return TCL_ERROR;
        }
        return RatMessageDeleteAttachments(interp, msgPtr, objv[2]);
    } else {
	Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
			 "\": must be one of header, body, rawText reply, get"
			 ", pgp, remove_internal, duplicate or "
                         "delete_attachments", (char*)NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatCreateBody --
 *
 *      Create the body of a message
 *
 * Results:
 *	None
 *
 * Side effects:
 *	msgPtr->bodyInfoPtr is filled in a command is created
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatCreateBody(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    msgPtr->bodyInfoPtr =
        (*messageProcInfo[msgPtr->type].createBodyProc)(interp, msgPtr);
    RatPGPBodyCheck(interp, messageProcInfo, &msgPtr->bodyInfoPtr);
    Tcl_CreateObjCommand(interp, msgPtr->bodyInfoPtr->cmdName,
                         RatBodyCmd, (ClientData) msgPtr->bodyInfoPtr, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * RatMessageGetContent --
 *
 *      Gets the content of the message
 *
 * Results:
 *	Is left in header and body.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
void
RatMessageGetContent(Tcl_Interp *interp, MessageInfo *msgPtr,
		     char **header, char **body)
{
    *header = (*messageProcInfo[msgPtr->type].getHeadersProc)(interp, msgPtr);
    *body = (*messageProcInfo[msgPtr->type].fetchTextProc)(interp, msgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatMessageGetHeader --
 *
 *      Gets the header of a message
 *
 * Results:
 *	The header is returned as a list in the result area.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
int
RatMessageGetHeader(Tcl_Interp *interp, char *srcHeader)
{
    char *header, *listArgv[2];
    char *dstPtr, *srcPtr = srcHeader, *cPtr, *tPtr;
    Tcl_Obj *oPtr = Tcl_NewObj(), *fPtr[2];
    int adr;

    if (!srcHeader) {
	RatLog(interp, RAT_FATAL, Tcl_GetStringResult(interp), RATLOG_TIME);
	exit(1);
    }
    header = (char*) ckalloc (strlen(srcHeader)+2);
    if (!strncmp("From ", srcPtr, 5)) {
	while ('\n' != *srcPtr) {
	    srcPtr++;
	}
	if ('\r' == *(++srcPtr)) {
	    srcPtr++;
	}
    }
    while (*srcPtr) {
	dstPtr = header;
	listArgv[0] = dstPtr = header;
	while (*srcPtr && ':' != *srcPtr && ' ' != *srcPtr) {
	    *dstPtr++ = *srcPtr++;
	}
	*dstPtr = '\0';
	fPtr[0] = Tcl_NewStringObj(header, -1);
	cPtr = ++dstPtr;
        if (*srcPtr) {
            do {
                srcPtr++;
            } while (' ' == *srcPtr || '\t' == *srcPtr);
        }
	do {
	    for (; *srcPtr && '\n' != *srcPtr; srcPtr++) {
		if ('\r' != *srcPtr) {
		    *dstPtr++ = *srcPtr;
		}
	    }
	    while ('\n' == *srcPtr || '\r' == *srcPtr) {
		srcPtr++;
	    }
	} while (*srcPtr && (' ' == *srcPtr || '\t' == *srcPtr));
	*dstPtr = '\0';
	tPtr = cPtr;
	if (0 == strncasecmp("resent-", tPtr, 7)) {
	    tPtr += 7;
	} 
	if (!strcasecmp(tPtr, "to")
		|| !strcasecmp(tPtr, "cc")
		|| !strcasecmp(tPtr, "bcc")
		|| !strcasecmp(tPtr, "from")
		|| !strcasecmp(tPtr, "sender")
		|| !strcasecmp(tPtr, "reply-to")) {
	    adr = 1;
	} else {
	    adr = 0;
	}
	fPtr[1] = Tcl_NewStringObj(RatDecodeHeader(interp, cPtr, adr), -1);
	Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewListObj(2, fPtr));
    }
    ckfree(header);
    Tcl_SetObjResult(interp, oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatMessageDelete --
 *
 *      Deletes the given message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The message and all its bodyparts are deleted from the interpreter
 *	and all the structures are freed.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatMessageDelete(Tcl_Interp *interp, char *msgName)
{
    Tcl_CmdInfo cmdInfo;
    MessageInfo *msgPtr;
    char buf[256];
    int i;

    if (0 == Tcl_GetCommandInfo(interp, msgName, &cmdInfo)) {
	Tcl_AppendResult(interp, "No such message: ", msgName, NULL);
	return TCL_ERROR;
    }
    msgPtr = (MessageInfo*)cmdInfo.objClientData;

    (*messageProcInfo[msgPtr->type].msgDeleteProc)(msgPtr);
    if (msgPtr->bodyInfoPtr) {
	if (msgPtr->bodyInfoPtr->altPtr) {
	    RatBodyDelete(interp, msgPtr->bodyInfoPtr->altPtr);
	}
	if (msgPtr->bodyInfoPtr->decodedTextPtr) {
	    Tcl_DStringFree(msgPtr->bodyInfoPtr->decodedTextPtr);
	    ckfree(msgPtr->bodyInfoPtr->decodedTextPtr);
	}
	if (msgPtr->bodyInfoPtr->secPtr) {
	    RatBodyDelete(interp, msgPtr->bodyInfoPtr->secPtr);
	} else {
	    RatBodyDelete(interp, msgPtr->bodyInfoPtr);
	}
    }
    snprintf(buf, sizeof(buf), "msgInfo_%s", msgPtr->name);
    Tcl_UnsetVar(interp, buf, TCL_GLOBAL_ONLY);
    Tcl_DeleteCommand(interp, msgName);
    for (i=0; i<sizeof(msgPtr->info)/sizeof(*msgPtr->info); i++) {
	if (msgPtr->info[i]) {
	    Tcl_DecrRefCount(msgPtr->info[i]);
	}
    }
    ckfree(msgPtr);

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * CreateBodyInfo --
 *
 *      Create and somewhat initialize a BodyInfo structure.
 *
 * Results:
 *	A pointer to a BodyInfo structure.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
BodyInfo*
CreateBodyInfo(Tcl_Interp *interp, MessageInfo *msgPtr, BODY *bodyPtr)
{
    BodyInfo *bodyInfoPtr;
    int pad = sizeof(char*) - sizeof(BodyInfo)%sizeof(char*);

    if (sizeof(char*) == pad) {
	pad = 0;
    }

    bodyInfoPtr = (BodyInfo*)ckalloc(sizeof(BodyInfo)+pad+16);

    bodyInfoPtr->cmdName = (char*)bodyInfoPtr + pad + sizeof(BodyInfo);
    
    sprintf(bodyInfoPtr->cmdName, "RatBody%d", numBodies++);
    bodyInfoPtr->firstbornPtr = NULL;
    bodyInfoPtr->nextPtr = NULL;
    bodyInfoPtr->containedEntity = NULL;
    bodyInfoPtr->bodyPtr = bodyPtr;
    bodyInfoPtr->type = msgPtr->type;
    bodyInfoPtr->msgPtr = msgPtr;
    bodyInfoPtr->secPtr = NULL;
    bodyInfoPtr->altPtr = NULL;
    bodyInfoPtr->decodedTextPtr = NULL;
    bodyInfoPtr->encoded = 0;
    bodyInfoPtr->sigStatus = RAT_UNSIGNED;
    bodyInfoPtr->pgpOutput = NULL;

    RatDecodeParameters(interp, bodyPtr->parameter);
    RatDecodeParameters(interp, bodyPtr->disposition.parameter);
    return bodyInfoPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * RatBodyCmd --
 *
 *      Main bodypart entity procedure. This routine implements the
 *	bodypart commands mentioned in ../INTERFACE.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	many.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatBodyCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    BodyInfo *bodyInfoPtr = (BodyInfo*) clientData;
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    unsigned long length;
    Tcl_Obj *ov[2], *rPtr;

    if (objc < 2) {
	goto usage;
    }
    if (!strcmp(Tcl_GetString(objv[1]), "children")) {
	BodyInfo *partInfoPtr;

	if (TYPEMULTIPART != bodyPtr->type) {
	    return TCL_OK;
	}

	if (!bodyInfoPtr->firstbornPtr) {
            RatCreateChildren(interp, bodyInfoPtr);
	}
	rPtr = Tcl_NewObj();
	for (partInfoPtr = bodyInfoPtr->firstbornPtr; partInfoPtr;
		partInfoPtr = partInfoPtr->nextPtr) {
	    Tcl_ListObjAppendElement(interp, rPtr,
				     Tcl_NewStringObj(partInfoPtr->cmdName,
						      -1));
	}
	Tcl_SetObjResult(interp, rPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "message")) {
	char *body;

	if (!bodyInfoPtr->containedEntity) {
	    if (TYPEMESSAGE != bodyPtr->type &&
		    !strcasecmp(bodyPtr->subtype, "rfc822")) {
		Tcl_SetResult(interp, "Not an message/rfc822 bodypart",
			TCL_STATIC);
		return TCL_ERROR;
	    }
	    body = (*messageProcInfo[bodyInfoPtr->type].fetchBodyProc)
		    (bodyInfoPtr, &length);
	    if (body && *body) {
		bodyInfoPtr->containedEntity =
			RatFrMessageCreate(interp, body, length, NULL);
		Tcl_SetResult(interp, bodyInfoPtr->containedEntity,TCL_STATIC);
	    } else {
		Tcl_SetResult(interp,"Failed to fetch mail body. "
			      "The message is damaged", TCL_STATIC);
		return TCL_ERROR;
	    }
	} else {
	    Tcl_SetResult(interp, bodyInfoPtr->containedEntity, TCL_STATIC);
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "type")) {
	Tcl_SetObjResult(interp, RatBodyType(bodyInfoPtr));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "params")) {
	PARAMETER *parameter;

	rPtr = Tcl_NewObj();
	for (parameter = bodyPtr->parameter; parameter;
		parameter = parameter->next) {
	    ov[0] = Tcl_NewStringObj(parameter->attribute, -1);
	    ov[1] = Tcl_NewStringObj(parameter->value, -1);
	    Tcl_ListObjAppendElement(interp, rPtr, Tcl_NewListObj(2, ov));
	}
	Tcl_SetObjResult(interp, rPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "parameter")) {
	PARAMETER *parameter;

	if (objc != 3) goto usage;
	for (parameter = bodyPtr->parameter; parameter;
		parameter = parameter->next) {
	    if (0 == strcasecmp(Tcl_GetString(objv[2]),parameter->attribute)) {
		Tcl_SetResult(interp, parameter->value, TCL_VOLATILE);
		break;
	    }
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "disp_type")) {
	Tcl_SetResult(interp, bodyPtr->disposition.type, TCL_VOLATILE);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "disp_parm")) {
	PARAMETER *parameter;

	rPtr = Tcl_NewObj();
	for (parameter = bodyPtr->disposition.parameter; parameter;
		parameter = parameter->next) {
	    ov[0] = Tcl_NewStringObj(parameter->attribute, -1);
	    ov[1] = Tcl_NewStringObj(parameter->value, -1);
	    Tcl_ListObjAppendElement(interp, rPtr, Tcl_NewListObj(2, ov));
	}
	Tcl_SetObjResult(interp, rPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "id")) {
	if (bodyPtr->id) {
	    Tcl_SetResult(interp, bodyPtr->id, TCL_VOLATILE);
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "description")) {
	if (bodyPtr->description) {
	    char *desc = RatDecodeHeader(interp, bodyPtr->description, 0);
	    Tcl_SetResult(interp, desc, TCL_VOLATILE);
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "size")) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(bodyPtr->size.bytes));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "lines")) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(bodyPtr->size.lines));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "encoding")) {
	char *enc;

	switch(bodyPtr->encoding) {
	case ENC7BIT:		enc = "7bit"; break;
	case ENC8BIT:		enc = "8bit"; break;
	case ENCBINARY:		enc = "binary"; break;
	case ENCBASE64:		enc = "base64"; break;
	case ENCQUOTEDPRINTABLE:enc = "quoted-printable"; break;
	default:		enc = "unkown"; break;
	}
	Tcl_SetResult(interp, enc, TCL_STATIC);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "isGoodCharset")) {
	PARAMETER *parameter;
	CONST84 char *charset = "us-ascii", *alias;
	int b;

	for (parameter = bodyPtr->parameter; parameter;
		parameter = parameter->next) {
	    if ( 0 == strcasecmp("charset", parameter->attribute)) {
		charset = parameter->value;
		break;
	    }
	}
	if ((alias = Tcl_GetVar2(interp, "charsetAlias", charset,
		TCL_GLOBAL_ONLY))) {
	    charset = alias;
	}
	b = RatGetEncoding(interp, charset) ? 1 : 0;
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(b));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "data")) {
	char *isCharset;
	int encoded;

	if (3 != objc && 4 != objc) goto usage;
	if (TCL_OK != Tcl_GetBooleanFromObj(interp, objv[2], &encoded)) {
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    isCharset = Tcl_GetString(objv[3]);
	} else {
	    isCharset = NULL;
	}
	Tcl_SetObjResult(interp,
		RatBodyData(interp, bodyInfoPtr, encoded, isCharset));
	return TCL_OK;


    } else if (!strcmp(Tcl_GetString(objv[1]), "saveData")) {
	int encoded, convertNL;
	Tcl_Channel channel;

	if (5 != objc) goto usage;
	if (NULL==(channel=Tcl_GetChannel(interp,Tcl_GetString(objv[2]),NULL))
	    || TCL_OK != Tcl_GetBooleanFromObj(interp, objv[3], &encoded)
	    || TCL_OK != Tcl_GetBooleanFromObj(interp, objv[4], &convertNL)) {
	    goto usage;
	}
	return RatBodySave(interp, channel, bodyInfoPtr, encoded, convertNL);

    } else if (!strcmp(Tcl_GetString(objv[1]), "getShowCharset")) {
	CONST84 char *c_charset = "us-ascii", *alias;
	PARAMETER *parmPtr;
	char *charset;

	if (TYPETEXT != bodyPtr->type) {
	    Tcl_AppendElement(interp, "good");
	    Tcl_AppendElement(interp, "us-ascii");
	    return TCL_OK;
	}
	for (parmPtr = bodyPtr->parameter; parmPtr; parmPtr = parmPtr->next) {
	    if (!strcasecmp(parmPtr->attribute, "charset")) {
		c_charset = parmPtr->value;
		break;
	    }
	}

	charset = cpystr(c_charset);
	lcase((unsigned char*)charset);

	/*
	 * - See if this charset is an alias and resolve that if so.
	 * - Check if this is a charset we do have a font for
	 *   return good and the charset in that case.
	 * - Check if this is a character set we know about and convert.
	 *   return lose if that is the case.
	 * - return none.
	 */
	if (NULL != ( alias = Tcl_GetVar2(interp, "charsetAlias", charset,
					  TCL_GLOBAL_ONLY))) {
	    ckfree(charset);
	    charset = cpystr(alias);
	}
	if (Tcl_GetVar2(interp, "fontEncoding", charset, TCL_GLOBAL_ONLY)) {
	    ov[0] = Tcl_NewStringObj("good", 4);
	    ov[1] = Tcl_NewStringObj("charset", -1);
	    Tcl_SetObjResult(interp, Tcl_NewListObj(2, ov));
	    ckfree(charset);
	    return TCL_OK;
	}
	/*
	 * This converting part is not implemented yet
	 */
	ov[0] = Tcl_NewStringObj("none", 4);
	ov[1] = Tcl_NewStringObj("", 0);
	Tcl_SetObjResult(interp, Tcl_NewListObj(2, ov));
	ckfree(charset);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "findShowCommand")) {
	return RatMcapFindCmd(interp, bodyInfoPtr);

    } else if (!strcmp(Tcl_GetString(objv[1]), "filename")) {
	PARAMETER *parmPtr;
	char *filename = NULL, *delim, *c;
        int gen = 0;

	if (objc == 3 && !strcmp("gen_if_empty", Tcl_GetString(objv[2]))) {
            gen = 1;
        }

        for (parmPtr = bodyPtr->disposition.parameter; parmPtr;
		parmPtr = parmPtr->next) {
	    if (!strcasecmp(parmPtr->attribute, "filename")
		    || !strcasecmp(parmPtr->attribute, "name")) {
		filename = parmPtr->value;
		break;
	    }
	}
	for (parmPtr = bodyPtr->parameter; !filename && parmPtr;
		parmPtr = parmPtr->next) {
	    if (!strcasecmp(parmPtr->attribute, "filename")
		    || !strcasecmp(parmPtr->attribute, "name")) {
		filename = parmPtr->value;
		break;
	    }
	}
	if (!filename && bodyPtr->description
		&& !strchr(bodyPtr->description, ' ')) {
	    filename = bodyPtr->description;
	}
        if (!filename && gen) {
            filename = RatGenId();
        }
        for (c=filename; c && *c; c++) {
            if (!isalnum((int)*c)
                && NULL == strchr("_.,-=+", *c)) {
                *c = '_';
            }
        }
	if (filename) {
	    delim = strrchr(filename, '/');
	    if (delim) {
		Tcl_SetResult(interp, delim+1, TCL_VOLATILE);
	    } else {
		Tcl_SetResult(interp, filename, TCL_VOLATILE);
	    }
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "encoded")) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(bodyInfoPtr->encoded));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "sigstatus")) {
	char *status = NULL;

	switch (bodyInfoPtr->sigStatus) {
	case RAT_UNSIGNED:	status = "pgp_none"; break;
	case RAT_UNCHECKED:	status = "pgp_unchecked"; break;
	case RAT_SIG_GOOD:	status = "pgp_good"; break;
	case RAT_SIG_BAD:	status = "pgp_bad"; break;
	}
	Tcl_SetResult(interp, status, TCL_STATIC);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "checksig")) {
	RatPGPChecksig(interp, messageProcInfo, bodyInfoPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "getPGPOutput")) {
	if (bodyInfoPtr->pgpOutput) {
	    Tcl_SetResult(interp, Tcl_DStringValue(bodyInfoPtr->pgpOutput),
		    TCL_VOLATILE);
	} else {
	    Tcl_ResetResult(interp);
	}
	return TCL_OK;

    }

 usage:
    Tcl_AppendResult(interp, "Illegal argument string", NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCreateChildren --
 *
 *      Creates the children of a multipart body
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static void
RatCreateChildren(Tcl_Interp *interp, BodyInfo *bodyInfoPtr)
{
    BodyInfo **partInfoPtrPtr;

    (*messageProcInfo[bodyInfoPtr->type].makeChildrenProc)
        (interp, bodyInfoPtr);
    for (partInfoPtrPtr = &bodyInfoPtr->firstbornPtr; *partInfoPtrPtr;
         partInfoPtrPtr = &(*partInfoPtrPtr)->nextPtr) {
        RatPGPBodyCheck(interp, messageProcInfo, partInfoPtrPtr);
        Tcl_CreateObjCommand(interp, (*partInfoPtrPtr)->cmdName,
                             RatBodyCmd, (ClientData)(*partInfoPtrPtr),
                             NULL);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatBodyDelete --
 *
 *      Deletes the given body.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The bodypart and all its siblings are deleted from the interpreter
 *	and all the structures are freed.
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatBodyDelete(Tcl_Interp *interp, BodyInfo *bodyInfoPtr)
{
    BodyInfo *siblingInfoPtr, *nextSiblingInfoPtr;
    Tcl_DeleteCommand(interp, bodyInfoPtr->cmdName);
    siblingInfoPtr = bodyInfoPtr->firstbornPtr;
    (*messageProcInfo[bodyInfoPtr->type].bodyDeleteProc)(bodyInfoPtr);
    while (siblingInfoPtr) {
	nextSiblingInfoPtr = siblingInfoPtr->nextPtr;
	RatBodyDelete(interp, siblingInfoPtr);
	siblingInfoPtr = nextSiblingInfoPtr;
    }
    if (bodyInfoPtr->containedEntity) {
	RatMessageDelete(interp, bodyInfoPtr->containedEntity);
    }
    if (bodyInfoPtr->pgpOutput) {
	Tcl_DStringFree(bodyInfoPtr->pgpOutput);
	ckfree(bodyInfoPtr->pgpOutput);
    }
    ckfree(bodyInfoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatMessageGet --
 *
 *      Retrieves a message in textual form. The text is placed in the
 *	supplied Tcl_DString.
 *
 * Results:
 *	No result.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatMessageGet(Tcl_Interp *interp, MessageInfo *msgPtr, Tcl_DString *ds,
	      char *flags, size_t flaglen, char *date, size_t datelen)
{
    char *data, *status, *status_end;
    int seen;
    Tcl_Obj *oPtr;

    data = (*messageProcInfo[msgPtr->type].getHeadersProc)(interp, msgPtr);
    status = strstr(data, "\r\nStatus: ");
    if (status) {
        status += 2;
        Tcl_DStringAppend(ds, data, status-data);
        status_end = strstr(status, "\r\n");
        if (status_end && strlen(status_end+2)) {
            Tcl_DStringAppend(ds, status_end+2, -1);
        }
    } else {
        Tcl_DStringAppend(ds, data, -1);
    }
    if (msgPtr->folderInfoPtr) {
	seen  = (*msgPtr->folderInfoPtr->getFlagProc)(msgPtr->folderInfoPtr,
		interp, msgPtr->msgNo, RAT_SEEN);
    } else {
	seen = 1;
    }
    Tcl_DStringAppend(ds, "\r\n", 2);
    data = (*messageProcInfo[msgPtr->type].fetchTextProc)(interp, msgPtr);
    Tcl_DStringAppend(ds, data, strlen(data));
    if (!seen) {
        (*msgPtr->folderInfoPtr->setFlagProc)(msgPtr->folderInfoPtr,
	    interp, &msgPtr->msgNo, 1, RAT_SEEN, 0);
    }
    if (flags) {
	oPtr = (*messageProcInfo[msgPtr->type].getInfoProc)(interp,
		(ClientData)msgPtr, RAT_FOLDER_FLAGS, 0);
	strlcpy(flags, Tcl_GetString(oPtr), flaglen);
	oPtr = (*messageProcInfo[msgPtr->type].getInfoProc)(interp,
		(ClientData)msgPtr, RAT_FOLDER_DATE_IMAP4, 0);
	strlcpy(date, Tcl_GetString(oPtr), datelen);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatInsertCmd --
 *
 *      Inserts the given message into the database
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatInsertCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	     Tcl_Obj *const objv[])
{
    Tcl_CmdInfo cmdInfo;
    MessageInfo *msgPtr;

    if (objc != 5) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]),
			 " msgId keywords exDate exType\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (0 == Tcl_GetCommandInfo(interp, Tcl_GetString(objv[1]), &cmdInfo)) {
	Tcl_AppendResult(interp, "No such message: ", Tcl_GetString(objv[1]),
			 NULL);
	return TCL_ERROR;
    }
    msgPtr = (MessageInfo*)cmdInfo.objClientData;
    return RatInsertMsg(interp, msgPtr, Tcl_GetString(objv[2]),
			Tcl_GetString(objv[3]), Tcl_GetString(objv[4]));
}


/*
 *----------------------------------------------------------------------
 *
 * RatInsertMsg --
 *
 *      Inserts the given message into the database
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatInsertMsg (Tcl_Interp *interp, MessageInfo *msgPtr, char *keywords,
	char *exDate, char *exType)
{
    char *to, *from, *cc, *subject, *msgid, *ref, *flags, *key, *value;
    MESSAGECACHE elt;
    int listObjc, elemObjc;
    char *eFrom, *header, *body, *s, *e, *d;
    Tcl_DString dString;
    int result, i;
    struct tm tm;
    time_t date = 0, exTime;
    Tcl_Obj *oPtr, **listObjv, **elemObjv;

    to = from = cc = subject = msgid = ref = flags = NULL;
    if (TCL_OK != RatMessageGetHeader(interp,
	    (*messageProcInfo[msgPtr->type].getHeadersProc)(interp, msgPtr))) {
	return TCL_ERROR;
    }
    oPtr = Tcl_GetObjResult(interp);
    Tcl_ListObjGetElements(interp, oPtr, &listObjc, &listObjv);
    for (i=0; i<listObjc; i++) {
	Tcl_ListObjGetElements(interp, listObjv[i], &elemObjc, &elemObjv);
	key = Tcl_GetString(elemObjv[0]);
	value = Tcl_GetString(elemObjv[1]);
	if (!strcasecmp(key, "to")) {
	    to = cpystr(value);
	} else if (!strcasecmp(key, "from")) {
	    from = cpystr(value);
	} else if (!strcasecmp(key, "cc")) {
	    cc = cpystr(value);
	} else if (!strcasecmp(key, "subject")) {
	    subject = cpystr(value);
	} else if (!strcasecmp(key, "message-id")) {
	    msgid = cpystr(value);
	} else if (!strcasecmp(key, "references")
		&& !ref
		&& (s = strchr(value, '<'))
		&& (e = strchr(s, '>'))) {
	    ref = (char*)ckalloc(e-s+1);
	    strlcpy(ref, s, e-s+1);
	} else if (!strcasecmp(key, "in-reply-to")
		&& (s = strchr(value, '<'))
		&& (e = strchr(s, '>'))) {
	    ckfree(ref);
	    ref = (char*)ckalloc(e-s+1);
	    strlcpy(ref, s, e-s+1);
	    ref = cpystr(value);
	} else if (!strcasecmp(key, "status") ||
		   !strcasecmp(key, "x-status")) {
	    if (flags) {
		flags = (char*)ckrealloc(flags,
			strlen(flags)+strlen(value)+1);
		strcpy(&flags[strlen(flags)], value);
	    } else {
		flags = cpystr(value);
	    }
	} else if (!strcasecmp(key, "date")) {
	    if (T == mail_parse_date(&elt, (unsigned char*)value)) {
		tm.tm_sec = elt.seconds;
		tm.tm_min = elt.minutes;
		tm.tm_hour = elt.hours;
		tm.tm_mday = elt.day;
		tm.tm_mon = elt.month - 1;
		tm.tm_year = elt.year+70;
		tm.tm_wday = 0;
		tm.tm_yday = 0;
		tm.tm_isdst = -1;
		date = (int)mktime(&tm);
	    } else {
		date = 0;
	    }
	}
    }
    if (flags) {
	for (s = d = flags; *s; s++) {
	    if ('D' != *s && 'F' != *s) {
		*d++ = *s;
	    }
	}
	*d = '\0';
    } else {
	oPtr = (*messageProcInfo[msgPtr->type].getInfoProc)(interp,
		(ClientData)msgPtr, RAT_FOLDER_UNIXFLAGS, 0);
	flags = cpystr(Tcl_GetString(oPtr));
    }
    if (0 == date) {
	long myLong = 0;
	oPtr = (*messageProcInfo[msgPtr->type].getInfoProc)(interp,
		(ClientData)msgPtr, RAT_FOLDER_DATE_N, 0);
	Tcl_GetLongFromObj(interp, oPtr, &myLong);
	date = (time_t) myLong;
    }
    Tcl_DStringInit(&dString);
    eFrom = (*messageProcInfo[msgPtr->type].getEnvelopeProc)(interp, msgPtr);
    header = (*messageProcInfo[msgPtr->type].getHeadersProc)(interp, msgPtr);
    Tcl_DStringAppend(&dString, header, strlen(header));
    Tcl_DStringAppend(&dString, "\r\n", 2);
    body = (*messageProcInfo[msgPtr->type].fetchTextProc)(interp, msgPtr);
    Tcl_DStringAppend(&dString, body, strlen(body));
    Tcl_ResetResult(interp);
    exTime = atol(exDate);
    if (!strcmp("none", exType)) {
	exTime = 0;
    }
    result = RatDbInsert(interp, to, from, cc, msgid, ref, subject, date,
	    flags, keywords, exTime, exType, eFrom, Tcl_DStringValue(&dString),
	    Tcl_DStringLength(&dString));
    Tcl_DStringFree(&dString);
    ckfree(to);
    ckfree(from);
    ckfree(cc);
    ckfree(msgid);
    ckfree(ref);
    ckfree(subject);
    ckfree(flags);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RatMsgInfo --
 *
 *      get information about a message
 *
 * Results:
 *	A tcl object
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatMsgInfo(Tcl_Interp *interp, MessageInfo *msgPtr, RatFolderInfoType type)
{
    return (*messageProcInfo[msgPtr->type].getInfoProc)(interp,
	    (ClientData)msgPtr, type, 0);
}


/*
 *----------------------------------------------------------------------
 *
 * RatBodySave --
 *
 *      Save a bodypart to an open channel.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatBodySave(Tcl_Interp *interp,Tcl_Channel channel, BodyInfo *bodyInfoPtr,
	    int encoded, int convertNL)
{
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    char *body;
    int result = 0, i;
    unsigned long length;
    Tcl_DString *dsPtr = NULL;

    if (NULL == (body = (*messageProcInfo[bodyInfoPtr->type].fetchBodyProc)
	    (bodyInfoPtr, &length))) {
	Tcl_SetResult(interp, "[Body not available]\n", TCL_STATIC);
	return TCL_OK;
    }
    if (!encoded) {
	dsPtr = RatDecode(interp, bodyPtr->encoding, body, length, NULL);
	body =Tcl_DStringValue(dsPtr);
	length = Tcl_DStringLength(dsPtr);
    }
    if (convertNL) {
	/* 
	 * This isn't really elegant but since the channel is buffered
	 * we shouldn't suffer too badly.
	 */
	for (i=0; i<length && -1 != result; i++) {
	    if ('\r' == body[i] && '\n' == body[i+1]) {
		i++;
	    }
	    result = Tcl_Write(channel, &body[i], 1);
	}
    } else {
	result = Tcl_Write(channel, body, length);
    }
    if (!encoded) {
	Tcl_DStringFree(dsPtr);
	ckfree(dsPtr);
    }
    if (-1 == result) {
	Tcl_AppendResult(interp, "error writing : ",
		Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFindFirstText --
 *
 *      Finds the first text part of a message
 *
 * Results:
 *	A pointer to the BodyInfo of the first text part, or NULL if none
 *	are found.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static BodyInfo*
RatFindFirstText(BodyInfo *bodyInfoPtr)
{
    BodyInfo *b2Ptr;

    for (; NULL != bodyInfoPtr; bodyInfoPtr = bodyInfoPtr->nextPtr) {
	if (TYPETEXT == bodyInfoPtr->bodyPtr->type) {
	    return bodyInfoPtr;
	}
	if (bodyInfoPtr->firstbornPtr && (NULL !=
		(b2Ptr = RatFindFirstText(bodyInfoPtr->firstbornPtr)))) {
	    return b2Ptr;
	}
    }
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * RatGetCitation --
 *
 *      Get the citation to use
 *
 * Results:
 *	A pointer to a citation string to use. This pointer will
 *	remain valid until the next call to this function.
 *	are found.
 *
 * Side effects:
 *	May call userproc.
 *
 *
 *----------------------------------------------------------------------
 */

static CONST84 char*
RatGetCitation(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    Tcl_CmdInfo cmdInfo;
    static char citation[80];

    if (0 != Tcl_GetCommandInfo(interp, "RatUP_Citation", &cmdInfo)) {
	if (TCL_OK != Tcl_VarEval(interp,"RatUP_Citation ",msgPtr->name,NULL)){
	    RatLog(interp, RAT_ERROR, Tcl_GetStringResult(interp),
		    RATLOG_EXPLICIT);
	    return "";
	}
	if (79 < strlen(Tcl_GetStringResult(interp))) {
	    RatLog(interp, RAT_ERROR, "Too long citation", RATLOG_EXPLICIT);
	    return "";
	}
	strlcpy(citation, Tcl_GetStringResult(interp), sizeof(citation));
	return citation;
    }
    return Tcl_GetVar2(interp, "option", "reply_lead", TCL_GLOBAL_ONLY);
}

/*
 *----------------------------------------------------------------------
 *
 * RatCiteMessage --
 *
 *      Copy a message and add citation.
 *
 * Results:
 *	The cited text is appended to dstObjPtr
 *
 * Side effects:
 *	Modifies *dstObjPtr
 *
 *----------------------------------------------------------------------
 */

static void
RatCiteMessage(Tcl_Interp *interp, Tcl_Obj *dstObjPtr, CONST84 char *src,
	       CONST84 char *myCitation)
{
    int i, skipSig, addCitBlank, myCitLength;
    Tcl_Obj *oPtr;
    CONST84 char *srcPtr;

    /*
     * Initialize and find out desired behaviour
     */
    myCitLength = strlen(myCitation);
    if (' ' == myCitation[myCitLength-1]) {
	addCitBlank = 1;
	myCitLength--;
    } else {
	addCitBlank = 0;
    }
    oPtr = Tcl_GetVar2Ex(interp, "option", "skip_sig", TCL_GLOBAL_ONLY);
    Tcl_GetBooleanFromObj(interp, oPtr, &skipSig);

    /*
     * Go over the text and add citation
     */
    for (srcPtr = src; *srcPtr;) {
	/*
	 * Stop when encoutering signature (if that option is true)
	 */
	if (skipSig && '-'== srcPtr[0] && '-' == srcPtr[1] && ' ' == srcPtr[2]
		&& '\n' == srcPtr[3]) {
	    break;
	}

	Tcl_AppendToObj(dstObjPtr, myCitation, myCitLength);
	if ('>' != *srcPtr && addCitBlank) {
	    Tcl_AppendToObj(dstObjPtr, " ", 1);
	}
	for (i=0; '\n' != srcPtr[i] && srcPtr[i]; i++);
	Tcl_AppendToObj(dstObjPtr, srcPtr, i);
	srcPtr += i;
	if ('\n' == *srcPtr) {
	    Tcl_AppendToObj(dstObjPtr, "\n", 1);
	    srcPtr++;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatCitELength --
 *
 *      Calculate the effective length of a citation. Assuming
 *      tab-stops are placed 8 chars apart.
 *
 * Results:
 *	The effective length
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static unsigned int
RatCitELength(const char *cit, unsigned int citLength)
{
    unsigned int l;
    const char *cPtr;

    for (l=0, cPtr=cit; cPtr < cit+citLength; cPtr = Tcl_UtfNext(cPtr)) {
        if ('\t' == *cPtr) {
            l = (l/8+1)*8;
        } else {
            l++;
        }
    }
    return l;
}

/*
 *----------------------------------------------------------------------
 *
 * RatWrapMessage --
 *
 *      Wraps the text of a message
 *
 * Results:
 *	An object containing the wrapped text is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatWrapMessage(Tcl_Interp *interp, Tcl_Obj *textPtr)
{
    int wrapLength, l, citLength, citLength2, overflow, i, add, mark, broken;
    int delta;
    CONST84 char *s, *e, *cPtr, *lineStartPtr, *startPtr, *citPtr = NULL;
    Tcl_RegExp citexp, bullexp;
    Tcl_Obj *nPtr = Tcl_NewObj(), *oPtr;
    unsigned char citbuf[80];

    Tcl_IncrRefCount(nPtr);
    oPtr = Tcl_GetVar2Ex(interp, "option", "wrap_length", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &wrapLength);
    s = Tcl_GetVar2(interp, "option", "citexp", TCL_GLOBAL_ONLY);
    citexp = Tcl_RegExpCompile(interp, s);
    if (NULL == citexp) {
	RatLogF(interp, RAT_ERROR, "illegal_regexp", RATLOG_EXPLICIT,
		Tcl_GetStringResult(interp));
    }
    s = Tcl_GetVar2(interp, "option", "bullexp", TCL_GLOBAL_ONLY);
    bullexp = Tcl_RegExpCompile(interp, s);
    if (NULL == bullexp) {
	RatLogF(interp, RAT_ERROR, "illegal_regexp", RATLOG_EXPLICIT,
		Tcl_GetStringResult(interp));
    }

    for (cPtr = Tcl_GetString(textPtr); *cPtr;) {
	/*
	 * Check if this line needs to be wrapped
	 */
	startPtr = cPtr;
	for (l=0; l < wrapLength && '\n' != *cPtr && *cPtr;
             cPtr = Tcl_UtfNext(cPtr)) {
            if ('\t' == *cPtr) {
                l = (l/8+1)*8;
            } else {
                l++;
            }
        }
	if (l < wrapLength) {
	    Tcl_AppendToObj(nPtr, startPtr, cPtr-startPtr);
	    if ('\n' == *cPtr) {
		Tcl_AppendToObj(nPtr, "\n", 1);
		cPtr++;
	    }
	    continue;
	}

	/*
	 * If it contains no letters after the wrap-point we keep it unwrapped
	 */
	for (s=cPtr; *s && '\n' != *s && !isalpha(*s); s = Tcl_UtfNext(s));
	if (!*s || '\n' == *s) {
	    Tcl_AppendToObj(nPtr, startPtr, s-startPtr);
	    cPtr = s;
	    if ('\n' == *cPtr) {
		Tcl_AppendToObj(nPtr, "\n", 1);
		cPtr++;
	    }
	    continue;
	}

	/*
	 * It should be wrapped, find citation
	 */
	if (citexp
		&& Tcl_RegExpExec(interp, citexp, startPtr, startPtr)
		&& (Tcl_RegExpRange(citexp, 0, &s, &e), s == startPtr)) {
	    citLength = e-s;
	    citPtr = startPtr;
	} else {
	    citLength = 0;
	}

        /*
         * Does it contain a bullet after the citation?
         * If so create a modified citation for the following lines.
         */
        if (citPtr
            && Tcl_RegExpExec(interp, bullexp, citPtr+citLength,
                              citPtr+citLength)
            && (Tcl_RegExpRange(bullexp, 0, &s, &e), 1)
            && e-citPtr < sizeof(citbuf)) {
            strncpy((char*)citbuf, citPtr, e-citPtr);
            for (i=citLength; i<e-s+citLength; i++) {
                if (!isspace(citbuf[i])) {
                    citbuf[i] = ' ';
                }
            }
            citPtr = (char*)citbuf;
            citLength += e-s;
        }

	/*
	 * Find point to break
	 *  First walk backwards until first LWSP.
	 *  Then check that we actually have some text left.
	 *  If not then do not bother wrapping this line
	 */
	for (; !isspace(*cPtr) && cPtr > startPtr+citLength; cPtr--);
	for (s = startPtr+citLength; s < cPtr && isspace(*s); s++);
	if (s == cPtr) {
	    for (; !isspace(*cPtr) && *cPtr; cPtr++);
	    Tcl_AppendToObj(nPtr, startPtr, cPtr-startPtr);
	    continue;
	}

	/*
	 * Add first part of line, linebreak and citation
	 */
	Tcl_AppendToObj(nPtr, startPtr, cPtr-startPtr);
	Tcl_AppendToObj(nPtr, "\n", 1);
	mark = nPtr->length;
	Tcl_AppendToObj(nPtr, citPtr, citLength);

	/*
	 * Continue adding the following lines.
	 * Keep doing that until we find either:
	 *  An empty line (citation does not count)
	 *  A line whose citation differs in any non LWSP-character
	 *  A line whose indention is longer than the curent one, and
	 *    where the difference does not match a bullet expression
	 */
	lineStartPtr = startPtr = ++cPtr;
	l = RatCitELength(citPtr, citLength);
	broken = 1;
	while (*cPtr) {
	    /* Found end of line? */
	    if ('\n' == *cPtr) {
		/* Skip trailing LWSP */
		for (e = cPtr; isspace(*e) && e > startPtr; e--);
		if (e >= startPtr) e++;
		Tcl_AppendToObj(nPtr, startPtr, e-startPtr);
		cPtr++;
		/* Find length of citation */
		if (citexp
			&& Tcl_RegExpExec(interp, citexp, cPtr, cPtr)
			&& (Tcl_RegExpRange(citexp, 0, &s, &e), s == cPtr)) {
		    citLength2 = e-s;
		} else {
		    citLength2 = 0;
		}
		add = 0;
		/* Check for empty line */
		for (s=cPtr+citLength2; isspace(*s) && '\n' != *s && *s;s++);
		if (*s != '\n' &&
		    (isalnum(*s) || '\'' == *s || '"' == *s || '(' == *s)) {
		    /* Is citation identical? */
                    delta = RatCitELength(citPtr, citLength)
                        - RatCitELength(cPtr, citLength2);
                    if (0 == delta) {
			add = 1;
		    } else if (delta > 0) {
                        /*
                         * We have found a line with shorter citation.
                         * Change data already inserted into dest as well
                         * as remembered citation
                         */
                        /* Data already there */
                        oPtr = Tcl_NewStringObj(
                            nPtr->bytes+mark+citLength,
                            nPtr->length-mark-citLength);
                        Tcl_IncrRefCount(oPtr);
                        Tcl_SetObjLength(nPtr, mark+citLength2);
                        Tcl_AppendObjToObj(nPtr, oPtr);
                        Tcl_DecrRefCount(oPtr);
                        l -= citLength-citLength2;
                        citLength = citLength2;
                        add = 1;
		    } else if (delta < 0
			       && Tcl_RegExpExec(interp, bullexp,
						 citPtr+citLength,
						 citPtr+citLength)
			       && (Tcl_RegExpRange(bullexp, 0, &s, &e), 1)
			       && citLength + e - s == citLength2) {
			/* Citation is longer and bullet exp matches */
			/* Data already there */
			oPtr = Tcl_NewStringObj(
			    nPtr->bytes+mark+citLength,
			    nPtr->length-mark-citLength);
			Tcl_IncrRefCount(oPtr);
			Tcl_SetObjLength(nPtr, mark);
			Tcl_AppendToObj(nPtr, cPtr, citLength2);
			Tcl_AppendObjToObj(nPtr, oPtr);
			Tcl_DecrRefCount(oPtr);
			l += citLength2-citLength;
			add = 1;
			citPtr = cPtr;
			citLength = citLength2;
		    }
		}
		if (add && broken) {
		    Tcl_AppendToObj(nPtr, " ", 1);
		    l++;
		    cPtr += citLength2;
                    startPtr = cPtr;
		    broken = 0;
		    continue;
		} else {
		    Tcl_AppendToObj(nPtr, "\n", 1);
		    l = 0;
                    startPtr = cPtr;
		    break;
		}
	    } else if (l >= wrapLength) {
		broken = 1;
		for (; !isspace(*cPtr) && cPtr > startPtr; cPtr--);
		l = overflow = 0;
		if (cPtr == startPtr && startPtr == lineStartPtr) {
		    while (!isspace(*cPtr)) cPtr++;
		    overflow = 1;
		}
		Tcl_AppendToObj(nPtr, startPtr, cPtr-startPtr);
		Tcl_AppendToObj(nPtr, "\n", 1);
		if (startPtr != cPtr) {
		    cPtr++;
		}
		lineStartPtr = startPtr = cPtr;
		if (overflow) break;
		mark = nPtr->length;
		Tcl_AppendToObj(nPtr, citPtr, citLength);
		l += RatCitELength(citPtr, citLength);
	    } else {
		l++;
		cPtr = Tcl_UtfNext(cPtr);
	    }
	}
        if (startPtr < cPtr) {
            Tcl_AppendToObj(nPtr, startPtr, cPtr-startPtr);
            Tcl_AppendToObj(nPtr, "\n", 1);
        }
    }

    return nPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatBodyType --
 *
 *      Gets the types of a bodypart
 *
 * Results:
 *	A list object containing two strings, the first is the major
 *	type and the second is the subtype.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj*
RatBodyType(BodyInfo *bodyInfoPtr)
{
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    Tcl_Obj *oPtr[2];

    oPtr[0] = Tcl_NewStringObj(body_types[bodyPtr->type], -1);
    if (bodyPtr->subtype) {
	oPtr[1] = Tcl_NewStringObj(bodyPtr->subtype, -1);
    } else {
	oPtr[1] = Tcl_NewStringObj("", 0);
    }
    return Tcl_NewListObj(2, oPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatBodyData --
 *
 *      Gets the content of a bodypart
 *
 * Results:
 *	An object containing the data.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj*
RatBodyData(Tcl_Interp *interp, BodyInfo *bodyInfoPtr, int encoded,
	char *charset)
{
    BODY *bodyPtr = bodyInfoPtr->bodyPtr;
    Tcl_Obj *oPtr;
    char *body;
    CONST84 char *isCharset = NULL, *alias;
    PARAMETER *parameter;
    unsigned long length;

    if (charset) {
	isCharset = charset;
    } else if (TYPETEXT == bodyPtr->type){
	isCharset = "us-ascii";
	for (parameter = bodyPtr->parameter; parameter;
		parameter = parameter->next) {
	    if ( 0 == strcasecmp("charset", parameter->attribute)) {
		isCharset = parameter->value;
	    }
	}
	if ((alias = Tcl_GetVar2(interp, "charsetAlias", isCharset,
		TCL_GLOBAL_ONLY))) {
	    isCharset = alias;
	}
    }

    body = (*messageProcInfo[bodyInfoPtr->type].fetchBodyProc)
	    (bodyInfoPtr, &length);
    if (body) {
	if (encoded) {
	    Tcl_Encoding enc;
	    Tcl_DString ds;

	    Tcl_DStringInit(&ds);
	    if (ENC8BIT == bodyPtr->encoding) {
		enc = RatGetEncoding(interp, isCharset);
		Tcl_ExternalToUtfDString(enc, body, length, &ds);
	    } else {
		 Tcl_DStringAppend(&ds, body, length);
	    }
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds),
				    Tcl_DStringLength(&ds));
	    Tcl_DStringFree(&ds);
	} else {
	    Tcl_DString *dsPtr = RatDecode(interp, bodyPtr->encoding,
		    body, length, isCharset);
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(dsPtr),
				    Tcl_DStringLength(dsPtr));
	    Tcl_DStringFree(dsPtr);
	    ckfree(dsPtr);
	}
    } else {
	oPtr = Tcl_NewStringObj("[Body not available]\n", -1);
    }
    return oPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatMessageInternalDate --
 *
 *      Gets the internal date of a message
 *
 * Results:
 *	A pointer to a MESSAGECACHE entry where only the date-fields may
 *	be used. May return NULL on errors.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
MESSAGECACHE*
RatMessageInternalDate(Tcl_Interp *interp, MessageInfo *msgPtr)
{
    return (*messageProcInfo[msgPtr->type].getInternalDateProc)(interp,msgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatPurgeFlags --
 *
 *      Purge Flagged, Deleted and Recent flags
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May modify the buffer passed as argument. However the result is
 *	never bigger than the argument.
 *
 *----------------------------------------------------------------------
 */

char*
RatPurgeFlags(char *flags, int level)
{
    char *cPtr, *toPurge[4];
    int i, l;

    i = 0;
    if (1 == level) {
	toPurge[i++] = flag_name[RAT_FLAGGED].imap_name;
	toPurge[i++] = flag_name[RAT_DELETED].imap_name;
	toPurge[i++] = flag_name[RAT_RECENT].imap_name;
    } else {
	toPurge[i++] = flag_name[RAT_RECENT].imap_name;
    }
    toPurge[i] = NULL;

    for (i=0; '\0' != toPurge[i]; i++) {
	if (NULL != (cPtr = strstr(flags, toPurge[i]))) {
	    l = strlen(toPurge[i]);
	    if (flags == cPtr) {
		if (' ' == cPtr[l]) {
		    l++;
		}
	    } else {
		cPtr--;
		l++;
	    }
	    strcpy(cPtr, cPtr+l);
	}
    }
    return flags;
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
    len += RatHeaderLineSize("Date", env, (char*)env->date); 
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


/*
 *----------------------------------------------------------------------
 *
 * RatMessageDeleteAttachments --
 *
 *	See ../doc/interface
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */
static int
RatMessageDeleteAttachments(Tcl_Interp *interp, MessageInfo *msgPtr,
                            Tcl_Obj *attachments)
{
    char flags[128], date[128], *name;
    Tcl_DString ds;
    int i, length;
    Tcl_Obj *oPtr;

    /* Get message text */
    Tcl_DStringInit(&ds);
    RatMessageGet(interp, msgPtr, &ds, flags, sizeof(flags),
                  date, sizeof(date));
    if ('\n' != Tcl_DStringValue(&ds)[Tcl_DStringLength(&ds)-1]) {
        Tcl_DStringAppend(&ds, "\r\n", 2);
    }

    /* Delete attachments */
    Tcl_ListObjLength(interp, attachments, &length);
    for (i=0; i<length; i++) {
        Tcl_ListObjIndex(interp, attachments, i, &oPtr);
        if (TCL_OK != RatDeleteAttachment(interp, msgPtr, &ds, oPtr)) {
            Tcl_DStringFree(&ds);
            return TCL_ERROR;
        }
    }

    /* Create new message */
    name = RatFrMessageCreate(interp, Tcl_DStringValue(&ds),
                              Tcl_DStringLength(&ds), NULL);

    Tcl_SetResult(interp, name, TCL_VOLATILE);
    return TCL_OK;
}

static int
RatDeleteAttachment(Tcl_Interp *interp, MessageInfo *msgPtr,
                    Tcl_DString *ds, Tcl_Obj *spec)
{
    char *start, *boundary, *end, buf[2048];
    const char *text;

    /* Find start and end of region to replace */
    start = RatFindAttachment(interp, msgPtr->bodyInfoPtr,
                              Tcl_DStringValue(ds), spec, 0, &boundary);
    if (!start) {
        return TCL_ERROR;
    }
    strlcpy(buf, "--", sizeof(buf));
    strlcat(buf, boundary, sizeof(buf));
    end = strstr(start+1, buf);
    if (!end) {
        Tcl_SetResult(interp, "Attachment end not found", TCL_STATIC);
        return TCL_ERROR;
    }
    start += strlen(buf)+2;

    /* Create replacement data */
    text = Tcl_GetVar2(interp, "t", "deleted_attachment", TCL_GLOBAL_ONLY);
    snprintf(buf, sizeof(buf), "Content-Type: TEXT/PLAIN; CHARSET=us-ascii\r\n"
             "\r\n%s\r\n", text);

    /* Make sure there is room */
    if (end-start < strlen(buf)) {
        Tcl_DStringSetLength(ds,Tcl_DStringLength(ds)+strlen(buf)-(end-start));
    }

    /* Move data */
    memmove(start+strlen(buf), end, strlen(end)+1);
    memmove(start, buf, strlen(buf));

    /* Set new length */
    if (end-start > strlen(buf)) {
        Tcl_DStringSetLength(ds,Tcl_DStringLength(ds)+strlen(buf)-(end-start));
    }

    return TCL_OK;
}

static char*
RatFindAttachment(Tcl_Interp *interp, BodyInfo *bodyInfoPtr,
                  char *text, Tcl_Obj *spec, int spec_index, char **boundary)
{
    char buf[1024];
    BodyInfo *child;
    PARAMETER *param;
    Tcl_Obj *oPtr;
    int index, i, length;

    /* Find boundary */
    if (bodyInfoPtr->bodyPtr->type != TYPEMULTIPART) {
        Tcl_SetResult(interp, "Not a multipart message", TCL_STATIC);
        return NULL;
    }
    for (param = bodyInfoPtr->bodyPtr->parameter;
         param && strcasecmp(param->attribute, "BOUNDARY");
         param = param->next) {
    }
    if (!param) {
        Tcl_SetResult(interp, "No boundary found", TCL_STATIC);
        return NULL;
    }
    *boundary = param->value;
    strlcpy(buf, "--", sizeof(buf));
    strlcat(buf, param->value, sizeof(buf));

    /* Make sure children exist */
    if (!bodyInfoPtr->firstbornPtr) {
        RatCreateChildren(interp, bodyInfoPtr);
    }

    /* Extract index of child we are interested in */
    if (TCL_OK != Tcl_ListObjIndex(interp, spec, spec_index, &oPtr)
        || TCL_OK != Tcl_GetIntFromObj(interp, oPtr, &index)) {
        Tcl_SetResult(interp, "Failed to extract index", TCL_STATIC);
        return NULL;
    }

    /* Find the location of the child */
    text = strstr(text+1, buf);
    for (i=0, child = bodyInfoPtr->firstbornPtr;
         text && i < index && child->nextPtr; i++, child = child->nextPtr) {
        text = strstr(text+1, buf);
    }
    if (i < index || !text) {
        Tcl_SetResult(interp, "Failed to locate child", TCL_STATIC);
        return NULL;
    }

    /* Are we done or do we need to recurse? */
    Tcl_ListObjLength(interp, spec, &length);
    if (spec_index < length-1) {
        return RatFindAttachment(interp, child, text, spec, spec_index+1,
                                 boundary);
    } else {
        return text;
    }
}

#ifdef MEM_DEBUG
void ratMessageCleanup()
{
    ckfree(messageProcInfo);
}
#endif /* MEM_DEBUG */
