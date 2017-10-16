/*
 * ratPGPprog.c --
 *
 *	This file contains compatibility functions.
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
 * Maximaum length of pass phrase (plus two)
 */
#define MAXPASSLENGTH 1024

/*
 * Cached pass phrase
 */
static char pgpPass[MAXPASSLENGTH];
static int pgpPassValid = 0;
static Tcl_TimerToken pgpPassToken;


/*
 *----------------------------------------------------------------------
 *
 * ClearPGPPass --
 *
 *      Clear the pgp pass phrase
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The pass phrase is cleared.
 *
 *
 *----------------------------------------------------------------------
 */

void
ClearPGPPass(ClientData unused)
{
    memset(pgpPass, '\0', sizeof(pgpPass));
    pgpPassValid = 0;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPPhrase --
 *
 *      Get the pass phrase.
 *
 * Results:
 *	A pointer to a buffer containing the pass phrase or NULL if the
 *	user aborted the operation.
 *
 * Side effects:
 *	It is the callers responsibility to free this buffer.
 *
 *----------------------------------------------------------------------
 */

char*
RatPGPPhrase(Tcl_Interp *interp)
{
    char buf[32], *result;
    int doCache, timeout, objc;
    Tcl_Obj *oPtr, **objv;

    oPtr = Tcl_GetVar2Ex(interp, "option","cache_pgp_timeout",TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &timeout);

    if (pgpPassValid) {
	Tcl_DeleteTimerHandler(pgpPassToken);
	if (timeout) {
	    pgpPassToken = Tcl_CreateTimerHandler(timeout*1000, ClearPGPPass,
		    NULL);
	}
	return cpystr(pgpPass);
    }

    strlcpy(buf, "RatGetPGPPassPhrase", sizeof(buf));
    Tcl_Eval(interp, buf);
    oPtr = Tcl_GetObjResult(interp);
    Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);
    if (!strcmp("ok", Tcl_GetString(objv[0]))) {
	oPtr = Tcl_GetVar2Ex(interp, "option", "cache_pgp", TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &doCache);
	if (doCache) {
	    strlcpy(pgpPass, Tcl_GetString(objv[1]), sizeof(pgpPass));
	    pgpPassValid = 1;
	    if (timeout) {
		pgpPassToken = Tcl_CreateTimerHandler(timeout*1000,
			ClearPGPPass, NULL);
	    } else {
		pgpPassToken = NULL;
	    }
	}
	result = cpystr(Tcl_GetString(objv[1]));
	return result;
    } else {
	return NULL;
    }
}



