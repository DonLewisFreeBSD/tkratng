/*
 * ratMailcap.c --
 *
 *	This file contains support for reading & parsing mailcap files
 *	Mailcap files are defined in rfc1343
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notices is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"

/*
 * Each entry is represented by one of the following structures
 */
typedef struct {
    char *type;
    char *subtype;
    char *test;
    char *view;
    char *compose;
    char *composetyped;
    char *edit;
    char *print;
    unsigned int needsterminal : 1;
    unsigned int copiousoutput : 1;
    char *description;
    char *bitmap;
} MailcapEntry;

/*
 * Id of the current load of the table
 */
static int tableId = 0;

/*
 * Pointer to the current table as well as current size, allocated size
 * and increment.
 */
static MailcapEntry *tablePtr = NULL;
static int tableSize = 0;
static int tableAllocated = 0;

/*
 * Local functions
 */
static void MailcapReload(Tcl_Interp *interp);
static char *ExpandString(Tcl_Interp *interp, BodyInfo *bodyInfoPtr, char *s,
	char **filePtr);


/*
 *----------------------------------------------------------------------
 *
 * MailcapReload --
 *
 *      Reloads the mailcaps into memory.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The mailcap files will be loaded.
 *
 *
 *----------------------------------------------------------------------
 */

static void
MailcapReload(Tcl_Interp *interp)
{
    static char **textBlock = NULL;
    static int numTextBlocks = 0;
    static int allocTextBlocks = 0;
    Tcl_DString ds;
    struct stat sbuf;
    char buf[1024], *cPtr, **cPtrPtr, *dstPtr, *data, *tPtr, *pPtr;
    CONST84 char *s;
    int i, fd, line;

    /*
     * Free old data
     */
    for (i=0; i<numTextBlocks; i++) {
	ckfree(textBlock[i]);
    }
    numTextBlocks = tableSize = 0;

    /*
     * Construct path to search
     */
    Tcl_DStringInit(&ds);
    if ((s = getenv("MAILCAP"))) {
	Tcl_DStringAppend(&ds, s, -1);
	Tcl_DStringAppend(&ds, ":", 1);
    }
    Tcl_DStringAppend(&ds,
	    Tcl_GetVar2(interp, "option", "mailcap_path", TCL_GLOBAL_ONLY),-1);
    /*
     * Check all files mentioned in the path
     */
    for (pPtr = Tcl_DStringValue(&ds); *pPtr; ) {
	for (s=pPtr++; *pPtr && ':' != *pPtr; pPtr++);
	if (*pPtr) {
	    *pPtr++ = '\0';
	}
	s = RatTranslateFileName(interp, s);
	if (stat(s, &sbuf) || !S_ISREG(sbuf.st_mode)) {
	    /*
	     * No such file
	     */
	    continue;
	}

	/*
	 * Now we should read the file into a buffer
	 */
	if (-1 == (fd = open(s, O_RDONLY))) {
	    continue;
	}
	if (allocTextBlocks == numTextBlocks) {
	    allocTextBlocks += 5;
	    textBlock =
		    (char**)ckrealloc(textBlock,allocTextBlocks*sizeof(char*));
	}
	data = textBlock[numTextBlocks++] = (char*)ckalloc(sbuf.st_size+1);
	read(fd, data, sbuf.st_size);
	close(fd);
	data[sbuf.st_size] = '\0';

	/*
	 * Extract the useful data (join lines and discard non-data lines)
	 *
	 * This is done in a loop which loops over all the data.
	 * - In the loop we start by skipping all initial whitespace.
	 * - Then we check the first character. If it is an '#' then we
	 *   skip until the next newline and restart the loop.
	 * - Else we start copying characters to the real area until
	 *   we find a newline. When we do we check the preceding character
	 *   and if it was a '\' we skip the last two characters and
	 *   continue to copy the next line etc.
	 */
	for (i = line = 0, dstPtr = data; i < sbuf.st_size; i++) {
	    while (data[i] && isspace((unsigned char)data[i])) {
	        if ('\n' == data[i]) {
		    line++;
		}
	       i++;
	    }
	    if (!data[i]) {
		break;
	    }
	    if ('#' == data[i]) {
		for (; '\n' != data[i] && data[i]; i++);
		line++;
		continue;
	    }
	    cPtr = dstPtr;
	    do {
		while (data[i] && '\n' != data[i]) {
		    *dstPtr++ = data[i++];
		}
		if ('\\' != data[i-1]) {
		    break;
		}
		dstPtr -= 1;
		line++;
	    } while (data[i++]);
	    *dstPtr++ = '\0';
	    line++;
	    /*
	     * Parse the line into an entry
	     *
	     * - First we make sure there are enough empty entries in the table
	     * - Then we find the first delimiter in the type specification.
	     *   If this is ';' then we assume that the subtype should be '*'.
	     *   If it is '/' then we read the subtype.
	     * - The we read the value (we start by discarding whitespace
	     *   and then we copy everything until the ';'.
	     * - Finally we loop over the other entries and read them
	     */
	    if (tableSize == tableAllocated) {
		tableAllocated = tableSize+64;
		tablePtr = (MailcapEntry*)ckrealloc(tablePtr,
			tableAllocated*sizeof(MailcapEntry));
	    }
	    tablePtr[tableSize].type = cPtr;
	    for (;'/' != *cPtr && ';' != *cPtr && *cPtr; cPtr++);
	    if (!*cPtr) {
		RatLogF(interp, RAT_ERROR, "syntax_error", RATLOG_TIME,s,line);
		continue;
	    }
	    if (';' == *cPtr) {
		*cPtr++ = '\0';
		tablePtr[tableSize].subtype = "*";
	    } else {
		*cPtr++ = '\0';
		tablePtr[tableSize].subtype = cPtr;
		for (;';' != *cPtr && *cPtr; cPtr++);
		if (!*cPtr) {
		    RatLogF(interp, RAT_ERROR, "syntax_error", RATLOG_TIME,
			    s, line);
		    continue;
		}
		for (tPtr = cPtr-1;
			*tPtr && isspace((unsigned char)*tPtr); tPtr--) {
		    *tPtr = '\0';
		}
		*cPtr++ = '\0';
	    }
	    for (;isspace((unsigned char)*cPtr) && *cPtr; cPtr++);
	    if (!*cPtr) {
		RatLogF(interp, RAT_ERROR, "syntax_error", RATLOG_TIME,s,line);
		continue;
	    }
	    tablePtr[tableSize].view = cPtr;
	    for (;*cPtr && (';' != *cPtr || '\\' == *(cPtr-1)); cPtr++);
	    tablePtr[tableSize].test = NULL;
	    tablePtr[tableSize].compose = NULL;
	    tablePtr[tableSize].composetyped = NULL;
	    tablePtr[tableSize].edit = NULL;
	    tablePtr[tableSize].print = NULL;
	    tablePtr[tableSize].description = NULL;
	    tablePtr[tableSize].bitmap = NULL;
	    tablePtr[tableSize].needsterminal = 0;
	    tablePtr[tableSize].copiousoutput = 0;
	    while (*cPtr) {
		*cPtr++ = '\0';
		cPtrPtr = NULL;
		for (;isspace((unsigned char)*cPtr) && *cPtr; cPtr++);
		if (!strncasecmp(cPtr, "test", 4)) {
		    cPtrPtr = &tablePtr[tableSize].test;
		} else if (!strncasecmp(cPtr, "composetyped", 12)) {
		    cPtrPtr = &tablePtr[tableSize].composetyped;
		} else if (!strncasecmp(cPtr, "compose", 7)) {
		    cPtrPtr = &tablePtr[tableSize].compose;
		} else if (!strncasecmp(cPtr, "edit", 4)) {
		    cPtrPtr = &tablePtr[tableSize].edit;
		} else if (!strncasecmp(cPtr, "print", 5)) {
		    cPtrPtr = &tablePtr[tableSize].print;
		} else if (!strncasecmp(cPtr, "description", 11)) {
		    cPtrPtr = &tablePtr[tableSize].description;
		} else if (!strncasecmp(cPtr, "bitmap", 6)) {
		    cPtrPtr = &tablePtr[tableSize].bitmap;
		} else if (!strncasecmp(cPtr, "needsterminal", 13)) {
		    tablePtr[tableSize].needsterminal = 1;
		} else if (!strncasecmp(cPtr, "copiousoutput", 13)) {
		    tablePtr[tableSize].copiousoutput = 1;
		}
		for (;isalpha((unsigned char)*cPtr) && *cPtr; cPtr++);
		for (;isspace((unsigned char)*cPtr) && *cPtr; cPtr++);
		if (!cPtrPtr) {
		    continue;
		}
		if ('=' != *cPtr) {
		     snprintf(buf, sizeof(buf),
			    "Syntax error in %s at line %d", s, line);
		     for (; *cPtr; cPtr++);
		     continue;
		}
		for (cPtr++;isspace((unsigned char)*cPtr) && *cPtr; cPtr++);
		if (!*cPtr) {
		     snprintf(buf, sizeof(buf),
			    "Syntax error in %s at line %d", s, line);
		     for (; *cPtr; cPtr++);
		     continue;
		}
		*cPtrPtr = cPtr;
		for (;*cPtr && (';' != *cPtr || '\\' == *(cPtr-1)); cPtr++);
	    }
	    tableSize++;
	}
    }
    tableId++;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpandString --
 *
 *      Expands the given string. If filePtr is null the any '%s' in the
 *	string will be left as is.
 *
 * Results:
 *	Returns a pointer to a static area which contains the expanded
 *	string. If the string contained any "%s" then a pointer to the
 *	filename they were replaced with will be placed in *filePtr. If
 *	not then this value will be NULL.
 *	If the expansion failed then NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static char*
ExpandString(Tcl_Interp *interp, BodyInfo *bodyInfoPtr, char *s,char **filePtr)
{
    static Tcl_DString ds;
    static Tcl_DString file;
    static int init = 0;
    char *cPtr, *srcPtr;
    PARAMETER *parmPtr;
    int l;

    /*
     * Initialize the string the first time.
     */
    if (!init) {
	Tcl_DStringInit(&ds);
	Tcl_DStringInit(&file);
	init = 1;
    }

    if (filePtr) {
	*filePtr = NULL;
    }
    Tcl_DStringSetLength(&ds, 0);
    Tcl_DStringSetLength(&file, 0);
    for (srcPtr = s; *srcPtr; ) {
	if ('\\' == *srcPtr) {
	    Tcl_DStringAppend(&ds, ++srcPtr, 1);
	    if (*srcPtr) {
		srcPtr++;
	    }
	    continue;
	}
	if ('%' != *srcPtr) {
	    Tcl_DStringAppend(&ds, srcPtr++, 1);
	    continue;
	}
	srcPtr++;
	if ('s' == *srcPtr) {
	    if (filePtr) {
		if (0 == Tcl_DStringLength(&file)) {
		    Tcl_DStringAppend(&file, "/tmp/rat.", -1);
		    RatGenIdCmd(NULL, interp, 0, NULL);
		    Tcl_DStringAppend(&file, Tcl_GetStringResult(interp), -1);
		    *filePtr = Tcl_DStringValue(&file);
		}
		Tcl_DStringAppend(&ds, Tcl_DStringValue(&file), -1);
	    } else {
		Tcl_DStringAppend(&ds, "%s", 2);
	    }
	    srcPtr++;
	    continue;
	}
	if ('t' == *srcPtr) {
	    for (cPtr = body_types[bodyInfoPtr->bodyPtr->type]; *cPtr; cPtr++){
		if (strchr("|<>%*?\"`'", *cPtr)) {
		    Tcl_DStringAppend(&ds, " ", 1);
		} else {
		    Tcl_DStringAppend(&ds, cPtr, 1);
		}
	    }
	    Tcl_DStringAppend(&ds, "/", 1);
	    for (cPtr = bodyInfoPtr->bodyPtr->subtype; *cPtr; cPtr++) {
		if (strchr("|<>%*?\"`'", *cPtr)) {
		    Tcl_DStringAppend(&ds, " ", 1);
		} else {
		    Tcl_DStringAppend(&ds, cPtr, 1);
		}
	    }
	    srcPtr++;
	    continue;
	}
	if ('{' != *srcPtr++) {
	    if (filePtr) {
		*filePtr = NULL;
	    }
	    return NULL;
	}
	for (cPtr = srcPtr, l = 0; *srcPtr && '}' != *srcPtr; srcPtr++, l++);
	if (*srcPtr) {
	    srcPtr++;
	}
	for (parmPtr = bodyInfoPtr->bodyPtr->parameter; parmPtr;
		parmPtr = parmPtr->next) {
	    if (!strncasecmp(cPtr, parmPtr->attribute, l)) {
		break;
	    }
	}
	if (!parmPtr) {
	    if (filePtr) {
		*filePtr = NULL;
	    }
	    return NULL;
	}
	/*
	 * Copy the parameter value and in the process we remove any
	 * chanacters that might be used to slip a trojan horse in
	 * through our gates.
	 */
	for (cPtr = parmPtr->value; *cPtr; cPtr++) {
	    if (strchr("|<>%*?\"`'", *cPtr)) {
		Tcl_DStringAppend(&ds, " ", 1);
	    } else {
		Tcl_DStringAppend(&ds, cPtr, 1);
	    }
	}
    }
    return Tcl_DStringValue(&ds);
}


/*
 *----------------------------------------------------------------------
 *
 * RatMcapFindCmd --
 *
 *      Find a matching mailcap entry for a bodypart
 *
 * Results:
 *	See ../doc/interface
 *
 * Side effects:
 *	The mailcap files may be loaded.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatMcapFindCmd(Tcl_Interp *interp, BodyInfo *bodyInfoPtr)
{
    char *cmd, *file, *s;
    Tcl_Channel channel;
    int i;
    Tcl_Obj *rPtr;

    /*
     * We start by making sure that the mailcap files have been loaded
     */
    if (0 == tableId) {
	MailcapReload(interp);
    }

    /*
     * Loop through all entries and check them.
     * - First we check the type/subtype for match.
     * - If they matched we check eventual test commands
     */
    for (i=0; i<tableSize; i++) {
	if (strcasecmp(tablePtr[i].type,body_types[bodyInfoPtr->bodyPtr->type])
		|| ('*' != *tablePtr[i].subtype
		    && strcasecmp(tablePtr[i].subtype,
				  bodyInfoPtr->bodyPtr->subtype))) {
	    continue;
	}
	if (tablePtr[i].test) {
	    if (!(cmd = ExpandString(interp, bodyInfoPtr, tablePtr[i].test,
		    &file))) {
		continue;
	    }
	    if (file) {
		channel = Tcl_OpenFileChannel(interp, file, "w", 0666);
		RatBodySave(interp, channel, bodyInfoPtr, 0, 1);
		Tcl_Close(interp, channel);
	    }
	    if (system(cmd)) {
		if (file) {
		    unlink(file);
		}
		continue;
	    }
	    if (file) {
		unlink(file);
	    }
	}
	rPtr = Tcl_NewObj();
	s = ExpandString(interp, bodyInfoPtr, tablePtr[i].view, NULL);
	Tcl_ListObjAppendElement(interp, rPtr, Tcl_NewStringObj(s, -1));
	Tcl_ListObjAppendElement(interp, rPtr,
				 Tcl_NewBooleanObj(tablePtr[i].needsterminal));
	Tcl_ListObjAppendElement(interp, rPtr,
				 Tcl_NewBooleanObj(tablePtr[i].copiousoutput));
	Tcl_ListObjAppendElement(interp, rPtr,
				 Tcl_NewStringObj(tablePtr[i].description,-1));
	Tcl_ListObjAppendElement(interp, rPtr,
				 Tcl_NewStringObj(tablePtr[i].bitmap, -1));
	Tcl_SetObjResult(interp, rPtr);
	return TCL_OK;
    }

    Tcl_SetResult(interp, "{} 0 0 {} {}", TCL_STATIC);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatMailcapReloadCmd --
 *
 *      This is just a wrapper which calls MailcapReload
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	The mailcap files will be loaded.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatMailcapReloadCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		    Tcl_Obj *const objv[])
{
    MailcapReload(interp);
    return TCL_OK;
}
