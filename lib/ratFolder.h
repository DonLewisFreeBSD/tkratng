/*
 * ratFolder.h --
 *
 *      Declarations of types used in the folder and messages system.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#ifndef _RATFOLDER
#define _RATFOLDER

#include "rat.h"

/*
 * The following is used as an argument to infoProc() and specifies
 * exactly which type of information we want.
 */
typedef enum {
    RAT_FOLDER_SUBJECT,		/* The subject of the message */
    RAT_FOLDER_CANONSUBJECT,	/* The canonical subject of the message. This
				   is the subject without any leading Re:*/
    RAT_FOLDER_NAME,		/* The full name of the sender (From:)
				   or To: if From: is me. Both fall
				   back to "RAT_FOLDER_MAIL" below
				   is the subject without any leading Re:*/
    RAT_FOLDER_ANAME,		/* Like RAT_FOLDER_NAME but without special
                                   handling of me-case */
    RAT_FOLDER_MAIL_REAL,	/* The mail address of the sender (From:) */
    RAT_FOLDER_MAIL,		/* The mail address of the sender (From:)
				   or To: if From: is me */
    RAT_FOLDER_NAME_RECIPIENT,	/* The full name of the recipient if available
				 * otherwise the mail address */
    RAT_FOLDER_MAIL_RECIPIENT,	/* The mail address of the recipient */
    RAT_FOLDER_SIZE,		/* The approximate size of the message in
				 * octets */
    RAT_FOLDER_SIZE_F,		/* The approximate size of the message in
				 * octets, as a mangled number */
    RAT_FOLDER_DATE_F,		/* The date of the message (formatted) */
    RAT_FOLDER_DATE_N,		/* The date of the message (numeric) */
    RAT_FOLDER_DATE_IMAP4,	/* The date of the message (imap4 format) */
    RAT_FOLDER_STATUS,		/* The status of the message */
    RAT_FOLDER_TYPE,		/* The type/subtype string */
    RAT_FOLDER_PARAMETERS,	/* A list of parameters */
    RAT_FOLDER_INDEX,		/* The index of this message in the folder */
    RAT_FOLDER_TO,		/* The To: header line */
    RAT_FOLDER_FROM,		/* The From: header line */
    RAT_FOLDER_SENDER,		/* The Sender: header line */
    RAT_FOLDER_CC,		/* The CC: header line */
    RAT_FOLDER_REPLY_TO,	/* The Reply-To: header line */
    RAT_FOLDER_FLAGS,		/* The flags list in imap4 format */
    RAT_FOLDER_UNIXFLAGS,	/* The flags list in unix format*/
    RAT_FOLDER_MSGID,		/* The message ID */
    RAT_FOLDER_REF,		/* The In-Reply-to header */
    RAT_FOLDER_THREADING,	/* Threading information */
    RAT_FOLDER_UID,	        /* Message UID */
    RAT_FOLDER_END
} RatFolderInfoType;

/*
 * These are the possible flags.
 *
 * OBSERVE: if you change this list you MUST update the flag_name array
 * in ratFolder.c
 */
typedef enum {
    RAT_SEEN,			/* Message content has been seen by the user */
    RAT_DELETED,		/* Message is marked for deletion */
    RAT_FLAGGED,		/* Message is flagged */
    RAT_ANSWERED,		/* Message has been answered */
    RAT_DRAFT,			/* Message is a draft */
    RAT_RECENT,			/* Message is a not seen but has been in the
				   folder for some time */
    RAT_FLAG_END
} RatFlag;
typedef struct {
    char *imap_name;	/* The name of the flag when used with c-client */
    char *tkrat_name;   /* The name of the flag when used in tkrat */
    char unix_char;	/* Character representing flag in unix mailboxes */
} flag_name_t;
extern flag_name_t flag_name[];

/*
 * The different types of updates which can be performed
 */
typedef enum {
    RAT_UPDATE,		/* Only check for new mail */
    RAT_CHECKPOINT,	/* Checkpoint flags etc */
    RAT_SYNC		/* Do an expunge on the folder */
} RatUpdateType;

/*
 * The different sort methods
 */
typedef enum {SORT_NONE, SORT_SUBJECT, SORT_SUBJDATE, SORT_THREADED,
	      SORT_SENDER, SORT_SENDERDATE, SORT_DATE, SORT_SIZE} SortOrder;

