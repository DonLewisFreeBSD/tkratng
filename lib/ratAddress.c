/*
 * ratAddress.c --
 *
 *	This file contains basic support for handling addresses.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"

#define PADDED(n)	(n+(4-((int)(n)%4))%4)

/*
 * This struct defines an alias.
 */
typedef struct {
    Tcl_Obj *book;	/* Address book this alias comes from */
    Tcl_Obj *fullname;	/* Long name of alias (phrase part) */
    Tcl_Obj *content;	/* Content that alias expands to */
    Tcl_Obj *comment;	/* Comment to alias */
    unsigned int flags;	/* Option flags */
    unsigned long mark;	/* Id of last use */
} AliasInfo;

#define ALIAS_FLAG_ISLIST	(1<<0)
#define ALIAS_FLAG_NOFULLNAME	(1<<1)

/*
 * This table contains all the aliases.
 */
Tcl_HashTable aliasTable;

/*
 * This struct us ised when aliases are expanded
 */
typedef struct {
    char *host;
    int lookup_in_passwd;
    int level;
} AliasExpand;

/*
 * The number of address entities created. This is used to create new
 * unique command names.
 */
static int numAddresses = 0;

/*
 * A mark used to prevent loops when resolving aliases
 */
static unsigned long aliasMark = 0;

/*
 * States used while parsing addresses
 */
typedef enum {
    STATE_NORMAL,
    STATE_ESCAPED,
    STATE_COMMENT,
    STATE_QUOTED,
    STATE_LITERAL
} ParseState;

/*
 * Internal functions
 */
static int AddressClean(Tcl_Obj *aPtr);

#ifdef MEM_DEBUG
static char *mem_store;
#endif /* MEM_DEBUG */

static int RatAddressIsMeRole(Tcl_Interp *interp, ADDRESS *adrPtr, char *role);
static void RatExpandAlias(Tcl_Interp *interp, Tcl_DString *list,
			   AliasExpand *ea);


/*
 *----------------------------------------------------------------------
 *
 * AddressClean --
 *
 *      Clean an address list by removing all whitespace around addresses
 *
 * Results:
 *	The number of addresses contained in the object
 *
 * Side effects:
 *	Modifies the given object
 *
 *
 *----------------------------------------------------------------------
 */

static int
AddressClean(Tcl_Obj *aPtr)
{
    char *mark, *dst, *src, *new;
    int quoted = 0;
    int skip = 1, length, num = 1;

    src = Tcl_GetStringFromObj(aPtr, &length);
    new = dst = mark = (char*)ckalloc(length);
    for (; *src; src++) {
	if ('\\' == *src) {
	    *dst++ = *src++;
	} else if ('"' == *src) {
	    skip = 0;
	    if (quoted) {
		quoted = 0;
	    } else {
		quoted = 1;
	    }
	} else if (!quoted) {
	    if (',' == *src) {
		num++;
		dst = mark;
		skip = 1;
	    } else if (isspace((unsigned char)*src)) {
		if (skip) {
		    continue;
		}
	    } else {
		mark = dst+1;
		skip = 0;
	    }
	}
	*dst++ = *src;
    }
    if (0 == mark-new) {
	num = 0;
    }
    Tcl_SetStringObj(aPtr, new, mark-new);
    ckfree(new);
    return num;
}



/*
 *----------------------------------------------------------------------
 *
 * RatCreateAddressCmd --
 *
 *      This routine creates an address command by an address given
 *	as argument
 *
 * Results:
 *	A list of address entity names is appended to the result
 *
 * Side effects:
 *	New address entities are created,
 *
 *
 *----------------------------------------------------------------------
 */

int
RatCreateAddressCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    ADDRESS *adrPtr = NULL;
    char *s, *ch;

    if (objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " address role\"", (char *) NULL);
	return TCL_ERROR;
    }

    ch = RatGetCurrent(interp, RAT_HOST, Tcl_GetString(objv[2]));
    s = cpystr(Tcl_GetString(objv[1]));
    rfc822_parse_adrlist(&adrPtr, s, ch);
    ckfree(s);
    RatEncodeAddresses(interp, adrPtr);
    RatInitAddresses(interp, adrPtr);
    mail_free_address(&adrPtr);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatInitAddresses --
 *
 *      This routine takes an address list as argument and constructs a
 *	list of address entities of it.
 *
 * Results:
 *	A list of address entity names is appended to the result
 *
 * Side effects:
 *	New address entities are created,
 *
 *
 *----------------------------------------------------------------------
 */