/*
 *----------------------------------------------------------------------
 *
 * RatSenderPGPPhrase --
 *
 *      Get the pass phrase. This function may only be called from the
 *	sender process.
 *
 * Results:
 *	A pointer to a static buffer containing the pass phrase. It is up to
 *	the caller to overwrite this buffer with nulls.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatSenderPGPPhrase(Tcl_Interp *interp)
{
    static CONST84 char **argv = NULL;
    int argc;
    char *result = RatSendPGPCommand("PGP getpass");

    if (!strncmp("PHRASE ", result, 7)) {
	ckfree(argv);
	Tcl_SplitList(interp, result, &argc, &argv);
	memset(result, '\0', strlen(result));
	return (char*)argv[1];
    } else {
	return NULL;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPBodyCheck --
 *
 *      Checks if the given bodypart is either signed or encoded with
 *	pgp. Parts of the bodypart signature are then initialized.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The passed BodyInfo structure is modified.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatPGPBodyCheck(Tcl_Interp *interp, MessageProcInfo *procInfo,
		BodyInfo **bodyInfoPtrPtr)
{
    PARAMETER *parPtr;
    int prot, enc;
    unsigned long length;
    char *text, *start, *end, *middle;
    const char *version;

    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);
    if (!version || !strcmp("0", version)) {
	return;
    }

    /*
     * Check for PGP/MIME messages
     */
    (*bodyInfoPtrPtr)->sigStatus = RAT_UNSIGNED;
    if ((*bodyInfoPtrPtr)->bodyPtr->type == TYPEMULTIPART
	    && !strcasecmp("encrypted", (*bodyInfoPtrPtr)->bodyPtr->subtype)) {
	enc = 0;
	for (parPtr = (*bodyInfoPtrPtr)->bodyPtr->parameter; parPtr;
		parPtr = parPtr->next) {
	    if (!strcasecmp(parPtr->attribute, "protocol")
		    && !strcasecmp(parPtr->value,"application/pgp-encrypted")){
		enc = 1;
		break;
	    }
	}
	if (enc) {
	    RatPGPDecrypt(interp, procInfo, bodyInfoPtrPtr);
	    (*bodyInfoPtrPtr)->encoded = 1;
	}

    } else if ((*bodyInfoPtrPtr)->bodyPtr->type == TYPEMULTIPART
	    && !strcasecmp("signed", (*bodyInfoPtrPtr)->bodyPtr->subtype)) {
	prot = 0;
	for (parPtr = (*bodyInfoPtrPtr)->bodyPtr->parameter; parPtr;
		parPtr = parPtr->next) {
	    if (!strcasecmp(parPtr->attribute, "protocol")
		    && !strcasecmp(parPtr->value,"application/pgp-signature")){
		prot = 1;
	    }
	}
	if (prot) {
	    BodyInfo *bodyInfoPtr;

	    (*procInfo[(*bodyInfoPtrPtr)->type].makeChildrenProc)(interp,
		    *bodyInfoPtrPtr);
	    bodyInfoPtr = *bodyInfoPtrPtr;
	    *bodyInfoPtrPtr = (*bodyInfoPtrPtr)->firstbornPtr;
	    (*bodyInfoPtrPtr)->sigStatus = RAT_UNCHECKED;
	    (*bodyInfoPtrPtr)->secPtr = bodyInfoPtr;
	}
    } else if ((*bodyInfoPtrPtr)->bodyPtr->type == TYPETEXT
	    || ((*bodyInfoPtrPtr)->bodyPtr->type == TYPEAPPLICATION
	    && !strcasecmp("pgp", (*bodyInfoPtrPtr)->bodyPtr->subtype))) {
	text = (*procInfo[(*bodyInfoPtrPtr)->type].fetchBodyProc)
		(*bodyInfoPtrPtr, &length);
	if (text && (((start = RatPGPStrFind(text,length,"BEGIN PGP SIGNED",1))
		&& (middle = RatPGPStrFind(start, length - (start-text),
			"BEGIN PGP SIGNATURE",1))
		&& (end = RatPGPStrFind(middle, length - (middle-text),
			"END PGP",1)))
		|| ((start = RatPGPStrFind(text, length,"BEGIN PGP MESSAGE",1))
		&& (end = RatPGPStrFind(start, length - (start-text),
			"END PGP",1))))) {
	    RatPGPHandleOld(interp, *bodyInfoPtrPtr, text, start, end+1);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatPGPCmd --
 *
 *      Handle ratPGP command.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	Depends on the arguments
 *
 *----------------------------------------------------------------------
 */

int
RatPGPCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	  Tcl_Obj *const objv[])
{
    if (objc < 2) goto usage;

    if (!strcmp(Tcl_GetString(objv[1]), "listkeys")) {
	if (objc != 3 && objc != 2) goto usage;
	if (objc == 3) {
	    return RatPGPListKeys(interp, Tcl_GetString(objv[2]));
	} else {
	    return RatPGPListKeys(interp, NULL);
	}

    } else if (!strcmp(Tcl_GetString(objv[1]), "extract")) {
	if (objc != 3 && objc != 4) goto usage;
	if (objc == 4) {
	    return RatPGPExtractKey(interp, Tcl_GetString(objv[2]),
				    Tcl_GetString(objv[3]));
	} else {
	    return RatPGPExtractKey(interp, Tcl_GetString(objv[2]), NULL);
	}

    } else if (!strcmp(Tcl_GetString(objv[1]), "add")) {
	if (objc != 3 && objc != 4) goto usage;
	if (objc == 4) {
	    return RatPGPAddKeys(interp, Tcl_GetString(objv[2]),
				 Tcl_GetString(objv[3]));
	} else {
	    return RatPGPAddKeys(interp, Tcl_GetString(objv[2]), NULL);
	}
    }

 usage:
    Tcl_AppendResult(interp, "Illegal usage of \"", Tcl_GetString(objv[0]),
		     "\"", (char *) NULL);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPStrFind --
 *
 *      Find a PGP string in a message
 *
 * Results:
 *	A pointer to the start of the string.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatPGPStrFind(char *haystack, long straws, char *needle, int linestart)
{
    long i, j, end;
    int needleSize = strlen(needle);

    end = straws-strlen(needle);

    for (i=0; i<=end; i+= 5) {
	if ('-' == haystack[i]) {
	    for (j=i; j>0 && j>i-5 && '-' == haystack[j]; j--);
	    if ((j >= end-5) || (linestart && j>0 && '\n' != haystack[j])) {
		continue;
	    }
	    if (j > 0) {
		j++;
	    }
	    if (!strncmp("-----", haystack+i, 5-(i-j))
		&& !strncmp(needle, haystack+j+5, needleSize)) {
		return haystack+j;
	    }
	}
    }
    return NULL;
}
