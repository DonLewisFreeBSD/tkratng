/*
 * ratAddress.c --
 *
 *	This file contains basic support for handling addresses.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
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
    ADDRESS *parsed;    /* Parsed contents */
    Tcl_Obj *comment;	/* Comment to alias */
    Tcl_Obj *pgp_key;	/* PGP key of alias expressed as {ID DESCR} */
    char *address;      /* Address contained in alias if not a list */
    unsigned int flags;	/* Option flags */
    unsigned long mark;	/* Id of last use */
} AliasInfo;

#define ALIAS_FLAG_ISLIST	(1<<0)
#define ALIAS_FLAG_NOFULLNAME	(1<<1)
#define ALIAS_FLAG_PGP_SIGN	(1<<2)
#define ALIAS_FLAG_PGP_ENCRYPT	(1<<3)

/*
 * Alias flags
 */
struct {
    const char *name;
    int flag;
} alias_flags[] = {
    {"nofullname",  ALIAS_FLAG_NOFULLNAME},
    {"pgp_sign",    ALIAS_FLAG_PGP_SIGN},
    {"pgp_encrypt", ALIAS_FLAG_PGP_ENCRYPT},
    { NULL, 0 }
};

/*
 * This table contains all the aliases. Stored as pointers to the AliasInfo
 * structs.
 */
static Tcl_HashTable aliasTable;

/*
 * This table contains all single email addresses contained in aliases.
 * The table stores pointers to the respective AliasInfo structs.
 * This is used to find the pgp settings for certain addresses.
 */
static Tcl_HashTable addressTable;

/*
 * This struct us ised when aliases are expanded
 */