void
RatInitAddresses(Tcl_Interp *interp, ADDRESS *addressPtr)
{
    ADDRESS *adrPtr, *newPtr;
    char name[32];
    Tcl_Obj *rPtr;

    rPtr = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(rPtr)) {
	rPtr = Tcl_DuplicateObj(rPtr);
    }
    for (adrPtr = addressPtr; adrPtr; adrPtr = adrPtr->next) {
	newPtr = mail_newaddr();
	if (adrPtr->personal)	{
	    newPtr->personal =
		    cpystr(RatDecodeHeader(interp, adrPtr->personal, 0));
	}
	if (adrPtr->adl)	newPtr->adl = cpystr(adrPtr->adl);
	if (adrPtr->mailbox)	newPtr->mailbox = cpystr(adrPtr->mailbox);
	if (adrPtr->host)	newPtr->host = cpystr(adrPtr->host);
	if (adrPtr->error)	newPtr->error = cpystr(adrPtr->error);
	sprintf(name, "RatAddress%d", numAddresses++);
	Tcl_CreateObjCommand(interp, name, RatAddress, (ClientData) newPtr,
		RatDeleteAddress);
	Tcl_ListObjAppendElement(interp, rPtr, Tcl_NewStringObj(name, -1));
    }
    Tcl_SetObjResult(interp, rPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatAddress --
 *
 *      This routine handles the address entity commands. See ../doc/interface
 *	for a documentation of them.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	May be some
 *
 *
 *----------------------------------------------------------------------
 */

int
RatAddress(ClientData clientData, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    ADDRESS *adrPtr = (ADDRESS*)clientData;
    Tcl_CmdInfo info;
    Tcl_Obj *oPtr;
    int useup;

    if (objc < 2) goto usage;
    if (!strcmp(Tcl_GetString(objv[1]), "isMe")) {
	if (3 == objc) {
	    Tcl_GetBooleanFromObj(interp, objv[2], &useup);
	} else {
	    useup = 1;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
	    RatAddressIsMe(interp, adrPtr, useup)));
	return TCL_OK;
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "compare")) {
	if (objc != 3) goto usage;
	if (0 == Tcl_GetCommandInfo(interp, Tcl_GetString(objv[2]), &info)) {
	    Tcl_AppendResult(interp, "there is no address entity \"",
		    Tcl_GetString(objv[2]), "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
	    RatAddressCompare(adrPtr, (ADDRESS*)info.objClientData)));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "set")) {
	if (objc != 5) goto usage;
	ckfree(adrPtr->mailbox);
	ckfree(adrPtr->personal);
	ckfree(adrPtr->host);
	adrPtr->personal = cpystr(Tcl_GetString(objv[2]));
	adrPtr->mailbox = cpystr(Tcl_GetString(objv[3]));
	adrPtr->host = cpystr(Tcl_GetString(objv[4]));
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "get")) {
	if (objc != 3) goto usage;
	if (!strcasecmp(Tcl_GetString(objv[2]), "rfc822")) {
	    if (adrPtr->personal) {
		char *personal;

		oPtr = Tcl_NewStringObj(adrPtr->personal, -1);
		personal = RatEncodeHeaderLine(interp, oPtr, 0);
		Tcl_DecrRefCount(oPtr);
		oPtr = Tcl_NewObj();
		Tcl_AppendStringsToObj(oPtr, personal, " <", NULL);
		Tcl_AppendToObj(oPtr, RatAddressMail(adrPtr), -1);
		Tcl_AppendToObj(oPtr, ">", 1);
		Tcl_SetObjResult(interp, oPtr);
	    } else {
		Tcl_SetResult(interp, RatAddressMail(adrPtr), TCL_VOLATILE);
	    }
	    return TCL_OK;

	} else if (!strcmp(Tcl_GetString(objv[2]), "mail")) {
	    Tcl_SetResult(interp, RatAddressMail(adrPtr), TCL_VOLATILE);
	    return TCL_OK;

	} else if (!strcmp(Tcl_GetString(objv[2]), "name")) {
	    if (adrPtr->personal) {
		Tcl_SetResult(interp, adrPtr->personal, TCL_VOLATILE);
	    }
	    return TCL_OK;

	} else {
	    goto usage;
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
 * RatDeleteAddress --
 *
 *      Frees the client data of an address entity.
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

void
RatDeleteAddress(ClientData clientData)
{
    ADDRESS *adrPtr = (ADDRESS*)clientData;
    ckfree(adrPtr->personal);
    ckfree(adrPtr->adl);
    ckfree(adrPtr->mailbox);
    ckfree(adrPtr->host);
    ckfree(adrPtr->error);
    ckfree(adrPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatAddressIsMeInRole --
 *
 *      Checks if the address points to me in the given role
 *
 * Results:
 *	If it is then non zero is returned otherwise zero.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static int
RatAddressIsMeRole(Tcl_Interp *interp, ADDRESS *adrPtr, char *role)
{
    char *from, *host;
    ADDRESS *a = NULL;

    host = cpystr(RatGetCurrent(interp, RAT_HOST, role));
    from = cpystr(RatGetCurrent(interp, RAT_MAILBOX, role));
    rfc822_parse_adrlist(&a, from, host);
    ckfree(from);
    ckfree(host);
    if (a && adrPtr->mailbox && adrPtr->host
	&& !strcasecmp(a->mailbox, adrPtr->mailbox)
	&& !strcasecmp(a->host, adrPtr->host)) {
	mail_free_address(&a);
	return 1;
    }
    mail_free_address(&a);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatAddressIsMe --
 *
 *      Checks if the address points to me.
 *
 * Results:
 *	If it is then non zero is returned otherwise zero.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatAddressIsMe(Tcl_Interp *interp, ADDRESS *adrPtr, int trustUser)
{
    Tcl_Obj **objv, *oPtr;
    Tcl_CmdInfo cmdInfo;
    int objc, i;

    if (adrPtr == NULL) {
	return 0;
    }
    
    if (RatAddressIsMeRole(interp, adrPtr, "")) {
	return 1;
    }

    if (trustUser) {
	oPtr = Tcl_GetVar2Ex(interp, "option", "roles", TCL_GLOBAL_ONLY);
	Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);
	for (i=0; i<objc; i++) {
	    if (RatAddressIsMeRole(interp, adrPtr, Tcl_GetString(objv[i]))) {
		return 1;
	    }
	}
	if (Tcl_GetCommandInfo(interp, "RatUP_IsMe", &cmdInfo)) {
	    Tcl_DString cmd;
	    int isMe;
	    Tcl_Obj *oPtr;

	    Tcl_DStringInit(&cmd);
	    Tcl_DStringAppendElement(&cmd, "RatUP_IsMe");
	    Tcl_DStringAppendElement(&cmd,adrPtr->mailbox?adrPtr->mailbox:"");
	    Tcl_DStringAppendElement(&cmd,adrPtr->host?adrPtr->host:"");
	    Tcl_DStringAppendElement(&cmd,
				     adrPtr->personal ? adrPtr->personal : "");
	    Tcl_DStringAppendElement(&cmd,adrPtr->adl?adrPtr->adl:"");
	    if (TCL_OK == Tcl_Eval(interp, Tcl_DStringValue(&cmd))
	    	    && (oPtr = Tcl_GetObjResult(interp))
		    && TCL_OK == Tcl_GetBooleanFromObj(interp, oPtr, &isMe)) {
		Tcl_DStringFree(&cmd);
		return isMe;
	    }
	    Tcl_DStringFree(&cmd);
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatAddressCompare --
 *
 *      Check if two addresses are equal.
 *
 * Results:
 *	If they are then zero is returned otherwise non zero.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatAddressCompare(ADDRESS *adr1Ptr, ADDRESS *adr2Ptr)
{
    if (((adr1Ptr->mailbox && adr2Ptr->mailbox
		&& !strcasecmp(adr1Ptr->mailbox, adr2Ptr->mailbox))
  	      || adr1Ptr->mailbox == adr2Ptr->mailbox)
   	    && ((adr1Ptr->host && adr2Ptr->host
		&& !strcasecmp(adr1Ptr->host, adr2Ptr->host))
	      || adr1Ptr->host == adr2Ptr->host)) {
	return 0;
    } else {
	return 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatAddressTranslate --
 *
 *      Let the user do their translation of this address.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The address may be affected.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatAddressTranslate(Tcl_Interp *interp, ADDRESS *adrPtr)
{
    Tcl_CmdInfo cmdInfo;
    Tcl_DString cmd;
    Tcl_Obj *oPtr, *lPtr;
    char **destPtrPtr = NULL, *s;
    int argc, i;

    if (!Tcl_GetCommandInfo(interp, "RatUP_Translate", &cmdInfo)) {
	return;
    }
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppendElement(&cmd, "RatUP_Translate");
    Tcl_DStringAppendElement(&cmd,adrPtr->mailbox?adrPtr->mailbox:"");
    Tcl_DStringAppendElement(&cmd,adrPtr->host?adrPtr->host:"");
    Tcl_DStringAppendElement(&cmd,adrPtr->personal?adrPtr->personal:"");
    Tcl_DStringAppendElement(&cmd,adrPtr->adl?adrPtr->adl:"");
    if (TCL_OK != Tcl_Eval(interp, Tcl_DStringValue(&cmd))
	    || !(lPtr = Tcl_GetObjResult(interp))
    	    || TCL_OK != Tcl_ListObjLength(interp, lPtr, &argc)
	    || 4 != argc) {
	RatLogF(interp, RAT_ERROR, "translate_error", RATLOG_TIME,
		Tcl_DStringValue(&cmd));
    } else {
	for (i=0; i<4; i++) {
	    switch(i) {
		case 0: destPtrPtr = &adrPtr->mailbox; break;
		case 1: destPtrPtr = &adrPtr->host; break;
		case 2: destPtrPtr = &adrPtr->personal; break;
		case 3: destPtrPtr = &adrPtr->adl; break;
	    }
	    Tcl_ListObjIndex(interp, lPtr, i, &oPtr);
	    s = Tcl_GetString(oPtr);
	    if (   (*s && (!(*destPtrPtr) || strcmp(s,*destPtrPtr)))
		|| (!*s && *destPtrPtr)) {
		ckfree(*destPtrPtr);
		if (*s) {
		    *destPtrPtr = cpystr(s);
		} else {
		    *destPtrPtr = NULL;
		}
	    }
	}
    }
    Tcl_DStringFree(&cmd);
}

/*
 *----------------------------------------------------------------------
 *
 * RatAliasCmd --
 *
 *      Implements the RatAlias command as per ../doc/interface
 *
 * Results:
 *	Probably.
 *
 * Side effects:
 *	Probably.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatAliasCmd(ClientData dummy,Tcl_Interp *interp,int objc,Tcl_Obj *CONST objv[])
{
    Tcl_Obj *oPtr;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (!strcmp(Tcl_GetString(objv[1]), "add")) {
	AliasInfo *aliasPtr;
	Tcl_HashEntry *entryPtr;
	char *key;
	int new;

	aliasPtr = (AliasInfo*)ckalloc(sizeof(AliasInfo));
	switch (objc) {
	    case 5:
		aliasPtr->book = Tcl_NewStringObj("Personal", -1);
		key = Tcl_GetString(objv[2]);
		aliasPtr->fullname = objv[3];
		aliasPtr->content = objv[4];
		aliasPtr->comment = Tcl_NewObj();
		aliasPtr->flags = 0;
		break;
	    case 6:
		aliasPtr->book = Tcl_NewStringObj("Personal", -1);
		key = Tcl_GetString(objv[2]);
		aliasPtr->fullname = objv[3];
		aliasPtr->content = objv[4];
		aliasPtr->comment = Tcl_NewObj();
		aliasPtr->flags = 0;
		break;
	    case 7:
		aliasPtr->book = objv[2];
		key = Tcl_GetString(objv[3]);
		aliasPtr->fullname = objv[4];
		aliasPtr->content = objv[5];
		aliasPtr->comment = objv[6];
		aliasPtr->flags = 0;
		break;
	    case 8:
		aliasPtr->book = objv[2];
		key = Tcl_GetString(objv[3]);
		aliasPtr->fullname = objv[4];
		aliasPtr->content = objv[5];
		aliasPtr->comment = objv[6];
		if (!strcmp(Tcl_GetString(objv[7]), "nofullname")) {
		    aliasPtr->flags = ALIAS_FLAG_NOFULLNAME;
		} else {
		    aliasPtr->flags = 0;
		}
		break;
	    default:
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			Tcl_GetString(objv[0]),
			" add book name fullname content comment options\"",
			(char *) NULL);
		return TCL_ERROR;
	}
	if (!key || !*key) {
	    ckfree(aliasPtr);
	    Tcl_SetResult(interp, "The name can not be an empty string",
		    TCL_STATIC);
	    return TCL_OK;
	}
	aliasPtr->mark = 0;
	if (Tcl_IsShared(aliasPtr->content)) {
	    aliasPtr->content = Tcl_DuplicateObj(aliasPtr->content);
	}
	if (1 < AddressClean(aliasPtr->content)) {
	    aliasPtr->flags |= ALIAS_FLAG_ISLIST;
	}
	Tcl_IncrRefCount(aliasPtr->book);
	Tcl_IncrRefCount(aliasPtr->fullname);
	Tcl_IncrRefCount(aliasPtr->content);
	Tcl_IncrRefCount(aliasPtr->comment);
	entryPtr = Tcl_CreateHashEntry(&aliasTable, key, &new);
	Tcl_SetHashValue(entryPtr, (ClientData)aliasPtr);
	return TCL_OK;
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "delete")) {
	Tcl_HashEntry *entryPtr;
	AliasInfo *aliasPtr;
	int i;

	for (i=2; i<objc; i++) {
	    if ((entryPtr = Tcl_FindHashEntry(&aliasTable,
		    Tcl_GetString(objv[i])))) {
		aliasPtr = (AliasInfo*)Tcl_GetHashValue(entryPtr);
		Tcl_DecrRefCount(aliasPtr->book);
		Tcl_DecrRefCount(aliasPtr->fullname);
		Tcl_DecrRefCount(aliasPtr->content);
		Tcl_DecrRefCount(aliasPtr->comment);
		ckfree(aliasPtr);
		Tcl_DeleteHashEntry(entryPtr);
	    }
	}
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "get")) {
	Tcl_HashEntry *entryPtr;
	AliasInfo *aliasPtr;

	if (objc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " get alias\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (!(entryPtr=Tcl_FindHashEntry(&aliasTable,Tcl_GetString(objv[2])))){
	    Tcl_SetResult(interp, "Illegal alias", TCL_STATIC);
	    return TCL_ERROR;
	}
	aliasPtr = (AliasInfo*)Tcl_GetHashValue(entryPtr);
	oPtr = Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->book);
	Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->fullname);
	Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->content);
	Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->comment);
	if (aliasPtr->flags & ALIAS_FLAG_NOFULLNAME) {
	    Tcl_ListObjAppendElement(interp, oPtr,
		    Tcl_NewStringObj("nofullname", -1));
	} else {
	    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewObj());
	}
	Tcl_SetObjResult(interp, oPtr);
	return TCL_OK;

    } else if (!strcmp(Tcl_GetString(objv[1]), "list")) {
	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch search;
	AliasInfo *aliasPtr;

	if (objc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " list var\"", (char *) NULL);
	    return TCL_ERROR;
	}

	for (entryPtr = Tcl_FirstHashEntry(&aliasTable, &search);
		entryPtr; entryPtr = Tcl_NextHashEntry(&search)) {
	    aliasPtr = (AliasInfo*) Tcl_GetHashValue(entryPtr);
	    oPtr = Tcl_NewObj();
	    Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->book);
	    Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->fullname);
	    Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->content);
	    Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->comment);
	    if (aliasPtr->flags & ALIAS_FLAG_NOFULLNAME) {
		Tcl_ListObjAppendElement(interp, oPtr,
			Tcl_NewStringObj("nofullname", -1));
	    } else {
		Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewObj());
	    }
	    Tcl_SetVar2Ex(interp, Tcl_GetString(objv[2]),
		    Tcl_GetHashKey(&aliasTable, entryPtr), oPtr, 0);
	}
	return TCL_OK;
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "read")) {
	Tcl_Channel channel;

	if (objc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " read filename\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (NULL == (channel = Tcl_OpenFileChannel(interp,
		Tcl_GetString(objv[2]), "r", 0))) {
	    return TCL_ERROR;
	}
	Tcl_SetChannelOption(interp, channel, "-encoding", "utf-8");
	oPtr = Tcl_NewObj();
	while (0 <= Tcl_GetsObj(channel, oPtr) && !Tcl_Eof(channel)) {
	    Tcl_AppendToObj(oPtr, ";", 1);
	}
        Tcl_Close(interp, channel);
	return Tcl_EvalObjEx(interp, oPtr,  TCL_EVAL_DIRECT);
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "save")) {
	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch search;
	AliasInfo *aliasPtr;
	Tcl_Channel channel;
	Tcl_Obj *lPtr;
	int perm;

	if (objc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
			     Tcl_GetString(objv[0])," save book filename\"",
			     (char*)NULL);
	    return TCL_ERROR;
	}

	oPtr = Tcl_GetVar2Ex(interp, "option", "permissions", TCL_GLOBAL_ONLY);
	Tcl_GetIntFromObj(interp, oPtr, &perm);
	if (NULL == (channel = Tcl_OpenFileChannel(interp,
		Tcl_GetString(objv[3]), "w", perm))) {
	    return TCL_ERROR;
	}

	for (entryPtr = Tcl_FirstHashEntry(&aliasTable, &search);
		entryPtr; entryPtr = Tcl_NextHashEntry(&search)) {
	    aliasPtr = (AliasInfo*) Tcl_GetHashValue(entryPtr);
	    if (strcmp(Tcl_GetString(objv[2]), Tcl_GetString(aliasPtr->book))){
		continue;
	    }
	    lPtr = Tcl_NewObj();
	    Tcl_ListObjAppendElement(interp, lPtr, aliasPtr->book);
	    Tcl_ListObjAppendElement(interp, lPtr,
		    Tcl_NewStringObj(Tcl_GetHashKey(&aliasTable,entryPtr),-1));
	    Tcl_ListObjAppendElement(interp, lPtr, aliasPtr->fullname);
	    Tcl_ListObjAppendElement(interp, lPtr, aliasPtr->content);
	    Tcl_ListObjAppendElement(interp, lPtr, aliasPtr->comment);
	    if (aliasPtr->flags & ALIAS_FLAG_NOFULLNAME) {
		Tcl_ListObjAppendElement(interp, lPtr,
			Tcl_NewStringObj("nofullname", -1));
	    } else {
		Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewObj());
	    }
	    Tcl_WriteChars(channel, "RatAlias add ", -1);
	    Tcl_WriteObj(channel, lPtr);
	    Tcl_DecrRefCount(lPtr);
	    Tcl_WriteChars(channel, "\n", 1);
	}
	return Tcl_Close(interp, channel);
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "expand1")
	    || !strcmp(Tcl_GetString(objv[1]), "expand2")) {
	Tcl_HashEntry *entryPtr;
	AliasInfo *aliasPtr;
	AliasExpand ae;
	Tcl_DString list;
	char *role, *c, *s;
	ADDRESS *adrPtr, *baseAdrPtr = NULL;

	if (objc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " expand[12] adrlist role\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Check syntax
	 */
	s = cpystr(Tcl_GetString(objv[2]));
	for (c=s; *c; c++) {
	    if ('\n' == *c || '\r' == *c || '\t' == *c) {
		*c = ' ';
	    }
	}
	rfc822_parse_adrlist(&baseAdrPtr, s, "");
	ckfree(s);
	for (adrPtr = baseAdrPtr; adrPtr; adrPtr = adrPtr->next) {
	    if (adrPtr->error || (adrPtr->host && adrPtr->host[0] == '.')) {
		mail_free_address(&baseAdrPtr);
		Tcl_SetResult(interp, "Error in address list", TCL_STATIC);
		return TCL_ERROR;
	    }
	}
	mail_free_address(&baseAdrPtr);


	/*
	 * Ignore empty addresses
	 */
	for (c = Tcl_GetString(objv[2]); *c && isspace((unsigned char)*c);c++);
	if (!*c) {
	    return TCL_OK;
	}
	
	role = Tcl_GetString(objv[3]);
	ae.host = RatGetCurrent(interp, RAT_HOST, role);
	oPtr = Tcl_GetVar2Ex(interp, "option", "lookup_name", TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &ae.lookup_in_passwd);
	if (!strcmp(Tcl_GetString(objv[1]), "expand2")) {
	    ae.level = 2;
	} else {
	    Tcl_GetIntFromObj(interp, Tcl_GetVar2Ex(interp, "option",
		    "alias_expand", TCL_GLOBAL_ONLY), &ae.level);
	}

	/*
	 * Create unique mark and possibly mark all aliases
	 */
	if (0 == ++aliasMark) {
	    Tcl_HashSearch search;
	    aliasMark++;

	    for (entryPtr = Tcl_FirstHashEntry(&aliasTable, &search);
		    entryPtr; entryPtr = Tcl_NextHashEntry(&search)) {
		aliasPtr = (AliasInfo*) Tcl_GetHashValue(entryPtr);
		aliasPtr->mark = 0;
	    }
	}

	Tcl_DStringInit(&list);
	Tcl_DStringAppend(&list, Tcl_GetString(objv[2]), -1);
	RatExpandAlias(interp, &list, &ae);
	Tcl_DStringResult(interp, &list);
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
		"\": must be one of add, delete, get, list, read, save,",
		" expand1 or expand2",
		(char *) NULL);
	return TCL_ERROR;
    }

}

/*
 *----------------------------------------------------------------------
 *
 * RatAddressMail --
 *
 *      Prints the mail address in rfc822 format of an ADDRESS entry.
 *	Only one address is printed and there is NO fullname.
 *
 * Results:
 *	Pointer to a static storage area where the string is stored.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatAddressMail(ADDRESS *adrPtr)
{
    static char *store = NULL;
    static int length = 0;
    size_t size = RatAddressSize(adrPtr, 1);

    if (size > length) {
	length = size+1024;
	store = ckrealloc(store, length);
    }
    store[0] = '\0';
    rfc822_address(store, adrPtr);
    return store;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSplitAddresses --
 *
 *	This routine takes an address list as argument and splits it.
 *
 * Results:
 *	A list of addresses contained in the argument
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
 
int
RatSplitAddresses(ClientData clientData, Tcl_Interp *interp, int objc,
		  Tcl_Obj *const objv[])
{
    Tcl_Obj *rPtr;
    CONST84 char *s, *e, *n;
 
    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]), " addresslist\"",
			 (char *) NULL);
        return TCL_ERROR;
    }

    rPtr = Tcl_NewObj();
    s = Tcl_GetString(objv[1]);
    while (*s) {
	while (*s && isspace(*s)) {
	    s++;
	}
	e = n = RatFindCharInHeader(s, ',');
	if (NULL == e) {
	    e = n = s+strlen(s);
	}
	for (e--; isspace(*e) && e>s; e--) {
	    /* Do nothing */
	}
	Tcl_ListObjAppendElement(interp, rPtr, Tcl_NewStringObj(s, e-s+1));
	s = n;
	if (*s) {
	    s++;
	}
    }
    Tcl_SetObjResult(interp, rPtr);
    return TCL_OK;
}

#ifdef MEM_DEBUG
void ratAddressCleanup()
{
    Tcl_HashEntry *e;
    Tcl_HashSearch s;

    for (e = Tcl_FirstHashEntry(&aliasTable, &s); e; e =Tcl_NextHashEntry(&s)){
	ckfree(Tcl_GetHashValue(e));
    }
    Tcl_DeleteHashTable(&aliasTable);

    ckfree(mem_store);
}
#endif /* MEM_DEBUG */


/*
 *----------------------------------------------------------------------
 *
 * RatAddressSize --
 *
 *	Calculate the maximum size of and address list (or single address)
 *
 * Results:
 *	The maximum length of the address
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

size_t
RatAddressSize(ADDRESS *adrPtr, int all)
{
    ADDRESS *a,tadr;
    char tmp[MAILTMPLEN];
    size_t len, t;

    tadr.next = NULL;
    for (len = 0, a = adrPtr; a; a = a->next) {
        t = (tadr.mailbox = a->mailbox) ? 2*strlen (a->mailbox) : 3;
        if ((tadr.personal = a->personal)) t += 3 + 2*strlen (a->personal);
        if ((tadr.adl = a->adl)) t += 1 + 2*strlen (a->adl);
        if ((tadr.host = a->host)) t += 1 + 2*strlen (a->host);
        if (t < MAILTMPLEN) {     /* ignore ridiculous addresses */
	    tmp[0] = '\0';
	    rfc822_write_address (tmp,&tadr); 
	    t = strlen(tmp);
	}
	len += t+2;
	if (!all) break;
    }
    return len;
}