/*
 * Below follows typedefs which declares the types of the function pointers
 * that can be found in the RatFolderInfo structure. Each folder MUST
 * provide all of these functions. The functions in question are:
 *
 * void initProc(RatFolderInfo *infoPtr, Tcl_Interp *interp, int index)
 *
 * 	This procedure should initialize the privatePtr part of
 *	the folder structure.
 *
 * void finalProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp)
 *	Does final initialization when creating the folder
 *
 * int closeProc(RatFolderInfo *infoPtr, Tcl_Interp *interp, int expunge)
 *
 *	This procedure should close the folder and free all private
 *	data structures. It should normally return TCL_OK, unless
 *	something goes wrong in which case it should return TCL_ERROR
 *	and leave an error message in interp->result.
 *
 * int updateProc(RatFolderInfo *infoPtr, Tcl_Interp *interp,RatUpdateType mode)
 *
 *	This procedure should update the folder. It return -1 on errors,
 *	otherwise the number of new messages is returned.
 *
 * int insertProc(RatFolderInfo *infoPtr, Tcl_Interp *interp, int argc,
 *		  char *argv[])
 *	This function should add the messages passed in argc/argv to
 *	the folder. The function can assume that msgCmdPtr has enough
 *	room to hold all new messages, and that all arguments are valid
 *	messages.
 *	The return value should normally be TCL_OK, but in case of error
 *	it should be TCL_ERROR and a message shpuld then be left in
 *	interp->result.
 *
 * void setFlagProc(RatFolderInfo *infoPtr, Tcl_Interp *interp, int index,
 *		   RatFlag flag, int value)
 *
 *	Sets the specified flag for message specified by 'index' to the
 *	value passed (only boolean values allowed).
 *
 * int getFlagProc(RatFolderInfo *infoPtr, Tcl_Interp *interp, int index,
 *		     RatFlag flag)
 *
 *	Gets the value of the specified flag for message specified by 'index'.
 *
 * Tcl_Obj *infoProc(Tcl_Interp *interp, ClientData clientData,
 *		     RatFolderInfoType type, int index)
 *
 *	Returns information about a message.
 *
 * void setInfoProc(Tcl_Interp *interp, ClientData clientData,
 *		  RatFolderInfoType type, int index, Tcl_Obj *oPtr)
 *
 *	Sets information about a message.
 *
 * char *createProc(RatFolderInfo *infoPtr, Tcl_Interp *interp, int index)
 *
 *	This function should create a message command for the message
 *	specified by "index". The message command is returned.
 *
 * int syncProc(RatFolderInfoPtr infoPtr, Tcl_Interp *interp)
 *	Does a network synchronization for the given folder
 */



typedef void (RatInitProc) (RatFolderInfoPtr infoPtr,
	Tcl_Interp *interp, int index);
typedef void (RatFinalProc) (RatFolderInfoPtr infoPtr, Tcl_Interp *interp);
typedef int (RatCloseProc) (RatFolderInfoPtr infoPtr, Tcl_Interp *interp,
	int expunge);
typedef int (RatUpdateProc) (RatFolderInfoPtr infoPtr,
	Tcl_Interp *interp, RatUpdateType mode);
typedef int (RatInsertProc) (RatFolderInfoPtr infoPtr,
	Tcl_Interp *interp, int argc, char *argv[]);
typedef int (RatSetFlagProc) (RatFolderInfoPtr infoPtr,
	Tcl_Interp *interp, int *ilist, int count, RatFlag flag, int value);
typedef int (RatGetFlagProc) (RatFolderInfoPtr infoPtr,
	Tcl_Interp *interp, int index, RatFlag flag);
typedef Tcl_Obj* (RatInfoProc) (Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index);
typedef void (RatSetInfoProc) (Tcl_Interp *interp, ClientData clientData,
	RatFolderInfoType type, int index,Tcl_Obj *oPtr);
typedef char* (RatCreateProc) (RatFolderInfoPtr infoPtr,
	Tcl_Interp *interp, int index);
typedef int (RatSyncProc) (RatFolderInfoPtr infoPtr, Tcl_Interp *interp);

/*
 * An instance of the structure below is created for each folder. It is then
 * used to hold the folder's internal state.
 */
