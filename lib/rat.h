/*
 * rat.h --
 *
 *      Declarations for things used internally by the Ratatosk
 *      procedures but not exported outside the module.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#ifndef _RAT_H
#define _RAT_H

#include "../config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <mail.h>
#include <tcp.h>
#include <nl.h>
#include <rfc822.h>
#include <env.h>
#include <smtp.h>
#include <misc.h>
#include <string.h>
#include <pwd.h>
#include <utime.h>
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#else
# include <time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <sys/resource.h>

/*
 * dirent definitions
 */
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

/*
 * Wait
 */
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8) 
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif  
/* Last chance guess for WNOHANG */
#ifndef WNOHANG
#define WNOHANG 1
#endif

#include <tcl.h>

#ifndef CONST84
#   define CONST84
#endif

/*
 * Sigh, tcl uses different prototypes for its replacement functions for
 * malloc, realloc and free than in the original functions. Also tcl
 * version 8.2 does always use these replacement functions.
 */

#ifdef TCL_MEM_DEBUG
#   undef ckalloc
#   undef ckfree
#   undef ckrealloc
#   define ckalloc(x) Tcl_DbCkalloc(x, __FILE__, __LINE__)
#   define ckfree(x)  Tcl_DbCkfree((char*)x, __FILE__, __LINE__)
#   define ckrealloc(x,y) ((x)? \
                  Tcl_DbCkrealloc((char*)(x), (y),__FILE__, __LINE__) \
                : Tcl_DbCkalloc((y), __FILE__, __LINE__))
#else /* TCL_MEM_DEBUG */
#   if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 2
#       undef ckalloc
#       undef ckfree
#       undef ckrealloc
#       define ckalloc(x)	Tcl_Alloc(x)
#       define ckfree(x)  	Tcl_Free((char*)x)
#       define ckrealloc(x,y)	((x) ? Tcl_Realloc((char*)x, y) : Tcl_Alloc(y))
#   else /* TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 2 */
#       undef ckrealloc
#       define ckrealloc(x,y)	((x) ? realloc(x, y) : malloc(y))
#   endif /* TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 2 */
#endif /* TCL_MEM_DEBUG */

#define FILEMODE 0600		/* The mode of created files */
#define DIRMODE 0700		/* The mode of created directories */

/*
 * The structure returned by RatDbGet which describes an entry in the
 * database.
 */

typedef enum {
    TO, FROM, CC, MESSAGE_ID, REFERENCE, SUBJECT, DATE, KEYWORDS, RSIZE,
    STATUS, EX_TIME, EX_TYPE, FILENAME, RATDBETYPE_END
} RatDbEType;

typedef struct RatDbEntry {
    char *content[RATDBETYPE_END];
} RatDbEntry;

/*
 * The different classes of log messages
 */
typedef enum {
    RAT_BABBLE, RAT_PARSE, RAT_WARN, RAT_ERROR, RAT_FATAL, RAT_INFO
} RatLogLevel;
typedef enum {
    RATLOG_TIME, RATLOG_EXPLICIT, RATLOG_NOWAIT
} RatLogType;

typedef struct RatFolderInfo *RatFolderInfoPtr;

/*
 * Current data
 */
typedef enum {
    RAT_HOST, RAT_MAILBOX, RAT_PERSONAL, RAT_HELO
} RatCurrentType;

/*
 * A SMTP channel
 */
typedef void *SMTPChannel;

/*
 * A DSN handle
 */
typedef void *DSNhandle;

/*
 * Hexadecimal characters
 */
extern char alphabetHEX[17];

/* ratAppInit.c */
extern Tcl_Interp *timerInterp;
extern char *dayName[];
extern char *monthName[];
extern char *RatGetCurrent(Tcl_Interp *interp, RatCurrentType what,
			   const char *role);
extern void RatLog (Tcl_Interp *interp, RatLogLevel level,
		    CONST84 char *message, RatLogType type);
extern void RatLogF (Tcl_Interp *interp, RatLogLevel level, char *tag,
	RatLogType type, ...);
extern Tcl_Obj *RatMangleNumber(int number);
extern int RatSearch (char *searchFor, char *searchIn);
extern long RatTclPuts (void *stream_x, char *string);
extern long RatStringPuts (void *stream_x, char *string);
extern long RatDelaySoutr (void *stream_x, char *string);
extern void RatInitDelayBuffer ();
extern int RatTranslateWrite(Tcl_Channel channel, CONST84 char *charbuf,
			     int len);
