/*
 * ratDbase.c --
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 *
 *	This file contains support for a database of messages. This file
 *	uses version 4 of the database. The format of the database is as
 *	follows:
 *
 *	The database directory contains the following entries:
 *	  index		The main database index. This file consists
 *			of a number of entries separated by newline and
 *			each entry has the following format:
 *				To
 *				From
 *				Cc
 *				Message-Id
 *				References
 *				Subject
 *				Date (UNIX time_t as a string)
 *				Keywords (SPACE separated list)
 *				Size
 *				Status
 *				Expiration time (UNIX time_t as a string)
 *				Expiration event (*)
 *				Filename
 *			Expiration event is one of none, remove, incoming,
 *			backup and custom. Custom is followed by the custom
 *			command.
 *	  index.info	This file contains information about the database.
 *			It contains two integers. The first is the version
 *			(in this case 3) and the second is the number of
 *			entries in the indexfile.
 *	  index.changes	This file contains a log of changes made to the
 *			index file. This log is only kept if the multiple
 *			agents has opened the database. In this file each
 *			entry is one line. There are addition entries;
 *			'a OFFSET' where OFFSET is the position in the
 *			index file where this entry starts. There are also
 *			deletion entries; 'd INDEX'. Finally there are
 *			the status changes; they are of the form:
 *			's INDEX STATUS' where STATUS is the new status of
 *			the specified message.
 *	  lock		If this file exists the database is locked and
 *			no other agent may do anything with it. It should
 *			contain a string identifying the agent owning the
 *			lock.
 *	  rlock.*	Each agent that opens the database should construct
 *			a file with the following name: rlock.HOST:PID .
 *			This file should be touched at least once every
 *			hour.
 *	  dbase/	This is a directory which holds a number of
 *			directories which in turn holds the actual messages.
 *
 *	In the dbase directory messages are stored as recipient-name/number.
 *	Where the last number taken in a recipient-name directory is
 *	contained in a .seq file found in said directory.
 *	All entries are stored in utf-8
 */

#include "ratFolder.h"

#define DBASE_VERSION 5		/* Version of the database format */
#define EXTRA_ENTRIES 100	/* How many extra entries we should allocate
				 * room for when allocating the entryPtr
				 * array */
#define RLOCK_TIMEOUT 2*60*60	/* Actual timeout time for rlock files */
#define UPDATE_INTERVAL 40*60	/* Time between updates to the rlock file */

static int isRead = 0;		/* 0 means that the database hasn't been
				 * read yet */
static int numRead = 0;		/* The number of entries in the entryPtr list*/
static int numAlloc;		/* The number of entries that will fit
				 * into the entryPtr list */
static RatDbEntry *entryPtr;	/* The list of entries in the database */
static char *dbDir = 0;		/* Full path to the database directory */
static char *ident = 0; 	/* String which is used to identify us.
				 * It is of the form hostname:pid */
static int changeSize = 0;	/* The number of bytes in the index.changes
				 * file that we have read and incorporated
				 * into our memory resident database */
static int needRewrite = 0;	/* 1 if we need to rewrite the indexfile */
static int numChanges = 0;	/* Number of changes in the changes file */
static int version;		/* Version read */
static long firstDate = 0;      /* The earliest date in the dbase */
static long lastDate = 0;       /* The last date in the dbase */
static long totSize = 0;        /* Total size of dbase file */

/*
 * This structure is used while checking the dbase
 */
typedef struct {
    int fileSize;	/* The actual size of the file */
    int index;		/* Index in the list that handles this entry */
    RatDbEntry entry;	/* The actual entry in the index file */
} RatDbItem;

/* XXX assiging these values to variable of type RatDbEType (enum) is iffy */
#define SEARCH_ALL           -1
#define SEARCH_ALL_ADDRESSES -2
#define SEARCH_TIME_FROM     -3
#define SEARCH_TIME_TO       -4

/*
 * Forward declarations for procedures defined in this file:
 */
static int	Read(Tcl_Interp *interp);
static void	Lock(Tcl_Interp *interp);
static void	Unlock(Tcl_Interp *interp);
static int	Sync(Tcl_Interp *interp, int force);
static void	Update(ClientData clientData);
static int	IsRlocked(char *ignore);
static void	RatDbBuildList(Tcl_Interp *interp, Tcl_DString *dsPtr,
	char *prefix, char *dir, Tcl_HashTable *tablePtr, int fix);
static int	NoLFPrint(FILE *fp, const char *s);
static void	DbaseConvert3to4(Tcl_Interp *interp);


/*
 *----------------------------------------------------------------------
 *
 * Read --
 *
 *      Reads the database from disk into memory.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *      The internal list of entries is allocated and initalized. An
 *	rlock-file is created to indicate that we have the database
 *	open. To keep this lock up to date the Update() procedure must
 *	be called at least once every hour. When the agent won't access
 *	the database anymore it must be closed with a call to RatDbClose().
 *
 *
 *----------------------------------------------------------------------
 */