/*
 *----------------------------------------------------------------------
 *
 * RatGenerateAddresses --
 *
 *	Generates addresses to be used when sending email
 *
 * Results:
 *	The generated addresses are left in the ADDRESS-pointers
 *	(which are assumed to not point to anything)
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
void
RatGenerateAddresses(Tcl_Interp *interp, const char *role, char *msgh,
		     ADDRESS **from, ADDRESS **sender)
{
    char *tmp, host[1024];
    const char *ctmp;
    Tcl_Obj *oPtr;
    int useFrom, cs;

    strlcpy(host, RatGetCurrent(interp, RAT_HOST, role), sizeof(host));

    *from = NULL;
    *sender = NULL;

    oPtr = Tcl_GetVar2Ex(interp, "option", "use_from", TCL_GLOBAL_ONLY);
    if (TCL_OK != Tcl_GetBooleanFromObj(interp, oPtr, &useFrom)) {
	useFrom = 0;
    }
    if (useFrom && (ctmp = Tcl_GetVar2(interp, msgh, "from",TCL_GLOBAL_ONLY))
	    && !RatIsEmpty(ctmp)) {
	tmp = cpystr(ctmp);
	rfc822_parse_adrlist(from, tmp, host);
	ckfree(tmp);
    }
    oPtr = Tcl_GetVar2Ex(interp, "option","create_sender",TCL_GLOBAL_ONLY);
    Tcl_GetBooleanFromObj(interp, oPtr, &cs);
    if (*from && cs) {
	ADDRESS *adrPtr;

	for (adrPtr = *from; adrPtr; adrPtr = adrPtr->next) {
	    if (RatAddressIsMe(interp, adrPtr, 0)) {
		break;
	    }
	}
	if (!adrPtr) {
	    *sender = mail_newaddr();
	    (*sender)->personal =
		cpystr(RatGetCurrent(interp, RAT_PERSONAL, role));
	    (*sender)->mailbox =
		cpystr(RatGetCurrent(interp, RAT_MAILBOX, role));
	    (*sender)->host = cpystr(host);
	    RatEncodeAddresses(interp, *sender);
	}
    } else if (!*from) {
	*from = mail_newaddr();
	(*from)->personal =
	    cpystr(RatGetCurrent(interp, RAT_PERSONAL, role));
	(*from)->mailbox =
	    cpystr(RatGetCurrent(interp, RAT_MAILBOX, role));
	(*from)->host = cpystr(host);
    }
    RatEncodeAddresses(interp, *from);
}

/*
 *----------------------------------------------------------------------
 *
 * RatGenerateAddressesCmd --
 *
 *      See ../doc/interface for documentation.
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
RatGenerateAddressesCmd(ClientData clientData, Tcl_Interp *interp, int objc,
			Tcl_Obj *const objv[])
{
    ADDRESS *from, *sender;
    const char *role;
    char buf[1024];
    Tcl_Obj *oPtr;
    
    if (2 != objc) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " handler\"", (char *) NULL);
	return TCL_ERROR;
    }

    role = Tcl_GetVar2(interp, Tcl_GetString(objv[1]), "role",TCL_GLOBAL_ONLY);
    RatGenerateAddresses(interp, role, Tcl_GetString(objv[1]),
			 &from, &sender);
    oPtr = Tcl_NewObj();
    buf[0] = '\0';
    rfc822_write_address_full(buf, from, NULL);
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewStringObj(buf, -1));
    buf[0] = '\0';
    rfc822_write_address_full(buf, sender, NULL);
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewStringObj(buf, -1));
    buf[0] = '\0';
    mail_free_address(&from);
    mail_free_address(&sender);
    Tcl_SetObjResult(interp, oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatExpandAlias --
 *
 *      Expand an alias list
 *
 * Results:
 *	None
 *
 * Side effects:
 *	May modify the given list.
 *
 *
 *----------------------------------------------------------------------
 */
