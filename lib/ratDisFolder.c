/*
 * ratDisFolder.c --
 *
 *      This file contains code which implements disconnected folders.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

/*
 *  * One directory per folder
 *  * In that directory we find the following files:
 *    master	- File containing information about master folder.
 *		  Contains the following entries one on each line:
 *			name
 *			folder_spec
 *    state	- State against master.
 *		  Contains the following entries one on each line:
 *			uidvalidity
 *			last known UID in master
 *    mappings	- File with mappings local_uid <> master_uid
 *    folder	- The local copy of the folder
 *    changes   - Changes which should be applied to the master once
 *		  we synchronize. This file should contain:
 *			delete uid		- message to delete
 *			flag UID flag value	- set flag to value
 *
 */

#include "ratFolder.h"
#include "ratStdFolder.h"
#include "mbx.h"

/*
 * The uid map
 */
typedef struct {
    unsigned long *map;
    unsigned long size;
    unsigned long allocated;
} RatUidMap;

/*
 * This is the private part of a disconnected folder info structure.
 */

typedef struct DisFolderInfo {
    char *dir;			/* Directory where local data is stored */
    Tcl_HashTable map;		/* Mappings local_uid > remote_uid */
    int mapChanged;		/* non null if mappings needs to be rewritten*/
    MAILSTREAM *master;		/* Mailstream, used only in online mode */
    int error;	                /* Error indicator variable */
    MAILSTREAM *local;		/* Mailstream of local folder */
    char *spec;	    	        /* Name of master folder */
    FolderHandlers handlers;	/* Event handlers */
    Tcl_Interp *interp;		/* Needed if event-handlers */
    RatFolderInfo *infoPtr;
    int exists, expunged;	/* Used by event handlers (indexes) */
    unsigned long lastUid;	/* Uid of last message in master folder */
    
    /* Original procs for local folder */
    RatInitProc *initProc;
    RatCloseProc *closeProc;
    RatUpdateProc *updateProc;
    RatInsertProc *insertProc;
    RatSetFlagProc *setFlagProc;
    RatGetFlagProc *getFlagProc;
    RatInfoProc *infoProc;
    RatSetInfoProc *setInfoProc;
    RatCreateProc *createProc;
} DisFolderInfo;

/*
 * Hashtable containing open disfolders
 * The dirname is the key and the infoPtr is the value
 */
Tcl_HashTable openDisFolders;

/*
 * Procedures private to this module.
 */
static RatInitProc Dis_InitProc;
static RatFinalProc Dis_FinalProc;
static RatCloseProc Dis_CloseProc;
static RatUpdateProc Dis_UpdateProc;
static RatInsertProc Dis_InsertProc;
static RatInfoProc Dis_InfoProc;
static RatSetFlagProc Dis_SetFlagProc;
static RatGetFlagProc Dis_GetFlagProc;
static RatCreateProc Dis_CreateProc;
static RatSetInfoProc Dis_SetInfoProc;
static RatSyncProc Dis_SyncProc;
static int CreateDir(char *dir);
static Tcl_ObjCmdProc RatSyncDisconnected;
static void Dis_FindAndSyncFolders(Tcl_Interp *interp, CONST84 char *dir);
static int Dis_SyncFolder(Tcl_Interp *interp, CONST84 char *dir, off_t size,
			  int force, MAILSTREAM **master);
static unsigned long DisDownloadMsgs(Tcl_Interp *interp,
				     MAILSTREAM *masterStream,
				     MAILSTREAM *localStream,
				     int *masterErrorPtr, CONST84 char *dir,
				     Tcl_HashTable *mapPtr,
				     FILE *mapFp, unsigned long startAfterUid,
				     unsigned long stopBeforeUid);
static unsigned long GetMasterUID(MAILSTREAM *s, Tcl_HashTable *mapPtr,
	int index);
static void UpdateFolderFlag(Tcl_Interp *interp, DisFolderInfo *disPtr,
	int index, RatFlag flag, int value);
static void ReadMappings(MAILSTREAM *s, const char *dir,Tcl_HashTable *mapPtr);
static void ReadOldMappings(MAILSTREAM *s, Tcl_HashTable *mapPtr, char *buf,
	int buflen, FILE *fp);
static RatUidMap *InitUidMap(MAILSTREAM *s);
static void FreeUidMap(RatUidMap *uidMap);
static unsigned long MsgNo(RatUidMap *uidMap, unsigned long uid);
static void CheckDeletion(RatFolderInfoPtr infoPtr, Tcl_Interp *interp);
static const char* PrepareDir(Tcl_Interp *interp, Tcl_Obj *defPtr);
static unsigned long DisUploadMsg(MAILSTREAM *masterStream,
				  MAILSTREAM *localStream,
				  const char *subject, const char *in_reply_to,
				  const char *message_id, char *envdate,
				  unsigned long local_uid,
				  Tcl_DString *message, char *date,
				  char *flags, FILE *mapFP,
				  Tcl_HashTable *mapPtr);
static HandleExists Dis_HandleExists;
static HandleExpunged Dis_HandleExpunged;
static void WriteMappings(DisFolderInfo *disPtr);


/*
 *----------------------------------------------------------------------
 *
 * RatDisFolderInit --
 *
 *      Initializes the disconnected folder command.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDisFolderInit(Tcl_Interp *interp)
{
    Tcl_InitHashTable(&openDisFolders, TCL_STRING_KEYS);
    Tcl_CreateObjCommand(interp, "RatSyncDisconnected", RatSyncDisconnected,
	    NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDisFolderCreate --
 *
 *      Creates a disconnected folder entity.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *	A disconnected folder is created.
 *
 *
 *----------------------------------------------------------------------
 */