typedef enum {
    EXPAND_DISPLAY, EXPAND_SENDING, EXPAND_PGP, EXPAND_PGPACTIONS
} expand_t;
typedef struct {
    char *host;
    char *mark;
    int lookup_in_passwd;
    expand_t target;
    int flags;
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
 * These tables contain the email addresses for each of my roles.
 */
static Tcl_HashTable myRoles1, myRoles2, *myRolesCurrent, *myRolesPrevious;

/*
 * This table contains my default email address and all my role addresses.
 */
static Tcl_HashTable myAddressesTable;

/*
 *  The default email address for the !trustUser case
 */
static char *myDefaultEmailAddress;

/*
 * Override the address book of new entries when this is set
 */
static Tcl_Obj *overrideBook = NULL;

#ifdef MEM_DEBUG
static char *mem_store;
#endif /* MEM_DEBUG */

static void RatRebuildMyAddressTable(Tcl_HashTable *myaddresstable);
static Tcl_VarTraceProc RatRoleWatcher;
static Tcl_VarTraceProc RatRoleListWatcher;
void RatInitMyAddessesTable(Tcl_Interp *interp);
static void RatExpandAlias(Tcl_Interp *interp, ADDRESS *address,
                           Tcl_DString *list, AliasExpand *ea);
static Tcl_Obj *RatGetFlagsList(Tcl_Interp *interp, AliasInfo *aliasPtr);
static Tcl_ObjCmdProc RatCreateAddressCmd;
static Tcl_ObjCmdProc RatAddressCmd;
static Tcl_CmdDeleteProc RatDeleteAddress;
static Tcl_ObjCmdProc RatAliasCmd;
static Tcl_ObjCmdProc RatSplitAdrCmd;
static Tcl_ObjCmdProc RatGenerateAddressesCmd;

/*
 *----------------------------------------------------------------------
 *
 * RatInitAddressHandling --
 *
 *      Initialize the address system
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
void RatInitAddressHandling(Tcl_Interp *interp)
{
    Tcl_InitHashTable(&aliasTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&addressTable, TCL_STRING_KEYS);

    Tcl_CreateObjCommand(interp, "RatCreateAddress", RatCreateAddressCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatAlias", RatAliasCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatSplitAdr", RatSplitAdrCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGenerateAddresses",
			 RatGenerateAddressesCmd, NULL, NULL);

    myRolesCurrent = &myRoles1;
    myRolesPrevious = &myRoles2;
    Tcl_InitHashTable(myRolesCurrent, TCL_STRING_KEYS);
    Tcl_InitHashTable(&myAddressesTable, TCL_STRING_KEYS);
    myDefaultEmailAddress = cpystr(RatGetCurrent(interp, RAT_EMAILADDRESS,""));
    RatRoleListWatcher(&myAddressesTable, interp, "option", "roles",
	               TCL_GLOBAL_ONLY);
    Tcl_TraceVar2(interp, "option", "roles",
	          TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                  RatRoleListWatcher, &myAddressesTable);
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

static int
RatCreateAddressCmd(ClientData clientData, Tcl_Interp *interp, int objc,
		    Tcl_Obj *CONST objv[])
{
    ADDRESS *adrPtr = NULL;
    char *s, *domain;

    if (objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
                         Tcl_GetString(objv[0]),
                         " ?-nodomain? address ?role?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (!strcmp("-nodomain", Tcl_GetString(objv[1]))) {
        domain = NODOMAIN;
        s = cpystr(Tcl_GetString(objv[2]));
    } else {
        domain = RatGetCurrent(interp, RAT_HOST, Tcl_GetString(objv[2]));
        s = cpystr(Tcl_GetString(objv[1]));
    }
    rfc822_parse_adrlist(&adrPtr, s, domain);
    ckfree(s);
    RatEncodeAddresses(interp, adrPtr);
    Tcl_ResetResult(interp);
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
	Tcl_CreateObjCommand(interp, name, RatAddressCmd, (ClientData) newPtr,
		RatDeleteAddress);
	Tcl_ListObjAppendElement(interp, rPtr, Tcl_NewStringObj(name, -1));
    }
    Tcl_SetObjResult(interp, rPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RatAddressCmd --
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

static int
RatAddressCmd(ClientData clientData, Tcl_Interp *interp, int objc,
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
		Tcl_IncrRefCount(oPtr);
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

static void
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
 * RatRebuildMyAddressTable --
 *
 *      Watch for changes to role email addresses and update the current
 *	role address table.
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	See above.
 *
 *
 *----------------------------------------------------------------------
 */
static void
RatRebuildMyAddressTable(Tcl_HashTable *myaddresstable)
{
    Tcl_HashEntry *hPtr, *entryPtr;
    Tcl_HashSearch search;
    int new;
    char *emailaddress;

    Tcl_DeleteHashTable(myaddresstable);
    /*
     * Walk current role address table to find all the email addresses
     * and add entries for each to the the address table.
     */
    Tcl_InitHashTable(myaddresstable, TCL_STRING_KEYS);
    for (hPtr = Tcl_FirstHashEntry(myRolesCurrent, &search);
	 hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	emailaddress = Tcl_GetHashValue(hPtr);
	entryPtr = Tcl_CreateHashEntry(myaddresstable, emailaddress, &new);
	Tcl_SetHashValue(entryPtr, "role");
    }
    entryPtr = Tcl_CreateHashEntry(myaddresstable, myDefaultEmailAddress, &new);
    Tcl_SetHashValue(entryPtr, "me");
}

/*
 *----------------------------------------------------------------------
 *
 * RatRoleWatcher --
 *
 *      Watch for changes to role email addresses and update the current
 *	role address table.
 *
 *      MyAddressTable is rebuilt if clientData != NULL
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	See above.
 *
 *
 *----------------------------------------------------------------------
 */
static char *
RatRoleWatcher(ClientData clientData, Tcl_Interp *interp,
	       CONST84 char *name1, CONST84 char *name2, int flags)
{
    char role[1024], *comma;
    char *emailaddress, *oldaddress, *cp;
    int new;
    Tcl_HashEntry *entryPtr;

    if (flags & TCL_INTERP_DESTROYED) {
	return NULL;
    }

    strlcpy(role, name2, sizeof(role));
    comma = strchr(role, ',');
    if (comma) {
	*comma = '\0';
    }
    emailaddress = cpystr(RatGetCurrent(interp, RAT_EMAILADDRESS, role));
    for (cp = emailaddress; *cp; cp++) {
	*cp = tolower((unsigned char) *cp);
    }
    entryPtr = Tcl_CreateHashEntry(myRolesCurrent, role, &new);
    if (new == 0) {
	oldaddress = Tcl_GetHashValue(entryPtr);
	if (oldaddress) {
	    ckfree(oldaddress);
	}
    }
    Tcl_SetHashValue(entryPtr, emailaddress);

    if (clientData) {
	RatRebuildMyAddressTable(clientData);
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * RatRoleListWatcher --
 *
 *      Update myAddressesTable whenever the roles option is changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	See above.
 *
 *
 *----------------------------------------------------------------------
 */
static char*
RatRoleListWatcher(ClientData clientData, Tcl_Interp *interp,
		   CONST84 char *name1, CONST84 char *name2, int flags)
{
    Tcl_Obj **objv, *oPtr;
    Tcl_HashTable *myRolesTmp;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    int objc, i, new;
    char *role, *emailaddress;
    char buf[1024];

    if (flags & TCL_INTERP_DESTROYED) {
    	return NULL;
    }

    /*
     * The role list has changed, so swap the the role tables so that
     * the changes can be found and the new table updated.
     */
    myRolesTmp = myRolesCurrent;
    myRolesCurrent = myRolesPrevious;
    myRolesPrevious = myRolesTmp;
    Tcl_InitHashTable(myRolesCurrent, TCL_STRING_KEYS);

    /*
     * Copy entries common to the old and new lists of roles.
     * Add email addresses and set up traces on any new roles.
     */
    oPtr = Tcl_GetVar2Ex(interp, name1, name2, flags & TCL_GLOBAL_ONLY);
    if (oPtr != NULL) {
	Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);
	for (i=0; i<objc; i++) {
	    role = Tcl_GetString(objv[i]);
	    entryPtr = Tcl_FindHashEntry(myRolesPrevious, role);
	    if (entryPtr) {
		emailaddress = Tcl_GetHashValue(entryPtr);
		Tcl_SetHashValue(entryPtr, NULL);
		entryPtr = Tcl_CreateHashEntry(myRolesCurrent, role, &new);
		Tcl_SetHashValue(entryPtr, emailaddress);
	    } else {
	        snprintf(buf, sizeof(buf), "%s,from", role);
	        RatRoleWatcher(NULL, interp, name1, buf, flags);
		Tcl_TraceVar2(interp, name1, buf,
			      TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
			      RatRoleWatcher, clientData);
		snprintf(buf, sizeof(buf), "%s,uqa_domain", role);
		Tcl_TraceVar2(interp, name1, buf,
			      TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
			      RatRoleWatcher, clientData);
	    }
	}
    }

    /*
     * Remove traces for any roles that have gone away.
     * Free the previous table.
     */
    for (entryPtr = Tcl_FirstHashEntry(myRolesPrevious, &search);
	 entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	role = Tcl_GetHashKey(myRolesPrevious, entryPtr);
	if (! Tcl_FindHashEntry(myRolesCurrent, role)) {
	    snprintf(buf, sizeof(buf), "%s,from", role);
	    Tcl_UntraceVar2(interp, name1, buf,
			    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
			    RatRoleWatcher, clientData);
	    snprintf(buf, sizeof(buf), "%s,uqa_domain", role);
	    Tcl_UntraceVar2(interp, name1, buf,
			    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
			    RatRoleWatcher, clientData);
	}
	emailaddress = Tcl_GetHashValue(entryPtr);
	if (emailaddress) {
	    ckfree(emailaddress);
	}
    }
    Tcl_DeleteHashTable(myRolesPrevious);

    RatRebuildMyAddressTable(clientData);

    return NULL;
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
    Tcl_CmdInfo cmdInfo;
    Tcl_HashEntry *entryPtr;
    char buf[1024], *cp;

    if ((adrPtr == NULL) || (adrPtr->mailbox == NULL) ||
	(adrPtr->host == NULL)) {
	return 0;
    }
    
    snprintf(buf, sizeof(buf), "%s@%s", adrPtr->mailbox, adrPtr->host);
    for (cp = buf; *cp; cp++) {
	*cp = tolower((unsigned char) *cp);
    }

    entryPtr = Tcl_FindHashEntry(&myAddressesTable, buf);
    if (entryPtr != NULL) {
        cp = (char *)Tcl_GetHashValue(entryPtr);
        if (cp[0] == 'm' || (trustUser && (cp[0] == 'r'))) {
            return 1;
        }
    }

    if (trustUser && Tcl_GetCommandInfo(interp, "RatUP_IsMe", &cmdInfo)) {
	Tcl_DString cmd;
	int isMe;
	Tcl_Obj *oPtr;
	
	Tcl_DStringInit(&cmd);
	Tcl_DStringAppendElement(&cmd, "RatUP_IsMe"); 
	Tcl_DStringAppendElement(&cmd, adrPtr->mailbox ? adrPtr->mailbox : "");
	Tcl_DStringAppendElement(&cmd, adrPtr->host ? adrPtr->host : "");
	Tcl_DStringAppendElement(&cmd, adrPtr->personal ? adrPtr->personal:"");
	Tcl_DStringAppendElement(&cmd, adrPtr->adl?adrPtr->adl:"");
	if (TCL_OK == Tcl_Eval(interp, Tcl_DStringValue(&cmd))
	    && (oPtr = Tcl_GetObjResult(interp))
	    && TCL_OK == Tcl_GetBooleanFromObj(interp, oPtr, &isMe)) {
	    Tcl_DStringFree(&cmd);
	    return isMe;
	}
	Tcl_DStringFree(&cmd);
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

static int
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
	Tcl_Obj **fobjv;
	char *key, *s;
	int new, fobjc;

	aliasPtr = (AliasInfo*)ckalloc(sizeof(AliasInfo));
	switch (objc) {
	    case 5:
		aliasPtr->book = Tcl_NewStringObj("Personal", -1);
		key = Tcl_GetString(objv[2]);
		aliasPtr->fullname = objv[3];
		aliasPtr->content = objv[4];
		aliasPtr->comment = Tcl_NewObj();
		aliasPtr->pgp_key = Tcl_NewObj();
		aliasPtr->flags = 0;
		break;
	    case 6:
		aliasPtr->book = Tcl_NewStringObj("Personal", -1);
		key = Tcl_GetString(objv[2]);
		aliasPtr->fullname = objv[3];
		aliasPtr->content = objv[4];
		aliasPtr->comment = Tcl_NewObj();
		aliasPtr->pgp_key = Tcl_NewObj();
		aliasPtr->flags = 0;
		break;
	    case 7:
		aliasPtr->book = objv[2];
		key = Tcl_GetString(objv[3]);
		aliasPtr->fullname = objv[4];
		aliasPtr->content = objv[5];
		aliasPtr->comment = objv[6];
		aliasPtr->pgp_key = Tcl_NewObj();
		aliasPtr->flags = 0;
		break;
  	    case 8:
		aliasPtr->book = objv[2];
		key = Tcl_GetString(objv[3]);
		aliasPtr->fullname = objv[4];
		aliasPtr->content = objv[5];
		aliasPtr->comment = objv[6];
		aliasPtr->pgp_key = Tcl_NewObj();
		if (!strcmp(Tcl_GetString(objv[7]), "nofullname")) {
		    aliasPtr->flags = ALIAS_FLAG_NOFULLNAME;
		} else {
		    aliasPtr->flags = 0;
		}
		break;
  	    case 9:
		aliasPtr->book = objv[2];
		key = Tcl_GetString(objv[3]);
		aliasPtr->fullname = objv[4];
		aliasPtr->content = objv[5];
		aliasPtr->comment = objv[6];
		aliasPtr->pgp_key = objv[7];
		aliasPtr->flags = 0;
		if (TCL_OK ==
		    Tcl_ListObjGetElements(interp, objv[8], &fobjc, &fobjv)) {
		    int i, j;
		    
		    for (i=0; i<fobjc; i++) {
			for (j=0; alias_flags[j].name; j++) {
			    if (!strcmp(alias_flags[j].name,
					Tcl_GetString(fobjv[i]))) {
				aliasPtr->flags |= alias_flags[j].flag;
				break;
			    }
			}
		    }
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
        if (overrideBook) {
            Tcl_IncrRefCount(aliasPtr->book);
            Tcl_DecrRefCount(aliasPtr->book);
            aliasPtr->book = overrideBook;
        }
        aliasPtr->parsed = NULL;
	aliasPtr->mark = 0;
	if (Tcl_IsShared(aliasPtr->content)) {
	    aliasPtr->content = Tcl_DuplicateObj(aliasPtr->content);
	}
        
        s = cpystr(Tcl_GetString(aliasPtr->content));
        rfc822_parse_adrlist(&aliasPtr->parsed, s, "");
        ckfree(s);

        aliasPtr->address = NULL;
        if (aliasPtr->parsed && aliasPtr->parsed->next) {
	    aliasPtr->flags |= ALIAS_FLAG_ISLIST;
	} else if (aliasPtr->parsed) {
	    char buf[1024];
	    
            snprintf(buf, sizeof(buf), "%s@%s", aliasPtr->parsed->mailbox,
                     aliasPtr->parsed->host);
            if (!aliasPtr->parsed->personal
                && !(aliasPtr->flags & ALIAS_FLAG_NOFULLNAME)) {
                aliasPtr->parsed->personal =
                    cpystr(Tcl_GetString(aliasPtr->fullname));
            }
            aliasPtr->address = cpystr(buf);

	    entryPtr = Tcl_CreateHashEntry(&addressTable, buf, &new);
	    Tcl_SetHashValue(entryPtr, (ClientData)aliasPtr);
	}
	Tcl_IncrRefCount(aliasPtr->book);
	Tcl_IncrRefCount(aliasPtr->fullname);
	Tcl_IncrRefCount(aliasPtr->content);
	Tcl_IncrRefCount(aliasPtr->comment);
	Tcl_IncrRefCount(aliasPtr->pgp_key);
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
		Tcl_DeleteHashEntry(entryPtr);
		Tcl_DecrRefCount(aliasPtr->book);
		Tcl_DecrRefCount(aliasPtr->fullname);
		Tcl_DecrRefCount(aliasPtr->content);
		Tcl_DecrRefCount(aliasPtr->comment);
		Tcl_DecrRefCount(aliasPtr->pgp_key);
		if (aliasPtr->address
		    && (entryPtr = Tcl_FindHashEntry(&addressTable,
						    aliasPtr->address))) {
		    Tcl_DeleteHashEntry(entryPtr);
		    ckfree(aliasPtr->address);
		}
                mail_free_address(&aliasPtr->parsed);
		ckfree(aliasPtr);
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
	Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->pgp_key);
	Tcl_ListObjAppendElement(interp, oPtr,
				 RatGetFlagsList(interp, aliasPtr));
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
	    Tcl_ListObjAppendElement(interp, oPtr, aliasPtr->pgp_key);
	    Tcl_ListObjAppendElement(interp, oPtr,
				     RatGetFlagsList(interp, aliasPtr));
	    Tcl_SetVar2Ex(interp, Tcl_GetString(objv[2]),
		    Tcl_GetHashKey(&aliasTable, entryPtr), oPtr, 0);
	}
	return TCL_OK;
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "read")) {
	Tcl_Channel channel;
        int ret;

	if (objc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
                             Tcl_GetString(objv[0]), " read book filename\"",
                             (char *) NULL);
	    return TCL_ERROR;
	}
	if (NULL == (channel = Tcl_OpenFileChannel(interp,
		Tcl_GetString(objv[3]), "r", 0))) {
	    return TCL_ERROR;
	}
        /* XXX */
	Tcl_SetChannelOption(interp, channel, "-encoding", "utf-8");
	oPtr = Tcl_NewObj();
	while (0 <= Tcl_GetsObj(channel, oPtr) && !Tcl_Eof(channel)) {
	    Tcl_AppendToObj(oPtr, ";", 1);
	}
        Tcl_Close(interp, channel);
        overrideBook = objv[2];
	ret = Tcl_EvalObjEx(interp, oPtr,  TCL_EVAL_DIRECT);
        overrideBook = NULL;
        return ret;
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "save")) {
	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch search;
	AliasInfo *aliasPtr;
	Tcl_Channel channel;
	Tcl_Obj *lPtr;

	if (objc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
			     Tcl_GetString(objv[0])," save book filename\"",
			     (char*)NULL);
	    return TCL_ERROR;
	}

	if (NULL == (channel = Tcl_OpenFileChannel(interp,
		Tcl_GetString(objv[3]), "w", 0666))) {
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
	    Tcl_ListObjAppendElement(interp, lPtr, aliasPtr->pgp_key);
	    Tcl_ListObjAppendElement(interp, lPtr,
				     RatGetFlagsList(interp, aliasPtr));
	    Tcl_IncrRefCount(lPtr);
	    Tcl_WriteChars(channel, "RatAlias add ", -1);
	    Tcl_WriteObj(channel, lPtr);
	    Tcl_DecrRefCount(lPtr);
	    Tcl_WriteChars(channel, "\n", 1);
	}
	return Tcl_Close(interp, channel);
	
    } else if (!strcmp(Tcl_GetString(objv[1]), "expand")) {
	Tcl_HashEntry *entryPtr;
	AliasInfo *aliasPtr;
	AliasExpand ae;
	Tcl_DString list;
	char *role, *c, *s;
	ADDRESS *adrPtr, *baseAdrPtr = NULL;

	if (objc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    Tcl_GetString(objv[0]), " expand level adrlist role\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	if (!strcmp(Tcl_GetString(objv[2]), "display")) {
	    ae.target = EXPAND_DISPLAY;
	} else if (!strcmp(Tcl_GetString(objv[2]), "sending")) {
	    ae.target = EXPAND_SENDING;
	} else if (!strcmp(Tcl_GetString(objv[2]), "pgp")) {
	    ae.target = EXPAND_PGP;
	} else if (!strcmp(Tcl_GetString(objv[2]), "pgpactions")) {
	    ae.target = EXPAND_PGPACTIONS;
	} else {
	    Tcl_AppendResult(interp, "bad level argument \"",
                             Tcl_GetString(objv[2]), "\" should be display,"
                             " sending, pgp or pgpactions", (char *) NULL);
	    return TCL_ERROR;
	}
	role = Tcl_GetString(objv[4]);
	ae.host = RatGetCurrent(interp, RAT_HOST, role);
	oPtr = Tcl_GetVar2Ex(interp, "option", "lookup_name", TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &ae.lookup_in_passwd);
        ae.flags = 0;

        /*
	 * Ignore empty addresses
	 */
	for (c = Tcl_GetString(objv[3]); *c && isspace((unsigned char)*c);c++);
	if (!*c) {
            if (EXPAND_PGPACTIONS == ae.target) {
                Tcl_Obj *robjv[2];

                robjv[0] = robjv[1] = Tcl_NewBooleanObj(0);
                Tcl_SetObjResult(interp, Tcl_NewListObj(2, robjv));
            }
	    return TCL_OK;
	}

	/*
	 * Create unique mark
         * Reset all aliases if alias mark has wrapped
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

	/*
	 * Simplify whitespace and parse list
	 */
	s = cpystr(Tcl_GetString(objv[3]));
	for (c=s; *c; c++) {
	    if ('\n' == *c || '\r' == *c || '\t' == *c) {
		*c = ' ';
	    }
	}
        ae.mark = cpystr(ae.host);
        for (c=ae.mark; *c; c++) {
            *c = '#';
        }
	rfc822_parse_adrlist(&baseAdrPtr, s, ae.mark);
	ckfree(s);
	for (adrPtr = baseAdrPtr; adrPtr; adrPtr = adrPtr->next) {
	    if (adrPtr->error || (adrPtr->host && adrPtr->host[0] == '.')) {
		mail_free_address(&baseAdrPtr);
		Tcl_SetResult(interp, "Error in address list", TCL_STATIC);
		return TCL_ERROR;
	    }
	}

        Tcl_DStringInit(&list);
	RatExpandAlias(interp, baseAdrPtr, &list, &ae);
	mail_free_address(&baseAdrPtr);

        if (EXPAND_PGPACTIONS == ae.target) {
            Tcl_Obj *robjv[2];

            robjv[0] = Tcl_NewBooleanObj(ae.flags & ALIAS_FLAG_PGP_SIGN);
            robjv[1] = Tcl_NewBooleanObj(ae.flags & ALIAS_FLAG_PGP_ENCRYPT);
            Tcl_SetObjResult(interp, Tcl_NewListObj(2, robjv));
        } else {
            Tcl_DStringResult(interp, &list);
        }
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
		"\": must be one of add, delete, get, list, read, save,",
		" or expand",
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
 * RatAddressFull --
 *
 *      Prints the full address in rfc822 format of an ADDRESS entry.
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
RatAddressFull(Tcl_Interp *interp, ADDRESS *adrPtr, char *role)
{
    static char *store = NULL;
    static int length = 0;
    size_t size = RatAddressSize(adrPtr, 1);
    ADDRESS *next = adrPtr->next;
    int host_replaced = 0;

    if (size > length) {
	length = size+1024;
	store = ckrealloc(store, length);
    }
    store[0] = '\0';
    adrPtr->next = NULL;
    if (NULL == adrPtr->host && role) {
        adrPtr->host = RatGetCurrent(interp, RAT_HOST, role);
        host_replaced = 1;
    }
    rfc822_write_address_full(store, adrPtr, NULL);
    adrPtr->next = next;
    if (host_replaced) {
        adrPtr->host = NULL;
    }
    if (strstr(store, "=?")) {
        return RatDecodeHeader(interp, store, 1);
    } else {
        return store;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatSplitAdrCmd --
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
 
static int
RatSplitAdrCmd(ClientData clientData, Tcl_Interp *interp, int objc,
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

static int
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
RatExpandAlias(Tcl_Interp *interp, ADDRESS *address, Tcl_DString *list,
               AliasExpand *ae)
{
    AliasInfo *aliasPtr = NULL;
    Tcl_HashEntry *entryPtr;
    struct passwd *pwPtr;
    ADDRESS *ta, *next_address, aliasAddr;
    char key_buf[1024], out_buf[1024];
    char *key, *s;

    for (; address; address = address->next) {
        if (address->host && strlen(address->host)
            && (ae->mark == NULL || strcmp(ae->mark, address->host))) {
            snprintf(key_buf, sizeof(key_buf), "%s@%s",
                     address->mailbox, address->host);
            key = key_buf;
        } else {
            key = address->mailbox;
        }
        if (!key) {
            continue;
        }
	if (NULL != (entryPtr = Tcl_FindHashEntry(&aliasTable, key))
	    && (aliasPtr = (AliasInfo*)Tcl_GetHashValue(entryPtr))
	    && aliasPtr->mark != aliasMark) {
            /* Key found in alias list */

            aliasPtr->mark = aliasMark;
            if (EXPAND_PGP == ae->target
                && Tcl_GetCharLength(aliasPtr->pgp_key)) {
                Tcl_DStringAppendElement(list,
                                         Tcl_GetString(aliasPtr->pgp_key));
                continue;
            } else if (EXPAND_PGPACTIONS == ae->target) {
                ae->flags |= aliasPtr->flags;
            }

            if (EXPAND_DISPLAY == ae->target) {
                ta = &aliasAddr;
                aliasAddr.personal = Tcl_GetString(aliasPtr->fullname);
                aliasAddr.adl = NULL;
                aliasAddr.mailbox = key;
                aliasAddr.host = "";
                aliasAddr.next = NULL;
                
            } else {
                RatExpandAlias(interp, aliasPtr->parsed, list, ae);
                continue;
            }

        } else if (EXPAND_PGPACTIONS == ae->target
                   && (entryPtr = Tcl_FindHashEntry(&addressTable, key))
                   && (aliasPtr = (AliasInfo*)Tcl_GetHashValue(entryPtr))) {
            ae->flags |= aliasPtr->flags;
            continue;
            
        } else if (EXPAND_PGP == ae->target
                   && (entryPtr = Tcl_FindHashEntry(&addressTable, key))
                   && (aliasPtr = (AliasInfo*)Tcl_GetHashValue(entryPtr))
                   && Tcl_GetCharLength(aliasPtr->pgp_key)) {
            Tcl_DStringAppendElement(list, Tcl_GetString(aliasPtr->pgp_key));
            continue;
            
	} else if (ae->lookup_in_passwd
                   && key
                   && NULL != (pwPtr = getpwnam(key))
                   && address->personal == NULL
                   && ae->target != EXPAND_PGP
                   && (NULL == entryPtr || NULL == aliasPtr)) {
            /* Key found in /etc/passwd */
            address->personal = cpystr(pwPtr->pw_gecos);
            if (NULL != (s = strchr(address->personal, ','))) {
                *s = '\0';
            }
            ta = address;
        } else {
            ta = address;
        }
        if (ae->mark != NULL && !strcmp(ae->mark, address->host)) {
            if (EXPAND_DISPLAY == ae->target) {
                address->host[0] = '\0';
            } else {
                strncpy(address->host, ae->host, strlen(address->host)+1);
            }
        }

        if (EXPAND_PGP == ae->target) {
            snprintf(out_buf, sizeof(out_buf), "%s@%s", ta->mailbox,
                     ((ta->host && *ta->host) ? ta->host : ae->host));
            Tcl_DStringAppendElement(list, out_buf);
        } else if (RatAddressSize(ta, 1) < sizeof(out_buf)) {
            next_address = ta->next;
            ta->next = NULL;
            out_buf[0] = '\0';
            rfc822_write_address_full(out_buf, ta, NULL);
            ta->next = next_address;
            
            if (Tcl_DStringLength(list)) {
                Tcl_DStringAppend(list, ", ", 2);
            }
            Tcl_DStringAppend(list, out_buf, -1);
        }
    }
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


/*
 *----------------------------------------------------------------------
 *
 * RatGetFlagsList --
 *
 *      Returns a list of flags defined for this alias
 *      field.
 *
 * Results:
 *	A tcl_object containing a list of strings.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj*
RatGetFlagsList(Tcl_Interp *interp, AliasInfo *aliasPtr)
{
    Tcl_Obj *flagsPtr = Tcl_NewObj();
    int i;
    
    for (i=0; alias_flags[i].name; i++) {
	if (aliasPtr->flags & alias_flags[i].flag) {
	    Tcl_ListObjAppendElement(
		interp, flagsPtr,Tcl_NewStringObj(alias_flags[i].name,-1));
	}
    }

    return flagsPtr;
}
