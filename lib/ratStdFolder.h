/*
 * ratStdFolder.h --
 *
 *      Declarations of functions used in the Std folder and messages
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#ifndef _RATSTDFOLDER
#define _RATSTDFOLDER

#include "ratFolder.h"

/*
 * This list correlates to the ratStdTypeNames array in ratStdFolder.c
 */
typedef enum {
    RAT_UNIX,
    RAT_IMAP,
    RAT_POP,
    RAT_MH,
    RAT_MBX,
    RAT_DIS
} RatStdFolderType;

/*
 * Here we handle the events which may come from the mail-server
 * via parts of the mm interface.
 */
typedef void (HandleExists)(void *state, unsigned long nmsgs);
typedef void (HandleExpunged)(void *state, unsigned long index);
typedef struct {
    void *state;
    HandleExists *exists;
    HandleExpunged *expunged;
} FolderHandlers;
 
MAILSTREAM *Std_StreamOpen(Tcl_Interp *interp, char *name, long options,
			   int *errorFlagPtr, FolderHandlers *handlers);
void Std_StreamClose(Tcl_Interp *interp, MAILSTREAM *stream);
void Std_StreamCloseAllCached(Tcl_Interp *interp);
RatCreateProc Std_CreateProc;
RatGetHeadersProc Std_GetHeadersProc;
RatGetEnvelopeProc Std_GetEnvelopeProc;
RatCreateBodyProc Std_CreateBodyProc;
RatFetchTextProc Std_FetchTextProc;
RatEnvelopeProc Std_EnvelopeProc;
RatMsgDeleteProc Std_MsgDeleteProc;
RatMakeChildrenProc Std_MakeChildrenProc;
RatFetchBodyProc Std_FetchBodyProc;
RatBodyDeleteProc Std_BodyDeleteProc;
RatInfoProc Std_GetInfoProc;
RatGetInternalDateProc Std_GetInternalDateProc;

RatInfoProc Std_InfoProc;
RatSetInfoProc Std_SetInfoProc;

/*
 * used to store search results
 */
extern long *searchResultPtr;
extern int searchResultSize;
extern int searchResultNum;

/*
 * Used to store status results
 */
extern MAILSTATUS stdStatus;

/*
 * Controls if we should ignore loging calls or not
 */
extern int logIgnore;

/*
 * This is the private part of a std folder info structure.
 */

typedef struct StdFolderInfo {
    MAILSTREAM *stream;		/* Handler to c-client entity */
    int referenceCount;		/* Number of entities referencing this entry */
    int exists;			/* Number of messages which actually exists
				   in this folder */
    int error;                  /* Error status */
    RatStdFolderType type;	/* The exact type of this folder */
    FolderHandlers handlers;	/* The event handlers */
    char *mailbox;              /* Mailbox specifier */
} StdFolderInfo;

/*
 * The ClientData for each message entity
 */
typedef struct StdMessageInfo {
    MAILSTREAM *stream;
    MESSAGECACHE *eltPtr;
    ENVELOPE *envPtr;
    BODY *bodyPtr;
    RatStdFolderType type;
    char *spec;
} StdMessageInfo;

/* ratStdMessage.c */
extern void RatStdMsgStructInit(RatFolderInfoPtr infoPtr, Tcl_Interp *interp,
				int index, MAILSTREAM *stream,
				RatStdFolderType type);
extern char *RatStdMessageCreate (Tcl_Interp *interp, RatFolderInfoPtr infoPtr,
				  MAILSTREAM *stream, int msgNo);

#endif /* _RATSTDFOLDER */