RatFolderInfo*
RatDisFolderCreate(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    const char *dir;
    Tcl_Obj **objv, *oPtr, *lPtr;
    RatFolderInfo *infoPtr;
    DisFolderInfo *disPtr;
    Tcl_HashEntry *entryPtr;
    int objc, unused, online;

    Tcl_ListObjGetElements(interp, defPtr, &objc, &objv);
    
    /*
     * Prepare directory
     */
    dir = PrepareDir(interp, defPtr);
    if (!dir) {
	return NULL;
    }
    disPtr = (DisFolderInfo *) ckalloc(sizeof(*disPtr));
    disPtr->dir = cpystr(dir);
    disPtr->spec = NULL;
    
    /*
     * Open filefolder
     */
    lPtr = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewStringObj("Base", 4));
    Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewStringObj("file", 4));
    Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewObj());
    oPtr = Tcl_NewStringObj(disPtr->dir, -1);
    Tcl_AppendToObj(oPtr, "/folder", 7);
    Tcl_ListObjAppendElement(interp, lPtr, oPtr);
    infoPtr = RatStdFolderCreate(interp, lPtr);
    if (NULL == infoPtr) {
	Tcl_DecrRefCount(lPtr);
	goto error;
    }
    Tcl_DecrRefCount(lPtr);

    /*
     * Read mappings
     */
    Tcl_InitHashTable(&disPtr->map, TCL_ONE_WORD_KEYS);
    ReadMappings(((StdFolderInfo*)infoPtr->private)->stream, 
	    disPtr->dir, &disPtr->map);

    infoPtr->name = Tcl_GetString(objv[3]);
    if (!*infoPtr->name) {
	infoPtr->name = "INBOX";
    }
    infoPtr->name = cpystr(infoPtr->name);
    infoPtr->type = "dis";
    infoPtr->private2 = (ClientData) disPtr;

    disPtr->master = NULL;
    disPtr->local = ((StdFolderInfo*)infoPtr->private)->stream;
    disPtr->lastUid = 0;
    disPtr->handlers.state = (void*)disPtr;
    disPtr->handlers.exists = Dis_HandleExists;
    disPtr->handlers.expunged = Dis_HandleExpunged;
    disPtr->interp = interp;
    disPtr->infoPtr = infoPtr;
    disPtr->initProc = infoPtr->initProc;
    disPtr->closeProc = infoPtr->closeProc;
    disPtr->updateProc = infoPtr->updateProc;
    disPtr->insertProc = infoPtr->insertProc;
    disPtr->setFlagProc = infoPtr->setFlagProc;
    disPtr->getFlagProc = infoPtr->getFlagProc;
    disPtr->infoProc = infoPtr->infoProc;
    disPtr->setInfoProc = infoPtr->setInfoProc;
    disPtr->createProc = infoPtr->createProc;

    infoPtr->initProc = Dis_InitProc;
    infoPtr->finalProc = NULL;
    infoPtr->closeProc = Dis_CloseProc;
    infoPtr->updateProc = Dis_UpdateProc;
    infoPtr->insertProc = Dis_InsertProc;
    infoPtr->setFlagProc = Dis_SetFlagProc;
    infoPtr->getFlagProc = Dis_GetFlagProc;
    infoPtr->infoProc = Dis_InfoProc;
    infoPtr->setInfoProc = Dis_SetInfoProc;
    infoPtr->createProc = Dis_CreateProc;
    infoPtr->syncProc = Dis_SyncProc;

    /*
     * Add to hash table
     */
    entryPtr = Tcl_CreateHashEntry(&openDisFolders, disPtr->dir, &unused);
    Tcl_SetHashValue(entryPtr, (ClientData)infoPtr);

    /*
     * Maybe go online?
     */
    oPtr = Tcl_GetVar2Ex(interp, "option", "online", TCL_GLOBAL_ONLY);
    Tcl_GetBooleanFromObj(interp, oPtr, &online);
    if (online) {
	infoPtr->finalProc = Dis_FinalProc;
    }
    
    return infoPtr;

error:
    ckfree(disPtr);
    return NULL;
}
/*
 *----------------------------------------------------------------------
 *
 * Dis_FinalProc --
 *
 *      Do final initialization if we are going online
 *
 * Results:
 *	None
 *
 * Side effects:
 *	May update the folder
 *
 *----------------------------------------------------------------------
 */
static void
Dis_FinalProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    char buf[1024];
    struct stat sbuf;

    snprintf(buf, sizeof(buf), "%s/master", disPtr->dir);
    stat(buf, &sbuf);
    Dis_SyncFolder(interp, disPtr->dir, sbuf.st_size, 1, &disPtr->master);
}

/*
 *----------------------------------------------------------------------
 *
 * RatDisPrepareDir --
 *
 *      Prepares the directory for a disconnected folder
 *
 * Results:
 *	A pointer to a static area holds the name of the directory or
 *	NULL on errors;
 *
 * Side effects:
 *	Updatesv the master-file
 *
 *----------------------------------------------------------------------
 */
static const char*
PrepareDir(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    const char *dir, *name;
    struct stat sbuf;
    Tcl_DString ds;
    FILE *fp;
    Tcl_Obj **objv, *lPtr;
    int objc;

    /*
     * Find directory and make sure it exists
     */
    if (!(dir = RatDisFolderDir(interp, defPtr))) {
	return NULL;
    }
    Tcl_ListObjGetElements(interp, defPtr, &objc, &objv);
    name = Tcl_GetString(objv[0]);
    if (!*name) {
	name = "INBOX";
    }

    /*
     * Initialize state-file and create folder-file if it does not exist
     */
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, dir, -1);
    Tcl_DStringAppend(&ds, "/state", 7);
    if (0 != stat(Tcl_DStringValue(&ds), &sbuf)) {
	fp = fopen(Tcl_DStringValue(&ds), "w");
	if (NULL == fp) {
	    Tcl_DStringFree(&ds);
	    return NULL;
	}
	fprintf(fp, "0\n0\n");
	fclose(fp);

	Tcl_DStringSetLength(&ds, strlen(dir));
	Tcl_DStringAppend(&ds, "/folder", 7);
	mbx_create(NIL, Tcl_DStringValue(&ds));
    }

    /*
     * Always update the master-file (the user may have changed some setting)
     */
    Tcl_DStringSetLength(&ds, strlen(dir));
    Tcl_DStringAppend(&ds, "/master", 7);
    fp = fopen(Tcl_DStringValue(&ds), "w");
    if (NULL == fp) {
	Tcl_DStringFree(&ds);
	return NULL;
    }
    lPtr = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewStringObj("Master", 6));
    Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewStringObj("imap", 4));
    Tcl_ListObjAppendElement(interp, lPtr, Tcl_NewObj());
    Tcl_ListObjAppendElement(interp, lPtr, objv[3]);
    Tcl_ListObjAppendElement(interp, lPtr, objv[4]);
    fprintf(fp, "%s\n%s\n", name, RatGetFolderSpec(interp, lPtr));
    Tcl_DecrRefCount(lPtr);
    fclose(fp);

    Tcl_DStringFree(&ds);
    return dir;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDisFolderOpenStream --
 *
 *      Opens the local part of a disconnected folder. This function may
 *	NOT be called while the folder is open.
 *
 * Results:
 *      A pointer to a MAILSTREAM or NULL on failures.
 *
 * Side effects:
 *	A disconnected folder is created.
 *
 *----------------------------------------------------------------------
 */

MAILSTREAM*
RatDisFolderOpenStream(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    static Tcl_DString ds;
    static int initialized = 0;
    const char *dir;
    MAILSTREAM *stream;

    if (initialized) {
	Tcl_DStringSetLength(&ds, 0);
    } else {
	Tcl_DStringInit(&ds);
	initialized = 1;
    }

    dir = PrepareDir(interp, defPtr);
    if (!dir) {
	return NULL;
    }

    /*
     * Open filefolder
     */
    Tcl_DStringAppend(&ds, dir, -1);
    Tcl_DStringAppend(&ds, "/folder", 7);
    stream = OpenStdFolder(interp, Tcl_DStringValue(&ds), NULL);
    return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_InitProc --
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
Dis_InitProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    /* Do nothing */
    (*disPtr->initProc)(infoPtr, interp, index);
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_CloseProc --
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
Dis_CloseProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int expunge)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    Tcl_HashEntry *entryPtr;
    int result;

    if (expunge) {
	CheckDeletion(infoPtr, interp);
    }
    result = (*disPtr->closeProc)(infoPtr, interp, expunge);
    entryPtr = Tcl_FindHashEntry(&openDisFolders, disPtr->dir);
    Tcl_DeleteHashEntry(entryPtr);
    WriteMappings(disPtr);
    Tcl_DeleteHashTable(&disPtr->map);
    ckfree(disPtr->dir);
    if (disPtr->master) {
	Std_StreamClose(interp, disPtr->master);
	disPtr->master = NULL;
    }
    ckfree(disPtr->spec);
    ckfree(disPtr);
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_UpdateProc --
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
Dis_UpdateProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp,RatUpdateType mode)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    int lastKnown;
    
    if (disPtr->master && 0 == disPtr->error) {
	disPtr->exists = lastKnown = infoPtr->number;
	disPtr->expunged = 0;
	if (RAT_SYNC == mode) {
	    mail_expunge(disPtr->master);
	} else {
	    mail_check(disPtr->master);
	}
	if (disPtr->exists != lastKnown-disPtr->expunged &&
	    0 == disPtr->error) {
	    MAILSTREAM *local =
		((StdFolderInfo*)disPtr->infoPtr->private)->stream;
	    char buf[1024];
	    FILE *fp;

	    /*
	     * Append new messages to local folder and uidmap
	     */
	    snprintf(buf, sizeof(buf), "%s/mappings", disPtr->dir);
	    fp = fopen(buf, "a");
	    if (NULL == fp) {
		return 0;
	    }
	    disPtr->lastUid = DisDownloadMsgs(disPtr->interp, disPtr->master,
					      local, &disPtr->error,
					      disPtr->dir, &disPtr->map, fp,
					      disPtr->lastUid, 0);
	    fclose(fp);
	}

	if (0 != disPtr->error) {
	    Std_StreamClose(interp, disPtr->master);
	    disPtr->master = NULL;
	    RatStdCheckNet(interp);
	}
    }
    if (RAT_SYNC == mode && 0 == disPtr->error) {
	CheckDeletion(infoPtr, interp);
	WriteMappings(disPtr);
    }

    return (*disPtr->updateProc)(infoPtr, interp, mode);
}

