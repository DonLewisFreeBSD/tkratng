/*
 * ratDbFolder.c --
 *
 *      This file contains code which implements standard c-client folders.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <time.h>
#include "ratFolder.h"


/*
 * This is the private part of a Db folder info structure.
 */

typedef struct DbFolderInfo {
    int *listPtr;		/* List of messages in this folder */
    Tcl_Obj *searchExpr;       	/* The search expression used to create
				 * this folder. */
    char *keywords;		/* Keywords to add to inserted messages */
    char *exDate;		/* Expiration date of inserted messages */
    char *exType;		/* Expiration type of new messages */
    Tcl_Obj **infoPtr;		/* List of information caches */
} DbFolderInfo;

typedef enum {Db_Name, Db_Mail} DbAdrInfo;

/*
 * Procedures private to this module.
 */
static RatInitProc Db_InitProc;
static RatCloseProc Db_CloseProc;
static RatUpdateProc Db_UpdateProc;
static RatInsertProc Db_InsertProc;
static RatSetFlagProc Db_SetFlagProc;
static RatGetFlagProc Db_GetFlagProc;
static RatCreateProc Db_CreateProc;
static RatSetInfoProc Db_SetInfoProc;
static RatDbInfoGetProc Db_DbinfoGetProc;
static RatDbInfoSetProc Db_DbinfoSetProc;
static int GetAddressInfo(Tcl_Interp *interp, Tcl_DString *dsPtr, char *adr,
	DbAdrInfo info);


/*
 *----------------------------------------------------------------------
 *
 * RatDbFolderInit --
 *
 *      Initializes the dbase folder data.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	The C-client library is initialized and the apropriate mail drivers
 *	are linked.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDbFolderInit(Tcl_Interp *interp)
{
    Tcl_CreateObjCommand(interp, "RatInsert", RatInsertCmd, (ClientData) NULL,
	    (Tcl_CmdDeleteProc *) NULL);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbFolderCreate --
 *
 *      Creates a db folder entity.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	A db folder is created.
 *
 *
 *----------------------------------------------------------------------
 */

RatFolderInfo*
RatDbFolderCreate(Tcl_Interp *interp, int append_only, Tcl_Obj *defPtr)
{
    RatFolderInfo *infoPtr;
    DbFolderInfo *dbPtr;
    int *listPtr, number, i, objc, eobjc, expError;
    RatDbEntry *entryPtr;
    Tcl_Obj **objv, **eobjv;

    Tcl_ListObjGetElements(interp, defPtr, &objc, &objv);
    
    Tcl_IncrRefCount(objv[5]);
    if (append_only) {
        number = 0;
        listPtr = NULL;
    } else if (TCL_OK !=
               RatDbSearch(interp, objv[5], &number, &listPtr, &expError)) {
	Tcl_DecrRefCount(objv[5]);
        if (!expError) {
            RatLogF(interp, RAT_ERROR, "dbase_error", RATLOG_TIME,
                    Tcl_GetStringResult(interp));
        }
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Failed to search dbase \"",
		Tcl_GetString(objv[5]), "\"", (char *) NULL);
	return (RatFolderInfo *) NULL;
    }

    infoPtr = (RatFolderInfo *) ckalloc(sizeof(*infoPtr));
    dbPtr = (DbFolderInfo *) ckalloc(sizeof(*dbPtr));

    infoPtr->name = cpystr("Database search");
    infoPtr->type = "dbase";
    infoPtr->number = number;
    infoPtr->recent = 0;
    infoPtr->unseen = 0;
    for (i=0; i<infoPtr->number; i++) {
	entryPtr = RatDbGetEntry(listPtr[i]);
	if (!strchr(entryPtr->content[STATUS], 'O')) {
	    infoPtr->recent++;
	}
	if (!strchr(entryPtr->content[STATUS], 'R')) {
	    infoPtr->unseen++;
	}
    }
    infoPtr->size = 0;
    for (i=0; i<infoPtr->number; i++) {
	infoPtr->size += atoi(RatDbGetEntry(listPtr[i])->content[RSIZE]);
    }
    infoPtr->initProc = Db_InitProc;
    infoPtr->finalProc = NULL;
    infoPtr->closeProc = Db_CloseProc;
    infoPtr->updateProc = Db_UpdateProc;
    infoPtr->insertProc = Db_InsertProc;
    infoPtr->setFlagProc = Db_SetFlagProc;
    infoPtr->getFlagProc = Db_GetFlagProc;
    infoPtr->infoProc = Db_InfoProc;
    infoPtr->setInfoProc = Db_SetInfoProc;
    infoPtr->createProc = Db_CreateProc;
    infoPtr->syncProc = NULL;
    infoPtr->dbinfoGetProc = Db_DbinfoGetProc;
    infoPtr->dbinfoSetProc = Db_DbinfoSetProc;
    infoPtr->private = (ClientData) dbPtr;
    dbPtr->listPtr = listPtr;
    dbPtr->searchExpr = objv[5];
    Tcl_ListObjGetElements(interp, objv[5], &eobjc, &eobjv);
    dbPtr->keywords = NULL;
    for (i=0; i<eobjc-1; i++) {
	if (!strcmp("keywords", Tcl_GetString(eobjv[i]))) {
	    dbPtr->keywords = cpystr(Tcl_GetString(eobjv[i+1]));
	    break;
	}
    }
    dbPtr->exDate = cpystr(Tcl_GetString(objv[4]));
    dbPtr->exType = cpystr(Tcl_GetString(objv[3]));
    dbPtr->infoPtr = (Tcl_Obj**)ckalloc(sizeof(Tcl_Obj*) * number *
					RAT_FOLDER_END);
    for (i=0; i < number*RAT_FOLDER_END; i++) {
	dbPtr->infoPtr[i] = NULL;
    }

    return infoPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_InitProc --
 *
 *      See the documentation for initProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for initProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static void