typedef struct RatFolderInfo {
    char *cmdName;		/* Name of the folder command */
    char *name;			/* Mailbox name (this is a pointer which
				 * may be ckfree():ed) */
    char *type;			/* Type of folder */
    char *ident_def;		/* Definition id of folder */
    int refCount;		/* Reference count (<=0 means closing) */
    SortOrder sortOrder;	/* Sort order for folder */
    Tcl_Obj *role;		/* Role to use */
    int sortOrderChanged;	/* Non null if sort order has changed */
    int reverse;		/* If the sort order should be reversed */
    int number;			/* Number of messages in folder */
    int recent;			/* Number of recent messages in folder */
    int unseen;			/* Number of unseen messages in folder */
    int size;			/* Approximate size of folder, or -1 if the
				 * folder type doesn't support size. */
    int allocated;		/* The number of messages that fits into the
				 * following lists. */
    char **msgCmdPtr;		/* A list of pointers to the message commands,
				 * or NULL's if the commands haven't been
				 * created yet. All these strings will be
				 * ckfree()ed sooner or later. */
    ClientData **privatePtr;	/* Pointer to folder private data */
    int *presentationOrder;	/* The order in which these messages should
				 * be presented to the user. The first element
				 * of this list is the index of the first
				 * message to show etc. */
    int flagsChanged;		/* Non null if the flags has been changed
				 * since the last checkpoint */
    RatInitProc *initProc;
    RatFinalProc *finalProc;
    RatCloseProc *closeProc;
    RatUpdateProc *updateProc;
    RatInsertProc *insertProc;
    RatSetFlagProc *setFlagProc;
    RatGetFlagProc *getFlagProc;
    RatInfoProc *infoProc;
    RatSetInfoProc *setInfoProc;
    RatCreateProc *createProc;
    RatSyncProc *syncProc;
    ClientData private, private2;  /* Data private for each folder type */
    struct RatFolderInfo *nextPtr; /* Pointer to next folder (if any)*/
} RatFolderInfo;

/*
 * Global list of folders
 */
extern RatFolderInfo *ratFolderList;

/*
 * The different types of messages. These are for internal use only.
 */
typedef enum { RAT_CCLIENT_MESSAGE,
	       RAT_DBASE_MESSAGE,
	       RAT_FREE_MESSAGE } RatMessageType;

/*
 * The state of the address is me check
 */
typedef enum { RAT_ISME_YES,
	       RAT_ISME_NO,
	       RAT_ISME_UNKOWN } RatIsMeStatus;

/*
 * The ClientData for each message entity
 */
typedef struct BodyInfo BodyInfo;
typedef struct MessageInfo {
    RatFolderInfo *folderInfoPtr;
    char name[16];
    RatMessageType type;
    int msgNo;
    RatIsMeStatus fromMe;
    RatIsMeStatus toMe;
    BodyInfo *bodyInfoPtr;
    ClientData clientData;
    Tcl_Obj *info[RAT_FOLDER_END];
} MessageInfo;

/*
 * The different signed statuses
 */
typedef enum { RAT_UNSIGNED,
	       RAT_UNCHECKED,
	       RAT_SIG_GOOD,
	       RAT_SIG_BAD } RatSigStatus;

/*
 * The ClientData for each bodypart entity
 */
struct BodyInfo {
    char *cmdName;
    MessageInfo *msgPtr;
    RatMessageType type;
    BODY *bodyPtr;
    BodyInfo *firstbornPtr;
    BodyInfo *nextPtr;
    char *containedEntity;
    RatSigStatus sigStatus;
    Tcl_DString *pgpOutput;
    int encoded;
    BodyInfo *secPtr;
    BodyInfo *altPtr;
    Tcl_DString *decodedTextPtr;
    ClientData clientData;
};

/*
 * The different operations that can be made on messages and bodyparts
 * which are specific for every type.
 */
typedef char* (RatGetHeadersProc) (Tcl_Interp *interp, MessageInfo *msgPtr);
typedef char* (RatGetEnvelopeProc) (Tcl_Interp *interp, MessageInfo *msgPtr);
typedef BodyInfo* (RatCreateBodyProc) (Tcl_Interp *interp, MessageInfo *msgPtr);
typedef char* (RatFetchTextProc) (Tcl_Interp *interp, MessageInfo *msgPtr);
typedef ENVELOPE* (RatEnvelopeProc) (MessageInfo *msgPtr);
typedef void (RatMsgDeleteProc) (MessageInfo *msgPtr);
typedef void (RatMakeChildrenProc) (Tcl_Interp *interp, BodyInfo *bodyPtr);
typedef char* (RatFetchBodyProc) (BodyInfo *bodyPtr, unsigned long *lengthPtr);
typedef void (RatBodyDeleteProc) (BodyInfo *bodyPtr);
typedef MESSAGECACHE* (RatGetInternalDateProc) (Tcl_Interp *interp,
	MessageInfo *msgPtr);