extern MESSAGE *RatParseMsg (Tcl_Interp *interp, unsigned char *message);
extern int RatIsEmpty (const char *string);
extern int RatEncodingCompat (Tcl_Interp *interp, char *wanted, char *avail);
extern char* RatLindex (Tcl_Interp *interp, const char *list, int index);
extern Tcl_ObjCmdProc RatGenId;
extern Tcl_Obj *RatFormatDate(Tcl_Interp *interp, int month, int day);
extern int RatGetTimeZone(unsigned long currentTime);
extern void RatDStringApendNoCRLF(Tcl_DString *ds, const char *s, int length);
extern unsigned char *RatReadFile(Tcl_Interp *interp, const char *filename,
			    unsigned long *length, int convert_to_crlf);
extern CONST84 char *RatGetPathOption(Tcl_Interp *interp, char *name);


/* ratHold.c */
extern Tcl_ObjCmdProc RatHold;
extern int RatHoldInsert(Tcl_Interp *interp, const char *dir,
			 char *handler, const char *description);
extern int RatHoldList(Tcl_Interp *interp, const char *dir,
		       Tcl_Obj *fileListPtr);
extern int RatHoldExtract(Tcl_Interp *interp, const char *prefix,
			  Tcl_Obj *usedArraysPtr, Tcl_Obj *filesPtr);
extern void RatHoldInitVars(Tcl_Interp *interp);
extern void RatHoldUpdateVars(Tcl_Interp *interp, const char *dir, int diff);

/* RatSender.c */
extern void RatSender(Tcl_Interp *interp);
extern char *RatSendPGPCommand(char *cmd);

/* ratFolder.c */
extern int RatFolderInit (Tcl_Interp *interp);

/* ratStdFolder.c */
extern void AppendToIMAP(Tcl_Interp *interp, const char *mailbox,
			 const char *flags, const char *date, const char *msg,
			 int length);
extern void ClearStdPasswds(int freethem);
extern Tcl_ObjCmdProc RatCheckEncodingsCmd;

/* ratCode.c */
extern char *RatDecodeHeader(Tcl_Interp *interp, const char *string, int adr);
extern Tcl_DString *RatDecode(Tcl_Interp *interp, int cte, const char *data,
			      int length, const char *charset);
extern char *RatEncodeHeaderLine(Tcl_Interp *interp, Tcl_Obj *line,
	int nameLength);
extern void RatEncodeAddresses(Tcl_Interp *interp, ADDRESS *adrPtr);
extern Tcl_Encoding RatGetEncoding(Tcl_Interp *interp, const char *name);
extern Tcl_Obj *RatCode64(Tcl_Obj *oPtr);
extern char *RatUtf8toMutf7(const char *src);
extern char *RatMutf7toUtf8(const char *src);
extern Tcl_DString* RatEncodeQP(const unsigned char *line);
extern Tcl_ObjCmdProc RatEncodeQPCmd;
extern unsigned char *RatDecodeQP(unsigned char *line);
extern Tcl_ObjCmdProc RatDecodeQPCmd;

/* ratAddress.c */
extern Tcl_HashTable aliasTable;
extern Tcl_ObjCmdProc RatCreateAddressCmd;
extern Tcl_ObjCmdProc RatAddress;
extern void RatInitAddresses (Tcl_Interp *interp, ADDRESS *addressPtr);
extern Tcl_CmdDeleteProc RatDeleteAddress;
extern int RatAddressIsMe (Tcl_Interp *interp, ADDRESS *adrPtr, int trustUser);
extern int RatAddressCompare (ADDRESS *adr1Ptr, ADDRESS* adr2Ptr);
extern void RatAddressTranslate (Tcl_Interp *interp, ADDRESS *adrPtr);
extern Tcl_ObjCmdProc RatAliasCmd;
extern void RatAddressTranslate (Tcl_Interp *interp, ADDRESS *adrPtr);
extern char *RatAddressMail(ADDRESS *adrPtr);
extern Tcl_ObjCmdProc RatSplitAddresses;
extern size_t RatAddressSize(ADDRESS *adrPtr, int all);
extern void RatGenerateAddresses(Tcl_Interp *interp, const char *role,
				 char *msgh, ADDRESS **from, ADDRESS **sender);
extern Tcl_ObjCmdProc RatGenerateAddressesCmd;
extern CONST84 char *RatFindCharInHeader(CONST84 char *header, char m);