Db_InitProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index)
{
    return;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_CloseProc --
 *
 *      See the documentation for closeProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for closeProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Db_CloseProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int expunge)
{
    DbFolderInfo *dbPtr = (DbFolderInfo *) infoPtr->private;
    int i;

    if (dbPtr->listPtr) {
        ckfree(dbPtr->listPtr);
    }
    Tcl_DecrRefCount(dbPtr->searchExpr);
    ckfree(dbPtr->keywords);
    ckfree(dbPtr->exDate);
    ckfree(dbPtr->exType);
    for (i=0; i<infoPtr->number*RAT_FOLDER_END; i++) {
	if (dbPtr->infoPtr[i]) {
	    Tcl_DecrRefCount(dbPtr->infoPtr[i]);
	}
    }
    ckfree(dbPtr->infoPtr);
    ckfree(dbPtr);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_UpdateProc --
 *
 *      See the documentation for updateProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for updateProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Db_UpdateProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, RatUpdateType mode)
{
    DbFolderInfo *dbPtr = (DbFolderInfo *) infoPtr->private;
    int *listPtr, number, numNew, i, expError;
    RatDbEntry *entryPtr;

    if (RAT_SYNC == mode) {
	int i, j, dst;

	if (TCL_OK != RatDbExpunge(interp)) {
	    return -1;
	}
	infoPtr->size = 0;
	for (i=dst=0; i<infoPtr->number; i++) {
	    if ((entryPtr = RatDbGetEntry(dbPtr->listPtr[i]))) {
		dbPtr->listPtr[dst] = dbPtr->listPtr[i];
		infoPtr->msgCmdPtr[dst] = infoPtr->msgCmdPtr[i];
		infoPtr->size += atoi(entryPtr->content[RSIZE]);
		for (j=0; j<RAT_FOLDER_END; j++) {
		    dbPtr->infoPtr[dst*RAT_FOLDER_END+j] =
			    dbPtr->infoPtr[i*RAT_FOLDER_END+j];
		}
		dst++;
	    } else {
		if (infoPtr->msgCmdPtr[i]) {
		    RatMessageDelete(interp, infoPtr->msgCmdPtr[i]);
		}
		for (j=0; j<RAT_FOLDER_END; j++) {
		    if (dbPtr->infoPtr[i*RAT_FOLDER_END+j]) {
			Tcl_DecrRefCount(dbPtr->infoPtr[i*RAT_FOLDER_END+j]);
		    }
		}
	    }
	}
	infoPtr->number = dst;
    }

    numNew = 0;
    if (RAT_SYNC == mode || RAT_UPDATE == mode) {
	if (TCL_OK != RatDbSearch(interp, dbPtr->searchExpr, &number,
                                  &listPtr, &expError)){
            if (!expError) {
                RatLogF(interp, RAT_ERROR, "dbase_error", RATLOG_TIME,
                        Tcl_GetStringResult(interp));
            }
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Failed to search dbase \"",
		    Tcl_GetString(dbPtr->searchExpr), "\"", (char *) NULL);
	    return -1;
	}
	for (i=0 ; i < infoPtr->number
		&& i < number
		&& listPtr[i] == dbPtr->listPtr[i];
		i++);
	if (i != number || i != infoPtr->number) {
	    for (i=0; i<infoPtr->number*RAT_FOLDER_END; i++) {
		if (dbPtr->infoPtr[i]) {
		    Tcl_DecrRefCount(dbPtr->infoPtr[i]);
		}
	    }
	    ckfree(dbPtr->infoPtr);
	    ckfree(dbPtr->listPtr);
	    dbPtr->listPtr = listPtr;
	    numNew = number - infoPtr->number;
	    infoPtr->number = number;
	    dbPtr->infoPtr = (Tcl_Obj**)
		    ckalloc(sizeof(Tcl_Obj*)*number*RAT_FOLDER_END);
	    for (i=0; i<number*RAT_FOLDER_END; i++) {
		dbPtr->infoPtr[i] = NULL;
	    }
	}
	infoPtr->recent = 0;
	infoPtr->unseen = 0;
	for (i=0; i<infoPtr->number; i++) {
	    entryPtr = RatDbGetEntry(dbPtr->listPtr[i]);
	    if (!strchr(entryPtr->content[STATUS], 'O')) {
		infoPtr->recent++;
	    }
	    if (!strchr(entryPtr->content[STATUS], 'R')) {
		infoPtr->unseen++;
	    }
	}
    }
    return numNew;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_InsertProc --
 *
 *      See the documentation for insertProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for insertProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Db_InsertProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int argc,
	char *argv[])
{
    DbFolderInfo *dbPtr = (DbFolderInfo *) infoPtr->private;
    Tcl_CmdInfo cmdInfo;
    int i;

    for (i=0; i<argc; i++) {
	if (0 == Tcl_GetCommandInfo(interp, argv[i], &cmdInfo)) {
	    Tcl_AppendResult(interp, "No such message: ", argv[i], NULL);
	    return TCL_ERROR;
	}
	RatInsertMsg(interp, (MessageInfo*)cmdInfo.objClientData,
		dbPtr->keywords, dbPtr->exDate, dbPtr->exType);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_SetFlagProc --
 *
 *      See the documentation for setFlagProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for setFlagProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Db_SetFlagProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int *ilist,
	       int count, RatFlag flag, int value)
{
    DbFolderInfo *dbPtr = (DbFolderInfo *) infoPtr->private;
    RatDbEntry *entryPtr;
    char newStatus[5];
    int dst, i, j;

    for (i=0; i<count; i++) {
	int flagArray[RAT_FLAG_END];
        memset(flagArray, 0, sizeof(flagArray));
	entryPtr = RatDbGetEntry(dbPtr->listPtr[ilist[i]]);
	for (j=0; entryPtr->content[STATUS][j]; j++) {
	    switch(entryPtr->content[STATUS][j]) {
	    case 'R':	flagArray[RAT_SEEN] = 1; break;
	    case 'D':	flagArray[RAT_DELETED] = 1; break;
	    case 'F':	flagArray[RAT_FLAGGED] = 1; break;
	    case 'A':	flagArray[RAT_ANSWERED] = 1; break;
	    case 'T':	flagArray[RAT_DRAFT] = 1; break;
	    case 'O':	flagArray[RAT_RECENT] = 1; break;
	    }
	}
	if (RAT_SEEN == flag && flagArray[RAT_SEEN] != value) {
	    if (value) {
		infoPtr->unseen--;
	    } else {
		infoPtr->unseen++;
	    }
	}
	flagArray[flag] = value;
	dst = 0;
	if (flagArray[RAT_SEEN]) { newStatus[dst++] = 'R'; }
	if (flagArray[RAT_DELETED]) { newStatus[dst++] = 'D'; }
	if (flagArray[RAT_FLAGGED]) { newStatus[dst++] = 'F'; }
	if (flagArray[RAT_ANSWERED]) { newStatus[dst++] = 'A'; }
	if (flagArray[RAT_DRAFT]) { newStatus[dst++] = 'T'; }
	if (flagArray[RAT_RECENT]) { newStatus[dst++] = 'O'; }
	newStatus[dst] = '\0';
	j = ilist[i]*RAT_FOLDER_END+RAT_FOLDER_STATUS;
	if (dbPtr->infoPtr[j]) {
	    Tcl_DecrRefCount(dbPtr->infoPtr[j]);
	    dbPtr->infoPtr[j] = NULL;
	}
	if (TCL_OK !=
	    RatDbSetStatus(interp, dbPtr->listPtr[ilist[i]], newStatus)) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_GetFlagProc --
 *
 *      See the documentation for getFlagProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for getFlagProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static int
Db_GetFlagProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index,
	RatFlag flag)
{
    DbFolderInfo *dbPtr = (DbFolderInfo *) infoPtr->private;
    RatDbEntry *entryPtr = RatDbGetEntry(dbPtr->listPtr[index]);
    char flagChar;
    int i;

    switch(flag) {
	case RAT_SEEN:		flagChar = 'R'; break;
	case RAT_DELETED:	flagChar = 'D'; break;
	case RAT_FLAGGED:	flagChar = 'F'; break;
	case RAT_ANSWERED:	flagChar = 'A'; break;
	case RAT_DRAFT:		flagChar = 'T'; break;
	case RAT_RECENT:	flagChar = 'O'; break;
	default:		return 0;
    }
    for (i=0; entryPtr->content[STATUS][i]; i++) {
	if (entryPtr->content[STATUS][i] == flagChar) {
	    return 1;
	}
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * GetAddressInfo --
 *
 *      Gets info from an address. The info argument decides what we
 *	wants to extract. The requested data will be appeded to dsPtr.
 *
 * Results:
 *	Returns true if this address points to me.
 *
 * Side effects:
 *	The dsPtr DString will be modified
 *
 *
 *----------------------------------------------------------------------
 */

static int
GetAddressInfo(Tcl_Interp *interp, Tcl_DString *dsPtr, char *adr,
	DbAdrInfo info)
{
    ADDRESS *addressPtr = NULL;
    char *s, *host;
    int ret;

    host = RatGetCurrent(interp, RAT_HOST, "");
    s = cpystr(adr);
    rfc822_parse_adrlist(&addressPtr, s, host);
    ckfree(s);
    if (!addressPtr) {
	return 0;
    }
    ret = RatAddressIsMe(interp, addressPtr, 1);

    if (Db_Name == info && addressPtr->personal) {
	Tcl_DStringAppend(dsPtr, addressPtr->personal, -1);
    } else {
	Tcl_DStringAppend(dsPtr, RatAddressMail(addressPtr), -1);
    }
    mail_free_address(&addressPtr);
    return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Db_InfoProc --
 *
 *      See the documentation for infoProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for infoProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
Db_InfoProc(Tcl_Interp *interp, ClientData clientData, RatFolderInfoType type,
	int index)
{
    RatFolderInfo *infoPtr = (RatFolderInfo*)clientData;

    return Db_InfoProcInt(interp, infoPtr, type, index);
}


/*
 *----------------------------------------------------------------------
 *
 * Db_InfoProcInt --
 *
 *      See the documentation for infoProc in ratFolder.h. The difference
 *	between this function and Db_InfoProc is that this expects the
 *	index (rIndex) to be the real folder index.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for infoProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
Db_InfoProcInt(Tcl_Interp *interp, RatFolderInfo *infoPtr,
	RatFolderInfoType type, int rIndex)
{
    static Tcl_DString ds;
    static int initialized = 0;
    static char buf[1024];
    int i, seen, deleted, marked, answered, me, dbIndex, zone, f;
    DbFolderInfo *dbPtr = (DbFolderInfo*)infoPtr->private;
    ADDRESS *addressPtr, *address2Ptr;
    Tcl_Obj *oPtr = NULL;
    MESSAGECACHE elt;
    RatDbEntry *entryPtr;
    struct tm *tmPtr;
    Tcl_CmdInfo info;
    time_t time;
    char *host;

    dbIndex = dbPtr->listPtr[rIndex];

    if (dbPtr->infoPtr[rIndex*RAT_FOLDER_END+type]) {
	if (type == RAT_FOLDER_INDEX) {
	    Tcl_GetIntFromObj(interp,
		    dbPtr->infoPtr[rIndex*RAT_FOLDER_END+type], &i);
	    if (i < infoPtr->number
		&& dbIndex == dbPtr->listPtr[infoPtr->presentationOrder[i]]) {
		return dbPtr->infoPtr[rIndex*RAT_FOLDER_END+type];
	    }
	} else {
	    return dbPtr->infoPtr[rIndex*RAT_FOLDER_END+type];
	}
    }

    entryPtr = RatDbGetEntry(dbIndex);

    if (!initialized) {
	Tcl_DStringInit(&ds);
	initialized = 1;
    }

    switch (type) {
	case RAT_FOLDER_SUBJECT:
	    oPtr = Tcl_NewStringObj(entryPtr->content[SUBJECT], -1);
	    break;
	case RAT_FOLDER_CANONSUBJECT:
	    oPtr = RatFolderCanonalizeSubject(entryPtr->content[SUBJECT]);
	    break;
	case RAT_FOLDER_NAME:
	    Tcl_DStringSetLength(&ds, 0);
	    if (GetAddressInfo(interp, &ds, entryPtr->content[FROM],Db_Name)) {
		Tcl_DStringSetLength(&ds, 0);
		Tcl_DStringAppend(&ds,
			Tcl_GetVar2(interp, "t", "to", TCL_GLOBAL_ONLY), -1);
		Tcl_DStringAppend(&ds, ": ", 2);
		GetAddressInfo(interp, &ds, entryPtr->content[TO], Db_Name);
	    }
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    break;
	case RAT_FOLDER_ANAME:
	    Tcl_DStringSetLength(&ds, 0);
	    GetAddressInfo(interp, &ds, entryPtr->content[FROM],Db_Name);
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    break;
	case RAT_FOLDER_MAIL_REAL:
	    Tcl_DStringSetLength(&ds, 0);
	    GetAddressInfo(interp, &ds, entryPtr->content[FROM], Db_Mail);
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    break;
	case RAT_FOLDER_MAIL:
	    Tcl_DStringSetLength(&ds, 0);
	    if (GetAddressInfo(interp, &ds, entryPtr->content[FROM],Db_Mail)) {
		Tcl_DStringSetLength(&ds, 0);
		Tcl_DStringAppend(&ds,
			Tcl_GetVar2(interp, "t", "to", TCL_GLOBAL_ONLY), -1);
		Tcl_DStringAppend(&ds, ": ", 2);
		GetAddressInfo(interp, &ds, entryPtr->content[TO], Db_Mail);
	    }
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    break;
	case RAT_FOLDER_NAME_RECIPIENT:
	    if (RatIsEmpty(entryPtr->content[TO])) {
		oPtr = Tcl_NewStringObj(entryPtr->content[TO], -1);
		break;
	    }
	    Tcl_DStringSetLength(&ds, 0);
	    if (GetAddressInfo(interp, &ds, entryPtr->content[TO], Db_Name)) {
		Tcl_DStringSetLength(&ds, 0);
		Tcl_DStringAppend(&ds,
			Tcl_GetVar2(interp, "t", "from", TCL_GLOBAL_ONLY), -1);
		Tcl_DStringAppend(&ds, ": ", 2);
		GetAddressInfo(interp, &ds, entryPtr->content[FROM], Db_Name);
	    }
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    break;
	case RAT_FOLDER_MAIL_RECIPIENT:
	    if (RatIsEmpty(entryPtr->content[TO])) {
		oPtr = Tcl_NewStringObj(entryPtr->content[TO], -1);
		break;
	    }
	    Tcl_DStringSetLength(&ds, 0);
	    if (GetAddressInfo(interp, &ds, entryPtr->content[TO], Db_Mail)) {
		Tcl_DStringSetLength(&ds, 0);
		Tcl_DStringAppend(&ds,
			Tcl_GetVar2(interp, "t", "from", TCL_GLOBAL_ONLY), -1);
		Tcl_DStringAppend(&ds, ": ", 2);
		GetAddressInfo(interp, &ds, entryPtr->content[FROM], Db_Mail);
	    }
	    oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    break;
	case RAT_FOLDER_SIZE:
	    oPtr = Tcl_NewIntObj(atoi(entryPtr->content[RSIZE]));
	    break;
	case RAT_FOLDER_SIZE_F:
	    oPtr = RatMangleNumber(atoi(entryPtr->content[RSIZE]));
	    break;
	case RAT_FOLDER_DATE_F:
	    time = atoi(entryPtr->content[DATE]);
	    tmPtr = localtime(&time);
	    oPtr = RatFormatDate(interp, tmPtr);
	    break;
	case RAT_FOLDER_DATE_N:
	    oPtr = Tcl_NewStringObj(entryPtr->content[DATE], -1);
	    break;
	case RAT_FOLDER_DATE_IMAP4:
	    Tcl_DStringSetLength(&ds, 256);
	    time = atoi(entryPtr->content[DATE]);
	    tmPtr = localtime(&time);
	    elt.day = tmPtr->tm_mday;
	    elt.month = tmPtr->tm_mon+1;
	    elt.year = tmPtr->tm_year+1900-BASEYEAR;
	    elt.hours = tmPtr->tm_hour;
	    elt.minutes = tmPtr->tm_min;
	    elt.seconds = tmPtr->tm_sec;
	    zone = RatGetTimeZone(time);
	    if (zone >= 0) {
		elt.zoccident = 1;
		elt.zhours = zone/(60*60);
		elt.zminutes = (zone%(60*60))/60;
	    } else {
		elt.zoccident = 0;
		elt.zhours = (-1*zone)/(60*60);
		elt.zminutes = ((-1*zone)%(60*60))/60;
	    }
	    oPtr = Tcl_NewStringObj(mail_date(Tcl_DStringValue(&ds), &elt),-1);
	    break;
	case RAT_FOLDER_STATUS:
	    seen = deleted = marked = answered = me = 0;
	    for (i=0; entryPtr->content[STATUS][i]; i++) {
		switch (entryPtr->content[STATUS][i]) {
		    case 'R': seen = 1;		break;
		    case 'D': deleted = 1;	break;
		    case 'F': marked = 1;	break;
		    case 'A': answered = 1;	break;
		}
	    }
	    addressPtr = NULL;
	    if (!RatIsEmpty(entryPtr->content[TO])) {
		Tcl_DStringSetLength(&ds, 0);
		Tcl_DStringAppend(&ds, entryPtr->content[TO], -1);
		host = RatGetCurrent(interp, RAT_HOST, "");
		rfc822_parse_adrlist(&addressPtr, Tcl_DStringValue(&ds), host);
		for (address2Ptr = addressPtr; !me && address2Ptr;
			address2Ptr = address2Ptr->next) {
		    if (RatAddressIsMe(interp, address2Ptr, 1)) {
			me = 1;
		    }
		}
	    }
	    mail_free_address(&addressPtr);
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
	    if (me) {
		buf[i++] = '+';
	    } else {
		buf[i++] = ' ';
	    }
	    buf[i] = '\0';
	    oPtr = Tcl_NewStringObj(buf, -1);
	    break;
	case RAT_FOLDER_TYPE:		
	    return NULL;
	case RAT_FOLDER_PARAMETERS:
	    return NULL;
	case RAT_FOLDER_INDEX:
	    for (i=0; i < infoPtr->number; i++) {
		if (dbIndex == dbPtr->listPtr[infoPtr->presentationOrder[i]]) {
		    oPtr = Tcl_NewIntObj(i+1);
		    break;
		}
	    }
	    if (i == infoPtr->number) {
		oPtr = Tcl_NewIntObj(1);
	    }
	    break;
	case RAT_FOLDER_UID:
	    oPtr = Tcl_NewIntObj(dbIndex);
	    break;
	case RAT_FOLDER_TO:
	    oPtr = Tcl_NewStringObj(entryPtr->content[TO], -1);
	    break;
	case RAT_FOLDER_FROM:
	    oPtr = Tcl_NewStringObj(entryPtr->content[FROM], -1);
	    break;
	case RAT_FOLDER_SENDER:		/* fallthrough */
	case RAT_FOLDER_CC:		/* fallthrough */
	case RAT_FOLDER_REPLY_TO:
	    if (NULL == infoPtr->msgCmdPtr[rIndex]) {
		infoPtr->msgCmdPtr[rIndex] =
		    Db_CreateProc(infoPtr, interp, rIndex);
	    }
	    Tcl_GetCommandInfo(interp, infoPtr->msgCmdPtr[rIndex], &info);
	    oPtr = RatMsgInfo(interp, (MessageInfo*)info.objClientData, type);
	    break;
	case RAT_FOLDER_FLAGS:
	    Tcl_DStringSetLength(&ds, 0);
	    for (i=0; entryPtr->content[STATUS][i]; i++) {
		for (f=0; flag_name[f].imap_name; f++) {
		    if (flag_name[f].unix_char==entryPtr->content[STATUS][i]) {
			Tcl_DStringAppend(&ds, " ", -1);
			Tcl_DStringAppend(&ds, flag_name[f].imap_name, -1);
			break;
		    }
		}
	    }
	    if (Tcl_DStringLength(&ds)) {
		oPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds)+1, -1);
	    } else {
		oPtr = Tcl_NewStringObj("", 0);
	    }
	    break;
	case RAT_FOLDER_UNIXFLAGS:
	    oPtr = Tcl_NewStringObj(entryPtr->content[STATUS], -1);
	    break;
	case RAT_FOLDER_MSGID:
	    oPtr = Tcl_NewStringObj(entryPtr->content[MESSAGE_ID], -1);
	    break;
	case RAT_FOLDER_REF:
	    oPtr = Tcl_NewStringObj(entryPtr->content[REFERENCE], -1);
	    break;
	case RAT_FOLDER_THREADING:
	    return NULL;
	case RAT_FOLDER_END:
	    break;
    }
    dbPtr->infoPtr[rIndex*RAT_FOLDER_END+type] = oPtr;
    return oPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Db_CreateProc --
 *
 *      See the documentation for createProc in ratFolder.h
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the documentation for createProc in ratFolder.h
 *
 *
 *----------------------------------------------------------------------
 */
static char*
Db_CreateProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index)
{
    DbFolderInfo *dbPtr = (DbFolderInfo *) infoPtr->private;
    return RatDbMessageCreate(interp, infoPtr, index, dbPtr->listPtr[index]);
}

/*
 *----------------------------------------------------------------------
 *
 * Db_SetInfoProc --
 *
 *      Sets information about a message
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
static void
Db_SetInfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index, Tcl_Obj *oPtr)
{
    RatFolderInfo *infoPtr = (RatFolderInfo*)clientData;
    DbFolderInfo *dbPtr = (DbFolderInfo*)infoPtr->private;
    int i = index*RAT_FOLDER_END+type;

    if (dbPtr->infoPtr[i]) {
	Tcl_DecrRefCount(dbPtr->infoPtr[i]);
    }
    dbPtr->infoPtr[i] = oPtr;
    if (oPtr) {
	Tcl_IncrRefCount(oPtr);
    }
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
Db_DbinfoGetProc(RatFolderInfo *infoPtr)
{
    Tcl_Obj *robjv[3];
    int len;
    
    DbFolderInfo *dbPtr = (DbFolderInfo*)infoPtr->private;

    len = dbPtr->keywords ? strlen(dbPtr->keywords) : 0;
    if (len && '{' == dbPtr->keywords[0]
        && '}' == dbPtr->keywords[len-1]) {
        robjv[0] = Tcl_NewStringObj(dbPtr->keywords+1, len-2);
    } else {
        robjv[0] = Tcl_NewStringObj(dbPtr->keywords, len);
    }
    robjv[1] = Tcl_NewLongObj(atol(dbPtr->exDate));
    robjv[2] = Tcl_NewStringObj(dbPtr->exType, -1);
    return Tcl_NewListObj(3, robjv);
}


/*
 *----------------------------------------------------------------------
 *
 * Db_DbinfoSetProc --
 *
 *      Handle the dbinfo_set command. See ../doc/interface for details
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
int
Db_DbinfoSetProc(Tcl_Interp *interp, RatFolderInfoPtr infoPtr,
                 Tcl_Obj *indexes, Tcl_Obj *keywords, Tcl_Obj *ex_date,
                 Tcl_Obj *ex_type)
{
    DbFolderInfo *dbPtr = (DbFolderInfo*)infoPtr->private;
    int objc, *db_indexes, i, index, r;
    Tcl_Obj **objv;

    /* Convert folder indexes to database indexes */
    if (TCL_OK != Tcl_ListObjGetElements(interp, indexes, &objc, &objv)) {
        return TCL_ERROR;
    }
    db_indexes = (int*)ckalloc(objc*sizeof(int));
    for (i=0; i<objc; i++) {
        if (TCL_OK != Tcl_GetIntFromObj(interp, objv[i], &index)) {
            ckfree(db_indexes);
            return TCL_ERROR;
        }
        db_indexes[i] = dbPtr->listPtr[infoPtr->presentationOrder[index]];
    }
    
    r = RatDbSetInfo(interp, db_indexes, objc, keywords, ex_date, ex_type);
    ckfree(db_indexes);

    return r;
}