/*
 * The following structure defines which functions to call for to
 * perform certain message type operations.
 */
typedef struct {
    RatGetHeadersProc *getHeadersProc;
    RatGetEnvelopeProc *getEnvelopeProc;
    RatInfoProc *getInfoProc;
    RatCreateBodyProc *createBodyProc;
    RatFetchTextProc *fetchTextProc;
    RatEnvelopeProc *envelopeProc;
    RatMsgDeleteProc *msgDeleteProc;
    RatMakeChildrenProc *makeChildrenProc;
    RatFetchBodyProc *fetchBodyProc;
    RatBodyDeleteProc *bodyDeleteProc;
    RatGetInternalDateProc *getInternalDateProc;
} MessageProcInfo;

/*
 * This structure holds a parsed list expression
 */
typedef struct {
    int size;			/* How many items the lists below has */
    char **preString;		/* Any characters that should be inserted
				   befor the next data part. */
    RatFolderInfoType *typeList;/* The type of the data */
    int *fieldWidth;		/* How wide this field should be (0=variable)*/
    int *leftJust;		/* True if it should be left justified */
    char *postString;		/* Any character sthat should be appended. */
} ListExpression;

/*
 * Folder management operations
 */
typedef enum {
    RAT_MGMT_CREATE,
    RAT_MGMT_CHECK,
    RAT_MGMT_DELETE,
    RAT_MGMT_SUBSCRIBE,
    RAT_MGMT_UNSUBSCRIBE
} RatManagementAction;

/* ratFolder.c (note that this file also exports functions in rat.h) */
extern RatFolderInfo *RatGetOpenFolder(Tcl_Interp *interp, Tcl_Obj *defPtr);
extern RatFolderInfo* RatOpenFolder(Tcl_Interp *interp, Tcl_Obj *def);
extern char* RatFolderCmdGet(Tcl_Interp *interp, RatFolderInfo *infoPtr,
			     int index);
extern void RatFolderCmdSetFlag(Tcl_Interp *interp, RatFolderInfo *infoPtr,
				int *ilist, int count,RatFlag flag, int value);
extern Tcl_Obj *RatFolderCanonalizeSubject (const char *s);
extern Tcl_Obj *RatGetMsgInfo(Tcl_Interp *interp, RatFolderInfoType type,
	MessageInfo *msgPtr, ENVELOPE *envPtr, BODY *bodyPtr,
	MESSAGECACHE *eltPtr, int size);
extern char* MsgFlags(MESSAGECACHE *eltPtr);
extern MESSAGECACHE *RatParseFrom(const char *from);
extern int RatFolderClose(Tcl_Interp *interp, RatFolderInfo *infoPtr,
			  int force);
extern int RatFolderInsert(Tcl_Interp *interp, RatFolderInfo *infoPtr,
			   int num, char **msgs);
extern int RatUpdateFolder(Tcl_Interp *interp, RatFolderInfo *infoPtr,
			   RatUpdateType mode);
extern char *RatGetFolderSpec(Tcl_Interp *interp, Tcl_Obj *def);
extern Tcl_Obj *RatExtractRef(CONST84 char *text);

/* ratStdFolder.c */
extern int RatStdFolderInit(Tcl_Interp *interp);
extern RatFolderInfo *RatStdFolderCreate(Tcl_Interp *interp, Tcl_Obj *defPtr);
extern MAILSTREAM* OpenStdFolder(Tcl_Interp *interp, char *spec, void *stdPtr);
extern void CloseStdFolder(Tcl_Interp *interp, MAILSTREAM *stream);
extern int RatStdManageFolder(Tcl_Interp *interp, RatManagementAction op,
			      int mbx, Tcl_Obj *fptr);
void RatStdCheckNet(Tcl_Interp *interp);

/* ratDbFolder.c */
extern int RatDbFolderInit (Tcl_Interp *interp);
extern RatFolderInfo *RatDbFolderCreate(Tcl_Interp *interp, Tcl_Obj *defPtr);
extern RatInfoProc Db_InfoProc;
extern Tcl_Obj* Db_InfoProcInt(Tcl_Interp *interp, RatFolderInfo *infoPtr,
	RatFolderInfoType type, int rIndex);

/* ratDisFolder.c */
extern int RatDisFolderInit (Tcl_Interp *interp);
extern RatFolderInfo *RatDisFolderCreate(Tcl_Interp *interp, Tcl_Obj *defPtr);
extern char* RatDisFolderDir(Tcl_Interp *interp, Tcl_Obj *defPtr);
extern int RatDisOnOffTrans(Tcl_Interp *interp, int newState);
extern void RatDisManageFolder(Tcl_Interp *interp, RatManagementAction op,
			       Tcl_Obj *fptr);