static void
RatExpandAlias(Tcl_Interp *interp, Tcl_DString *list, AliasExpand *ae)
{
    static char *buf = NULL;
    static int bufsize = 0;
    CONST84 char *entry_start, *entry_end, *key_start, *key_end, *c, *copy, *n;
    struct passwd *pwPtr;
    int length;
    AliasInfo *aliasPtr = NULL;
    Tcl_HashEntry *entryPtr;
    Tcl_DString expanded;
    ParseState ps;

    /*
     * Here we make a copy of the list of addresses.
     * Then we loop over all addresses and try to expand them.
     * After we have expanded an alias we try to expand the expansion as well.
     */
    copy = cpystr(Tcl_DStringValue(list));
    Tcl_DStringSetLength(list, 0);
    for (n = entry_start = copy; *n; entry_start = n+1) {
	/*
	 * Find addresses in string. We assume addresses are delimited by ','.
	 * We will also ignore any comments in addresses.
	 */
	while (isspace(*(unsigned char*)entry_start)) {
	    entry_start++;
	}
	ps = STATE_NORMAL;
	n = RatFindCharInHeader(entry_start, ',');
	if (NULL == n) {
	    n = entry_start + strlen(entry_start);
	}
	for (entry_end = n-1;
	     entry_end >= entry_start && isspace((unsigned char)*entry_end);
	     entry_end--);
	if (entry_start > entry_end) {
	    continue;
	}
	key_start = entry_start;
	while ('(' == *key_start && key_start < entry_end) {
	    for (key_start++; *key_start && ')' != *key_start;key_start++);
	    for (key_start++; *key_start&&isspace(*key_start);key_start++);
	}
	key_end = entry_end;
	while (')' == *key_end && key_end > key_start) {
	    for (; key_end >= key_start && '(' != *key_end; key_end--);
	    for (;
		 key_end >= key_start && (isspace(*key_end) || '(' ==*key_end);
		 key_end--);
	}
	if (bufsize < (key_end-key_start+1)) {
	    bufsize = key_end-key_start+256;
	    buf = (char*)ckrealloc(buf, bufsize);
	}
	memcpy(buf, key_start, key_end-key_start+1);
	buf[key_end-key_start+1] = '\0';

	if (NULL != (entryPtr = Tcl_FindHashEntry(&aliasTable, buf))
	    && (aliasPtr = (AliasInfo*)Tcl_GetHashValue(entryPtr))
	    && aliasPtr->mark != aliasMark) {
	    /*
	     * Found entry in alias database
	     */
	    aliasPtr->mark = aliasMark;
	    Tcl_DStringInit(&expanded);
	    switch (ae->level) {
	    case 0:
		Tcl_DStringAppend(&expanded, Tcl_GetString(aliasPtr->content),
				  -1);
		break;
	    case 1:
		Tcl_DStringAppend(&expanded, key_start,
				  key_end-key_start+1);
		if (Tcl_GetCharLength(aliasPtr->fullname)) {
		    Tcl_DStringAppend(&expanded, " (", 2);
		    Tcl_DStringAppend(&expanded,
				      Tcl_GetString(aliasPtr->fullname), -1);
		    Tcl_DStringAppend(&expanded, ")", 1);
		}
		break;
	    case 2:
		Tcl_DStringAppend(&expanded, Tcl_GetString(aliasPtr->content),
				  -1);
		if (Tcl_GetCharLength(aliasPtr->fullname)
		    && NULL == strchr(Tcl_GetString(aliasPtr->content), ',')) {
		    Tcl_DStringAppend(&expanded, " (", 2);
		    Tcl_DStringAppend(&expanded,
				      Tcl_GetString(aliasPtr->fullname), -1);
		    Tcl_DStringAppend(&expanded, ")", 1);
		}
	    }
	    RatExpandAlias(interp, &expanded, ae);
	    Tcl_DStringAppend(list, Tcl_DStringValue(&expanded),
			      Tcl_DStringLength(&expanded));
	    Tcl_DStringFree(&expanded);
	    
	} else if (ae->lookup_in_passwd && NULL != (pwPtr = getpwnam(buf))
	    && (NULL == entryPtr || NULL == aliasPtr)) {
	    /*
	     * Found user in the passwd-database
	     */
	    switch (ae->level) {
	    case 0:
		Tcl_DStringAppend(list, pwPtr->pw_name, -1);
		break;
	    case 1:
		Tcl_DStringAppend(list, pwPtr->pw_name, -1);
		Tcl_DStringAppend(list, " (", 2);
		if ((c = strchr(pwPtr->pw_gecos, ','))) {
		    length = c - pwPtr->pw_gecos;
		} else {
		    length = strlen(pwPtr->pw_gecos);
		}
		Tcl_DStringAppend(list, pwPtr->pw_gecos, length);
		Tcl_DStringAppend(list, ")", 1);
		break;
	    case 2:
		Tcl_DStringAppend(list, pwPtr->pw_name, -1);
		Tcl_DStringAppend(list, "@", 1);
		Tcl_DStringAppend(list, ae->host, -1);
		Tcl_DStringAppend(list, " (", 2);
		if ((c = strchr(pwPtr->pw_gecos, ','))) {
		    length = c - pwPtr->pw_gecos;
		} else {
		    length = strlen(pwPtr->pw_gecos);
		}
		Tcl_DStringAppend(list, pwPtr->pw_gecos, length);
		Tcl_DStringAppend(list, ")", 1);
		break;
	    }
	} else {
	    /*
	     * User not found anywhere
	     */
	    Tcl_DStringAppend(list, entry_start, entry_end-entry_start+1);
	}
	Tcl_DStringAppend(list, ", ", 2);
    }
    ckfree(copy);
    length = Tcl_DStringLength(list);
    Tcl_DStringSetLength(list, length-2);
}