/*
 *----------------------------------------------------------------------
 *
 * Dis_InsertProc --
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
Dis_InsertProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int argc,
	char *argv[])
{
    MAILSTREAM *local = ((StdFolderInfo*)infoPtr->private)->stream;
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    char flags[128], date[128], buf[1024];
    Tcl_Obj *subject, *ref, *msgid;
    unsigned long localUid, us, ue;
    MessageInfo *msgPtr;
    Tcl_CmdInfo cmdInfo;
    Tcl_DString ds;
    int i, ret;
    FILE *fp;

    localUid = local->uid_last;
    ret = (*disPtr->insertProc)(infoPtr, interp, argc, argv);

    if (disPtr->master && argc) {
	Tcl_DStringInit(&ds);
	snprintf(buf, sizeof(buf), "%s/mappings", disPtr->dir);
	fp = fopen(buf, "a");
	for (i=us=ue=0; i<argc; i++) {
	    Tcl_GetCommandInfo(interp, argv[i], &cmdInfo);
	    msgPtr = (MessageInfo*)cmdInfo.objClientData;
	    RatMessageGet(interp, msgPtr, &ds, flags, sizeof(flags),
			  date, sizeof(date));
	    RatPurgeFlags(flags, 0);
	    subject = RatMsgInfo(interp, msgPtr, RAT_FOLDER_SUBJECT);
	    ref = RatMsgInfo(interp, msgPtr, RAT_FOLDER_REF);
	    msgid = RatMsgInfo(interp, msgPtr, RAT_FOLDER_MSGID);
	    ue = DisUploadMsg(disPtr->master, local, Tcl_GetString(subject),
			      Tcl_GetString(ref), Tcl_GetString(msgid), NULL,
			      ++localUid, &ds, date, flags, fp, &disPtr->map);
	    if (0 == i) {
		us = ue;
	    }
	    disPtr->mapChanged = 1;
	    if (T != mail_ping(disPtr->master)) {
		disPtr->master = NULL;
		break;
	    }	
	    Tcl_DStringSetLength(&ds, 0);
	}
	Tcl_DStringFree(&ds);
	if (disPtr->lastUid+1 < us) {
            DisDownloadMsgs(interp, disPtr->master, local,
			    &disPtr->error, disPtr->dir,
			    &disPtr->map, fp, disPtr->lastUid+1, us);
        }
	fclose(fp);
	disPtr->lastUid = ue;
    }
    return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_SetFlagProc --
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
Dis_SetFlagProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index,
	RatFlag flag, int value)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    FILE *fp = NULL;
    char buf[1024];
    unsigned long uid;

    uid = GetMasterUID(((StdFolderInfo*)infoPtr->private)->stream,
		       &disPtr->map, index);
    if (uid && disPtr->master) {
	snprintf(buf, sizeof(buf), "%ld", uid);
	if (value) {
	    mail_setflag_full(disPtr->master, buf, flag_name[flag].imap_name,
			      ST_UID);
	} else {
	    mail_clearflag_full(disPtr->master, buf, flag_name[flag].imap_name,
				ST_UID);
	}
    } else if (uid) {
	snprintf(buf, sizeof(buf), "%s/changes", disPtr->dir);
	if (NULL != (fp = fopen(buf, "a"))) {
	    fprintf(fp, "flag %ld %d %d\n", uid, flag, value);
	    fclose(fp);
	}
    }

    return (*disPtr->setFlagProc)(infoPtr, interp, index, flag, value);
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_GetFlagProc --
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
Dis_GetFlagProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index,
	RatFlag flag)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    /* Do Nothing */
    return (*disPtr->getFlagProc)(infoPtr, interp, index,flag);
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_InfoProc --
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
Dis_InfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index)
{
    /* Do Nothing */
    return Std_InfoProc(interp, clientData, type, index);
}

/*
 *----------------------------------------------------------------------
 *
 * Dis_CreateProc --
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
Dis_CreateProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp, int index)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    /* Do Nothing */
    return (*disPtr->createProc)(infoPtr, interp, index);
}