/* ratMessage.c */
extern void RatInitMessages (void);
extern Tcl_ObjCmdProc RatMessageCmd;
extern void RatMessageGetContent(Tcl_Interp *interp, MessageInfo *msgPtr,
				 char **header, char **body);
extern BodyInfo *CreateBodyInfo(Tcl_Interp *interp, MessageInfo *msgPtr,
				BODY *bodyPtr);
extern Tcl_ObjCmdProc RatBodyCmd;
extern int RatMessageDelete (Tcl_Interp *interp, char *msgCmd);
extern void RatMessageGet (Tcl_Interp *interp, MessageInfo *msgPtr,
	Tcl_DString *ds, char *flags, size_t flaglen, char *date,
	size_t datelen);
extern Tcl_ObjCmdProc RatInsertCmd;
extern int RatInsertMsg (Tcl_Interp *interp, MessageInfo *msgPtr,
	char *keywords, char *exDate, char *exType);
extern Tcl_Obj *RatMsgInfo(Tcl_Interp *interp, MessageInfo *msgPtr,
	RatFolderInfoType type);
extern int RatBodySave(Tcl_Interp *interp,Tcl_Channel channel,
		BodyInfo *bodyInfoPtr, int encoded, int convertNL);
extern Tcl_Obj *RatBodyType(BodyInfo *bodyInfoPtr);
extern Tcl_Obj *RatBodyData(Tcl_Interp *interp, BodyInfo *bodyInfoPtr,
	int encoded, char *charset);
extern MESSAGECACHE *RatMessageInternalDate(Tcl_Interp *interp,
	MessageInfo *msgPtr);
extern char *RatPurgeFlags(char *flags, int level);
extern size_t RatHeaderSize(ENVELOPE *env,BODY *body);

/* ratMsgList.c */
extern ListExpression *RatParseList(const char *format, char *error);
extern void RatFreeListExpression(ListExpression *exprPtr);
extern Tcl_Obj *RatDoList(Tcl_Interp *interp, ListExpression *exprPtr,
	RatInfoProc *infoProc, ClientData clientData, int index);
extern Tcl_ObjCmdProc RatCheckListFormatCmd;

/* ratDbMessage.c */
extern char *RatDbMessageCreate (Tcl_Interp *interp, RatFolderInfoPtr infoPtr,
	int index, int dbIndex);
extern void RatDbMessagesInit (MessageProcInfo* procInfo);

/* ratFrMessage.c */
extern void RatFrMessagesInit (MessageProcInfo* procInfo);
extern Tcl_ObjCmdProc RatCreateMessageCmd;
extern BodyInfo* Fr_CreateBodyProc(Tcl_Interp *interp, MessageInfo *msgPtr);
extern char* RatFrMessageCreate(Tcl_Interp *interp, char *data, int length,
				MessageInfo **msgPtrPtr);
extern int RatFrMessagePGP(Tcl_Interp *interp, MessageInfo *msgPtr,
			   int sign, int encrypt, char *role, char *signer,
			   Tcl_Obj *rcpts);
extern int RatFrMessageRemoveInternal(Tcl_Interp *interp, MessageInfo *msgPtr);

/* ratStdMessage.c */
extern void RatStdMessagesInit (MessageProcInfo* procInfo);
extern int RatStdEasyCopyingOK(Tcl_Interp *interp, MessageInfo *msgPtr,
			       Tcl_Obj *defPtr);
extern int RatStdMessageCopy (Tcl_Interp *interp, MessageInfo *msgPtr,
			      char *destination);

/* ratExp.c */
extern Tcl_ObjCmdProc RatParseExpCmd;
extern Tcl_ObjCmdProc RatGetExpCmd;
extern Tcl_ObjCmdProc RatFreeExpCmd;
extern int RatExpMatch(Tcl_Interp *interp, int expId,
	RatInfoProc *infoProc, ClientData clientData, int index);

/* ratMailcap.c */
int RatMcapFindCmd(Tcl_Interp *interp, BodyInfo *bodyInfoPtr);

#ifdef MEM_DEBUG
void ratStdMessageCleanup(void);
void ratMessageCleanup(void);
void ratStdFolderCleanup(void);
void ratAddressCleanup(void);
void ratCodeCleanup(void);
#endif /* MEM_DEBUG */

#endif /* _RATFOLDER */