/*
 *----------------------------------------------------------------------
 *
 * RatFindCharInHeader --
 *
 *      Finds the next unquoted instance of a given character in a header
 *      field.
 *
 * Results:
 *	Returns the address of the next instance of the sought character
 *      or NULL if no instance was found.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
CONST84 char*
RatFindCharInHeader(CONST84 char *header, char m)
{
    ParseState ps;
    CONST84 char *c;
    
    ps = STATE_NORMAL;
    for (c = header; *c; c++) {
	switch (ps) {
	case STATE_NORMAL:
	    if ('"' == *c) {
		ps = STATE_QUOTED;
	    } else if ('[' == *c) {
		ps = STATE_LITERAL;
	    } else if ('(' == *c) {
		ps = STATE_COMMENT;
	    } else if ('\\' == *c) {
		ps = STATE_ESCAPED;
	    } else if (m == *c) {
		return c;
	    }
	    break;
	case STATE_ESCAPED:
	    ps = STATE_NORMAL;
	    break;
	case STATE_COMMENT:
	    if (')' == *c) {
		ps = STATE_NORMAL;
	    } else if ('\\' == *c) {
		ps = STATE_ESCAPED;
	    }
	    break;
	case STATE_QUOTED:
	    if ('"' == *c) {
		ps = STATE_NORMAL;
	    } else if ('\\' == *c) {
		ps = STATE_ESCAPED;
	    }
	    break;
	case STATE_LITERAL:
	    if (']' == *c) {
		ps = STATE_NORMAL;
	    } else if ('\\' == *c) {
		ps = STATE_ESCAPED;
	    }
	    break;
	}
    }
    return NULL;
}