static int
Read(Tcl_Interp *interp)
{
    char buf[1024];	/* Scratch area */
    int size;		/* Size of indexfile */
    struct stat sbuf;	/* Buffer for stat() calls */
    int fhIndex;	/* File handle for index-file */
    int fhReadlock;	/* File handle for read lock file */
    FILE *fpIndexinfo;	/* File pointer for index.info file */
    int i, j;		/* Loop variables */
    char *cPtr;		/* Running pointer */
    long l;             /* Long value */

    /*
     * First make sure we know where the database should reside and which
     * identifier we should use.
     */
    if (0 == dbDir) {
	const char *value = RatGetPathOption(interp, "dbase_dir");
	if (NULL == value) {
	    return TCL_ERROR;
	}
	dbDir = cpystr(value);
    }
    if (0 == ident) {
	gethostname(buf, sizeof(buf));
	ident = (char*)ckalloc(strlen(buf)+16);
	snprintf(ident, strlen(buf)+16, "%s:%d", buf, (int)getpid());
    }

    /*
     * Check if the database actually exists. If it doesn't then we
     * must create the needed directories and files.
     */
    snprintf(buf, sizeof(buf), "%s/index", dbDir);
    if (       (0 != stat(dbDir, &sbuf) && ENOENT == errno)
	    || (0 != stat(buf, &sbuf) && ENOENT == errno)) {
	if (0 != mkdir(dbDir, DIRMODE) && EEXIST != errno) {
	    Tcl_AppendResult(interp, "error creating directory \"", dbDir,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	snprintf(buf, sizeof(buf), "%s/dbase", dbDir);
	if (0 != mkdir(buf, DIRMODE) && EEXIST != errno) {
	    Tcl_AppendResult(interp, "error creating directory \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}

	Lock(interp);

	snprintf(buf, sizeof(buf), "%s/index", dbDir);
	if (0 > (fhIndex = open(buf, O_CREAT|O_WRONLY, FILEMODE))
		|| 0 != close(fhIndex)) {
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error creating file \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	snprintf(buf, sizeof(buf), "%s/index.info", dbDir);
	if (0 == (fpIndexinfo = fopen(buf, "w"))) {
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error creating file \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	if (0 > fprintf(fpIndexinfo, "%d 0\n", DBASE_VERSION)) {
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error writing to file \"", buf, "\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (0 != fclose(fpIndexinfo)) {
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error closing file \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    } else {
	Lock(interp);
    }

    /*
     * Create rlock file.
     */
    snprintf(buf, sizeof(buf), "%s/rlock.%s", dbDir, ident);
    if (0 > (fhReadlock = open(buf, O_CREAT|O_WRONLY, FILEMODE))
	    || 0 != close(fhReadlock)) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error creating file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    (void)Tcl_CreateTimerHandler(UPDATE_INTERVAL*1000,Update,(ClientData)NULL);

    /*
     * Read the index.info file
     */
    snprintf(buf, sizeof(buf), "%s/index.info", dbDir);
    if (0 == (fpIndexinfo = fopen(buf, "r"))) {
	Tcl_AppendResult(interp, "error opening file (for reading)\"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    if ( 2 != fscanf(fpIndexinfo, "%d %d", &version, &numRead)) {
	Tcl_SetResult(interp, "index.info file corrupt", TCL_STATIC);
	fclose(fpIndexinfo);
	goto error;
    }
    fclose(fpIndexinfo);

    /*
     * Check if this is the current version of the database. If not
     * complain!
     */
    if (version != DBASE_VERSION && version != 3) {
	snprintf(buf, sizeof(buf),
		 "wrong version of database got %d expected %d", version,
		 DBASE_VERSION);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	goto error;
    }

    /*
     * Read the indexfile and build internal data structures
     */
    snprintf(buf, sizeof(buf), "%s/index", dbDir);
    if (0 != stat(buf, &sbuf)) {
	Tcl_AppendResult(interp, "error stating file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    totSize = size = sbuf.st_size;
    if (size > 0) {
	char *indexPtr;	/* Pointer to read version of the indexfile */

	/*
	 * We should not free this value since we never close the database.
	 * Purify will note it as a leak, but we will have lots of
	 * pointers into this data.
	 */
	if ( 0 == (indexPtr = (char*)ckalloc(size))) {
	    Tcl_SetResult(interp, "failed to allocate memory for index",
		    TCL_STATIC);
	    goto error;
	}
	fhIndex = open(buf, O_RDONLY);
	if (size != SafeRead(fhIndex, indexPtr, size)) {
	    Tcl_SetResult(interp, "error reading index", TCL_STATIC);
	    close(fhIndex);
	    goto error;
	}
	close(fhIndex);
	entryPtr = (RatDbEntry*)ckalloc((numRead+EXTRA_ENTRIES) *
		sizeof(RatDbEntry));
	numAlloc = numRead+EXTRA_ENTRIES;

	cPtr = indexPtr;
	for (i=0; i<numRead; i++) {
	    for (j=0; j<RATDBETYPE_END; j++) {
		entryPtr[i].content[j] = cPtr;
		while (cPtr <= &indexPtr[size] && *cPtr != '\n') {
		    cPtr++;
		}
		if (cPtr > &indexPtr[size]) {
		    Tcl_SetResult(interp, "error in index-file", TCL_STATIC);
		    ckfree(indexPtr);
		    goto error;
		}
		*cPtr++ = '\0';
	    }
            totSize += atol(entryPtr[i].content[RSIZE]);
	    /*
	     * This is a KLUDGE to work around a bug which existed for a
	     * short time /MaF 960218
	     */
	    if ('+' == entryPtr[i].content[EX_TYPE][0]) {
		char buf[64];
		sprintf(buf, "%ld",
			atoi(entryPtr[i].content[EX_TYPE])*24*60*60+
			atol(entryPtr[i].content[DATE]));
		entryPtr[i].content[EX_TIME] = cpystr(buf);
		entryPtr[i].content[EX_TYPE] = "backup";
	    }
            l = atol(entryPtr[i].content[DATE]);
            if (l > 0 && (l < firstDate || 0 == firstDate)) {
                firstDate = l;
            }
            if (l > 0 && (l > lastDate || 0 == lastDate)) {
                lastDate = l;
            }
	}
    } else {
	entryPtr = (RatDbEntry*)ckalloc(EXTRA_ENTRIES * sizeof(RatDbEntry));
	numAlloc = EXTRA_ENTRIES;
    }
    isRead = 1;

    /*
     * Let's get up to date with any changes made.
     */
    if (3 == version) {
	Sync(interp, 1);
	DbaseConvert3to4(interp);
	Unlock(interp);
	return Read(interp);
    } else {
	Sync(interp, 0);
    }
    Unlock(interp);

    return TCL_OK;

error:
    snprintf(buf, sizeof(buf), "%s/rlock.%s", dbDir, ident);
    unlink(buf);
    Unlock(interp);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * Lock --
 *
 *      Exculsively lock the database. A lock like this must be obtained
 *	before you do anything at all with the database. And while a
 *	lock like this is active nobody but the owner of the lock may
 *	do anything to the database. The lock obtained must be released
 *	with a call to the Unlock() procedure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Creates a lockfile in the database directory.
 *
 *----------------------------------------------------------------------
 */

static void
Lock(Tcl_Interp *interp)
{
    char buf[1024];	/* Scratch area */
    int fhLock;		/* Lockfile filehandle */
    int msgPost = 0;	/* True if message has been posted */

    do {
	snprintf(buf, sizeof(buf), "%s/lock", dbDir);
	if (-1 == (fhLock = open(buf, O_CREAT|O_EXCL|O_WRONLY, FILEMODE))) {
	    if (EEXIST == errno) {
		if (!msgPost) {
		    RatLogF(interp, RAT_INFO, "waiting_dbase_lock",
			    RATLOG_EXPLICIT);
		    Tcl_Eval(interp, buf);
		    msgPost = 1;
		}
		sleep(2);
	    } else {
		RatLogF(interp, RAT_FATAL, "failed_to_create_file",
			RATLOG_TIME, buf, Tcl_PosixError(interp));
		exit(1);
	    }
	}
    } while (-1 == fhLock);
    if (-1 == safe_write(fhLock, ident, strlen(ident))) {
        fprintf(stderr, "Failed to write dbase lock\n");
    }
    close(fhLock);
    if (msgPost) {
	RatLog(interp, RAT_INFO, "", RATLOG_TIME);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Unlock --
 *
 *      Releases a lock previously obtained with the Lock() procedure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The lockfile is removed.
 *
 *----------------------------------------------------------------------
 */

static void
Unlock(Tcl_Interp *interp)
{
    char buf[1024];	/* Scratch area */

    snprintf(buf, sizeof(buf), "%s/lock", dbDir);
    if (0 != unlink(buf)) {
	RatLogF(interp, RAT_FATAL, "failed_to_unlink_file", RATLOG_TIME, buf,
		Tcl_PosixError(interp));
	exit(1);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Sync --
 *
 *      Make sure that the database in memory is consistent with the
 *	master on disk. This is accomplished by reading the index.changes
 *	file. This call assumes that we have an exclusive lock on the
 *	database.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *      The internal list of entries may be affected as well as the
 *	index-file on disk.
 *
 *----------------------------------------------------------------------
 */

static int
Sync(Tcl_Interp *interp, int force)
{
    static int stale = 0;  /* Number of stale entries */
    char buf[1024];	   /* Scratch area */
    struct stat sbuf;	   /* Buffer for stat() calls */
    FILE *fpChanges;	   /* index.changes file pointer */
    FILE *fpIndex;	   /* Index file pointer */
    char command;	   /* Command in changes file */
    int cmdArg;		   /* Argument to command */
    int i;		   /* Loop counter */
    int doWrite = 0;	   /* 1 if we should write the changes to the disk */
    int numEntries;	   /* How many entries there actually are */
    char *indexBuf = NULL; /* New part of index file */
    int indexOffset = 0;   /* Offset of new part of index file */
    char *cPtr;
    int size;
    long l;

    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    if (0 > stat(buf, &sbuf)) {
	if (ENOENT != errno) {
	    Tcl_AppendResult(interp, "error stating file \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	if (!force) {
	    return TCL_OK;
	} else {
	    sbuf.st_size = 0;
	}
    }
    if (changeSize >= sbuf.st_size && !force) {
	return TCL_OK;
    }

    /*
     * Read and perform changes mentioned in index.changes file
     */
    if (0 == (fpChanges = fopen(buf, "r"))) {
	Tcl_AppendResult(interp, "error opening file (for reading) \"",
		buf, "\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    if (0 != fseek(fpChanges, changeSize, SEEK_SET)) {
	Tcl_AppendResult(interp, "error seeking in file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	fclose(fpChanges);
	return TCL_ERROR;
    }
    changeSize = sbuf.st_size;
    while(1) {
	if (2 != fscanf(fpChanges, "%c %d ", &command, &cmdArg) ||
		('d'!=command && 'a'!=command && 's'!=command && 'k'!=command)
		|| (('s' == command || 'k' == command) &&
		buf != fgets(buf, sizeof(buf), fpChanges))) {
	    if (feof(fpChanges)) {
		break;
	    }
	    Tcl_SetResult(interp, "syntax error in changes file",
		    TCL_STATIC);
	    fclose(fpChanges);
	    return TCL_ERROR;
	}
	numChanges++;
	if ('d' == command) {
	    if (cmdArg < 0 || cmdArg >= numRead) {
		continue;
	    }
	    needRewrite = 1;
	    entryPtr[cmdArg].content[FROM] = NULL;
	} else if ('s' == command) {
	    if (cmdArg < 0 || cmdArg >= numRead) {
		continue;
	    }
	    needRewrite = 1;
	    buf[strlen(buf)-1] = '\0';
	    if ( (int) strlen(buf) <=
		    (int) strlen(entryPtr[cmdArg].content[STATUS])){
		strcpy(entryPtr[cmdArg].content[STATUS], buf);
	    } else {
		/*
		 * This code may leak the memory occupied by the
		 * previous status string. I believe this loss can
		 * be lived with (it should be quite rare).
		 */
		entryPtr[cmdArg].content[STATUS] =
			(char *) ckalloc(strlen(buf)+1);
		strcpy(entryPtr[cmdArg].content[STATUS], buf);
	    }
	} else if ('k' == command) {
            Tcl_Obj *line, **elemv, **indexes;
            int elemc, indexc, i, index;
            char *keywords, *ex_time, *ex_type;

            line = Tcl_NewStringObj(buf, -1);
            if (TCL_OK != Tcl_ListObjGetElements(interp, line, &elemc,&elemv)
                || elemc != 4
                || TCL_OK != Tcl_ListObjGetElements(interp, elemv[0],
                                                    &indexc, &indexes)) {
                continue;
            }
            keywords = cpystr(Tcl_GetString(elemv[1]));
            ex_time  = cpystr(Tcl_GetString(elemv[2]));
            ex_type  = cpystr(Tcl_GetString(elemv[3]));
            for (i=0; i<indexc; i++) {
                Tcl_GetIntFromObj(interp, indexes[i], &index);
                entryPtr[index].content[KEYWORDS] = keywords;
                entryPtr[index].content[EX_TIME] = ex_time;
                entryPtr[index].content[EX_TYPE] = ex_type;
            }
            Tcl_DecrRefCount(line);
            
	} else {
	    if (numRead == numAlloc) {
		numAlloc += EXTRA_ENTRIES;
		entryPtr = (RatDbEntry*)ckrealloc(entryPtr,
			numAlloc*sizeof(RatDbEntry));
	    }
	    if (!indexBuf) {
		snprintf(buf, sizeof(buf), "%s/index", dbDir);
		if (NULL == (fpIndex = fopen(buf, "r"))) {
		    Tcl_AppendResult(interp,
			    "error opening file (for reading) \"", buf,
			    "\": ", Tcl_PosixError(interp), (char *) NULL);
		    fclose(fpChanges);
		    return TCL_ERROR;
		}
		if (0 != fseek(fpIndex, cmdArg, SEEK_SET)) {
		    Tcl_AppendResult(interp, "error seeking in file \"",buf,
			    "\": ", Tcl_PosixError(interp), (char *) NULL);
		    fclose(fpIndex);
		    fclose(fpChanges);
		    return TCL_ERROR;
		}
		(void)fstat(fileno(fpIndex), &sbuf);
		/*
		 * Purify will probably report this as a leak, but that
		 * is not true.
		 */
		size = sbuf.st_size - cmdArg;
		indexBuf = (char*)ckalloc(size + 1);
		if (!fread(indexBuf, size, 1, fpIndex)) {
                    size = 0;
                }
		fclose(fpIndex);
		indexBuf[size] = '\0';
		indexOffset = cmdArg;
	    }
	    cPtr = indexBuf + (cmdArg - indexOffset);
	    for (i=0; i<RATDBETYPE_END; i++) {
		entryPtr[numRead].content[i] = cPtr;
		for (; *cPtr != '\n' && *cPtr; cPtr++);
		if (!*cPtr) {
		    Tcl_AppendResult(interp, "error reading \"",buf,
			    "\": ", Tcl_PosixError(interp),(char*)NULL);
		    fclose(fpChanges);
		    return TCL_ERROR;
		}
		*cPtr++ = '\0';
	    }
            totSize += atol(entryPtr[numRead].content[RSIZE]);
            l = atol(entryPtr[numRead].content[DATE]);
            if (l < firstDate || 0 == firstDate) {
                firstDate = l;
            }
            if (l > lastDate || 0 == lastDate) {
                lastDate = l;
            }
	    numRead++;
	}
    }
    fclose(fpChanges);

    /* 
     * If the number of changes is at least 20 and we are the only agent
     * which have the database open we write the changes into the main
     * index. This also happens if we have read an older version of the
     * database.
     */
    if (20 <= numChanges || force) {
	char myLock[1024];		/* Name of my rlock file */
	snprintf(myLock, sizeof(myLock), "rlock.%s", ident);

	if (IsRlocked(myLock) && !force) {
	    doWrite = 0;
	} else {
	    doWrite = 1;
	}
    }

    if (doWrite) {
	FILE *fpIndexinfo;		/* Filepointer to index info */
	if (needRewrite || force) {
	    char oldIndex[1024];	/* Name of old index file */
	    char newIndex[1024];	/* Name of new index file */
	    FILE *fpNewIndex;		/* Filepointer to new index */
	    int j;			/* Loop variable */

	    snprintf(oldIndex, sizeof(oldIndex), "%s/index", dbDir);
	    snprintf(newIndex, sizeof(newIndex), "%s/index.new", dbDir);
	    if (0 == (fpNewIndex = fopen(newIndex, "w"))) {
		Tcl_AppendResult(interp, "error creating file \"", newIndex,
			"\": ", Tcl_PosixError(interp), (char *) NULL);
		return TCL_ERROR;
	    }
	    if (0 == (fpIndex = fopen(oldIndex, "r"))) {
		Tcl_AppendResult(interp, "error opening file (for reading)\"",
			oldIndex,"\": ", Tcl_PosixError(interp), (char*)NULL);
		(void)fclose(fpNewIndex);
		return TCL_ERROR;
	    }

            totSize = 0;
	    for (i=0, numEntries=0 ; i < numRead; i++) {
		if (0 != entryPtr[i].content[FROM]) {
		    numEntries++;
		    for (j=0; j<RATDBETYPE_END; j++) {
			if (0 > fprintf(fpNewIndex, "%s\n",
				entryPtr[i].content[j])) {
			    Tcl_AppendResult(interp,"error writing to file \"",
				    newIndex, "\"", (char *) NULL);
			    (void)fclose(fpNewIndex);
			    (void)unlink(newIndex);
			    (void)fclose(fpIndex);
			    return TCL_ERROR;
			}
		    }
                    totSize += atol(entryPtr[i].content[RSIZE]);
		} else {
		    snprintf(buf, sizeof(buf), "%s/dbase/%s", dbDir,
			    entryPtr[i].content[FILENAME]);
		    (void)unlink(buf);
		}
	    }
	    (void)fclose(fpIndex);
            totSize += ftell(fpNewIndex);
	    if (0 != fclose(fpNewIndex)) {
		Tcl_AppendResult(interp,"error closing file \"", newIndex,
				 "\": ", Tcl_PosixError(interp),
				 (char *) NULL);
		(void)unlink(newIndex);
		return TCL_ERROR;
	    }
	    if (0 != rename(newIndex, oldIndex)) {
		Tcl_AppendResult(interp,"error moving file \"", newIndex,
			"\" -> \"", oldIndex, "\": ", Tcl_PosixError(interp),
			(char *) NULL);
		return TCL_ERROR;
	    }
	    stale = numRead-numEntries;
	} else {
	    numEntries = numRead-stale;
	}
	snprintf(buf, sizeof(buf), "%s/index.info", dbDir);
	if (0 == (fpIndexinfo = fopen(buf, "w"))) {
	    Tcl_AppendResult(interp, "error opening file (for writing)\"",
		    buf, "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	if (0 > fprintf(fpIndexinfo, "%d %d\n", DBASE_VERSION, numEntries)) {
	    Tcl_AppendResult(interp, "error writing to file \"", buf, "\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (0 > fclose(fpIndexinfo)) {
	    Tcl_AppendResult(interp, "error closing file \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
	if (0 != unlink(buf)) {
	    Tcl_AppendResult(interp, "error unlinking file \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	
	changeSize = 0;
	numChanges = 0;
	needRewrite = 0;
	version = DBASE_VERSION;
    }

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Update --
 *
 *      This routine updates the read-lock we have on the database. This
 *	must be done at least as often as is specified by the Read
 *	rotine. Preferably more often. If the database hasn't been read
 *	yet (or has been closed) this routine does nothing.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The rlock-file will be touched (if the database is open).
 *
 *----------------------------------------------------------------------
 */

static void
Update(ClientData clientData)
{
    char rlockName[1024];	/* Name of rlock file */

    if (0 != isRead) {
	return;
    }

    snprintf(rlockName, sizeof(rlockName), "%s/rlock.%s", dbDir, ident);
    (void)utime(rlockName, (struct utimbuf*) NULL);

    (void)Tcl_CreateTimerHandler(UPDATE_INTERVAL*1000,Update,(ClientData)NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * IsRlocked --
 *
 *      Checks if any othe rprocess has an read lock on the database.
 *
 * Results:
 *      True if any othe rprocess has.
 *
 * Side effects:
 *      Stale rlock-files will be removed
 *
 *----------------------------------------------------------------------
 */

static int
IsRlocked(char *ignore)
{
    time_t deadline;		/* Lockfiles older than this can be ignored */
    struct dirent *direntPtr;
    struct stat sbuf;
    int result = 0;
    DIR *dirPtr;
    char buf[1024];

    deadline = time((time_t*) NULL) - RLOCK_TIMEOUT;

    dirPtr = opendir(dbDir);
    while (0 != (direntPtr = readdir(dirPtr))) {
	if (!strncmp("rlock.", (char*)direntPtr->d_name, 6)
		&& (ignore && strcmp((char*)direntPtr->d_name, ignore))) {
	    snprintf(buf, sizeof(buf),"%s/%s", dbDir,(char*)direntPtr->d_name);
	    (void)stat(buf, &sbuf);
	    if (deadline < sbuf.st_mtime) {
		result = 1;
		break;
	    } else {
		unlink(buf);
	    }
	}
    }
    closedir(dirPtr);
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbInsert --
 *
 *      This procedure inserts a copy of the message, whose id is passed
 *	in the mail parameter, into the database. An entry is made in the
 *	index file. One of the arguments is the expiration date as a string.
 *	This string is the number of days the message should stay in the
 *	database until it expires.
 *
 *	The algorithm is to update the index on disk and then let Sync()
 *	insert the new value into the internal database.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *      The internal and external databases are updated.
 *
 *----------------------------------------------------------------------
 */

int
RatDbInsert (Tcl_Interp *interp, const char *to, const char *from,
	     const char *cc, const char *msgid, const char *ref,
	     const char *subject, long date, const char *flags,
	     const char *keywords, long exDate, const char *exType,
	     const char *fromline, const char *mail, int length)
{
    static char *tobuf = NULL;	/* Scratch area */
    static int tobufsize = 0;	/* Size of scratch area */
    char fname[1024];		/* filename of new entry */
    char buf[1024];		/* Scratch area */
    char *dir = NULL;		/* Message directory */
    FILE *indexFP;		/* File pointer to index file */
    long indexPos;		/* Start position in index file */
    char *cPtr;			/* Misc character pointer */
    FILE *seqFP;		/* Filepointer to seq file */
    int seq;			/* sequence number */
    FILE *indchaFP;		/* File pointer to the index.changes file */
    Tcl_Channel data;	        /* Data channel */
    ADDRESS *adrPtr;		/* Address list */
    int i;
    int fd;

    if (0 == isRead) {
	if (TCL_OK != Read(interp)) {
	    return TCL_ERROR;
	}
    }
    Lock(interp);

    /*
     * Generate the filename we are to use for this entry in the database.
     * Create the directory it will be stored in too (if needed).
     */
    adrPtr = NULL;
    if (to && *to) {
	if (tobufsize < strlen(to)+1) {
	    tobufsize = strlen(to)+1;
	    tobuf = (char*)ckrealloc(tobuf, tobufsize);
	}
	strlcpy(tobuf, to, tobufsize);
	rfc822_parse_adrlist(&adrPtr, tobuf, "not.used");
	if (adrPtr && adrPtr->mailbox && *adrPtr->mailbox) {
	    dir = cpystr(adrPtr->mailbox);
	}
    }
    if (!dir) {
	dir = cpystr("default");
    }
    mail_free_address(&adrPtr);
    for (cPtr = dir; *cPtr; cPtr++) {
	if ('/' == *cPtr) {
	    *cPtr = '_';
	}
    }

    snprintf(fname, sizeof(fname), "%s/", dir);
    snprintf(buf, sizeof(buf), "%s/dbase/%s/.seq", dbDir, dir);
    if (NULL == (seqFP = fopen(buf, "r+"))) {
	snprintf(buf, sizeof(buf), "%s/dbase/%s", dbDir, dir);
	if (0 != mkdir(buf, DIRMODE) && EEXIST != errno) {
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error creating directory \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    ckfree(dir);
	    return TCL_ERROR;
	}
	seq = 0;
	snprintf(buf, sizeof(buf), "%s/dbase/%s/.seq", dbDir, dir);
	if (NULL == (seqFP = fopen(buf, "w"))) {
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error opening (for writing)\"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    ckfree(dir);
	    return TCL_ERROR;
	}
    } else {
	if (1 != fscanf(seqFP, "%d", &seq)) {
	    (void)fclose(seqFP);
	    Unlock(interp);
	    Tcl_AppendResult(interp, "error parsing: \"", buf, "\"",
		    (char*) NULL);
	    ckfree(dir);
	    return TCL_ERROR;
	}
	seq++;
    }
    ckfree(dir);
    rewind(seqFP);
    if (0 > fprintf(seqFP, "%d", seq)) {
	(void)fclose(seqFP);
	Unlock(interp);
	Tcl_AppendResult(interp, "error writing to \"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    if (0 != fclose(seqFP)) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error closing file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    sprintf(buf, "%d", seq);
    cPtr = fname + strlen(fname);
    for (i=strlen(buf)-1; i>=0; i--) {
	*cPtr++ = buf[i];
    }
    *cPtr = '\0';

    /*
     * Open the indexfile and remember where we are
     */
    snprintf(buf, sizeof(buf), "%s/index", dbDir);
    if (NULL == (indexFP = fopen(buf, "a"))) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error opening (for append)\"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    indexPos = ftell(indexFP);

    /*
     * Construct the entries... some are simple some others are more
     * complicated :-) The order and format of the entries is documented
     * at the top of this file.
     */
    NoLFPrint(indexFP, to);
    NoLFPrint(indexFP, from);
    NoLFPrint(indexFP, cc);
    NoLFPrint(indexFP, msgid);
    NoLFPrint(indexFP, ref);
    NoLFPrint(indexFP, subject);
    fprintf(indexFP, "%ld\n", date);
    NoLFPrint(indexFP, (keywords ? keywords : ""));
    fprintf(indexFP, "%d\n", length);
    NoLFPrint(indexFP, flags);
    fprintf(indexFP, "%ld\n", exDate*24*60*60 + time((time_t*) NULL));
    NoLFPrint(indexFP, exType);
    if (0 > NoLFPrint(indexFP, fname)) {
	goto losing;
    }
    if (0 != fclose(indexFP)) {
	Tcl_AppendResult(interp, "error closing index file :",
		Tcl_PosixError(interp), (char *) NULL);
	goto losing;
    }

    /*
     * Create the actual entry in the database.
     */
    snprintf(buf, sizeof(buf), "%s/dbase/%s", dbDir, fname);
    if (0 > (fd = open(buf, O_WRONLY|O_CREAT|O_TRUNC, 0666))
        || NULL == (data = Tcl_MakeFileChannel((ClientData)fd,TCL_WRITABLE))) {
	Tcl_AppendResult(interp, "error creating file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto losing;
    }
    /* The casts here are needed to build on tcl <8.4 */
    Tcl_Write(data, (char*)fromline, strlen(fromline));
    RatTranslateWrite(data, (char*)mail, length);
    if (TCL_OK != Tcl_Close(interp, data)) {
	Tcl_AppendResult(interp, "error closing file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto losing_but_nearly_got_it;
    }

    /*
     * Write an entry to the index.changes file and then update
     */
    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    if (NULL == (indchaFP = fopen(buf, "a"))) {
	Tcl_AppendResult(interp, "error opening file (for append)\"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto losing_but_nearly_got_it;
    }
    if (0 > fprintf(indchaFP, "a %ld\n", indexPos)) {
	Tcl_AppendResult(interp, "error writing to file \"", buf, "\"",
		(char *) NULL);
	goto losing_but_nearly_got_it;
    }
    if (0 != fclose(indchaFP)) {
	Tcl_AppendResult(interp, "error closing file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto losing_but_nearly_got_it;
    }
    Sync(interp, 0);

    Unlock(interp);
    return TCL_OK;

losing_but_nearly_got_it:
    snprintf(buf, sizeof(buf), "%s/dbase/%s", dbDir, fname);
    (void)unlink(buf);

losing:
    (void)snprintf(buf, sizeof(buf), "%s/index", dbDir);
    i = truncate(buf, indexPos); /* Ignore result */
    Unlock(interp);

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbSetStatus --
 *
 *	This procedure modifies the status of the given message.
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *      The internal and external databases are updated.
 *
 *----------------------------------------------------------------------
 */

int
RatDbSetStatus(Tcl_Interp *interp, int index, char *status)
{
    char buf[1024];	/* Name of index.changes file */
    FILE *indexFP;	/* FIle pointer to index.changes file */

    /*
     * Check the index for validity.
     */
    if (index >= numRead || index < 0) {
	Tcl_SetResult(interp, "error: the given index is invalid", TCL_STATIC);
	return TCL_ERROR;
    }

    /*
     * Check if we really need to do this
     */
    if (!strcmp(status, entryPtr[index].content[STATUS])) {
	return TCL_OK;
    }

    Lock(interp);
    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    if (NULL == (indexFP = fopen(buf, "a"))) {
	Tcl_AppendResult(interp, "error opening (for append)\"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	Unlock(interp);
	return TCL_ERROR;
    }
    if (0 > fprintf(indexFP, "s %d %s\n", index, status)){
	Tcl_AppendResult(interp, "Failed to write to file \"", buf, "\"",
		(char*) NULL);
	(void)fclose(indexFP);
	Unlock(interp);
	return TCL_ERROR;
    }
    if (0 != fclose(indexFP)) {
	Tcl_AppendResult(interp, "error closing file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	Unlock(interp);
	return TCL_ERROR;
    }

    Sync(interp, 0);
    Unlock(interp);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbSearch --
 *
 *      Searches the database for entries matching the given expression.
 *	The search expression is in the following form:
 *	    [interval] op exp [exp ...]
 *      Where interval (which is optional) is defined as:
 *          int start_time end_time
 *	Where op is either "and" or "or". and exp is as follows:
 *	    [not] field value
 *	Where field is one of "to", "from", "cc", "subject", "keywords",
 *	"all", "time_from" and "time_to". if op is "and" the all following
 *      expressions must be true but if it is "or" then only one has to bes
 *      true.
 *
 * Results:
 *      The number of items found is returned in numFoundPtr if it is
 *	non zero a pointer to a list of found ones is put into *foundPtrPtr.
 *	It is the callers resopnsibility to free this list with a
 *	call to free(). The rotine normally returns TCL_OK except when
 *	there is an error. In that case TCL_ERROR is returned and the cause
 *	may be found in interp->error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
RatDbSearch(Tcl_Interp *interp, Tcl_Obj *exp, int *numFoundPtr,
	    int **foundPtrPtr, int *expError)
{
    int or;			/* Indicates the operation; 0 = and  1 = or */
    int i, j, k;		/* Loop counters */
    int match, matchl;
    int numAlloc = 0;		/* Number of entries allocated room for */
    int expNumWords, objc;
    Tcl_Obj **expWords, **objv;
    long long_value;
    long time_start, time_end;
    int numExp;			/* The number of subexpressions in the exp */
    char fname[1024];		/* Filename of actual message */
    int bodyfd;			/* File descriptor to actual message */
    char *message = NULL;	/* Actual message */
    int messageSize = 0;	/* Size of message area */
    struct stat sbuf;		/* Buffer for stat calls */
    ssize_t l;
    char *s;
    
    /*
     * The following three lists describes the search expression. Every
     * expression has one entry in each list.
     */
    int *notPtr;
    RatDbEType *fieldPtr;
    Tcl_Obj **valuePtr;

    *numFoundPtr = 0;
    *foundPtrPtr = NULL;

    /*
     * Parse the expression and build the lists.
     * We start by being pessimistic and assume the expression is faulty:-)
     */
    if (expError != NULL) {
        *expError = 1;
    }
    if (TCL_OK != Tcl_ListObjGetElements(interp, exp,&expNumWords,&expWords)) {
	return TCL_ERROR;
    }
    i=0;
    s = Tcl_GetString(expWords[i++]);
    if (!strcmp(s, "and") && !strcmp(s, "or") && !strcmp(s, "int")) {
	Tcl_SetResult(interp, "exp must start with 'and', 'or' or 'int'.",
		TCL_STATIC);
	return TCL_ERROR;
    }
    
    /* These might be sligthly larger than needed, but who cares:-) */
    notPtr = (int*) ckalloc(sizeof(int) * (expNumWords/2));
    fieldPtr = (RatDbEType*) ckalloc(sizeof(RatDbEType) * (expNumWords/2));
    valuePtr = (Tcl_Obj**) ckalloc(sizeof(Tcl_Obj*) * (expNumWords/2));

    if (!strcmp(s, "int")) {
        if (expNumWords < 4
            || TCL_OK != Tcl_GetLongFromObj(interp, expWords[i+0], &time_start)
            || TCL_OK != Tcl_GetLongFromObj(interp, expWords[i+1], &time_end)){
            Tcl_SetResult(interp, "syntax error in expression", TCL_STATIC);
            return TCL_ERROR;
        }
        i += 2;
        s = Tcl_GetString(expWords[i++]);
    } else {
        time_start = 0;
        time_end = 0;
    }
    if (!strcmp(s, "or")) {
	or = 1;
    } else {
	or = 0;
    }

    numExp = 0;
    while (i < expNumWords) {
	s = Tcl_GetString(expWords[i]);
	if (!strcmp(s, "not")) {
	    notPtr[numExp] = 1;
	    s = Tcl_GetString(expWords[++i]);
	} else {
	    notPtr[numExp] = 0;
	}

	if (i > expNumWords-1) {
	    Tcl_SetResult(interp, "Parse error in exp (to few words)",
		    TCL_STATIC);
	    goto losing;
	}
	if (!strcmp(s, "to")) {
	    fieldPtr[numExp] = TO;
	} else if (!strcmp(s, "from")) {
	    fieldPtr[numExp] = FROM;
	} else if (!strcmp(s, "cc")) {
	    fieldPtr[numExp] = CC;
	} else if (!strcmp(s, "subject")) {
	    fieldPtr[numExp] = SUBJECT;
	} else if (!strcmp(s, "keywords")) {
	    fieldPtr[numExp] = KEYWORDS;
	} else if (!strcmp(s, "all")) {
	    fieldPtr[numExp] = SEARCH_ALL;
	} else if (!strcmp(s, "all_addresses")) {
	    fieldPtr[numExp] = SEARCH_ALL_ADDRESSES;
	} else if (!strcmp(s, "time_from")) {
	    fieldPtr[numExp] = SEARCH_TIME_FROM;
	} else if (!strcmp(s, "time_to")) {
	    fieldPtr[numExp] = SEARCH_TIME_TO;
	} else {
	    Tcl_SetResult(interp, "Parse error in exp (illegal field value)",
		    TCL_STATIC);
	    goto losing;
	}
	i++;
	valuePtr[numExp++] = expWords[i++];
    }

    /*
     * The expression was good...
     */
    if (expError != NULL) {
        *expError = 0;
    }

    /*
     * Now we are ready to do the searching. First make sure that the
     * database is read and synced, then run through it.
     */
    if (0 == isRead) {
	if (TCL_OK != Read(interp)) {
	    goto losing;
	}
    } else {
	if (TCL_OK != Sync(interp, 0)) {
	    goto losing;
	}
    }
    for (i=0; i < numRead; i++) {
	if (!entryPtr[i].content[FROM]) {	/* Entry deleted */
	    continue;
	}
        if (time_start != 0) {
            long_value = atol(entryPtr[i].content[DATE]);
            if (long_value != 0
                && ((long_value < time_start) || (long_value > time_end))) {
                continue;
            }
        }
	match = 0;
	for(j=0; j < numExp && !(j != 0 && or == match); j++) {
	    Tcl_ListObjGetElements(interp, valuePtr[j], &objc, &objv);
	    for (k=0, matchl=0; k<objc; k++) {
		if ((int)fieldPtr[j] == SEARCH_ALL) {
		    snprintf(fname, sizeof(fname), "%s/dbase/%s", dbDir,
			    entryPtr[i].content[FILENAME]);
		    if (0 > (bodyfd = open(fname, O_RDONLY))) {
			Tcl_AppendResult(interp,
				"error opening file (for read)\"", fname,
				"\": ", Tcl_PosixError(interp), (char*)NULL);
			goto losing;
		    }
		    if (0 != fstat(bodyfd, &sbuf)) {
			Tcl_AppendResult(interp, "error stating file \"",fname,
				"\": ", Tcl_PosixError(interp), (char *) NULL);
			close(bodyfd);
			goto losing;
		    }
		    if (messageSize < sbuf.st_size+1) {
			ckfree(message);
			message = (char*)ckalloc(messageSize = sbuf.st_size+1);
		    }
		    l = SafeRead(bodyfd, message, sbuf.st_size);
                    if (l > 0) {
                        message[l] = '\0';
                        (void)close(bodyfd);
                        matchl = RatSearch(Tcl_GetString(objv[k]), message);
                    }
                } else if ((int)fieldPtr[j] == SEARCH_ALL_ADDRESSES) {
		    matchl = RatSearch(Tcl_GetString(objv[k]),
                                       entryPtr[i].content[TO])
                        || RatSearch(Tcl_GetString(objv[k]),
                                     entryPtr[i].content[CC])
                        || RatSearch(Tcl_GetString(objv[k]),
                                     entryPtr[i].content[FROM]);
                } else if ((int)fieldPtr[j] == SEARCH_TIME_FROM) {
                    Tcl_GetLongFromObj(interp, objv[k], &long_value);
                    matchl = atol(entryPtr[i].content[DATE]) >= long_value;
                } else if ((int)fieldPtr[j] == SEARCH_TIME_TO) {
                    Tcl_GetLongFromObj(interp, objv[k], &long_value);
                    matchl = atol(entryPtr[i].content[DATE]) <= long_value;
		} else {
		    matchl = RatSearch(Tcl_GetString(objv[k]),
                                       entryPtr[i].content[fieldPtr[j]]);
		}
                if ((or && matchl) || (!or && !matchl)) {
                    break;
                }
	    }
            if (1 == notPtr[j]) {
                match = matchl ? 0 : 1;
            } else {
                match = matchl;
            }
	}

	if (match || (or && 0 == numExp)) {
	    if (*numFoundPtr >= numAlloc) {
		numAlloc += EXTRA_ENTRIES;
		*foundPtrPtr =(int*)ckrealloc(*foundPtrPtr,
					      numAlloc*sizeof(int));
	    }
	    (*foundPtrPtr)[(*numFoundPtr)++] = i;
	}
    }

    ckfree((char*) notPtr);
    ckfree((char*) fieldPtr);
    ckfree((char*) valuePtr);
    if (messageSize > 0) {
	ckfree(message);
    }

    return TCL_OK;

losing:
    ckfree((char*) expWords);
    ckfree((char*) notPtr);
    ckfree((char*) fieldPtr);
    ckfree((char*) valuePtr);
    if (messageSize > 0) {
	ckfree(message);
    }

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbGetEntry --
 *
 *      This routine retrieves an entry from the database. The pointer
 *	returned is ONLY good until the next call to RatDbInsert(),
 *	RatDbSetStatus(), RatDbSearch().
 *
 * Results:
 *      The routine returns a pointer to a RatDbEntry structure which
 *	should be treated as read only. If the index is invalid or
 *	points to a deleted entry a null pointer is returned.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

RatDbEntry*
RatDbGetEntry(int index)
{
    if (index<0 || index>=numRead || NULL == entryPtr[index].content[FROM]) {
	return NULL;
    }

    return &entryPtr[index];
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbGetMessage --
 *
 *      This routine extracts a copy of a message in the database and
 *	returns a MESSAGE structure.
 *
 * Results:
 *	A pointer to a MESSAGE* structure. It alse fills in the pointer
 *	to a buffer among the arguments with the address that needs to be
 *	freed when the message is deleted.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MESSAGE*
RatDbGetMessage(Tcl_Interp *interp, int index, char **buffer)
{
    char fname[1024];	/* Filename of message */
    int messfd;		/* Message file descriptor */
    struct stat sbuf;	/* Buffer for stat call (to find out size of message)*/
    char *message;	/* Pointer to actual message */
    ssize_t l;

    /*
     * Check the index for validity.
     */
    if (index >= numRead || index < 0) {
	Tcl_SetResult(interp, "error: the given index is invalid", TCL_STATIC);
	return NULL;
    }
    if (NULL == entryPtr[index].content[FROM]) {
	Tcl_SetResult(interp, "error: the message is deleted", TCL_STATIC);
	return NULL;
    }

    Lock(interp);

    /*
     * Read the message into an array pointed to by 'message'.
     */
    snprintf(fname, sizeof(fname), "%s/dbase/%s",
	    dbDir, entryPtr[index].content[FILENAME]);
    if (0 > (messfd = open(fname, O_RDONLY))) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error opening file (for read)\"",
		fname, "\": ", Tcl_PosixError(interp), (char*)NULL);
	return NULL;
    }
    if (0 != fstat(messfd, &sbuf)) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error stating file \"", fname,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	close(messfd);
	return NULL;
    }
    *buffer = message = (char*)ckalloc(sbuf.st_size+1);
    l = SafeRead(messfd, message, sbuf.st_size);
    if (l < 0) {
        return NULL;
    }
    message[l] = '\0';
    (void)close(messfd);

    Unlock(interp);

    return RatParseMsg(interp, (unsigned char*)message);
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbGetHeaders --
 *
 *      This routine extracts a copy of the headers of a message in
 *	the database.
 *
 * Results:
 *      A pointer to a static area containing the message headers
 *	is returned.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char*
RatDbGetHeaders(Tcl_Interp *interp, int index)
{
    static char *header = NULL;	/* Static storage area */
    static int headerSize = 0;	/* Size of static storage area */
    char fname[1024];		/* Filename of message */
    char *hPtr;			/* The header to return */
    FILE *messFp;		/* Message file pointer */
    int length = 0;		/* Length of header */
    int c;

    /*
     * Check the index for validity.
     */
    if (index >= numRead || index < 0) {
	Tcl_SetResult(interp, "error: the given index is invalid", TCL_STATIC);
	return NULL;
    }
    if (NULL == entryPtr[index].content[FROM]) {
	Tcl_SetResult(interp, "error: the message is deleted", TCL_STATIC);
	return NULL;
    }

    Lock(interp);

    /*
     * Read the message into an array pointed to by 'message'.
     */
    snprintf(fname, sizeof(fname), "%s/dbase/%s",
	    dbDir, entryPtr[index].content[FILENAME]);
    if (NULL == (messFp = fopen(fname, "r"))) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error opening file (for read)\"",
		fname, "\": ", Tcl_PosixError(interp), (char*)NULL);
	return NULL;
    }

    while (c = fgetc(messFp), !feof(messFp)) {
        if (length >= headerSize-1) {
            headerSize += 1024;
            header = (char*)ckrealloc(header, headerSize);
        }
        if ('\n' == c && (length == 0 || header[length-1] != '\r')) {
            header[length++] = '\r';
        }
        header[length++] = c;
        if (length > 4
            && header[length-4] == '\r' && header[length-3] == '\n'
            && header[length-2] == '\r' && header[length-1] == '\n') {
            length -= 2;
            break;
        }
    }

    header[length] = '\0';
    fclose(messFp);
    Unlock(interp);
    if (strncmp("From ", header, 5)) {
	hPtr = header;
    } else {
	hPtr = strchr(header, '\n')+1;
	if ('\r' == *hPtr) {
	    hPtr++;
	}
    }
    return hPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbGetFrom --
 *
 *      This routine extracts a copy of the first line of the headers
 *
 * Results:
 *      A pointer to a static area containing the from line
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char*
RatDbGetFrom(Tcl_Interp *interp, int index)
{
    static char header[8192];	/* Static storage area */
    char fname[1024];		/* Filename of message */
    FILE *messFp;		/* Message file pointer */

    /*
     * Check the index for validity.
     */
    if (index >= numRead || index < 0) {
	Tcl_SetResult(interp, "error: the given index is invalid", TCL_STATIC);
	return NULL;
    }
    if (NULL == entryPtr[index].content[FROM]) {
	Tcl_SetResult(interp, "error: the message is deleted", TCL_STATIC);
	return NULL;
    }

    Lock(interp);

    /*
     * Read the message into an array pointed to by 'message'.
     */
    snprintf(fname, sizeof(fname), "%s/dbase/%s",
	    dbDir, entryPtr[index].content[FILENAME]);
    if (NULL == (messFp = fopen(fname, "r"))) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error opening file (for read)\"",
		fname, "\": ", Tcl_PosixError(interp), (char*)NULL);
	return NULL;
    }
    Unlock(interp);
    if (fgets(header, sizeof(header)-1, messFp)) {
        header[sizeof(header)-1] = '\0';
    } else {
        header[0] = '\0';
    }
    fclose(messFp);
    return header;
}



/*
 *----------------------------------------------------------------------
 *
 * RatDbGetText --
 *
 *      This routine extracts a copy of the body of a message in
 *	the database.
 *
 * Results:
 *      A pointer to a static area containing the message body
 *	is returned.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char*
RatDbGetText(Tcl_Interp *interp, int index)
{
    static char *body = NULL;	/* Static storage area */
    static int bodySize = 0;	/* Size of static storage area */
    char fname[1024];		/* Filename of message */
    FILE *messFp;		/* Message file pointer */
    int length = 0;		/* Length of header */
    char buf[2048];		/* Temporary holding area */
    int c;

    /*
     * Check the index for validity.
     */
    if (index >= numRead || index < 0) {
	Tcl_SetResult(interp, "error: the given index is invalid", TCL_STATIC);
	return NULL;
    }
    if (NULL == entryPtr[index].content[FROM]) {
	Tcl_SetResult(interp, "error: the message is deleted", TCL_STATIC);
	return NULL;
    }

    Lock(interp);

    /*
     * Read the message into an array pointed to by 'message'.
     */
    snprintf(fname, sizeof(fname), "%s/dbase/%s",
	    dbDir, entryPtr[index].content[FILENAME]);
    if (NULL == (messFp = fopen(fname, "r"))) {
	Unlock(interp);
	Tcl_AppendResult(interp, "error opening file (for read)\"",
		fname, "\": ", Tcl_PosixError(interp), (char*)NULL);
	return NULL;
    }
    while (fgets(buf, sizeof(buf), messFp) != NULL && !feof(messFp)) {
	if ('\n' == buf[0] || '\r' == buf[0]) {
	    break;
	}
    }

    while (c = fgetc(messFp), !feof(messFp)) {
        if (length >= bodySize-1) {
            bodySize += 8192;
            body = (char*)ckrealloc(body, bodySize);
        }
        if ('\n' == c && (length == 0 || body[length-1] != '\r')) {
            body[length++] = '\r';
        }
        body[length++] = c;
    }
    body[length] = '\0';
    fclose(messFp);
    Unlock(interp);
    return body;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDbExpunge --
 *
 *      Deletes all entries marked for deletion (status contains D).
 *
 * Results:
 *      The return value is normally TCL_OK; if something goes wrong
 *	TCL_ERROR is returned and an error message will be left in
 *	the result area.
 *
 * Side effects:
 *      Both the internal and the disk copy of the database are affected.
 *	Observer that if some caller has previously retrieved this entry
 *	from the database with a call to RatDbGet() the RatDbEntry
 *	obtained will be destroyed (filled with nulls).
 *
 *----------------------------------------------------------------------
 */

int
RatDbExpunge(Tcl_Interp *interp)
{
    char buf[1024];	/* Name of index.changes file */
    FILE *indexFP;	/* File pointer to index.changes file */
    int index, i;

    Lock(interp);

    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    if (NULL == (indexFP = fopen(buf, "a"))) {
	Tcl_AppendResult(interp, "error opening (for append)\"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	Unlock(interp);
	return TCL_ERROR;
    }
    for (index=0; index < numRead; index++) {
	for (i=0; entryPtr[index].content[STATUS][i]; i++) {
	    if ('D' == entryPtr[index].content[STATUS][i]) {
		fprintf(indexFP, "d %d\n", index);
		break;
	    }
	}
    }
    if (0 != fclose(indexFP)) {
	Tcl_AppendResult(interp, "error closing file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	Unlock(interp);
	return TCL_ERROR;
    }

    Sync(interp, 0);
    Unlock(interp);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbDaysSinceExpire --
 *
 *	Finds ut how long it was since the database was expired last.
 *
 * Results:
 *	An integer which is the number of days since the database was expired.
 *	If the user has no database we return 0.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
RatDbDaysSinceExpire(Tcl_Interp *interp)
{
    struct stat sbuf;
    char buf[1024];

    /*
     * First make sure we know where the database should reside.
     */
    if (0 == dbDir) {
	const char *value = RatGetPathOption(interp, "dbase_dir");
	if (NULL == value) {
	    return TCL_ERROR;
	}
	dbDir = cpystr(value);
    }

    snprintf(buf, sizeof(buf), "%s/expired", dbDir);
    if (stat(buf, &sbuf)) {
	snprintf(buf, sizeof(buf), "%s/dbase", dbDir);
	if (stat(buf, &sbuf)) {
	    return 0;
	}
    }
    if (sbuf.st_mtime > time(NULL)) {
	return 0;
    } else {
	return (time(NULL)-sbuf.st_mtime)/(24*60*60);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbExpire --
 *
 *      Runs through the database and carries out any expiration that
 *	should be done. This routine should be called periodically.
 *
 * Results:
 *	If nothing went wrong TCL_OK is returned and in the result area is
 *	a list containing 5 numbers {num_scanned num_delete, num_backup,
 *	num_inbox num_custom}. Otherwise TCL_ERROR is returned.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
RatDbExpire(Tcl_Interp *interp, char *infolder, char *backupDirectory)
{
    char *compressProg, *compressSuffix, *statusId;
    const char *backupDir;
    int numScan = 0, numDelete = 0, numBackup = 0, numInbox = 0, numCustom = 0;
    int i, len, delete, fd, doBackup = 0, changed = 0, error = 0;
    int move;
    char buf[1024], buf2[1024];
    FILE *indexFP = NULL;
    time_t t, now = time(NULL);
    struct tm *tmPtr;
    struct stat sbuf;
    struct dirent *direntPtr;
    Tcl_Obj *oPtr;
    DIR *dirPtr;

    if (0 == isRead) {
	if (TCL_OK != Read(interp)) {
	    return TCL_ERROR;
	}
    }

    /*
     * Make sure the inbox directory exists.
     */
    snprintf(buf, sizeof(buf), "%s/inbox", dbDir);
    if (-1 == stat(buf, &sbuf)) {
	if (mkdir(buf, DIRMODE)) {
	    Tcl_AppendResult(interp, "error creating\"", buf,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * Prepare backup
     */
    if (-1 == stat(backupDirectory, &sbuf)) {
	if (mkdir(backupDirectory, DIRMODE)) {
	    Tcl_AppendResult(interp, "error creating\"", backupDirectory,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    }
    compressProg = getenv("COMPRESS");
    compressSuffix = getenv("CSUFFIX");
    backupDir = RatGetPathOption(interp, "dbase_backup");
    if (!compressProg || !compressSuffix || !backupDir) {
	Tcl_AppendResult(interp, "Internal error: compressProg, ",
		"compressSuffix or option(dbase_backup) not defined",
		(char*) NULL);
	return TCL_ERROR;
    }

    RatLogF(interp, RAT_INFO, "db_expire", RATLOG_EXPLICIT);
    statusId = cpystr(Tcl_GetStringResult(interp));
    Lock(interp);
    for (i=0; !error && i < numRead; i++) {
	if (!entryPtr[i].content[FROM]) {	/* Entry deleted */
	    continue;
	}
	numScan++;
	t = atol(entryPtr[i].content[EX_TIME]);
	if (!t || t >now) {
	    continue;
	}

	/*
	 * If we get here the entry should be expired.
	 *
	 * Currently we only handle the backup, delete and inbox actions.
	 * The rest are quitely ignored.
	 */
	delete = move = 0;
	if (!strcmp("delete", entryPtr[i].content[EX_TYPE])) {
	    delete = 1;
	    numDelete++;

	} else if (!strcmp("backup", entryPtr[i].content[EX_TYPE])) {
	    move = 1;
	    numBackup++;
	    snprintf(buf, sizeof(buf), "%s/dbase/%s",
		    dbDir, entryPtr[i].content[FILENAME]);
	    RatGenIdCmd(NULL, interp, 0, NULL);
	    snprintf(buf2, sizeof(buf2), "%s/message.%s", backupDirectory, 
		    Tcl_GetStringResult(interp));

	} else if (!strcmp("incoming", entryPtr[i].content[EX_TYPE])) {
	    move = 1;
	    numInbox++;
	    snprintf(buf, sizeof(buf), "%s/dbase/%s",
		    dbDir, entryPtr[i].content[FILENAME]);
	    RatGenIdCmd(NULL, interp, 0, NULL);
	    snprintf(buf2, sizeof(buf2), "%s/inbox/%s",
		    dbDir, Tcl_GetStringResult(interp));

	} else if (!strncmp("custom", entryPtr[i].content[EX_TYPE], 6)) {
	    numCustom++;

	} else if (!strcmp("none", entryPtr[i].content[EX_TYPE])) {
	    continue;

	} else {
	    /*
	     * If we get here it is an unkown type and we just silently
	     * deletes it (old versions of tkrat may have generated it.
	     */
	    delete = 1;
	    numDelete++;
	}
	if (move) {
	    if (link(buf, buf2)) {
		int fdSrc, fdDst;
		/*
		 * Sigh. the files are on different filesystems. We have to
		 * copy them.
		 */
		fdSrc = open(buf, O_RDONLY);
		fdDst = open(buf2, O_WRONLY|O_TRUNC|O_CREAT, 0666);
		do {
		    len = SafeRead(fdSrc, buf, sizeof(buf));
		    if (0 > safe_write(fdDst, buf, len)) {
			error = errno;
			len = 0;
		    }
		} while (len);
		close(fdSrc);
		if (close(fdDst) || error) {
		    RatLogF(interp, RAT_ERROR, "failed_to_move_to_file",
			    RATLOG_TIME, buf2,  Tcl_PosixError(interp));
		    delete = 0;
		}
	    }
	}
	if (delete || move) {
	    if (!indexFP) {
		changed = 1;
		snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
		if (NULL == (indexFP = fopen(buf, "a"))) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "error opening (for append)\"",
			    buf, "\":", Tcl_PosixError(interp), "\n", NULL);
		    Unlock(interp);
		    return TCL_ERROR;
		}
	    }
	    fprintf(indexFP, "d %d\n", i);
	}
    }
    if (changed) {
	fclose(indexFP);
	Sync(interp, 0);
    }
    Unlock(interp);

    /*
     * Compress the messages in the backup directory if we have enough
     * messages to make it meaningful.
     */
    if (numBackup) {
	int chunkSize;
	Tcl_Obj *oPtr;

	oPtr = Tcl_GetVar2Ex(interp, "option", "chunksize", TCL_GLOBAL_ONLY);
	if (oPtr) {
	    Tcl_GetIntFromObj(interp, oPtr, &chunkSize);
	} else {
	    chunkSize = 100;
	}

	if (numBackup < chunkSize) {
	    i = 0;
	    dirPtr = opendir(backupDirectory);
	    while (0 != (direntPtr = readdir(dirPtr))) {
		if (!strncmp(direntPtr->d_name, "message", 7)) {
		    i++;
		}
	    }
	    closedir(dirPtr);
	    doBackup = (i>=chunkSize);
	} else {
	    doBackup = 1;
	}
    }
    if (doBackup) {
	Tcl_Channel backupChannel, inChannel;
	CONST84 char *argv[3], *error = NULL;

	ckfree(statusId);
	RatLogF(interp, RAT_INFO, "packing_backup", RATLOG_EXPLICIT);
	statusId = cpystr(Tcl_GetStringResult(interp));
	tmPtr = localtime(&now);
	snprintf(buf2, sizeof(buf2),
		">%s/backup_%04d%02d%02d.%s", backupDirectory,
		tmPtr->tm_year+1900, tmPtr->tm_mon+1, tmPtr->tm_mday+1,
		compressSuffix);
	argv[0] = compressProg;
	argv[1] = buf2;
	if (!(backupChannel = Tcl_OpenCommandChannel(interp, 2, argv,
		TCL_STDIN))) {
	    Tcl_BackgroundError(interp);
	}
	if (backupChannel) {
	    dirPtr = opendir(backupDirectory);
	    while (!error && 0 != (direntPtr = readdir(dirPtr))) {
		if (strncmp(direntPtr->d_name, "message", 7)) {
		    continue;
		}
		snprintf(buf, sizeof(buf), "%s/%s",
			backupDirectory, direntPtr->d_name);
		inChannel = Tcl_OpenFileChannel(interp, buf, "r", 0);
		do {
		    len = Tcl_Read(inChannel, buf, sizeof(buf));
		    if (-1 == Tcl_Write(backupChannel, buf, len)) {
			error = Tcl_PosixError(interp);
		    }
		} while (!error && !Tcl_Eof(inChannel));
		Tcl_Write(backupChannel, "\n", 1);
		Tcl_Close(interp, inChannel);
	    }
	    if (TCL_OK != Tcl_Close(interp, backupChannel)) {
		error = Tcl_PosixError(interp);
	    }
	    if (error) {
		unlink(buf2);
		Tcl_SetResult(interp, (char*)error, TCL_STATIC);
		Tcl_BackgroundError(interp);
	    } else {
		rewinddir(dirPtr);
		while (0 != (direntPtr = readdir(dirPtr))) {
		    if (!strncmp(direntPtr->d_name, "message", 7)) {
			snprintf(buf, sizeof(buf), "%s/%s",
				backupDirectory, direntPtr->d_name);
			unlink(buf);
		    }
		}
	    }
	    closedir(dirPtr);
	}
    }

    /*
     * Move messages to inbox
     */
    if (numInbox) {
	char *data = NULL, *msg, *cPtr;
	int fd, allocated = 0;
        ssize_t l;

	snprintf(buf, sizeof(buf), "%s/inbox", dbDir);
	dirPtr = opendir(buf);
	while (0 != (direntPtr = readdir(dirPtr))) {
	    snprintf(buf2, sizeof(buf2), "%s/%s", buf, direntPtr->d_name);
	    if (stat(buf2, &sbuf) || !S_ISREG(sbuf.st_mode)) {
		continue;
	    }
	    if (allocated < sbuf.st_size+1) {
		allocated = sbuf.st_size+1;
		data = (char*)ckrealloc(data, allocated);
	    }
	    fd = open(buf2, O_RDONLY);
	    l = SafeRead(fd, data, sbuf.st_size);
	    close(fd);
            if (l <= 0) {
                continue;
            }
	    data[sbuf.st_size] = '\0';
	    unlink(buf2);
	    for (cPtr = data; *cPtr != '\n'; cPtr++);
	    if (*(++cPtr) == '\r') {
		cPtr++;
	    }
	    msg = RatFrMessageCreate(interp, cPtr, sbuf.st_size, NULL);
	    if (TCL_OK == Tcl_VarEval(interp, infolder, " insert ", msg,NULL)){
		unlink(buf2);
	    }
	}
	closedir(dirPtr);
    }
    /*
     * Mark that we have done expire
     */
    snprintf(buf, sizeof(buf), "%s/expired", dbDir);
    (void)unlink(buf);
    if (0 <= (fd = open(buf, O_WRONLY|O_CREAT, 0666))) {
	close(fd);
    }
    Tcl_VarEval(interp, "RatClearLog ", statusId, "; update idletasks",
	    (char*) NULL);
    ckfree(statusId);

    /*
     * Create result list and return
     */
    oPtr = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(numScan));
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(numDelete));
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(numBackup));
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(numInbox));
    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewIntObj(numCustom));
    Tcl_SetObjResult(interp, oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDbClose --
 *
 *      Closes the database on disk.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The rlock file is removed and some of the internal data structures
 *	are freed (but not all :-().
 *
 *----------------------------------------------------------------------
 */

void
RatDbClose()
{
    char buf[1024];	/* Scratch area */

    if (1 == isRead) {
	ckfree(entryPtr);
	isRead = 0;

	snprintf(buf, sizeof(buf), "%s/rlock.%s", dbDir, ident);
	unlink(buf);
    }
#ifdef MEM_DEBUG
    ckfree(dbDir);
#endif /* MEM_DEBUG */
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbBuildList --
 *
 *      Builds a list of the files in the database
 *
 *	The algorithm is to open the directory and for all files do:
 *	  - If the file is ".seq" then we read it and remember the number
 *	  - If the name starts with a dot ('.') then we continue with the
 *	    next file.
 *	  - If it is a directory the we recursively call ourselves to check
 *	    that directory.
 *	  - If it is an ordinary file then we add it to the hastable (the
 *	    hash is computed from the prefix/filename). We also decode the
 *	    number of the file and remembers the highest number found.
 *	When all files are checked we check if the highest number found
 *	was bigger than the content of seq. If it is so then we
 *	  - Append a warning to the result area.
 *	  - write a new .seq file IF fix is true.
 *
 * Results:
 *	May append diagnostic messages to the result area.
 *
 * Side effects:
 *	Will probably add items to the given hashtable.
 *
 *----------------------------------------------------------------------
 */

static void
RatDbBuildList(Tcl_Interp *interp, Tcl_DString *dsPtr, char *prefix, char *dir,
	Tcl_HashTable *tablePtr, int fix)
{
    char buf[1024], path[1024];
    unsigned long  seq = 0, maxnum = 0, num, fact;
    int i;
    Tcl_HashEntry *entryPtr;
    RatDbItem *itemPtr;
    struct dirent *entPtr;
    struct stat sbuf;
    DIR *dirPtr;
    FILE *fp;

    if (NULL == (dirPtr = opendir(dir))) {
	snprintf(buf, sizeof(buf), "Failed to open directory \"%s\": %s", dir,
		Tcl_PosixError(interp));
	Tcl_DStringAppendElement(dsPtr, buf);
	return;
    }
    while (NULL != (entPtr = readdir(dirPtr))) {
	if (!strcmp(entPtr->d_name, ".seq")) {
	    snprintf(path, sizeof(buf), "%s/.seq", dir);
	    if (NULL == (fp = fopen(path, "r"))) {
		snprintf(buf, sizeof(buf), "Failed to open file \"%s\": %s",
			path, Tcl_PosixError(interp));
		Tcl_DStringAppendElement(dsPtr, buf);
		if (fix) {
		    if (unlink(path)) {
			snprintf(buf, sizeof(buf),
				"Failed to unlink file \"%s\": %s", path,
				Tcl_PosixError(interp));
			Tcl_DStringAppendElement(dsPtr, buf);
		    }
		}
	    } else {
		if (1 != fscanf(fp, "%ld", &seq)) {
                    seq = -1;
                }
		fclose(fp);
	    }
	}
	if ('.' == entPtr->d_name[0]) {
	    continue;
	}
	snprintf(path, sizeof(path), "%s/%s", dir, entPtr->d_name);
	if (stat(path, &sbuf)) {
	    snprintf(buf, sizeof(buf), "Failed to stat file %s: %s\n", path,
		    Tcl_PosixError(interp));
	    Tcl_DStringAppendElement(dsPtr, buf);
	    continue;
	}
	if (S_IFREG == (sbuf.st_mode&S_IFMT)) {
	    if (!(S_IRUSR & sbuf.st_mode)) {
		snprintf(buf, sizeof(buf),
			"\"%s\" is not readable by the owner", path);
		Tcl_DStringAppendElement(dsPtr, buf);
		if (fix) {
		    if (chmod(path, sbuf.st_mode|S_IRUSR)) {
			snprintf(buf, sizeof(buf),
				"Failed to chmod \"%s\": %s", path,
				Tcl_PosixError(interp));
			Tcl_DStringAppendElement(dsPtr, buf);
			continue;
		    }
		}
	    }
	    if (0 == sbuf.st_size) {
		snprintf(buf, sizeof(buf), "Empty file \"%s\" found", path);
		if (fix) {
		    if (unlink(path)) {
			snprintf(buf, sizeof(buf),
				"Failed to unlink \"%s\": %s", path,
				Tcl_PosixError(interp));
			Tcl_DStringAppendElement(dsPtr, buf);
		    }
		}
		continue;
	    }
	    if (*prefix) {
		snprintf(buf, sizeof(buf), "%s/%s", prefix, entPtr->d_name);
	    } else {
		strlcpy(buf, entPtr->d_name, sizeof(buf));
	    }
	    itemPtr = (RatDbItem*)ckalloc(sizeof(RatDbItem));
	    itemPtr->fileSize = sbuf.st_size;
	    itemPtr->index = -1;
	    for (i=0; i<RATDBETYPE_END; i++) {
		itemPtr->entry.content[i] = NULL;
	    }
	    entryPtr = Tcl_CreateHashEntry(tablePtr, buf, &i);
	    Tcl_SetHashValue(entryPtr, (ClientData)itemPtr);

            /* Check sequence number */
            num = 0;
            for (i=0, fact=1; isdigit(entPtr->d_name[i]); i++, fact *= 10) {
                num += (entPtr->d_name[i]-'0') * fact;
            }
            if (num > maxnum) {
                maxnum = num;
            }
	} else if (S_IFDIR == (sbuf.st_mode&S_IFMT)) {
	    if (prefix && *prefix) {
		snprintf(path, sizeof(path), "%s/%s", prefix, entPtr->d_name);
	    } else {
		strlcpy(path, entPtr->d_name, sizeof(path));
	    }
	    snprintf(buf, sizeof(buf), "%s/%s", dir, entPtr->d_name);
	    RatDbBuildList(interp, dsPtr, path, buf, tablePtr, fix);
	} else {
	    snprintf(buf, sizeof(buf), "\"%s\" is not a file", path);
	    Tcl_DStringAppendElement(dsPtr, buf);
	}
    }
    closedir(dirPtr);

    if (maxnum > seq) {
        snprintf(buf, sizeof(buf),
                 "Bad sequence number was %ld but expected %ld", seq, maxnum);
        Tcl_DStringAppendElement(dsPtr, buf);
        if (fix) {
            snprintf(path, sizeof(buf), "%s/.seq", dir);
            if (NULL != (fp = fopen(path, "w"))) {
                fprintf(fp, "%ld", maxnum);
                fclose(fp);
            }
        }
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbCheck --
 *
 *      Checks the database.
 *
 * Results:
 *	A diagnostic string.
 *
 * Side effects:
 *	The database on disk may be rewritten (depends on the fix argument).
 *
 *----------------------------------------------------------------------
 */

#define EXP_NUM		"[0-9]*"
#define EXP_TYPE	"^((none)|(remove)|(incoming)|(backup)|(custom.*))?$"
#define EXP_FILE	"[^/]+/[0-9]*"

int
RatDbCheck(Tcl_Interp *interp, int fix)
{
    int numFound = 0, numMal = 0, numAlone = 0, numUnlinked = 0, size = 0,
        fd, lines, start, index, i, j, extraNum = 0, extraAlloc = 0,
	msgLen = 0, date = 0, listArgc, elemArgc, indexInfo = 0, numDel = 0;
    char buf[8092], *indexPtr = NULL, **linePtrPtr = NULL, *cPtr,
	 *to, *from, *cc, *subject, *flags, *msgBuf = NULL;
    CONST84 char **listArgv, **elemArgv;
    Tcl_HashTable items, status;
    char **extraPtrPtr = NULL;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tcl_DString reportDS;
    RatDbItem *itemPtr;
    struct stat sbuf;
    MESSAGECACHE elt;
    ssize_t l;
    struct tm tm;
    FILE *fp;

    /*
     * Initialize variables
     */
    if (0 == dbDir) {
	const char *value = RatGetPathOption(interp, "dbase_dir");
	if (NULL == value) {
	    return TCL_ERROR;
	}
	dbDir = cpystr(value);
    }
    if (0 == ident) {
	gethostname(buf, sizeof(buf));
	ident = (char*)ckalloc(strlen(buf)+16);
	snprintf(ident, strlen(buf)+16, "%s:%d", buf, (int)getpid());
    }

    /*
     * Check that the database directory exists. If not we return zeros
     */
    if (0 > stat(dbDir, &sbuf) ||
	    !S_ISDIR(sbuf.st_mode)) {
	Tcl_SetResult(interp, "0 0 0 0 0 {}", TCL_STATIC);
	return TCL_OK;
    }


    /*
     * Lock the database. We should also check that nobody else has
     * a read lock as well.
     */
    Lock(interp);
    if (IsRlocked(NULL)) {
	Unlock(interp);
	Tcl_SetResult(interp, "Some other process has locked the database.",
		TCL_STATIC);
	return TCL_ERROR;
    }

    /*
     * Check index.info file
     */
    snprintf(buf, sizeof(buf), "%s/index.info", dbDir);
    if (NULL == (fp = fopen(buf, "r"))) {
	Tcl_SetResult(interp, "Failed to open index.info file", TCL_STATIC);
	return TCL_ERROR;
    } else {
	if (2 != fscanf(fp, "%d %d", &i, &indexInfo)) {
            i = -1;
        }
	fclose(fp);
	if (i != DBASE_VERSION) {
	    Tcl_SetResult(interp, "Wrong version of dbase", TCL_STATIC);
	    Unlock(interp);
	    return TCL_ERROR;
	}
    }

    /*
     * Initialize variables
     */
    Tcl_DStringInit(&reportDS);
    Tcl_InitHashTable(&items, TCL_STRING_KEYS);
    Tcl_InitHashTable(&status, TCL_ONE_WORD_KEYS);

    /*
     * Get a list of messages actually stored
     */
    snprintf(buf, sizeof(buf), "%s/dbase", dbDir);
    RatDbBuildList(interp, &reportDS, "", buf, &items, fix);

    /*
     * Check the changes file for flag changes and store them in the
     * status hash table.
     */
    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    if (NULL != (fp = fopen(buf, "r"))) {
	while (fgets(buf, sizeof(buf), fp) != NULL && !feof(fp)) {
	    switch (buf[0]) {
	    case 'a':
		indexInfo++;
		break;
	    case 'd':
		indexInfo--;
		numDel++;
		break;
	    case 's':
		if (extraNum == extraAlloc) {
		    extraAlloc += 32;
		    extraPtrPtr = (char**)ckrealloc(extraPtrPtr,
			    extraAlloc*sizeof(char*));
		}
		extraPtrPtr[extraNum] = (char*)ckalloc(strlen(buf));
		sscanf(buf, "%*s %d %s", &index, extraPtrPtr[extraNum]);
		entryPtr = Tcl_CreateHashEntry(&status, (char*)index, &i);
		Tcl_SetHashValue(entryPtr, (ClientData)extraPtrPtr[extraNum]);
		extraNum++;
		break;
	    }
	}
	fclose(fp);
    }

    /*
     * Check the index file
     */
    snprintf(buf, sizeof(buf), "%s/index", dbDir);
    if (-1 != (fd = open(buf, O_RDONLY))) {
	/*
	 * Read file and build pointers to the lines
	 */
	fstat(fd, &sbuf);
	indexPtr = (char*)ckalloc(sbuf.st_size+1);
        if (sbuf.st_size != SafeRead(fd, indexPtr, sbuf.st_size)) {
            close(fd);
            return TCL_ERROR;
        }
	close(fd);
	indexPtr[sbuf.st_size] = '\0';
	for (lines = 0, cPtr = indexPtr; *cPtr; cPtr++) {
	    if ('\n' == *cPtr) {
		lines++;
	    }
	}
	linePtrPtr = (char**)ckalloc(sizeof(char*)*lines);
	for (cPtr = indexPtr, i = 0; cPtr && *cPtr && i < lines; i++) {
	    linePtrPtr[i] = cPtr;
	    if ((cPtr = strchr(cPtr, '\n'))) {
		*cPtr++ = '\0';
	    }
	}

	/*
	 * Now we are ready to reconstruct the index. We do this one entry
	 * a time. For each entry we read one line at a time and check the
	 * content against what we expected. We expect the following lines
	 * and contents:
	 *	to	   - any string
	 *	from	   - any string
	 *	cc	   - any string
	 *	message-id - any string
	 *	references - any string
	 *	subject	   - any string
	 *	date	   - a number
	 *	keywords   - any string
	 *	size	   - a number
	 *	status	   - any string
	 *	extime	   - a number
	 *	exevent	   - one of none, remove, incoming, backup and custom
	 *	filename   - (.*)/[0-9]+
	 * When we have found an index we check for the corresponding entry
	 * in the list of files and fill it in.
	 */
	for (start = index = 0; start < lines; index++) {
	    if (start > lines-RATDBETYPE_END) {
		break;
	    }
	    if (!Tcl_RegExpMatch(interp, linePtrPtr[start+DATE], EXP_NUM)
		    || !Tcl_RegExpMatch(interp, linePtrPtr[start+RSIZE],
					EXP_NUM)
		    || !Tcl_RegExpMatch(interp, linePtrPtr[start+EX_TIME],
					EXP_NUM)
		    || !Tcl_RegExpMatch(interp, linePtrPtr[start+EX_TYPE],
					EXP_TYPE)
		    || !Tcl_RegExpMatch(interp, linePtrPtr[start+FILENAME],
					EXP_FILE)){
		sprintf(buf, "Entry %d is malformed", index);
		Tcl_DStringAppendElement(&reportDS, buf);
		numMal++;

		/*
		 * We have found an malformed entry, first we search for the
		 * filename which should be the last item. From that we go
		 * backwards and try to collapse lines that somehow was
		 * splitted.
		 */
		i=0;
		while (!Tcl_RegExpMatch(interp,linePtrPtr[start+i],EXP_FILE)) {
		    if (start + (++i) == lines) {
			break;
		    };
		}
		i++;
		if (start+i >= lines) {
		    /*
		     * We have reached the end of the file
		     */
		    break;
		}

		/* 
		 * Here we should collapse the lines but for now we
		 * just continue with the next item. /MaF
		 */
		start += i;
		continue;

	    }
	    if (!(entryPtr =
		    Tcl_FindHashEntry(&items, linePtrPtr[start+FILENAME]))) {
		numAlone++;
		snprintf(buf, sizeof(buf),
			"Entry %d has no associated file '%s'",
			index, linePtrPtr[start+FILENAME]);
		Tcl_DStringAppendElement(&reportDS, buf);
		start += 13;
		continue;
	    }
	    itemPtr = (RatDbItem*)Tcl_GetHashValue(entryPtr);
	    for (i=0; i<RATDBETYPE_END; i++, start++) {
		if (i == STATUS && (entryPtr = Tcl_FindHashEntry(&status,
			(char*)numFound))) {
		    itemPtr->entry.content[i] =
			(char*)Tcl_GetHashValue(entryPtr);
		} else {
		    itemPtr->entry.content[i] = linePtrPtr[start];
		}
	    }
	    numFound++;
	}
    }

    /*
     * Check for unlinked messages
     * And calculate total size while we are at it.
     */
    for (entryPtr = Tcl_FirstHashEntry(&items, &search);
	    entryPtr;
	    entryPtr = Tcl_NextHashEntry(&search)) {
	itemPtr = (RatDbItem*)Tcl_GetHashValue(entryPtr);
	size += itemPtr->fileSize;
	if (itemPtr->entry.content[0]) {
	    continue;
	}
	numUnlinked++;
	if (fix) {
	    /*
	     * Generate index entries for this message
	     */
	    to = from = cc = subject = flags = NULL;
	    date = 0;
	    if (extraNum+8 >= extraAlloc) {
		extraAlloc += 32;
		extraPtrPtr = (char**)ckrealloc(extraPtrPtr,
			extraAlloc*sizeof(char*));
	    }
	    if (msgLen < itemPtr->fileSize+1) {
		msgLen = itemPtr->fileSize+4096;
		msgBuf = (char*)ckrealloc(msgBuf, msgLen);
	    }
	    snprintf(buf, sizeof(buf), "%s/dbase/%s",
		    dbDir, Tcl_GetHashKey(&items,entryPtr));
	    if (-1 == (fd = open(buf, O_RDONLY))) {
		continue;
	    }
	    l = SafeRead(fd, msgBuf, itemPtr->fileSize);
            if (l <= 0) {
                continue;
            }
	    msgBuf[l] = '\0';
	    close(fd);
	    if (NULL == (cPtr = strstr(msgBuf, "\n\n"))) {
		if (NULL == (cPtr = strstr(msgBuf, "\r\n\r"))) {
		    cPtr = msgBuf + strlen(msgBuf);
		}
	    }
	    *(++cPtr) = '\0';
	    RatMessageGetHeader(interp, msgBuf);
	    Tcl_SplitList(interp, Tcl_GetStringResult(interp),
		    &listArgc, &listArgv);
	    for (i=0; i<listArgc; i++) {
		Tcl_SplitList(interp, listArgv[i], &elemArgc, &elemArgv);
		if (!to && !strcasecmp(elemArgv[0], "to")) {
		    to = extraPtrPtr[extraNum++] = cpystr(elemArgv[1]);
		} else if (!from && !strcasecmp(elemArgv[0], "from")) {
		    from = extraPtrPtr[extraNum++] = cpystr(elemArgv[1]);
		} else if (!cc && !strcasecmp(elemArgv[0], "cc")) {
		    cc = extraPtrPtr[extraNum++] = cpystr(elemArgv[1]);
		} else if (!subject && !strcasecmp(elemArgv[0], "subject")) {
		    subject = extraPtrPtr[extraNum++] = cpystr(elemArgv[1]);
		} else if (!strcasecmp(elemArgv[0], "status") ||
			   !strcasecmp(elemArgv[0], "x-status")) {
		    if (flags) {
			flags=(char*)ckrealloc(flags,strlen(flags)+
				strlen(elemArgv[1])+1);
			strcpy(&flags[strlen(flags)], elemArgv[1]);
		    } else {
			flags = cpystr(elemArgv[1]);
		    }
		} else if (!strcasecmp(elemArgv[0], "date")) {
		    if (T == mail_parse_date(&elt,
                                             (unsigned char*)elemArgv[1])) {
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
		ckfree(elemArgv);
	    }
	    ckfree(listArgv);
	    if (flags) {
		extraPtrPtr[extraNum++] = flags;
	    }
	    itemPtr->entry.content[TO] = to ? to : "";
	    itemPtr->entry.content[FROM] = from ? from : "";
	    itemPtr->entry.content[CC] = cc ? cc : "";
	    itemPtr->entry.content[SUBJECT] = subject ? subject : "";
	    sprintf(buf, "%d", date);
	    itemPtr->entry.content[DATE] = extraPtrPtr[extraNum++]=cpystr(buf);
	    itemPtr->entry.content[KEYWORDS] = "LostMessage";
	    sprintf(buf, "%d", itemPtr->fileSize);
	    itemPtr->entry.content[RSIZE] =extraPtrPtr[extraNum++]=cpystr(buf);
	    itemPtr->entry.content[STATUS] = flags ? flags : "";
	    sprintf(buf, "%ld", time(NULL) + 60L*60L*24L*100L);
	    itemPtr->entry.content[EX_TIME] = extraPtrPtr[extraNum++] =
		cpystr(buf);
	    itemPtr->entry.content[EX_TYPE] = "backup";
	    itemPtr->entry.content[FILENAME] = Tcl_GetHashKey(&items,entryPtr);
	}
    }
    if (numUnlinked && fix) {
	Tcl_DStringAppendElement(&reportDS,
"The unlinked messages has been inserted with the keyword 'LostMessage'");
    }
    if (indexInfo != numFound+numUnlinked-numDel) {
	if (fix) {
	    sprintf(buf, "Number of messages in index.info was wrong "
		    "(was: %d is now: %d)",
		indexInfo, numFound+numUnlinked-numDel);
	} else {
	    sprintf(buf, "Number of messages in index.info is wrong "
		    "(was: %d should be: %d)",
		indexInfo, numFound+numUnlinked-numDel);
	}
	Tcl_DStringAppendElement(&reportDS, buf);
    }

    /*
     * Write new index if fixing and needing
     */
    if (fix && (numMal || numAlone || numUnlinked ||
	    indexInfo != numFound+numUnlinked-numDel)) {
	snprintf(buf, sizeof(buf), "%s/index", dbDir);
	fp = fopen(buf, "w");
	for (entryPtr = Tcl_FirstHashEntry(&items, &search), j=0;
		entryPtr;
		entryPtr = Tcl_NextHashEntry(&search)) {
	    itemPtr = (RatDbItem*)Tcl_GetHashValue(entryPtr);
	    for (i=0; i<RATDBETYPE_END; i++) {
		if (itemPtr->entry.content[i]) {
		    fputs(itemPtr->entry.content[i], fp);
		}
		fputc('\n', fp);
	    }
	    j++;
	}
	fclose(fp);
	snprintf(buf, sizeof(buf), "%s/index.info", dbDir);
	fp = fopen(buf, "w");
	fprintf(fp, "%d %d\n", DBASE_VERSION, j);
	fclose(fp);
	snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
	(void)unlink(buf);

	if (isRead) {
	    isRead = 0;
	    strlcpy(buf, "Popup $t(need_restart)", sizeof(buf));
	    Tcl_Eval(interp, buf);
	}
    }
    
    /*
     * Cleaning up
     */
    Unlock(interp);

    for (entryPtr = Tcl_FirstHashEntry(&items, &search);
	    entryPtr;
	    entryPtr = Tcl_NextHashEntry(&search)) {
	ckfree(Tcl_GetHashValue(entryPtr));
    }
    Tcl_DeleteHashTable(&items);
    Tcl_DeleteHashTable(&status);
    ckfree(indexPtr);
    ckfree(linePtrPtr);

    Tcl_ResetResult(interp);
    sprintf(buf, "%d", numFound);
    Tcl_AppendElement(interp, buf);
    sprintf(buf, "%d", numMal);
    Tcl_AppendElement(interp, buf);
    sprintf(buf, "%d", numAlone);
    Tcl_AppendElement(interp, buf);
    sprintf(buf, "%d", numUnlinked);
    Tcl_AppendElement(interp, buf);
    sprintf(buf, "%d", size);
    Tcl_AppendElement(interp, buf);
    Tcl_AppendElement(interp, Tcl_DStringValue(&reportDS));
    Tcl_DStringFree(&reportDS);
    if (extraAlloc) {
	for (i=0; i<extraNum; i++) {
	    ckfree(extraPtrPtr[i]);
	}
	ckfree(extraPtrPtr);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NoLFPrint --
 *
 *      Prints the given string, but replaces any newlines with space.
 *	Also handles null strings.
 *
 * Results:
 *	-1 if any error ocurred
 *
 * Side effects:
 *	The mentioned file is modified
 *
 *----------------------------------------------------------------------
 */

static int
NoLFPrint(FILE *fp, const char *s)
{
    unsigned char *cPtr;

    for (cPtr = (unsigned char*)s; cPtr && *cPtr; cPtr++) {
	if ('\n' == *cPtr) {
	    if (isspace(cPtr[1])) {
		cPtr++;
	    } else {
		fputc(' ', fp);
	    }
	} else {
	    fputc(*cPtr, fp);
	}
    }
    return fputc('\n', fp);
}


/*
 *----------------------------------------------------------------------
 *
 * DbaseConvert3to4 --
 *
 *      Convert version 3 of the database to version 4
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The databse index is rewritten
 *
 *----------------------------------------------------------------------
 */

static void
DbaseConvert3to4(Tcl_Interp *interp)
{
    char buf[1024];		/* Scratch area */
    char oldIndex[1024];	/* Name of old index file */
    char newIndex[1024];	/* Name of new index file */
    FILE *fpNewIndex;		/* File pointer to new index file */
    FILE *fpIndexinfo;		/* File pointer to new index.info file */
    Tcl_DString ds;		/* String to store converted texts in */
    int i, j;			/* Loop variables */
    char *s;			/* Scratch string pointer */
    int p, p2;			/* percentage counters */
    int numEntries = 0;		/* Number of entries in written file */

    RatLogF(interp, RAT_INFO, "converting_dbase", RATLOG_EXPLICIT, 0);
    strcpy(buf, "update idletasks");
    Tcl_Eval(interp, buf);

    snprintf(oldIndex, sizeof(oldIndex), "%s/index", dbDir);
    snprintf(newIndex, sizeof(newIndex), "%s/index.new", dbDir);
    if (0 == (fpNewIndex = fopen(newIndex, "w"))) {
	return;
    }

    Tcl_DStringInit(&ds);
    for (i=p=0,p2=-1; i < numRead; i++) {
	p = (i*100)/numRead;
	if (p != p2) {
	    RatLogF(interp, RAT_INFO, "converting_dbase", RATLOG_EXPLICIT,
		    (i*100)/numRead);
	    strcpy(buf, "update idletasks");
	    Tcl_Eval(interp, buf);
	    p2 = p;
	}
	if (0 != entryPtr[i].content[FROM]) {
	    numEntries++;
	    for (j=0; j<RATDBETYPE_END; j++) {
		for (s = entryPtr[i].content[j]; *s && !(0x80 & *s); s++);
		if (*s) {
		    Tcl_DStringSetLength(&ds, 0);
		    Tcl_ExternalToUtfDString(NULL, entryPtr[i].content[j], -1,
			    &ds);
		    s = Tcl_DStringValue(&ds);
		} else if (TO == j || FROM == j || CC == j || SUBJECT == j) {
		    s = RatDecodeHeader(interp,  entryPtr[i].content[j],
			    SUBJECT != j);
		} else {
		    s =  entryPtr[i].content[j];
		}
		if (0 > fprintf(fpNewIndex, "%s\n", s)) {
		    return;
		}
	    }
	}
    }
    fclose(fpNewIndex);
    rename(newIndex, oldIndex);
    snprintf(buf, sizeof(buf), "%s/index.info", dbDir);
    if (0 == (fpIndexinfo = fopen(buf, "w"))
	    || (0 > fprintf(fpIndexinfo, "%d %d\n", DBASE_VERSION, numEntries))
	    || (0 > fclose(fpIndexinfo))) {
	return;
    }
    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    unlink(buf);
    isRead = 0;
    ckfree(entryPtr[0].content[0]);
    ckfree(entryPtr);

    RatLog(interp, RAT_INFO, "", RATLOG_EXPLICIT);
}

/*
 *----------------------------------------------------------------------
 *
 * RatDbInfoCmd --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A standard tcl result.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDbaseInfoCmd(ClientData dummy, Tcl_Interp *interp, int objc,
                Tcl_Obj *const objv[])
{
    Tcl_Obj *robjv[4];

    if (0 == isRead) {
	if (TCL_OK != Read(interp)) {
	    goto losing;
	}
    } else {
	if (TCL_OK != Sync(interp, 0)) {
	    goto losing;
	}
    }

    robjv[0] = Tcl_NewLongObj(numRead);
    robjv[1] = Tcl_NewLongObj(firstDate);
    robjv[2] = Tcl_NewLongObj(lastDate);
    robjv[3] = Tcl_NewLongObj(totSize);
    Tcl_SetObjResult(interp, Tcl_NewListObj(4, robjv));
    return TCL_OK;

 losing:
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDbKeywordsCmd --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A standard tcl result.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDbaseKeywordsCmd(ClientData dummy, Tcl_Interp *interp, int objc,
                Tcl_Obj *const objv[])
{
    Tcl_HashTable keywords;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    int i, j, new, num, argc;
    Tcl_Obj *result, *robjv[2];
    char *s, buf[1024];
    const char **argv;

    Tcl_InitHashTable(&keywords, TCL_STRING_KEYS);
    
    /* Loop over messages */
    for (i = 0; i<numRead; i++) {
	if (!entryPtr[i].content[FROM]) {	/* Entry deleted */
	    continue;
	}

        /* Loop over keywords of message*/
        s = entryPtr[i].content[KEYWORDS];
        if ('{' == s[0] && '}' == s[strlen(s)-1]) {
            strlcpy(buf, s+1, sizeof(buf));
            if ('}' == buf[strlen(buf)-1]) {
                buf[strlen(buf)-1] = '\0';
            }
            s = buf;
        }
        if (TCL_OK != Tcl_SplitList(interp, s, &argc, &argv)) {
            continue;
        }
        for (j=0; j<argc; j++) {
            entry = Tcl_CreateHashEntry(&keywords, argv[j], &new);
            if (new) {
                Tcl_SetHashValue(entry, 1);
            } else {
                num = (int)Tcl_GetHashValue(entry);
                Tcl_SetHashValue(entry, num+1);
            }
        }
    }

    /* Build result */
    result = Tcl_NewObj();
    for (entry = Tcl_FirstHashEntry(&keywords, &search); entry;
         entry = Tcl_NextHashEntry(&search)) {
        robjv[0] = Tcl_NewStringObj(Tcl_GetHashKey(&keywords, entry), -1);
        robjv[1] = Tcl_NewIntObj((int)Tcl_GetHashValue(entry));
        Tcl_ListObjAppendElement(interp, result, Tcl_NewListObj(2, robjv));
    }
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDbSetInfo --
 *
 *      Update keywords, expiration_time and expiration action for a
 *      number of messages.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int RatDbSetInfo(Tcl_Interp *interp, int *indexes, int num_indexes,
                 Tcl_Obj *keywords, Tcl_Obj *ex_time, Tcl_Obj *ex_type)
{
    char buf[1024];	/* Name of index.changes file */
    FILE *indexFP;	/* FIle pointer to index.changes file */
    Tcl_Obj *indlist, *line, *lobjv[4];
    int i;
    
    /*
     * Check indexes for validity and build list of indexes
     */
    indlist = Tcl_NewObj();
    for (i=0; i<num_indexes; i++) {
        if (indexes[i] >= numRead || indexes[i] < 0) {
            Tcl_DecrRefCount(indlist);
            return TCL_ERROR;
        }
        Tcl_ListObjAppendElement(interp, indlist, Tcl_NewIntObj(indexes[i]));
    }

    /*
     * Prepare line
     */
    lobjv[0] = indlist;
    lobjv[1] = keywords;
    lobjv[2] = ex_time;
    lobjv[3] = ex_type;
    line = Tcl_NewListObj(4, lobjv);

    /*
     * Write entry to index.changes
     */
    Lock(interp);
    snprintf(buf, sizeof(buf), "%s/index.changes", dbDir);
    if (NULL == (indexFP = fopen(buf, "a"))) {
	Tcl_AppendResult(interp, "error opening (for append)\"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	Unlock(interp);
	return TCL_ERROR;
    }
    if (0 > fprintf(indexFP, "k 0 %s\n", Tcl_GetString(line))) {
	Tcl_AppendResult(interp, "Failed to write to file \"", buf, "\"",
		(char*) NULL);
	(void)fclose(indexFP);
	Unlock(interp);
	return TCL_ERROR;
    }
    if (0 != fclose(indexFP)) {
	Tcl_AppendResult(interp, "error closing file \"", buf,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	Unlock(interp);
	return TCL_ERROR;
    }

    Sync(interp, 0);
    Unlock(interp);
    return TCL_OK;
}