/* ratDbase.c */
extern int RatDbInsert (Tcl_Interp *interp, const char *to, const char *from,
			const char *cc, const char *msgid, const char *ref,
			const char *subject, long date, const char *flags,
			const char *keywords, long exDate, const char *exType,
			const char *fromline, const char *mail, int length);
extern int RatDbSetStatus (Tcl_Interp *interp, int index, char *status);
extern int RatDbSearch (Tcl_Interp *interp, Tcl_Obj *exp, int *numFoundPtr,
	int **foundPtrPtr);
extern RatDbEntry *RatDbGetEntry (int index);
extern MESSAGE *RatDbGetMessage (Tcl_Interp *interp, int index, char **bufPtr);
extern char *RatDbGetHeaders (Tcl_Interp *interp, int index);
extern char *RatDbGetFrom(Tcl_Interp *interp, int index);
extern char *RatDbGetText (Tcl_Interp *interp, int index);
extern int RatDbDelete (Tcl_Interp *interp, int index);
extern int RatDbExpunge (Tcl_Interp *interp);
extern int RatDbDaysSinceExpire (Tcl_Interp *interp);
extern int RatDbExpire (Tcl_Interp *interp, char *infolder,
	char *backupDirectory);
extern void RatDbClose(void);
extern int RatDbCheck(Tcl_Interp *interp, int fix);

/* ratSMTP.c */
extern SMTPChannel RatSMTPOpen (Tcl_Interp *interp, char *host, int verbose,
				const char *role);
extern void RatSMTPClose (Tcl_Interp *interp, SMTPChannel channel,int verbose);
extern void RatSMTPCloseAll (Tcl_Interp *interp, int verbose);
extern int RatSMTPSend (Tcl_Interp *interp, SMTPChannel channel,
	ENVELOPE *envelopePtr, BODY *bodyPtr, int requestDSN, int verbose);
extern Tcl_ObjCmdProc RatSMTPSupportDSN;
extern long RatTclPutsSMTP (void *stream_x, char *string);
extern long RatTclPutsSendmail (void *stream_x, char *string);
extern size_t RatHeaderSize(ENVELOPE *env,BODY *body);

/* ratDSN.c */
extern int RatDSNInit (Tcl_Interp *interp);
extern DSNhandle RatDSNStartMessage (Tcl_Interp *interp, const char *id,
				     const char *subject);
extern void RatDSNAddRecipient (Tcl_Interp *interp, DSNhandle handle,
	char *recipient);
extern void RatDSNAbort (Tcl_Interp *interp, DSNhandle handle);
extern void RatDSNFinish (Tcl_Interp *interp, DSNhandle handle);
extern int RatDSNHandle (Tcl_Interp *interp, char *msg);
extern int RatDSNExtract (Tcl_Interp *interp, Tcl_Obj *body);

/* ratMessage.c */
extern int RatMessageGetHeader(Tcl_Interp *interp, char *srcHeader);
Tcl_Obj *RatWrapMessage(Tcl_Interp *interp, Tcl_Obj *oPtr);

/* ratMailcap.c */
extern Tcl_ObjCmdProc RatMailcapReload;

/* ratCompat.c */
#ifndef HAVE_SNPRINTF
extern size_t snprintf (char *buf, size_t buflen, const char *fmt, ...);
#endif /* HAVE_SNPRINTF */
#ifndef HAVE_STRLCPY
extern char *strlcpy(char *dst, const char *src, size_t n);
#endif /* HAVE_STRLCPY */
#ifndef HAVE_STRLCAT
extern char *strlcat(char *dst, const char *src, size_t n);
#endif /* HAVE_STRLCAT */

/* ratPwCache.c */
extern char *RatGetCachedPassword(Tcl_Interp *interp, const char *spec);
extern void RatCachePassword(Tcl_Interp *interp, const char *spec,
			     const char *passwd, int store);
extern void RatPasswdCachePurge(Tcl_Interp *interp, int disk_also);
extern Tcl_ObjCmdProc RatPasswdCachePurgeCmd;

/* ratPrint.c */
extern Tcl_ObjCmdProc RatPrettyPrintMsg;

/* ratWatchdog.c */
extern void RatReleaseWatchdog(const char *tmpdir);

/* ratBusy.c */
extern void RatSetBusy(Tcl_Interp *interp);
extern void RatClearBusy(Tcl_Interp *interp);
extern Tcl_ObjCmdProc RatBusyCmd;

#endif /* _RAT_H */