/*
 *----------------------------------------------------------------------
 *
 * Dis_SetInfoProc --
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
Dis_SetInfoProc(Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index, Tcl_Obj *oPtr)
{
    /* Do Nothing */
    Std_SetInfoProc(interp, clientData, type, index, oPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateDir --
 *
 *      Checks that a given directory exists and creates it and any
 *	parent directories if they do not exist. This routine expects
 *	a complete path starting with '/'.
 *
 * Results:
 *	Non zero if failed and sets errno.
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
static int
CreateDir(char *dir)
{
    struct stat sbuf;
    char *cPtr;

    /*
     * First we check if it already exists.
     */
    if (0 == stat(dir, &sbuf) && S_ISDIR(sbuf.st_mode)) {
	return 0;
    }

    /*
     * Go through all directories from the top and down and create those
     * which do not exist.
     */
    for (cPtr = strchr(dir+1, '/'); cPtr; cPtr = strchr(cPtr+1, '/')) {
	*cPtr = '\0';
	if (0 != stat(dir, &sbuf)) {
	    if (mkdir(dir, 0700)) {
		return 1;
	    }
	} else if (!S_ISDIR(sbuf.st_mode)) {
	    errno = ENOTDIR;
	    return 1;
	}
	*cPtr = '/';
    }
    if (0 != stat(dir, &sbuf)) {
	if (mkdir(dir, 0700)) {
	    return 1;
	}
    } else if (!S_ISDIR(sbuf.st_mode)) {
	errno = ENOTDIR;
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSyncDisconnected --
 *
 *	Synchronizes all disconnected folders
 *
 * Results:
 *	None
 *
 * Side effects:
 *	All disconnected folders are updated
 *
 *
 *----------------------------------------------------------------------
 */
static int
RatSyncDisconnected(ClientData op, Tcl_Interp *interp, int objc,
		    Tcl_Obj *const objv[])
{
    CONST84 char *dirname;

    if (NULL == (dirname = RatGetPathOption(interp, "disconnected_dir"))) {
	return TCL_ERROR;
    }
    Dis_FindAndSyncFolders(interp, dirname);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Dis_FindAndSyncFolders --
 *
 *	Recurses over a directory tree and calls Dis_SyncFolder for each
 *	found folder.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	All disconnected folders are updated
 *
 *
 *----------------------------------------------------------------------
 */
static void
Dis_FindAndSyncFolders(Tcl_Interp *interp, CONST84 char *dir)
{
    struct stat sbuf;
    char buf[1024];
    DIR *dirPtr;
    struct dirent *direntPtr;

    /*
     * Check if this is a folder directory (contains a master-file)
     */
    strlcpy(buf, dir, sizeof(buf)-7);
    strlcat(buf, "/master", sizeof(buf));
    if (0 == stat(buf, &sbuf) && S_ISREG(sbuf.st_mode)) {
	Dis_SyncFolder(interp, dir, sbuf.st_size, 0, NULL);
	return;
    }

    /*
     * Otherwise check all entries and call Dis_FindAndSyncFolders for all
     * descendants
     */
    if (NULL == (dirPtr = opendir(dir))) {
	return;
    }
    while (dirPtr && 0 != (direntPtr = readdir(dirPtr))) {
	snprintf(buf, sizeof(buf), "%s/%s", dir, direntPtr->d_name);
	if (0 != stat(buf, &sbuf)
		|| !S_ISDIR(sbuf.st_mode)
		|| !strcmp(".", direntPtr->d_name)
		|| !strcmp("..", direntPtr->d_name)) {
	    continue;
	}
	Dis_FindAndSyncFolders(interp, buf);
    }
    closedir(dirPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_SyncFolder --
 *
 *	Synchronizes a specified folder.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
static int
Dis_SyncFolder(Tcl_Interp *interp, CONST84 char *dir, off_t size, int force,
	       MAILSTREAM **master)
{
    char buf[1024], *name, *spec, *data, 
	    *header, *body, localMailbox[1024], datebuf[128], *cPtr;
    MESSAGECACHE *elt;
    unsigned long uid, msgno, lastUid, uidvalidity, len, nmsgs;
    MAILSTREAM *masterStream, *localStream;
    Tcl_HashTable *mapPtr;
    RatFolderInfoPtr infoPtr = NULL;
    DisFolderInfo *disPtr = NULL;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tcl_CmdInfo cmdInfo;
    ENVELOPE *envPtr;
    int fd, i, *masterErrorPtr, error;
    FILE *fp = NULL;
    Tcl_DString ds;
    RatUidMap *uidMap = NULL;
    
    Tcl_DStringInit(&ds);

    /*
     * Read and parse masterfile & statefile
     */
    snprintf(buf, sizeof(buf), "%s/master", dir);
    if (-1 == (fd = open(buf, O_RDONLY))
	    || NULL == (data = (char*)ckalloc(size+1))
	    || size != read(fd, data, size)
	    || 0 != close(fd)) {
	RatLogF(interp, RAT_ERROR, "Failed to read masterfile", RATLOG_TIME);
	if (master) {
	    *master = NULL;
	}
	return TCL_ERROR;
    }
    name = data;
    if (NULL == (spec = strchr(data, '\n'))) {
	if (master) {
	    *master = NULL;
	}
	return TCL_ERROR;
    }
    *spec++ = '\0';
    if (NULL == (cPtr = strchr(spec, '\n'))) {
	if (master) {
	    *master = NULL;
	}
	return TCL_ERROR;
    }
    *cPtr = '\0';
    snprintf(buf, sizeof(buf), "%s/state", dir);
    if (NULL == (fp = fopen(buf, "r"))
	    || 2 != fscanf(fp, "%ld\n%ld", &uidvalidity, &lastUid)
	    || 0 != fclose(fp)) {
	RatLog(interp, RAT_ERROR, "Failed to read statefile", RATLOG_TIME);
	if (master) {
	    *master = NULL;
	}
	return TCL_ERROR;
    }

    if (!force && Tcl_GetCommandInfo(interp, "RatUP_NetsyncFolder", &cmdInfo)){
	Tcl_Obj *oPtr = Tcl_NewObj();
	int doSync = 0;

	Tcl_ListObjAppendElement(interp, oPtr,
		Tcl_NewStringObj("RatUP_NetsyncFolder", -1));
	Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewStringObj(spec, -1));
	Tcl_IncrRefCount(oPtr);
	if (TCL_OK != Tcl_EvalObjEx(interp, oPtr, TCL_EVAL_GLOBAL)
		|| TCL_OK != Tcl_GetBooleanFromObj(interp,
			Tcl_GetObjResult(interp),&doSync)
		|| 0 == doSync) {
	    Tcl_DecrRefCount(oPtr);
	    ckfree(data);
	    if (master) {
		*master = NULL;
	    }
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(oPtr);
    }

    RatLogF(interp, RAT_INFO, "synchronizing", RATLOG_EXPLICIT, name);

    /*
     * Open connection
     * Check uidvalidity
     * Apply deletion commands and expunge
     * Apply flag commands
     * loop over messages in local folder
     *   Check if has master uid
     *   if YES check if still exists in master
     *     if NO delete
     *     if YES check flags and update from master
     *   if NO then append to master, should update with masters uid
     * Loop over new messages in master and append them to local
     */

    /*
     * Open connections
     */
    if (NULL != (entryPtr = Tcl_FindHashEntry(&openDisFolders, dir))) {
	infoPtr = Tcl_GetHashValue(entryPtr);
	strlcpy(localMailbox,
		   ((StdFolderInfo*)infoPtr->private)->stream->mailbox,
		   sizeof(localMailbox));
	disPtr = (DisFolderInfo*)infoPtr->private2;
	localStream = disPtr->local;
	masterStream = disPtr->master;
	masterErrorPtr = &disPtr->error;
	mapPtr = &disPtr->map;
	if (disPtr->lastUid) {
	    lastUid = disPtr->lastUid;
	}
    } else {
	snprintf(localMailbox, sizeof(localMailbox), "%s/folder", dir);
	masterStream = NIL;
	localStream = mail_open(NIL, localMailbox, NIL);
	mapPtr = (Tcl_HashTable*)ckalloc(sizeof(*mapPtr));
	Tcl_InitHashTable(mapPtr, TCL_ONE_WORD_KEYS);
	ReadMappings(localStream, dir, mapPtr);
	masterErrorPtr = &error;
    }
    if (!masterStream) {
	*masterErrorPtr = 0;
	masterStream = Std_StreamOpen(interp, spec, NIL, masterErrorPtr,
				      (disPtr ? &disPtr->handlers : NULL));
    }
    if (NULL == masterStream) {
	RatLog(interp, RAT_INFO, "", RATLOG_EXPLICIT);
	goto error;
    }
    if (disPtr && disPtr->spec) {
	spec = disPtr->spec;
    } else if (disPtr) {
	disPtr->spec = cpystr(spec);
    }

    if (!(0 == uidvalidity && 0 == lastUid)
	    && uidvalidity != masterStream->uid_validity) {
	RatLogF(interp, RAT_ERROR, "uidvalidity_changed", RATLOG_TIME);
	goto error;
    }

    /*
     * Apply deletion commands and expunge
     */
    RatLogF(interp, RAT_INFO, "uploading", RATLOG_EXPLICIT);
    snprintf(buf, sizeof(buf), "%s/changes", dir);
    if (NULL != (fp = fopen(buf, "r"))) {
	buf[sizeof(buf)-1] = '\0';
	while (fgets(buf, sizeof(buf)-1, fp), !feof(fp)) {
	    if (!strncmp(buf, "delete", 6)) {
		if (Tcl_DStringLength(&ds)) {
		    Tcl_DStringAppend(&ds, ",", 1);
		}
		sprintf(buf, "%ld", atol(buf+7));
		Tcl_DStringAppend(&ds, buf, -1);
	    }
	}
	if (Tcl_DStringLength(&ds)) {
	    mail_setflag_full(masterStream, Tcl_DStringValue(&ds),
		    flag_name[RAT_DELETED].imap_name, ST_UID);
	    mail_expunge(masterStream);
	}
	if (*masterErrorPtr) goto error;
    }

    /*
     * Build list of uids
     */
    uidMap = InitUidMap(masterStream);
    if (*masterErrorPtr) goto error;

    /*
     * Apply flag commands (and remove changes file)
     */
    if (NULL != fp) {
	RatFlag flag;
	int value;

	fseek(fp, 0, SEEK_SET);
	while (fgets(buf, sizeof(buf)-1, fp), !feof(fp)) {
	    if (!strncmp(buf, "flag", 4)) {
		sscanf(buf+5, "%ld %d %d", &uid, (int*)&flag, &value);
		sprintf(buf, "%ld", uid);
		if (0 == (msgno = MsgNo(uidMap, uid))) {
		    continue;
		}
		mail_fetchenvelope(masterStream, msgno);
		elt = mail_elt(masterStream, msgno);
		switch(flag) {
		case RAT_SEEN:
		    elt->seen = value;
		    break;
		case RAT_FLAGGED:
		    elt->flagged = value;
		    break;
		case RAT_DELETED:
		    elt->deleted = value;
		    break;
		case RAT_ANSWERED:
		    elt->answered = value;
		    break;
		case RAT_DRAFT:
		    elt->draft = value;
		    break;
		case RAT_RECENT:
		    break;
		}
		if (value) {
		    mail_setflag_full(masterStream, buf,
				      flag_name[flag].imap_name, ST_UID);
		} else {
		    mail_clearflag_full(masterStream, buf,
					flag_name[flag].imap_name, ST_UID);
		}
	    }
	}
	fclose(fp);
	if (*masterErrorPtr) goto error;
	snprintf(buf, sizeof(buf), "%s/changes", dir);
	unlink(buf);
    }

    /*
     * Download new messages
     */
    nmsgs = localStream->nmsgs;
    snprintf(buf, sizeof(buf), "%s/mappings", dir);
    fp = fopen(buf, "a");
    lastUid = DisDownloadMsgs(interp, masterStream, localStream,
			      masterErrorPtr, dir, mapPtr, fp, lastUid, 0);

    /*
     * Loop over messages and update
     */
    RatLogF(interp, RAT_INFO, "downloading_flags", RATLOG_EXPLICIT);
    for (i = 1; i <= nmsgs && !*masterErrorPtr; i++) {
	if ((uid = GetMasterUID(localStream, mapPtr, i-1))) {
	    msgno = MsgNo(uidMap, uid);
	    if (0 == msgno) {
		if (disPtr) {
		    UpdateFolderFlag(interp, disPtr, i, RAT_DELETED, 1);
		} else {
		    sprintf(buf, "%d", i);
		    mail_setflag(localStream, buf,
				 flag_name[RAT_DELETED].imap_name);
		}
		continue;
	    }
	    /*
	     * Update flags from master
	     */
	    envPtr = mail_fetchenvelope(masterStream, MsgNo(uidMap, uid));
	    elt = mail_elt(masterStream, MsgNo(uidMap, uid));
	    if (disPtr) {
		UpdateFolderFlag(interp,disPtr,i,RAT_SEEN,elt->seen);
		UpdateFolderFlag(interp,disPtr,i,RAT_DELETED,elt->deleted);
		UpdateFolderFlag(interp,disPtr,i,RAT_FLAGGED,elt->flagged);
		UpdateFolderFlag(interp,disPtr,i,RAT_ANSWERED,
			elt->answered);
		UpdateFolderFlag(interp,disPtr,i,RAT_DRAFT,elt->draft);
	    } else {
		MESSAGECACHE *lelt;

		lelt = mail_elt(localStream, i);
		sprintf(buf, "%d", i);
		if (elt->seen != lelt->seen) {
		    if (elt->seen) {
			mail_setflag(localStream, buf,
				     flag_name[RAT_SEEN].imap_name);
		    } else {
			mail_clearflag(localStream, buf,
				       flag_name[RAT_SEEN].imap_name);
		    }
		}
		if (elt->deleted != lelt->deleted) {
		    if (elt->deleted) {
			mail_setflag(localStream, buf,
				     flag_name[RAT_DELETED].imap_name);
		    } else {
			mail_clearflag(localStream, buf,
				       flag_name[RAT_DELETED].imap_name);
		    }
		}
		if (elt->flagged != lelt->flagged) {
		    if (elt->flagged) {
			mail_setflag(localStream, buf,
				     flag_name[RAT_FLAGGED].imap_name);
		    } else {
			mail_clearflag(localStream, buf,
				       flag_name[RAT_FLAGGED].imap_name);
		    }
		}
		if (elt->answered != lelt->answered) {
		    if (elt->answered) {
			mail_setflag(localStream, buf,
				     flag_name[RAT_ANSWERED].imap_name);
		    } else {
			mail_clearflag(localStream, buf,
				       flag_name[RAT_ANSWERED].imap_name);
		    }
		}
		if (elt->draft != lelt->draft) {
		    if (elt->draft) {
			mail_setflag(localStream, buf,
				     flag_name[RAT_DRAFT].imap_name);
		    } else {
			mail_clearflag(localStream, buf,
				       flag_name[RAT_DRAFT].imap_name);
		    }
		}
	    }
	} else {
	    /*
	     * Append the message to the master stream
	     */
	    Tcl_DStringSetLength(&ds, 0);
	    header = mail_fetchheader(localStream, i);
	    Tcl_DStringAppend(&ds, header, -1);
	    body = mail_fetchtext_full(localStream, i, &len, FT_PEEK);
	    Tcl_DStringAppend(&ds, body, len);
	    elt = mail_elt(localStream, i);
	    mail_date(datebuf, elt);
	    envPtr = mail_fetch_structure(localStream, i, NULL, 0);
	    lastUid = DisUploadMsg(masterStream, localStream, envPtr->subject,
				   envPtr->in_reply_to, envPtr->message_id,
				   envPtr->date, mail_uid(localStream, i), &ds,
				   datebuf, MsgFlags(elt), fp, mapPtr);
	    if (disPtr) {
		disPtr->mapChanged = 1;
	    }
	}
    }
    fclose(fp);

    /*
     * Update state file
     */
    snprintf(buf, sizeof(buf), "%s/state", dir);
    fp = fopen(buf, "w");
    fprintf(fp, "%ld\n%ld\n", masterStream->uid_validity, lastUid);
    fclose(fp);

    /*
     * Cleanup
     */
    if (!disPtr) {
	mail_close(localStream);
	for (entryPtr = Tcl_FirstHashEntry(mapPtr, &search); entryPtr;
		entryPtr = Tcl_NextHashEntry(&search)) {
	    ckfree(Tcl_GetHashValue(entryPtr));
	}
	Tcl_DeleteHashTable(mapPtr);
	ckfree(mapPtr);
	Std_StreamClose(interp, masterStream);
	masterStream = NULL;
    } else {
	Tcl_Obj *oPtr;
	int online;
	
	/*	
	 * If we are offline, then we should close the masterStream
	 */
	oPtr = Tcl_GetVar2Ex(interp, "option", "online", TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &online);
	if (!online) {
	    Std_StreamClose(interp, disPtr->master);
	    disPtr->master = NULL;
	}
    }
	
    RatLog(interp, RAT_INFO, "", RATLOG_EXPLICIT);
    ckfree(data);
    Tcl_DStringFree(&ds);

    /*
     * Trigger an update of the folder as well (if we are active)
     */
    if (disPtr) {
	disPtr->lastUid = lastUid;
	RatUpdateFolder(interp, disPtr->infoPtr, RAT_UPDATE);
    }
    FreeUidMap(uidMap);
    if (master) {
	*master = masterStream;
    }
    return TCL_OK;

error:
    RatLog(interp, RAT_INFO, "", RATLOG_EXPLICIT);
    if (!disPtr) {
	mail_close(localStream);
	for (entryPtr = Tcl_FirstHashEntry(mapPtr, &search); entryPtr;
		entryPtr = Tcl_NextHashEntry(&search)) {
	    ckfree(Tcl_GetHashValue(entryPtr));
	}
	Tcl_DeleteHashTable(mapPtr);
	ckfree(mapPtr);
    }
    if (masterStream) {
	Std_StreamClose(interp, masterStream);
	if (disPtr) {
	    disPtr->master = NULL;
	}
    }
    if (master) {
	*master = NULL;
    }
    if (uidMap) {
	FreeUidMap(uidMap);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DisDownloadMsgs
 *
 *	Downloads a new message from the master folder to the local
 *	folder.
 *
 * Results:
 *	Last uid
 *
 * Side effects:
 *	The uidmap is updated (if one exists)
 *
 *
 *----------------------------------------------------------------------
 */

static unsigned long
DisDownloadMsgs(Tcl_Interp *interp, MAILSTREAM *masterStream,
		MAILSTREAM *localStream, int *masterErrorPtr,
		CONST84 char *dir, Tcl_HashTable *mapPtr, FILE *mapFp,
		unsigned long startAfterUid, unsigned long stopBeforeUid)
{

    ENVELOPE *envPtr;
    MESSAGECACHE *elt;
    unsigned long len, uid;
    char *body, *header, datebuf[128], statebuf[1024];
    Tcl_DString ds;
    STRING string;
    SEARCHPGM *pgm;
    int i;
    FILE *stateFp;

    if (0 == masterStream->nmsgs) {
	return masterStream->uid_last;
    }
    snprintf(statebuf, sizeof(statebuf), "%s/state", dir);

    pgm = mail_newsearchpgm();
    if (0 == stopBeforeUid) {
	stopBeforeUid = mail_uid(masterStream, masterStream->nmsgs)+1;
    }
    pgm->uid = mail_newsearchset();
    pgm->uid->first = startAfterUid+1;
    pgm->uid->last = stopBeforeUid;
    searchResultNum = 0;
    mail_search_full(masterStream, NULL, pgm, SE_FREE);
    for (i = 0; i < searchResultNum; i++) {
	RatLogF(interp, RAT_INFO, "downloading", RATLOG_EXPLICIT, i+1,
		searchResultNum);
	envPtr = mail_fetchenvelope(masterStream, searchResultPtr[i]);
	if (*masterErrorPtr) goto done;
	elt = mail_elt(masterStream, searchResultPtr[i]);
	if (*masterErrorPtr) goto done;
	body = mail_fetchtext_full(masterStream, searchResultPtr[i], &len,
				   FT_PEEK);
	if (*masterErrorPtr) goto done;
	header = mail_fetchheader(masterStream, searchResultPtr[i]);
	if (*masterErrorPtr) goto done;
	if (!body || !header) continue;
	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, header, -1);
	Tcl_DStringAppend(&ds, body, len);
	INIT(&string, mail_string, Tcl_DStringValue(&ds),
	     Tcl_DStringLength(&ds));
	mail_date(datebuf, elt);
	mail_append_full(localStream, localStream->mailbox,
			 RatPurgeFlags(MsgFlags(elt), 0), datebuf, &string);
	Tcl_DStringFree(&ds);
	uid = mail_uid(masterStream, searchResultPtr[i]);
	fprintf(mapFp, "%ld %ld\n", uid, ++localStream->uid_last);
	masterStream->uid_last = uid;
	stateFp = fopen(statebuf, "w");
	fprintf(stateFp, "%ld\n%ld\n", masterStream->uid_validity, uid);
	fclose(stateFp);
	if (mapPtr) {
	    unsigned long *lPtr = (unsigned long*)ckalloc(sizeof(*lPtr));
	    Tcl_HashEntry *entryPtr;
	    int unused;

	    *lPtr = uid;
	    entryPtr = Tcl_CreateHashEntry(mapPtr,
					   (char*)(localStream->uid_last),
					   &unused);
	    Tcl_SetHashValue(entryPtr, lPtr);
	}
    }
 done:
    RatLog(interp, RAT_INFO, "", RATLOG_EXPLICIT);    
    return masterStream->uid_last;
}


/*
 *----------------------------------------------------------------------
 *
 * GetMasterUID
 *
 *	Returns the UID in the master folder of the specified message
 *
 * Results:
 *	The UID is returned or zero if the message is not present
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
static unsigned long
GetMasterUID(MAILSTREAM *s, Tcl_HashTable *mapPtr, int index)
{
    Tcl_HashEntry *entryPtr;

    if ((entryPtr = Tcl_FindHashEntry(mapPtr, (char*)mail_uid(s, index+1)))) {
	return *((unsigned long*)Tcl_GetHashValue(entryPtr));
    } else {
	return 0;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * UpdateFolderFlag --
 *
 *	Synchronizes a flag with master
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
UpdateFolderFlag(Tcl_Interp *interp, DisFolderInfo *disPtr,
	int index, RatFlag flag, int value)
{
    int local;
    
    local = (*disPtr->getFlagProc)(disPtr->infoPtr, interp, index-1, flag);
    if (value == local) {
	return;
    }
    (*disPtr->setFlagProc)(disPtr->infoPtr, interp, index-1, flag, value);
}


/*
 *----------------------------------------------------------------------
 *
 * ReadMappings --
 *
 *	Reads the mappings-file into the given hash-table
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
ReadMappings(MAILSTREAM *s, const char *dir, Tcl_HashTable *mapPtr)
{
    Tcl_HashEntry *entryPtr;
    char buf[1024];
    int unused;
    unsigned long *lPtr, uid;
    FILE *fp;

    snprintf(buf, sizeof(buf), "%s/mappings", dir);
    if (NULL != (fp = fopen(buf, "r"))) {
	buf[sizeof(buf)-1] = '\0';
	while(fgets(buf, sizeof(buf)-1, fp), !feof(fp)) {
	    if (strchr(buf, '<')) {
		ReadOldMappings(s, mapPtr, buf, sizeof(buf)-1, fp);
		break;
	    }
	    buf[strlen(buf)-1] = '\0';
	    uid = atol(strchr(buf, ' '));
	    entryPtr = Tcl_CreateHashEntry(mapPtr, (char*)uid, &unused);
	    lPtr = (unsigned long*)ckalloc(sizeof(unsigned long));
	    *lPtr = atol(buf);
	    Tcl_SetHashValue(entryPtr, lPtr);
	}
	fclose(fp);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * ReadOldMappings --
 *
 *	Reads the old style mappings-file into the given hash-table
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
ReadOldMappings(MAILSTREAM *s, Tcl_HashTable *mapPtr, char *buf, int buflen,
	FILE *fp)
{
    Tcl_HashTable tmap;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    unsigned long *lPtr, l, uid;
    ENVELOPE *envPtr;
    int unused;

    /*
     * Read file into local hash-table tmap
     */
    Tcl_InitHashTable(&tmap, TCL_STRING_KEYS);
    do {
	buf[strlen(buf)-1] = '\0';
	entryPtr = Tcl_CreateHashEntry(&tmap, strchr(buf, '<'), &unused);
	lPtr = (unsigned long*)ckalloc(sizeof(unsigned long));
	*lPtr = atol(buf);
	Tcl_SetHashValue(entryPtr, lPtr);
    } while (fgets(buf, buflen, fp), !feof(fp));

    /*
     * Loop through folder and add the new mappings to the real map
     */
    for (l=1; l <= s->nmsgs; l++) {
	envPtr = mail_fetch_structure(s, l, NIL, 0);
	entryPtr = Tcl_FindHashEntry(&tmap, envPtr->message_id);
	if (entryPtr) {
	    uid = *(unsigned long*)Tcl_GetHashValue(entryPtr);
	    entryPtr = Tcl_CreateHashEntry(mapPtr, (char*)mail_uid(s, l),
		    &unused);
	    lPtr = (unsigned long*)ckalloc(sizeof(unsigned long));
	    *lPtr = uid;
	    Tcl_SetHashValue(entryPtr, lPtr);
	}
    }

    /*
     * Free the temporary hashtable from memory
     */
    for (entryPtr = Tcl_FirstHashEntry(&tmap, &search); entryPtr;
	    entryPtr = Tcl_NextHashEntry(&search)) {
	ckfree(Tcl_GetHashValue(entryPtr));
    }
    Tcl_DeleteHashTable(&tmap);
}


/*
 *----------------------------------------------------------------------
 *
 * RatDisFolderDir --
 *
 *	Calculates the directory name of a disconnected folder and
 *	makes sure the directory exists.
 *
 * Results:
 *	A pointer to a static area containign the name
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */
char*
RatDisFolderDir(Tcl_Interp *interp, Tcl_Obj *defPtr)
{
    static Tcl_DString ds;
    static int initialized = 0;
    CONST84 char *dir;
    int objc, mobjc;
    Tcl_Obj **objv, **mobjv;

    if (!initialized) {
	Tcl_DStringInit(&ds);
    } else {
	Tcl_DStringSetLength(&ds, 0);
    }

    if (NULL == (dir = RatGetPathOption(interp, "disconnected_dir"))) {
	return NULL;
    }
    Tcl_ListObjGetElements(interp, defPtr, &objc, &objv);
    Tcl_ListObjGetElements(interp,
			   Tcl_GetVar2Ex(interp, "mailServer",
					 Tcl_GetString(objv[3]),
					 TCL_GLOBAL_ONLY),
			   &mobjc, &mobjv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, dir, -1);
    Tcl_DStringAppend(&ds, "/", 1);
    Tcl_DStringAppend(&ds,Tcl_GetString(mobjv[0]),Tcl_GetCharLength(mobjv[0]));
    Tcl_DStringAppend(&ds, ":", 1);
    if (Tcl_GetCharLength(mobjv[1])) {
	Tcl_DStringAppend(&ds, Tcl_GetString(mobjv[1]),
			  Tcl_GetCharLength(mobjv[1]));
    } else {
	Tcl_DStringAppend(&ds, "143", 3);
    }
    Tcl_DStringAppend(&ds, "/", 1);
    if (Tcl_GetCharLength(objv[4])) {
	Tcl_DStringAppend(&ds, Tcl_GetString(objv[4]),
			  Tcl_GetCharLength(objv[4]));
    } else {
	Tcl_DStringAppend(&ds, "INBOX", 5);
    }
    Tcl_DStringAppend(&ds, "+", 1);
    Tcl_DStringAppend(&ds,Tcl_GetString(mobjv[3]),Tcl_GetCharLength(mobjv[3]));
    Tcl_DStringAppend(&ds, "+imap", 5);
    if (CreateDir(Tcl_DStringValue(&ds))) {
	return NULL;
    }
    return Tcl_DStringValue(&ds);
}


/*
 *----------------------------------------------------------------------
 *
 * Dis_SyncProc --
 *
 *	Synchronizes the specified folder
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The folder may be modified
 *
 *
 *----------------------------------------------------------------------
 */
static int
Dis_SyncProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    struct stat sbuf;
    char buf[1024];

    snprintf(buf, sizeof(buf), "%s/master", disPtr->dir);
    stat(buf, &sbuf);
    return Dis_SyncFolder(interp, disPtr->dir, sbuf.st_size, 1, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * InitUidMap --
 *
 *	initializes the uid map
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The uidMap structure is initialized
 *
 *----------------------------------------------------------------------
 */
static RatUidMap*
InitUidMap(MAILSTREAM *s)
{
    unsigned long i;
    RatUidMap *uidMap;

    uidMap = (RatUidMap*)ckalloc(sizeof(RatUidMap));
    uidMap->allocated = s->nmsgs+32;
    uidMap->map = (unsigned long*)
	ckalloc(uidMap->allocated*sizeof(unsigned long));
    uidMap->size = s->nmsgs;
    for (i=0; i<s->nmsgs; i++) {
	uidMap->map[i] = mail_uid(s, i+1);
    }
    return uidMap;
}


/*
 *----------------------------------------------------------------------
 *
 * FreeUidMap --
 *
 *	Frees the uid map
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
static void
FreeUidMap(RatUidMap *uidMap)
{
    ckfree(uidMap->map);
    ckfree(uidMap);
}


/*
 *----------------------------------------------------------------------
 *
 * MsgNo --
 *
 *	Lookup a message uid in the map and return the msgno
 *
 * Results:
 *	The msgno for the given uid or 0 if there is no such message
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
static unsigned long
MsgNo(RatUidMap *uidMap, unsigned long uid)
{
    unsigned long i;

    for (i=0; i<uidMap->size; i++) {
	if (uidMap->map[i] == uid) {
	    return i+1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CheckDeletion --
 *
 *      Checks which messages are going to be deleted before an expunge
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Appends things to the changes file
 *
 *----------------------------------------------------------------------
 */
static void
CheckDeletion(RatFolderInfoPtr infoPtr, Tcl_Interp *interp)
{
    DisFolderInfo *disPtr = (DisFolderInfo*)infoPtr->private2;
    Tcl_HashEntry *entryPtr;
    FILE *fp = NULL;
    char buf[1024];
    unsigned long uid;
    int i;

    for (i = 0; i < infoPtr->number; i++) {
	if (0 != (*disPtr->getFlagProc)(infoPtr, interp, i, RAT_DELETED)) {
	    if (NULL == fp) {
		snprintf(buf, sizeof(buf), "%s/changes", disPtr->dir);
		fp = fopen(buf, "a");
	    }
	    uid = GetMasterUID(((StdFolderInfo*)infoPtr->private)->stream,
		    &disPtr->map, i);
	    if (uid && NULL != fp) {
		fprintf(fp, "delete %ld\n", uid);
	    }
	    entryPtr = Tcl_FindHashEntry(&disPtr->map, (char*)
		    mail_uid(((StdFolderInfo*)infoPtr->private)->stream,
			     i+1));
	    if (entryPtr) {
		disPtr->mapChanged = 1;
		ckfree(Tcl_GetHashValue(entryPtr));
		Tcl_DeleteHashEntry(entryPtr);
	    }
	}
    }
    if (NULL != fp) {
	fclose(fp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DisUploadMsg --
 *
 *      Uploads the given message to the master folder
 *
 * Results:
 *	The uid of the newly uploaded messages
 *
 * Side effects:
 *	Modifies master folder
 *
 *
 *----------------------------------------------------------------------
 */

static unsigned long
DisUploadMsg(MAILSTREAM *masterStream, MAILSTREAM *localStream,
	     const char *subject, const char *in_reply_to,
	     const char *message_id, char *envdate,
	     unsigned long local_uid,
	     Tcl_DString *message, char *date,
	     char *flags, FILE *mapFP, Tcl_HashTable *mapPtr)
{
    Tcl_HashEntry *entryPtr;
    SEARCHPGM *pgm;
    STRING string;
    unsigned long *lPtr, uid;
    int unused;

    uid = masterStream->uid_last;
    
    INIT(&string, mail_string, Tcl_DStringValue(message),
	 Tcl_DStringLength(message));
    RatPurgeFlags(flags, 0);
    mail_append_full(masterStream, masterStream->mailbox, flags, date,&string);

    pgm = mail_newsearchpgm();
    if (subject && *subject) {
	pgm->subject = mail_newstringlist();
	pgm->subject->text.data = (unsigned char*)cpystr(subject);
	pgm->subject->text.size = strlen(subject);
    }
    if (in_reply_to && *in_reply_to) {
	pgm->in_reply_to = mail_newstringlist();
	pgm->in_reply_to->text.data = (unsigned char*)cpystr(in_reply_to);
	pgm->in_reply_to->text.size = strlen(in_reply_to);
    }
    if (message_id && *message_id) {
	pgm->message_id = mail_newstringlist();
	pgm->message_id->text.data = (unsigned char*)cpystr(message_id);
	pgm->message_id->text.size = strlen(message_id);
    }
    pgm->uid = mail_newsearchset();
    pgm->uid->first = uid+1;
    pgm->uid->last = 0;
    if (envdate && *envdate) {
	pgm->header = mail_newsearchheader("date", envdate);
    }
    searchResultNum = 0;
    mail_search_full(masterStream, NULL, pgm, SE_FREE|SE_UID);
    if (searchResultNum == 1) {
	fprintf(mapFP, "%ld %ld\n", searchResultPtr[0], local_uid);
	if (mapPtr) {
	    lPtr = (unsigned long*)ckalloc(sizeof(*lPtr));
	    *lPtr = searchResultPtr[0];
	    entryPtr = Tcl_CreateHashEntry(mapPtr, (char*)local_uid, &unused);
	    Tcl_SetHashValue(entryPtr, lPtr);
	}
	masterStream->uid_last = searchResultPtr[0];
	return searchResultPtr[0];
    } else {
	return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Handle* --
 *
 *      Handle events from mailbox
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
Dis_HandleExists(void *state, unsigned long nmsg)
{ 
    DisFolderInfo *disPtr = (DisFolderInfo *) state;
    disPtr->exists = nmsg;
}

static void
Dis_HandleExpunged(void *state, unsigned long index)
{ 
    DisFolderInfo *disPtr = (DisFolderInfo *) state;

    disPtr->expunged++;
}

/*
 *----------------------------------------------------------------------
 *
 * WriteMappings --
 *
 *      Writes the mappings-file
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
WriteMappings(DisFolderInfo *disPtr)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    unsigned long *lPtr;
    char buf[1024];
    FILE *fp;
    
    if (!disPtr->mapChanged) {
	return;
    }
    snprintf(buf, sizeof(buf), "%s/mappings", disPtr->dir);
    fp = fopen(buf, "w");
    for (entryPtr = Tcl_FirstHashEntry(&disPtr->map, &search); entryPtr;
	    entryPtr = Tcl_NextHashEntry(&search)) {
	lPtr = (unsigned long*)Tcl_GetHashValue(entryPtr);
	fprintf(fp, "%ld %ld\n", *lPtr,
		    (unsigned long)Tcl_GetHashKey(&disPtr->map, entryPtr));
    }
    fclose(fp);
    disPtr->mapChanged = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDisOnOffTrans	--
 *
 *      Handle transitions between online and offline state
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Opens/closes folders
 *
 *
 *----------------------------------------------------------------------
 */
int
RatDisOnOffTrans(Tcl_Interp *interp, int newState)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    RatFolderInfo *infoPtr;
    DisFolderInfo *disPtr;
    char buf[1024];
    struct stat sbuf;
    int count = 0, allfail = 1;

    for (entryPtr = Tcl_FirstHashEntry(&openDisFolders, &search);
	 entryPtr;
	 entryPtr = Tcl_NextHashEntry(&search), count++) {
	infoPtr = Tcl_GetHashValue(entryPtr);
	disPtr = (DisFolderInfo*)infoPtr->private2;
	
	if (newState && !disPtr->master) {
	    /* Go online */
	    snprintf(buf, sizeof(buf), "%s/master", disPtr->dir);
	    stat(buf, &sbuf);
	    if (TCL_OK == Dis_SyncFolder(interp,disPtr->dir, sbuf.st_size,
					 1, &disPtr->master)) {
		allfail = 0;
	    }
	    
	} else if (!newState && disPtr->master) {
	    /* Go offline */
	    Std_StreamClose(interp, disPtr->master);
	    disPtr->master = NULL;
	    allfail = 0;
	}
    }
    if (!newState) {
	/*
	 * Force closing of all pending connections
	 */
	Std_StreamCloseAllCached(interp);
    }
    if (allfail && 0 != count) {
	return TCL_ERROR;
    } else {
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatDisManageFolder --
 *
 *      Create or delete folders
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
void
RatDisManageFolder(Tcl_Interp *interp, RatManagementAction op, Tcl_Obj *fPtr)
{
    struct dirent *dirent;
    const char *dirname;
    char buf[1024];
    DIR *dir;
    int i;

    if (NULL == (dirname = PrepareDir(interp, fPtr))) {
	return;
    }
    if (RAT_MGMT_DELETE == op) {
	if (NULL == (dir = opendir(dirname))) {
	    return;
	}
	while (NULL != (dirent = readdir(dir))) {
	    if (!strcmp(".", dirent->d_name) || !strcmp("..", dirent->d_name)){
		continue;
	    }
	    snprintf(buf, sizeof(buf), "%s/%s", dirname, dirent->d_name);
	    unlink(buf);
	}
	closedir(dir);
	i = rmdir(dirname);
    }
}
