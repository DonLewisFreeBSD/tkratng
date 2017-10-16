/* 
 * ratAppInit.c --
 *
 *	Provides a default version of the Tcl_AppInit procedure for
 *	use in wish and similar Tk-based applications.
 *
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <pwd.h>
#include <signal.h>
#include "ratFolder.h"
#include "ratStdFolder.h"
#include "ratPGP.h"
#include <locale.h>

/*
 * Version
 */
#define LIBVERSION	"2.1"
#define LIBDATE		"20020607"

/*
 * Length of status string
 */
#define STATUS_STRING "Status: RO\n"
#define STATUS_LENGTH 11

/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

#ifdef NEED_MATHERR
extern int matherr();
int *tclDummyMathPtr = (int *) matherr;
#endif

/*
 * The following structure is used by the RatBgExec command to keep the
 * information about processes running in the background.
 */
typedef struct RatBgInfo {
    Tcl_Interp *interp;
    int numPids;
    int *pidPtr;
    int status;
    Tcl_Obj *exitStatus;
    struct RatBgInfo *nextPtr;
} RatBgInfo;

/*
 * How often we should check for dead processes (in milliseconds)
 */
#define DEAD_INTERVAL 200

/*
 * Names of days and months as per rfc822.
 */
char *dayName[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char *monthName[] = {"jan", "Feb", "Mar", "Apr", "May", "Jun",
		     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


/*
 * If we have the display or not. This is used by forked processes so
 * they do not inadvertly tries to use the display.
 */
static int hasDisplay = 1;

/*
 * Buffer for delayed output
 */
static char ratDelayBuffer[3];

/*
 * Communication with the child
 */
static FILE *toSender = NULL;
static int fromSender;
static int sendSequence = 0;

static Tcl_FileProc RatHandleSender;
static int RatCreateSender(Tcl_Interp *interp);

/*
 * KOD-handler (Kiss Of Death)
 */
static Tcl_AsyncHandler kodhandler;

/*
 * Directory of sent messages
 */
static char *deferredDir = NULL;

/*
 * List of sent messages
 */
typedef struct SentMsg {
    int id;
    char *handler;
    struct SentMsg *nextPtr;
} SentMsg;
static SentMsg *sentMsg = NULL;

/*
 * Interpreter for timer procedures
 */
Tcl_Interp *timerInterp;

/*
 * Local functions
 */
static Tcl_TimerProc RatChildHandler;
static Tcl_VarTraceProc RatReject;
static Tcl_AppInitProc RatAppInit;
static Tcl_VarTraceProc RatOptionWatcher;
static Tcl_ObjCmdProc RatGetCurrentCmd;
static Tcl_ObjCmdProc RatBgExec;
static Tcl_ObjCmdProc RatSend;
static int RatSendDeferred(Tcl_Interp *interp);
static Tcl_ObjCmdProc RatGetCTE;
static Tcl_ObjCmdProc RatCleanup;
static Tcl_ObjCmdProc RatTildeSubst;
static Tcl_ObjCmdProc RatTime;
static Tcl_ObjCmdProc RatLock;
static Tcl_ObjCmdProc RatIsLocked;
static Tcl_ObjCmdProc RatType;
static Tcl_ObjCmdProc RatEncoding;
static Tcl_ObjCmdProc RatDSE;
static Tcl_ObjCmdProc RatExpire;
static Tcl_ObjCmdProc RatLL;
static Tcl_ObjCmdProc RatGen;
static Tcl_ObjCmdProc RatWrapCited;
static Tcl_ObjCmdProc RatDbaseCheck;
static Tcl_ObjCmdProc RatMangleNumberCmd;
static Tcl_ObjCmdProc RatFormatDateCmd;
static void KodHandlerSig(int s);
static Tcl_AsyncProc KodHandlerAsync;
static Tcl_IdleProc KodHandlerIdle;
static void RatPopulateStruct(char *base, BODY *bodyPtr);
static Tcl_VarTraceProc RatSetCharset;
static Tcl_ExitProc RatExit;
static Tcl_ObjCmdProc RatEncodeMutf7Cmd;
static Tcl_ObjCmdProc RatLibSetOnlineModeCmd;
static Tcl_ObjCmdProc RatTestCmd;

#ifdef MEM_DEBUG
static char **mem_months = NULL;
#endif /* MEM_DEBUG */

int Ratatosk_Init(Tcl_Interp *interp)
{
    RatAppInit(interp);
    return Tcl_PkgProvide(interp, "ratatosk", VERSION);
}
int Ratatosk_SafeInit(Tcl_Interp *interp)
{
    RatAppInit(interp);
    return Tcl_PkgProvide(interp, "ratatosk", VERSION);
}


/*
 *----------------------------------------------------------------------
 *
 * RatAppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in the result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

static int
RatAppInit(Tcl_Interp *interp)
{
    struct passwd *pwPtr;
    double tcl_version;
    Tcl_Obj *oPtr;
    char *c, tmp[1024];
    CONST84 char *v;
    int i;

    setlocale(LC_CTYPE, "");
    setlocale(LC_COLLATE, "");

    /*
     * Check tcl version
     * But do it softly and ignore unexpected errors
     */
    oPtr = Tcl_GetVar2Ex(interp, "tcl_version", NULL, TCL_GLOBAL_ONLY);
    if (TCL_OK == Tcl_GetDoubleFromObj(interp, oPtr, &tcl_version)
	&& tcl_version < 8.1) {
	fprintf(stderr,
		"TkRat requires tcl/tk 8.1 or later (detected %4.1f)\n",
		tcl_version);
	exit(1);
    }

    /*
     * Create temp-directory
     */
    if (NULL == (v = RatGetPathOption(interp, "tmp"))) {
	v = "/tmp";
    }
    for (i=0; i<100; i++) {
	snprintf(tmp, sizeof(tmp), "%s/rat.%x-%d", v, getpid(), i);
	if (0 == mkdir(tmp, 0700)) {
	    break;
	}
	if (EEXIST != errno) {
	    fprintf(stderr, "Faield to create tmp-directory '%s': %s\n", tmp,
		    strerror(errno));
	    exit(1);
	}
    }
    if (100 == i) {
	fprintf(stderr, "Failed to create temporary directory '%s'\n", tmp);
    }
    Tcl_SetVar(interp, "rat_tmp", tmp, TCL_GLOBAL_ONLY);
    RatReleaseWatchdog(tmp);
    
    /*
     * Initialize some variables
     */
    Tcl_SetVar(interp, "ratSenderSending", "0", TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "ratNetOpenFailures", NULL, Tcl_NewIntObj(0),
		  TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "ratCurrent", "charset", Tcl_GetEncodingName(NULL),
	    TCL_GLOBAL_ONLY);
    Tcl_TraceVar2(interp, "ratCurrent", "charset",
	    TCL_TRACE_WRITES | TCL_GLOBAL_ONLY, RatSetCharset, NULL);
    timerInterp = interp;
    Tcl_SetVar2(interp, "rat_lib", "version", LIBVERSION, TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "rat_lib", "date", LIBDATE, TCL_GLOBAL_ONLY);
#ifdef HAVE_OPENSSL
    Tcl_SetVar(interp, "ratHaveOpenSSL", "1", TCL_GLOBAL_ONLY);    
#else /* HAVE_OPENSSL */
    Tcl_SetVar(interp, "ratHaveOpenSSL", "0", TCL_GLOBAL_ONLY);    
#endif /* HAVE_OPENSSL */
    
    /*
     * Initialize c-client library
     */
    v = RatGetPathOption(interp, "ssh_path");
    if (v && *v) {
	tcp_parameters(SET_SSHPATH, (void*)v);
    }
    v = Tcl_GetVar2(interp, "option", "ssh_command", TCL_GLOBAL_ONLY);
    if (v && *v) {
	tcp_parameters(SET_SSHCOMMAND, (void*)v);
    }
    oPtr = Tcl_GetVar2Ex(interp, "option", "ssh_timeout", TCL_GLOBAL_ONLY);
    if (oPtr && TCL_OK == Tcl_GetIntFromObj(interp, oPtr, &i) && i != 0) {
	tcp_parameters(SET_SSHTIMEOUT, (void*)i);
    }

    /*
     * Initialize async handlers and setup signal handler
     */
    kodhandler = Tcl_AsyncCreate(KodHandlerAsync, (ClientData)interp);
    signal(SIGUSR2, KodHandlerSig);

    /*
     * Make sure we know who we are and that we keep track of any changes
     */
    Tcl_TraceVar2(interp, "option", NULL, TCL_GLOBAL_ONLY|TCL_TRACE_WRITES,
		  RatOptionWatcher, NULL);

    /*
     * Make sure that env(USER), env(GECOS), env(HOME) and env(MAIL) are set.
     * If not then we initialize them.
     */
    if (!Tcl_GetVar2(interp, "env", "USER", TCL_GLOBAL_ONLY)) {
	pwPtr = getpwuid(getuid());
	Tcl_SetVar2(interp, "env", "USER", pwPtr->pw_name, TCL_GLOBAL_ONLY);
    }
    if (!Tcl_GetVar2(interp, "env", "GECOS", TCL_GLOBAL_ONLY)) {
	pwPtr = getpwuid(getuid());
	strlcpy(tmp, pwPtr->pw_gecos, sizeof(tmp));
	if ((c = strchr(tmp, ','))) {
	    *c = '\0';
	}
	Tcl_SetVar2(interp, "env", "GECOS", tmp, TCL_GLOBAL_ONLY);
    }
    if (!Tcl_GetVar2(interp, "env", "HOME", TCL_GLOBAL_ONLY)) {
	pwPtr = getpwuid(getuid());
	Tcl_SetVar2(interp, "env", "HOME", pwPtr->pw_dir, TCL_GLOBAL_ONLY);
    }
    if (!Tcl_GetVar2(interp, "env", "MAIL", TCL_GLOBAL_ONLY)) {
	char buf[1024];

	pwPtr = getpwuid(getuid());
	snprintf(buf, sizeof(buf), "/var/spool/mail/%s", pwPtr->pw_name);
	Tcl_SetVar2(interp, "env", "MAIL", buf, TCL_GLOBAL_ONLY);
    }

    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */
    if (RatFolderInit(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (RatDSNInit(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    Tcl_InitHashTable(&aliasTable, TCL_STRING_KEYS);

    /*
     * Call Tcl_CreateObjCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */
    Tcl_CreateObjCommand(interp, "RatGetCurrent", RatGetCurrentCmd, NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatBgExec", RatBgExec, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGenId", RatGenId, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatSend", RatSend, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGetCTE", RatGetCTE, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCleanup", RatCleanup, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatTildeSubst", RatTildeSubst, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatTime", RatTime, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatLock", RatLock, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatIsLocked", RatIsLocked, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatHold", RatHold, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatAlias", RatAliasCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatType2", RatType, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDaysSinceExpire", RatDSE, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatExpire", RatExpire, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatSMTPSupportDSN", RatSMTPSupportDSN,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatLL", RatLL, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGen", RatGen, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatWrapCited", RatWrapCited, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDbaseCheck", RatDbaseCheck, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatSplitAdr", RatSplitAddresses, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatMailcapReload", RatMailcapReload,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatPGP", RatPGPCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatMangleNumber", RatMangleNumberCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatFormatDate", RatFormatDateCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCheckEncodings", RatCheckEncodingsCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCreateAddress", RatCreateAddressCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatPurgePwChache", RatPasswdCachePurgeCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatPrettyPrintMsg", RatPrettyPrintMsg,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatEncodeMutf7", RatEncodeMutf7Cmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatLibSetOnlineMode", RatLibSetOnlineModeCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatEncoding", RatEncoding,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatBusy", RatBusyCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGenerateAddresses",
			 RatGenerateAddressesCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatTest", RatTestCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatEncodeQP", RatEncodeQPCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDecodeQP", RatDecodeQPCmd, NULL, NULL);
    Tcl_CreateExitHandler(RatExit, NULL);

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatGetCurrent --
 *
 *	Get current host, domain, mailbox and personal values.
 *	These values depends on the current role.
 *	The algorithm for building host is:
 *	  if option($role,from) is set and contains a domian then
 *	     use that domain
 *	  else
 *	      if gethostname() returns a name with a dot in it then
 *	         use it as host
 *	      else
 *	         use the result of gethostname and the value of option(domain)
 *	      endif
 *	  endif
 *
 * Results:
 *      A pointer to the requested value. This pointer is valid until the
 *	next call to RatGetCurrent.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatGetCurrent(Tcl_Interp *interp, RatCurrentType what, const char *role)
{
    struct passwd *passwdPtr;
    ADDRESS *address = NULL;
    static char buf[1024];
    char *result = NULL, *personal, hostbuf[1024], *c;
    CONST84 char *host, *from, *uqdom, *helo;
    Tcl_Obj *oPtr;

    host = Tcl_GetHostName();
    if (!strchr(host, '.')) {
	CONST84 char *domain = Tcl_GetVar2(interp, "option", "domain",
					   TCL_GLOBAL_ONLY);
	if (domain && *domain) {
	    strlcpy(hostbuf, host, sizeof(buf));
	    strlcat(hostbuf, ".", sizeof(buf));
	    strlcat(hostbuf, domain, sizeof(buf));
	    host = hostbuf;
	}
    }

    snprintf(buf, sizeof(buf), "%s,from", role);
    from = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
    if (from && '\0' != *from) {
	char *s = cpystr(from);
	rfc822_parse_adrlist(&address, s, (char*)host);
	ckfree(s);
    }
    passwdPtr = getpwuid(getuid());

    switch (what) {
    case RAT_HOST:
	snprintf(buf, sizeof(buf), "%s,uqa_domain", role);
	uqdom = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);

	if (uqdom && 0 < strlen(uqdom)) {
	    strlcpy(buf, uqdom, sizeof(buf));
	} else if (address && address->host) {
	    strlcpy(buf, address->host, sizeof(buf));
	} else {
	    strlcpy(buf, host, sizeof(buf));
	}
	result = buf;
	break;
	
    case RAT_MAILBOX:
	if (address && address->mailbox) {
	    strlcpy(buf, address->mailbox, sizeof(buf));
	    result = buf;
	} else {
	    result = passwdPtr->pw_name;
	}
	break;
	
    case RAT_PERSONAL:
	if (address && address->personal) {
	    strlcpy(buf, address->personal, sizeof(buf));
	} else {
	    strlcpy(buf, passwdPtr->pw_gecos, sizeof(buf));
	    if ((c = strchr(buf, ','))) {
		*c = '\0';
	    }
	}
	oPtr = Tcl_NewStringObj(buf, -1);
	personal = RatEncodeHeaderLine(interp, oPtr, 0);
	Tcl_DecrRefCount(oPtr);
	strlcpy(buf, personal, sizeof(buf));
	result = buf;
	break;
    case RAT_HELO:
	snprintf(buf, sizeof(buf), "%s,smtp_helo", role);
	helo = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);

	if (helo && 0 < strlen(helo)) {
	    strlcpy(buf, helo, sizeof(buf));
	} else if (address && address->host) {
	    strlcpy(buf, address->host, sizeof(buf));
	} else {
	    strlcpy(buf, host, sizeof(buf));
	}
	result = buf;
	break;
    }
    
    if (from && '\0' != *from) {
	mail_free_address(&address);
    }
    
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetCurrentCmd --
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

static int
RatGetCurrentCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		 Tcl_Obj *const objv[])
{
    RatCurrentType what = -1;
    char *result;

    if (3 == objc) {
	if (!strcmp("host", Tcl_GetString(objv[1]))) {
	    what = RAT_HOST;
	} else if (!strcmp("mailbox", Tcl_GetString(objv[1]))) {
	    what = RAT_MAILBOX;
	} else if (!strcmp("personal", Tcl_GetString(objv[1]))) {
	    what = RAT_PERSONAL;
	} else if (!strcmp("smtp_helo", Tcl_GetString(objv[1]))) {
	    what = RAT_HELO;
	}
    }
    if (3 != objc || -1 == what) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]),
			 " what role", (char*) NULL);
	return TCL_ERROR;
    }

    result = RatGetCurrent(interp, what, Tcl_GetString(objv[2]));
    Tcl_SetResult(interp, result, TCL_VOLATILE);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatLog --
 *
 *	Sends a log message to the interface
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	The tcl command 'RatLog' will be called
 *
 *
 *----------------------------------------------------------------------
 */

void
RatLog(Tcl_Interp *interp, RatLogLevel level, CONST84 char *message,
       RatLogType type)
{
    CONST84 char *argv = message;
    char *parsedMsg;
    char *typeStr;
    int levelNumber;

    switch(level) {
    case RAT_BABBLE:	levelNumber = 0; break;
    case RAT_PARSE:	levelNumber = 1; break;
    case RAT_INFO:	levelNumber = 2; break;
    case RAT_WARN:	levelNumber = 3; break;
    case RAT_ERROR:	levelNumber = 4; break;
    case RAT_FATAL:	/* fallthrough */
    default:		levelNumber = 5; break;
    }
    switch(type) {
    case RATLOG_TIME:	    typeStr = "time"; break;
    case RATLOG_EXPLICIT:   typeStr = "explicit"; break;
    case RATLOG_NOWAIT:	    /* fallthrough */
    default:		    typeStr = "nowait"; break;
    }

    parsedMsg = Tcl_Merge(1, (CONST84 char * CONST84 *)&argv);
    if (hasDisplay) {
        char *buf = (char*) ckalloc(16 + strlen(parsedMsg) + 9);
        sprintf(buf, "RatLog %d %s %s", levelNumber, parsedMsg, typeStr);
        if (TCL_OK != Tcl_GlobalEval(interp, buf)) {
	    Tcl_AppendResult(interp, "Error: '", Tcl_GetStringResult(interp),
		    "'\nWhile executing '", buf, "'\n", NULL);
        }
        ckfree(buf); 
    } else {
        fprintf(stdout, "STATUS %d %s %d", levelNumber, parsedMsg, type);
        fputc('\0', stdout);
        fflush(stdout);
    }
    ckfree(parsedMsg);
}

/*
 *----------------------------------------------------------------------
 *
 * RatLogF --
 *
 *	Sends a log message to the interface. The difference between this
 *	function and RatLog is that this one takes arguments like printf.
 *	But instead of the format string this one takes and index into
 *	the text array, thus giving localized logging.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	See RatLog
 *
 *
 *----------------------------------------------------------------------
 */

void
RatLogF (Tcl_Interp *interp, RatLogLevel level, char *tag, RatLogType type,...)
{
    va_list argList;
    char buf[1024];
    CONST84 char *fmt = Tcl_GetVar2(interp, "t", tag, TCL_GLOBAL_ONLY);

    if (NULL == fmt) {
	snprintf(buf, sizeof(buf), "Internal error: RatLogF '%s'", tag);
	RatLog(interp, RAT_ERROR, buf, 0);
	return;
    }
    va_start(argList, type);
#ifdef HAVE_SNPRINTF
    vsnprintf(buf, sizeof(buf), fmt, argList);
#else
    vsprintf(buf, fmt, argList);
#endif
    va_end(argList);
    RatLog(interp, level, buf, type);
}


/*
 *----------------------------------------------------------------------
 *
 * RatMangleNumber --
 *
 *      Creates a string representation of the given number that is maximum
 *      four characters long. The actual mangling is done in the tcl-proc
 *      ratMangleNumber.
 *
 * Results:
 *      Returns a pointer to a static buffer containg the string
 *	representation of the number.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatMangleNumber(int number)
{
    static char buf[32];     /* Scratch area */

    if (number < 1000) {
	sprintf(buf, "%d", number);
    } else if (number < 10240) {
	sprintf(buf, "%.1fk", number/1024.0);
    } else if (number < 1048576) {
	sprintf(buf, "%dk", (number+512)/1024);
    } else if (number < 10485760) {
	sprintf(buf, "%.1fM", number/1048576.0);
    } else {
	sprintf(buf, "%dM", (number+524288)/1048576);
    }
    return Tcl_NewStringObj(buf, -1);
}


/*
 *----------------------------------------------------------------------
 *
 * RatMangleNumberCmd --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A list of strings to display to the user.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatMangleNumberCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		   Tcl_Obj *const objv[])
{
    int number;

    if (2 != objc || TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &number)) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), " number",
			 (char*) NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, RatMangleNumber(number));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatBgExec --
 *
 *	See ../doc/interface
 *
 * Results:
 *      The return value is normally TCL_OK and the result can be found
 *      in the result. If something goes wrong TCL_ERROR is returned
 *      and an error message will be left in the result.
 *
 * Side effects:
 *      AN entry is added to ratBgInfoPtr.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatBgExec(ClientData dummy, Tcl_Interp *interp, int objc,Tcl_Obj *CONST objv[])
{
    static RatBgInfo *ratBgList = NULL;
    RatBgInfo *bgInfoPtr;
    Tcl_Obj *lPtr, *oPtr;
    Tcl_DString ds;
    int i;

    if (objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " exitStatus cmd\"", (char *) NULL);
	return TCL_ERROR;
    }

    bgInfoPtr = (RatBgInfo*)ckalloc(sizeof(*bgInfoPtr));
    bgInfoPtr->interp = interp;
    bgInfoPtr->exitStatus = objv[1];
    Tcl_IncrRefCount(objv[1]);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "exec -- ", 5);
    Tcl_DStringAppend(&ds, Tcl_GetString(objv[2]), -1);
    Tcl_DStringAppend(&ds, " &", 2);
    if (TCL_OK != Tcl_Eval(interp, Tcl_DStringValue(&ds))) {
	Tcl_DStringFree(&ds);
	Tcl_SetVar(bgInfoPtr->interp, Tcl_GetString(bgInfoPtr->exitStatus),
		   "-1", TCL_GLOBAL_ONLY);
	Tcl_DecrRefCount(objv[1]);
	ckfree(bgInfoPtr);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&ds);
    lPtr = Tcl_GetObjResult(interp);
    Tcl_ListObjLength(interp, lPtr, &bgInfoPtr->numPids);
    bgInfoPtr->pidPtr = (int*)ckalloc(bgInfoPtr->numPids*sizeof(int));
    for (i=0; i<bgInfoPtr->numPids; i++) {
	Tcl_ListObjIndex(interp, lPtr, i, &oPtr);
	Tcl_GetIntFromObj(interp, oPtr, &bgInfoPtr->pidPtr[i]);
    }
    if (!ratBgList) {
        Tcl_CreateTimerHandler(DEAD_INTERVAL, RatChildHandler, &ratBgList);
    }
    bgInfoPtr->nextPtr = ratBgList;
    ratBgList = bgInfoPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatChildHandler --
 *
 *	This process checks if processes in a pipeline are dead. When
 *	all are dead the corresponding variables are set etc.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Sets variables mentioned in the RatBgInfo structure.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatChildHandler(ClientData clientData)
{
    RatBgInfo *bgInfoPtr, **bgInfoPtrPtr = (RatBgInfo**)clientData;
    int i, allDead, status, result;

    while (*bgInfoPtrPtr) {
	bgInfoPtr = *bgInfoPtrPtr;
	allDead = 1;
	for (i = 0; i < bgInfoPtr->numPids; i++) {
	    if (bgInfoPtr->pidPtr[i]) {
		result = waitpid(bgInfoPtr->pidPtr[i], &status, WNOHANG);
		if ((result == bgInfoPtr->pidPtr[i])
			|| ((result == -1) && (errno == ECHILD))) {
		    bgInfoPtr->pidPtr[i] = 0;
		    if (i == bgInfoPtr->numPids-1) {
			bgInfoPtr->status = WEXITSTATUS(status);
		    }
		} else {
		    allDead = 0;
		}
	    }
	}
	if (allDead) {
	    char buf[36];

	    sprintf(buf, "%d", bgInfoPtr->status);
	    Tcl_SetVar(bgInfoPtr->interp, Tcl_GetString(bgInfoPtr->exitStatus),
		    buf, TCL_GLOBAL_ONLY);
	    *bgInfoPtrPtr = bgInfoPtr->nextPtr;
	    ckfree(bgInfoPtr->pidPtr);
	    Tcl_DecrRefCount(bgInfoPtr->exitStatus);
	    ckfree(bgInfoPtr);
	} else {
	    bgInfoPtrPtr = &(*bgInfoPtrPtr)->nextPtr;
	}
    }
    if (*(RatBgInfo**)clientData) {
	Tcl_CreateTimerHandler(DEAD_INTERVAL, RatChildHandler, clientData);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatGenId --
 *
 *	See ../doc/interface
 *
 * Results:
 *      The return value is normally TCL_OK and the result can be found
 *      in the result. If something goes wrong TCL_ERROR is returned
 *      and an error message will be left in the result area.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatGenId(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    static long lastid = 0;
    char buf[64];

    long t = time(NULL);
    if (t <= lastid)
        lastid++;
    else
        lastid = t;
    sprintf(buf, "%lx.%x", lastid, (int)getpid());
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSend --
 *
 *	See ../doc/interface
 *
 *	This checks that we have something that looks like a good message.
 *	The actual sending is done by a subprocess called the sending
 *	process. We communicate with that process via the stdin and stdout
 *	channels. The follwing commands can be sent to the sender:
 *	    SEND id prefix 
 *	    QUIT
 *	The sender can send the following commands:
 *	    STATUS level status_text type
 *	    FAILED id prefix text
 *	    SAVE file save_to to from cc msgid ref subject flags date
 *	    SENT id 
 *	    PGP pgp_specific_data
 *	The server will respond to all PGP commands
 *
 * Results:
 *      The return value is normally TCL_OK and the result can be found
 *      in the result area. If something goes wrong TCL_ERROR is returned
 *      and an error message will be left in the result area.
 *
 * Side effects:
 *      A message is sent.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatSend(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    char *handler = NULL;
    CONST84 char *tmp, *v;
    SentMsg **smPtrPtr;
    Tcl_Obj *oPtr;
    int online;

    if (objc != 2 && objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " action ?handler?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (NULL == deferredDir) {
	if (NULL == (v = RatGetPathOption(interp, "send_cache"))) {
	    return TCL_ERROR;
	}
	deferredDir = cpystr(v);
    }

    if (!strcmp("kill", Tcl_GetString(objv[1]))) {
	if (toSender) {
	    fprintf(toSender, "QUIT\n");
	    fflush(toSender);
	}
    } else if (!strcmp("init", Tcl_GetString(objv[1]))) {
	RatHoldInitVars(interp);
    } else if (!strcmp("sendDeferred", Tcl_GetString(objv[1]))) {
	return RatSendDeferred(interp);
    } else if (objc == 3) {
	/*
	 * The algorithm here is:
	 *  - First we make sure that we got something that at least looks
	 *	  like a letter.
	 *	- Insert the message into the send cache.
	 *	- If we do not have a child process then we create one.
	 *	- Make the child process send the message.
	 *  - Return.
	 */
	if (((NULL == (tmp = Tcl_GetVar2(interp, Tcl_GetString(objv[2]),
					"to", TCL_GLOBAL_ONLY)))
	     || RatIsEmpty(tmp))
	    && ((NULL == (tmp = Tcl_GetVar2(interp, Tcl_GetString(objv[2]),
					   "cc", TCL_GLOBAL_ONLY)))
		|| RatIsEmpty(tmp))
	    && ((NULL == (tmp = Tcl_GetVar2(interp, Tcl_GetString(objv[2]),
					   "bcc", TCL_GLOBAL_ONLY)))
		|| RatIsEmpty(tmp))) {
	    Tcl_SetResult(interp, "RatSend needs at least one recipient",
			  TCL_STATIC);
	    goto error;
	}

	if (TCL_OK != RatHoldInsert(interp, deferredDir,
				    Tcl_GetString(objv[2]), "")) {
	    goto error;
	}
	handler = cpystr(Tcl_GetStringResult(interp));

	oPtr = Tcl_GetVar2Ex(interp, "option", "online", TCL_GLOBAL_ONLY);
	Tcl_GetIntFromObj(interp, oPtr, &online);

	if (online) {
	    if (TCL_OK != RatCreateSender(interp)) {
		ckfree(handler);
		goto error;
	    }
	    Tcl_SetVar(interp, "ratSenderSending", "1", TCL_GLOBAL_ONLY);
	    for (smPtrPtr=&sentMsg;*smPtrPtr;smPtrPtr = &(*smPtrPtr)->nextPtr);
	    *smPtrPtr = (SentMsg*)ckalloc(sizeof(SentMsg)+strlen(handler)+1);
	    (*smPtrPtr)->id = sendSequence;
	    (*smPtrPtr)->handler = (char*)*smPtrPtr+sizeof(SentMsg);
	    strcpy((*smPtrPtr)->handler, handler);
	    (*smPtrPtr)->nextPtr = NULL;
	    fprintf(toSender, "SEND {%d %s}\n", sendSequence++, handler);
	    fprintf(toSender, "RSET\n");
	    fflush(toSender);
	}
	ckfree(handler);
    }

    return TCL_OK;

error:
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendDeferred --
 *
 *	Send all defered messages
 *
 * Results:
 *	A standard TCL result
 *
 * Side effects:
 *      A new process may be created.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatSendDeferred(Tcl_Interp *interp)
{
    Tcl_DString cmd;
    char buf[1024], *entity;
    int listArgc, i, sent;
    SentMsg **smPtrPtr;
    Tcl_Obj *oPtr, *fileListPtr, **listArgv;
    int online;

    if (NULL == deferredDir) {
	CONST84 char *v;
	
	if (NULL == (v = RatGetPathOption(interp, "send_cache"))) {
	    return TCL_ERROR;
	}
	deferredDir = cpystr(v);
    }

    oPtr = Tcl_GetVar2Ex(interp, "option", "online", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &online);

    fileListPtr = Tcl_NewObj();
    if (TCL_OK != RatHoldList(interp, deferredDir, fileListPtr)
	|| TCL_OK != Tcl_ListObjGetElements(interp, fileListPtr,
					    &listArgc, &listArgv)) {
	goto error;
    }
    if (0 == listArgc) {
	goto done;
    }

    if (TCL_OK != RatCreateSender(interp)) {
	goto error;
    }
    Tcl_SetVar(interp, "ratSenderSending", "1", TCL_GLOBAL_ONLY);
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppendElement(&cmd, "SEND");
    for (i=0, sent=0; i<listArgc; i++) {
	entity = Tcl_GetString(listArgv[i]);
	for (smPtrPtr=&sentMsg;
	     *smPtrPtr && strcmp((*smPtrPtr)->handler, entity);
	     smPtrPtr = &(*smPtrPtr)->nextPtr);
	if (NULL != *smPtrPtr) continue;
	*smPtrPtr =
	    (SentMsg*)ckalloc(sizeof(SentMsg)+strlen(entity)+1);
	(*smPtrPtr)->id = sendSequence;
	(*smPtrPtr)->handler = (char*)*smPtrPtr+sizeof(SentMsg);
	strcpy((*smPtrPtr)->handler, entity);
	(*smPtrPtr)->nextPtr = NULL;
	Tcl_DStringStartSublist(&cmd);
	sprintf(buf, "%d", sendSequence++);
	Tcl_DStringAppendElement(&cmd, buf);
	snprintf(buf, sizeof(buf), "%s/%s", deferredDir, entity);
	Tcl_DStringAppendElement(&cmd, buf);
	Tcl_DStringEndSublist(&cmd);
	sent++;
    }
    fprintf(toSender, "%s\n", Tcl_DStringValue(&cmd));
    fprintf(toSender, "RSET\n");
    fflush(toSender);
    Tcl_DStringFree(&cmd);
    sprintf(buf, "%d", sent);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);

 done:
    Tcl_DecrRefCount(fileListPtr);
    return TCL_OK;

 error:
    Tcl_DecrRefCount(fileListPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCreateSender --
 *
 *	Create the sender subprocess (if not already running).
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *      A new process may be created.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatCreateSender(Tcl_Interp *interp)
{
    int toPipe[2], fromPipe[2], senderPid, i;
    struct rlimit rlim;
    Tcl_Pid tp[1];

    if (toSender) {
	return TCL_OK;
    }

    Tcl_ReapDetachedProcs();
    /*
     * Create the sender subprocess and create a handler on the from pipe.
     */
    pipe(toPipe);
    pipe(fromPipe);
    if (0 == (senderPid = fork())) {
	getrlimit(RLIMIT_NOFILE, &rlim);	
	for (i=0; i<rlim.rlim_cur; i++) {
	    if (i != toPipe[0] && i != fromPipe[1] && 2 != i) {
		close(i);
	    }
	}
	dup2(toPipe[0], 0);
	dup2(fromPipe[1], 1);
	fcntl(0, F_SETFD, 0);
	fcntl(1, F_SETFD, 0);
	hasDisplay = 0;
	RatSender(interp);
	/* notreached */
    }
    if (-1 == senderPid) {
	Tcl_SetResult(interp, "Failed to fork sender process", TCL_STATIC);
	return TCL_ERROR;
    }
    close(toPipe[0]);
    close(fromPipe[1]);
    toSender = fdopen(toPipe[1], "w");
    fromSender = fromPipe[0];
    Tcl_CreateFileHandler(fromSender, TCL_READABLE, RatHandleSender,
	    (ClientData)interp);
    tp[0] = (Tcl_Pid)senderPid;
    Tcl_DetachPids(1, tp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatHandleSender --
 *
 *	Handle events from the sender process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *      Whatever the sender dictates.
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatHandleSender(ClientData clientData, int mask)
{
    RatFolderInfo *infoPtr;
    static Tcl_DString *bufDS = NULL;
    Tcl_Interp *interp = (Tcl_Interp*)clientData;
    char buf[1024], *msg, *msgbuf, *tmp, c;
    CONST84 char **argv, **destArgv;
    int argc, destArgc, fd, id = 0, flags, hardError = 0;
    Tcl_DString cmd;
    struct stat sbuf;
    SentMsg *smPtr, **smPtrPtr;

    if (!bufDS) {
	bufDS = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(bufDS);
    } else {
	Tcl_DStringSetLength(bufDS, 0);
    }
    do {
	if (1 != read(fromSender, &c, 1)) {
	    Tcl_SetVar(interp, "ratSenderSending", "0", TCL_GLOBAL_ONLY);
	    Tcl_DeleteFileHandler(fromSender);
	    fclose(toSender);
	    toSender = NULL;
	    while (sentMsg) {
		smPtr = sentMsg->nextPtr;
		ckfree(sentMsg);
		sentMsg = smPtr;
	    }
	    return;
	}
	if (c) {
	    Tcl_DStringAppend(bufDS, &c, 1);
	}
    } while (c != '\0');

    Tcl_SplitList(interp, Tcl_DStringValue(bufDS), &argc, &argv);

    if (!strcmp(argv[0], "STATUS")) {
	RatLog(interp, atoi(argv[1]), argv[2], atoi(argv[3]));

    } else if (!strcmp(argv[0], "FAILED")) {
	RatLog(interp, RAT_ERROR, argv[3], RATLOG_TIME);
	hardError = atoi(argv[4]);
	if (!hardError) {
	    if (TCL_OK != RatHoldExtract(interp, argv[2], NULL, NULL)) {
		return;
	    }
	    if (TCL_OK != Tcl_VarEval(interp, "ComposeExtracted ",
		    Tcl_GetStringResult(interp), NULL)) {
		RatLog(interp, RAT_ERROR, Tcl_GetStringResult(interp),
			RATLOG_TIME);
	    }
	}
	id = atoi(argv[1]);

    } else if (!strcmp(argv[0], "SAVE")) {
	Tcl_Obj *defPtr;
	int i;

	Tcl_SplitList(interp, argv[2], &destArgc, &destArgv);
	defPtr = Tcl_NewObj();
	for (i=0; i<destArgc; i++) {
	    Tcl_ListObjAppendElement(interp, defPtr,
				     Tcl_NewStringObj(destArgv[i], -1));
	}
	(void)stat(argv[1], &sbuf);
	msgbuf = (char*)ckalloc(sbuf.st_size+STATUS_LENGTH);
	msg = msgbuf + STATUS_LENGTH;
	fd = open(argv[1], O_RDONLY);
	read(fd, msg, sbuf.st_size);
	close(fd);
        infoPtr = RatGetOpenFolder(interp, defPtr);
	if (infoPtr) {
	    msg -= STATUS_LENGTH;
	    memcpy(msg, STATUS_STRING, STATUS_LENGTH);
	    tmp = RatFrMessageCreate(interp, msg, sbuf.st_size+STATUS_LENGTH,
		    NULL);
	    RatFolderInsert(interp, infoPtr, 1, &tmp);
	    Tcl_DeleteCommand(interp, tmp);
	    RatFolderClose(interp, infoPtr, 0);

	} else if (!strcmp(destArgv[1], "file")) {
	    Tcl_Channel channel;
	    int perm;
	    struct tm *tmPtr;
	    time_t now;
	    Tcl_Obj *oPtr;

	    if (5 == destArgc) {
		Tcl_GetInt(interp, destArgv[4], &perm);
	    } else {
		oPtr = Tcl_GetVar2Ex(interp, "option", "permissions",
				     TCL_GLOBAL_ONLY);
		Tcl_GetIntFromObj(interp, oPtr, &perm);
	    }
	    channel = Tcl_OpenFileChannel(interp,destArgv[3],"a", perm);
	    if (NULL != channel) {
		now = time(NULL);
		tmPtr = gmtime(&now);
		strlcpy(buf, "From ", sizeof(buf));
		strlcat(buf, RatGetCurrent(interp,RAT_MAILBOX,""),sizeof(buf));
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
			"@%s %s %s %2d %02d:%02d GMT %04d\n",
			 RatGetCurrent(interp, RAT_HOST, ""),
			 dayName[tmPtr->tm_wday], monthName[tmPtr->tm_mon],
			 tmPtr->tm_mday, tmPtr->tm_hour, tmPtr->tm_min,
			 tmPtr->tm_year+1900);
		strlcat(buf, "Status: RO\n", sizeof(buf));
		Tcl_Write(channel, buf, strlen(buf));
		RatTranslateWrite(channel, msg, sbuf.st_size);
		Tcl_Close(interp, channel);
	    } else {
		RatLogF(interp, RAT_ERROR, "outgoing_save_failed", RATLOG_TIME,
			Tcl_PosixError(interp));
	    }
	} else if (!strcmp(destArgv[1], "dis")) {
	    MAILSTREAM *stream;
	    STRING string;

	    stream = RatDisFolderOpenStream(interp, defPtr);
	    INIT(&string, mail_string, msg, sbuf.st_size);
	    if (!stream || !mail_append_full(stream, stream->mailbox, "\\Seen",
					     NIL, &string)) {
		RatLogF(interp, RAT_ERROR, "outgoing_save_failed", RATLOG_TIME,
			"");
	    }
	    if (stream) {
		CloseStdFolder(interp, stream);
	    }
	
	} else if (!strcmp(destArgv[1], "dbase")) {
	    struct tm *tmPtr;
	    time_t now;

	    now = time(NULL);
	    tmPtr = gmtime(&now);
	    strlcpy(buf, "From ", sizeof(buf));
	    strlcat(buf, RatGetCurrent(interp,RAT_MAILBOX,""),sizeof(buf));
	    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
		     "@%s %s %s %2d %02d:%02d GMT %04d\n",
		     RatGetCurrent(interp, RAT_HOST, ""),
		     dayName[tmPtr->tm_wday], monthName[tmPtr->tm_mon],
		     tmPtr->tm_mday, tmPtr->tm_hour, tmPtr->tm_min,
		     tmPtr->tm_year+1900);

	    if (TCL_OK != RatDbInsert(interp, argv[3], argv[4], argv[5],
		    argv[6], argv[7], argv[8], time(NULL), "RO", destArgv[5],
		    atol(destArgv[4]), destArgv[3], buf, msg, sbuf.st_size)) {
		RatLogF(interp, RAT_ERROR, "outgoing_save_failed", RATLOG_TIME,
			Tcl_GetStringResult(interp));
	    }
	} else if (!strcmp(destArgv[1], "imap")) {
            char *spec;
            
            spec = RatGetFolderSpec(interp, defPtr);
	    AppendToIMAP(interp, spec, argv[9], argv[10], msg, sbuf.st_size);
	} else if (!strcmp(destArgv[1], "mh")) {
	    RatLogF(interp, RAT_ERROR, "save_to_mh", RATLOG_TIME);
	} else {
	    RatLog(interp, RAT_ERROR,
		    "Internal error: illegal save type in RatHandleSender",
		    RATLOG_TIME);
	}
	unlink(argv[1]);
	ckfree(msgbuf);
	ckfree(destArgv);
	Tcl_DecrRefCount(defPtr);
    } else if (!strcmp(argv[0], "PGP")) {
	if (!strcmp("getpass", argv[1])) {
	    tmp = RatPGPPhrase(interp);
	    if (tmp) {
		Tcl_ScanElement(tmp, &flags);
		Tcl_ConvertElement(tmp, buf, flags);
		fprintf(toSender, "PGP PHRASE %s\n", buf);
		memset(buf, '\0', strlen(buf));
		memset(tmp, '\0', strlen(tmp));
		ckfree(tmp);
	    } else {
		fprintf(toSender, "PGP NOPHRASE\n");
	    }
	    fflush(toSender);
	} else if (!strcmp("error", argv[1])) {
	    ClearPGPPass(NULL);
	    Tcl_DStringInit(&cmd);
	    Tcl_DStringAppend(&cmd, "RatPGPError", -1);
	    Tcl_DStringAppendElement(&cmd, argv[2]);
	    if (TCL_OK != Tcl_Eval(interp, Tcl_DStringValue(&cmd))) {
		fprintf(toSender, "PGP ABORT\n");
	    } else {
		fprintf(toSender, "PGP %s\n", Tcl_GetStringResult(interp));
	    }
	    fflush(toSender);
	    Tcl_DStringFree(&cmd);
	}
    } else if (!strcmp(argv[0], "SENT")) {
	RatHoldUpdateVars(interp, deferredDir, -1);

	id = atoi(argv[1]);
    }
    if (!strcmp(argv[0], "SENT") || !strcmp(argv[0], "FAILED")) {
	if (id == sendSequence-1 || hardError) {
	    Tcl_SetVar(interp, "ratSenderSending", "0", TCL_GLOBAL_ONLY);
	}
	for (smPtrPtr=&sentMsg; *smPtrPtr; ) {
	    if ((*smPtrPtr)->id == id || hardError) {
		smPtr = *smPtrPtr;
		*smPtrPtr = smPtr->nextPtr;
		ckfree(smPtr);
	    } else {
		smPtrPtr = &(*smPtrPtr)->nextPtr;
	    }
	}
    }
    ckfree(argv);
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetCTE --
 *
 *	See ../doc/interface
 *
 * Results:
 *      The return value is normally TCL_OK and the result can be found
 *      in the result area If something goes wrong TCL_ERROR is returned
 *      and an error message will be left in the result area.
 *
 * Side effects:
 *      The file passed as argument is read.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatGetCTE(ClientData dummy, Tcl_Interp *interp, int objc,Tcl_Obj *CONST objv[])
{
    Tcl_DString ds;
    char *fileName;
    FILE *fp;
    int seen8bit = 0;
    int seenZero = 0;
    int c;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " filename\"", (char *) NULL);
	return TCL_ERROR;
    }

    fileName = Tcl_UtfToExternalDString(NULL, Tcl_GetString(objv[1]), -1, &ds);
    if (NULL == (fp = fopen(fileName, "r"))) {
	RatLogF(interp, RAT_ERROR, "failed_to_open_file", RATLOG_TIME,
		Tcl_PosixError(interp));
	Tcl_SetResult(interp, "binary", TCL_STATIC);
	Tcl_DStringFree(&ds);
	return TCL_OK;
    }
    Tcl_DStringFree(&ds);

    while (c = getc(fp), !feof(fp)) {
	if (0 == c) {
	    seenZero = 1;
	    break;
	} else if (0x80 & c) {
	    seen8bit = 1;
	}
    }
    if (seenZero) {
	Tcl_SetResult(interp, "binary", TCL_STATIC);
    } else if (seen8bit) {
	Tcl_SetResult(interp, "8bit", TCL_STATIC);
    } else {
	Tcl_SetResult(interp, "7bit", TCL_STATIC);
    }

    fclose(fp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCleanup --
 *
 *	See ../doc/interface
 *
 * Results:
 *      The return value is always TCL_OK.
 *
 * Side effects:
 *      The database is closed.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatCleanup(ClientData dummy, Tcl_Interp *interp,int objc,Tcl_Obj *CONST objv[])
{
    RatDbClose();
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatTildeSubst --
 *
 *	See ../doc/interface
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

static int
RatTildeSubst(ClientData dummy, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    Tcl_DString buffer;
    char *expandedName;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " filename\"", (char *) NULL);
	return TCL_ERROR;
    }

    expandedName = Tcl_TranslateFileName(interp, Tcl_GetString(objv[1]),
					 &buffer);
    Tcl_SetResult(interp, expandedName, TCL_VOLATILE);
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatTime --
 *
 *	See ../doc/interface
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

static int
RatTime(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    time_t goal;

    if (objc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " [+days]\"", (char *) NULL);
	return TCL_ERROR;
    }

    goal = time(NULL);
    if (objc == 2) {
	int i;

	Tcl_GetIntFromObj(interp, objv[1], &i);
	goal += i*24*60*60;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj((int)goal));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSearch --
 *
 *	Does a case insensitive search of a string.
 *
 * Results:
 *      Returns 1 if the searchFor string is found in the searchIn string
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatSearch(char *searchFor, char *searchIn)
{
    static unsigned char *buf = NULL;	/* Used to hold lowercase version */
    static int bufLength = 0;	        /* Length of static buffer */
    int i, j, lengthFor, lengthIn, s, d;

    for (s=d=0; searchFor[s];) {
	if (d >= bufLength) {
	    bufLength += 16;
	    buf = ckrealloc(buf, bufLength);
	}
	if (!(0x80 & (unsigned char)searchFor[s]) &&
		   isupper((unsigned char)searchFor[s])) {
	    buf[d++] = tolower((unsigned char)searchFor[s++]);
	} else {
	    buf[d++] = searchFor[s++];
	}
    }
    buf[d] = '\0';
    lengthFor = d;
    lengthIn = strlen(searchIn);
    for (i = 0; i <= lengthIn-lengthFor; i++) {
	for (j=0; buf[j]; j++) {
	    if (0x80 & buf[j]) {
		if (!(0x80 & (unsigned char)searchIn[i+j])
		    || Tcl_UtfNcasecmp(buf+j, searchIn+i+j, 1)) {
		    break;
		}
		j = Tcl_UtfNext(buf+j)-(char*)buf-1;
	    } else if (isupper((unsigned char)searchIn[i+j])) {
		if (buf[j] != tolower((unsigned char)searchIn[i+j])) {
		    break;
		}
	    } else if (buf[j] != searchIn[i+j]) {
		break;
	    }
	}
	if (!buf[j]) {
	    return 1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatLock --
 *
 *	See ../doc/interface
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

static int
RatLock(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Tcl_Obj *value;
    int i;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " variable ...\"", (char *) NULL);
	return TCL_ERROR;
    }

    for (i=1; i<objc;i++) {
	value = Tcl_ObjGetVar2(interp, objv[i], NULL, TCL_GLOBAL_ONLY);
	Tcl_IncrRefCount(value);
	Tcl_TraceVar(interp, Tcl_GetString(objv[i]), 
		TCL_GLOBAL_ONLY | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		RatReject, (ClientData)value);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatReject --
 *
 *	See ../doc/interface
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

static char*
RatReject(ClientData clientData, Tcl_Interp *interp, CONST84 char *name1,
	CONST84 char *name2, int flags)
{
    Tcl_Obj *correct = (Tcl_Obj*)clientData;

    if (flags & TCL_INTERP_DESTROYED) {
	Tcl_DecrRefCount(correct);
	return NULL;
    }
    if (flags & TCL_TRACE_DESTROYED) {
	Tcl_TraceVar2(interp, name1, name2,
		TCL_GLOBAL_ONLY | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		RatReject, (ClientData)correct);
    }
    if (name2) {
	fprintf(stderr, "Can not set %s(%s) since it has been locked\n",
		name1, name2);
    } else {
	fprintf(stderr, "Can not set %s since it has been locked\n", name1);
    }
    Tcl_SetVar2Ex(interp, name1, name2, correct, TCL_GLOBAL_ONLY);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * RatIsLocked --
 *
 *	See ../doc/interface
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

static int
RatIsLocked(ClientData dummy, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    int b;
    
    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " variable\"", (char *) NULL);
	return TCL_ERROR;
    }
    b = (Tcl_VarTraceInfo(interp, Tcl_GetString(objv[1]), TCL_GLOBAL_ONLY,
			  RatReject, NULL) ? 1 : 0);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(b));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatEncoding --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 *	Opens a file an analyses it to determine the encoding.
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

static int
RatEncoding(ClientData dummy, Tcl_Interp *interp, int objc,
	    Tcl_Obj *CONST objv[])
{
    char *encodingName, *fileName;
    unsigned char c;
    int length, encoding;
    FILE *fp;
    Tcl_DString ds;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " filename\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Determine encoding
     */
    fileName = Tcl_UtfToExternalDString(NULL, Tcl_GetString(objv[1]), -1, &ds);
    if (NULL == (fp = fopen(fileName, "r"))) {
	Tcl_AppendResult(interp, "error opening file \"", fileName, "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	Tcl_DStringFree(&ds);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&ds);
    encoding = ENC7BIT;
    length = 0;
    while (c = (unsigned char)getc(fp), !feof(fp)) {
	if ('\0' == c) {
	    encoding = ENCBINARY;
	    break;
	}
	if ('\n' == c) {
	    length = 0;
	} else {
	    if (++length == 1024) {
		encoding = ENCBINARY;
		break;
	    }
	}
	if (c & 0x80) {
	    encoding = ENC8BIT;
	}
    }
    fclose(fp);
    switch(encoding) {
    case ENC7BIT:   encodingName = "7bit";   break;
    case ENC8BIT:   encodingName = "8bit";   break;
    case ENCBINARY: encodingName = "binary"; break;
    default: 	    encodingName = "unknown"; break;
    }

    Tcl_ResetResult(interp);
    Tcl_AppendElement(interp, encodingName);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatType --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 *	The algorithm is to first determine if the file exists and its
 *	encoding, then run the file command on it and try to match the
 *	result agains the typetable. If we don't find any match the type
 *	defaults to application/octet-stream.
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

static int
RatType(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int listArgc, elemArgc;
    Tcl_Obj *oPtr, **listArgv, **elemArgv, *robjv[2];
    CONST84 char *cmdArgv[3];
    char buf[1024], *encodingName, *fileType;
    Tcl_Channel channel;
    char c;
    int length, i, encoding;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " filename\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Determine encoding
     */
    channel = Tcl_OpenFileChannel(interp, Tcl_GetString(objv[1]), "r", 0);
    if (NULL == channel) {
	Tcl_AppendResult(interp, "error opening file \"",
			 Tcl_GetString(objv[1]), "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    encoding = ENC7BIT;
    length = 0;
    while (Tcl_Read(channel, &c, 1), !Tcl_Eof(channel)) {
	if ('\0' == c) {
	    encoding = ENCBINARY;
	    break;
	}
	if ('\n' == c) {
	    length = 0;
	} else {
	    if (++length == 1024) {
		encoding = ENCBINARY;
		break;
	    }
	}
	if (c & 0x80) {
	    encoding = ENC8BIT;
	}
    }
    Tcl_Close(interp, channel);
    switch(encoding) {
    case ENC7BIT:   encodingName = "7bit";   break;
    case ENC8BIT:   encodingName = "8bit";   break;
    case ENCBINARY: encodingName = "binary"; break;
    default: 	    encodingName = "unkown"; break;
    }

    /*
     * Run the "file" command.
     */
    cmdArgv[0] = "file";
    cmdArgv[1] = Tcl_GetString(objv[1]);
    if (!(channel = Tcl_OpenCommandChannel(interp, 2, cmdArgv, TCL_STDOUT))) {
	return TCL_ERROR;
    }
    length = Tcl_Read(channel, buf, sizeof(buf)-1);
    buf[length] = '\0';
    Tcl_Close(interp, channel);
    fileType = strchr(buf, ':')+1;
    oPtr = Tcl_GetVar2Ex(interp, "option", "typetable", TCL_GLOBAL_ONLY);
    Tcl_ListObjGetElements(interp, oPtr, &listArgc, &listArgv);
    for (i=0; i<listArgc; i++) {
	Tcl_ListObjGetElements(interp, listArgv[i], &elemArgc, &elemArgv);
	if (Tcl_StringMatch(fileType, Tcl_GetString(elemArgv[0]))) {
	    robjv[0] = elemArgv[1];
	    robjv[1] = Tcl_NewStringObj(encodingName, -1);
	    break;
	}
    }
    if (i == listArgc) {
	robjv[0] = Tcl_NewStringObj("application/octet-stream", -1);
	robjv[1] = Tcl_NewStringObj(encodingName, -1);
    }
    oPtr = Tcl_NewListObj(2, robjv);
    Tcl_SetObjResult(interp, oPtr);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatTclPuts --
 *
 *	A version of the unix puts which converts CRLF to the local
 *	newline convention.
 *
 * Results:
 *      Always returns 1L.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

long
RatTclPuts(void *stream_x, char *string)
{
    Tcl_Channel channel = (Tcl_Channel)stream_x;

    if (-1 == Tcl_Write(channel, string, -1)) {
	return 0;
    }
    return(1L);                                 /* T for c-client */
}

/*
 *----------------------------------------------------------------------
 *
 * RatStringPuts --
 *
 *	A version of the unix puts which converts CRLF to the local
 *	newline convention, and instead of storing into a file we
 *	append the data to an Tcl_DString.
 *
 * Results:
 *      Always returns 1L.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

long
RatStringPuts(void *stream_x, char *string)
{
    Tcl_DString *dsPtr = (Tcl_DString*)stream_x;
    char *p;

    for (p = string; *p; p++) {
      if (*p=='\015' && *(p+1)=='\012') {
	  Tcl_DStringAppend(dsPtr, "\n", 1);
	  p++;
      } else {
	  Tcl_DStringAppend(dsPtr, p, 1);
      }
    }

    return(1L);                                 /* T for c-client */
}

/* 
 *----------------------------------------------------------------------
 * 
 * RatDelaySoutr --
 *
 *	A output function to use with rfc822_output that writes to
 *	a file destriptor. This function is special in this that it
 *	always delay writing the last two characters. This allows one to
 *	filter final newlines (which rfc822_output_body insists to add).
 * 
 * Results:
 *	Always returns 1L.
 * 
 * Side effects:
 *	Modifies the ratDelayBuffer array.
 * 
 *
 *----------------------------------------------------------------------
 */

long
RatDelaySoutr(void *stream_x, char *string)
{
    int len1, len2;
    len1 = strlen(ratDelayBuffer);
    len2 = strlen(string);

    if (len1+len2 <= 2) {
	strlcat(ratDelayBuffer, string, sizeof(ratDelayBuffer));
	return 1;
    }
    write((int)stream_x, ratDelayBuffer, len1);
    write((int)stream_x, string, len2-2);
    ratDelayBuffer[0] = string[len2-2];
    ratDelayBuffer[1] = string[len2-1];
    return 1;
}
void
RatInitDelayBuffer()
{
  ratDelayBuffer[0] = '\0';
}

/*
 *----------------------------------------------------------------------
 *
 * RatTranslateWrite --
 *
 *      Write to channel and translate all CRLF to just LF
 *
 * Results:
 *      Numbe rof bytes written
 *
 * SideEffects:
 *      None
 *
 *----------------------------------------------------------------------
 */
int
RatTranslateWrite(Tcl_Channel channel, CONST84 char *b, int len)
{
    int s, e, l;

    for (s=e=l=0; e<len-1; e++) {
        if (b[e] == '\015' && b[e+1] == '\012') {
	    l += Tcl_Write(channel, &b[s], e-s);
	    e++;
	    s = e;
        }
    }
    l += Tcl_Write(channel, &b[s], e-s);

    return l;
}

/*
 *----------------------------------------------------------------------
 *
 * RatPopulateStruct --
 *
 *	Populate a message structure with the content pointers
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Modifies the structure in place
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatPopulateStruct(char *base, BODY *bodyPtr)
{
    PART *partPtr;

    if (TYPEMULTIPART == bodyPtr->type) {
	for (partPtr = bodyPtr->nested.part; partPtr;
		partPtr = partPtr->next) {
	    RatPopulateStruct(base, &partPtr->body);
	}
    } else {
	bodyPtr->contents.text.data =
		(unsigned char*)ckalloc(bodyPtr->contents.text.size+1);
	memcpy(bodyPtr->contents.text.data, base+bodyPtr->contents.offset,
		bodyPtr->contents.text.size);
	bodyPtr->contents.text.data[bodyPtr->contents.text.size] = '\0';
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatParseMsg --
 *
 *	Parses the message given as argument into an MESSAGE structure.
 *	The data at message is used in place so it may not be freed
 *	before the MESSAGE structure is freed.
 *
 * Results:
 *      Returns a pointer to a newly allocated MESSAGE structure
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

MESSAGE*
RatParseMsg(Tcl_Interp *interp, unsigned char *message)
{
    int length;		/* Length of header */
    int bodyOffset = 0;	/* Offset of body from start of header */
    MESSAGE *msgPtr;	/* Pointer to message to return */
    STRING bodyString;	/* Body data */

    for (length = 0; message[length]; length++) {
	if (message[length] == '\n' && message[length+1] == '\n') {
	    length++;
	    bodyOffset = length+1;
	    break;
	}
	if (message[length]=='\r' && message[length+1]=='\n'
		&& message[length+2]=='\r' && message[length+3]=='\n') {
	    length += 2;
	    bodyOffset = length+2;
	    break;
	}
    }
    msgPtr = (MESSAGE*)ckalloc(sizeof(MESSAGE));
    msgPtr->text.text.data = (unsigned char*)message;
    msgPtr->text.text.size = strlen((char*)message);
    msgPtr->text.offset = bodyOffset;
    INIT(&bodyString, mail_string, (void*) (char*)(message+bodyOffset),
	    strlen((char*)message)-bodyOffset);
    rfc822_parse_msg(&msgPtr->env, &msgPtr->body, (char*)message, length,
                     &bodyString, RatGetCurrent(interp, RAT_HOST, ""), NIL);
    RatPopulateStruct((char*)message+bodyOffset, msgPtr->body);
    return msgPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDSE --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A standard tcl result and the requested number is left in the
 *	result string.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatDSE(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(RatDbDaysSinceExpire(interp)));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatExpire --
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

static int
RatExpire(ClientData dummy, Tcl_Interp *interp, int objc,Tcl_Obj *const objv[])
{
    if (objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]), " inbox backupDir\"",
			 (char *) NULL);
	return TCL_ERROR;
    }
    return RatDbExpire(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

/*
 *----------------------------------------------------------------------
 *
 * RatIsEmpty --
 *
 *	Check if a string contains anything else than whitespace.
 *
 * Results:
 *	Returns null if the string contains other chars than whitespace.
 *	Otherwise non-null is returned.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatIsEmpty (const char *string)
{
    while (string && *string && isspace((unsigned char)*string)) {
	string++;
    }
    if (string && *string) {
	return 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * RatLindex --
 *
 *	Get a specific entry of a list.
 *
 * Results:
 *	A pointer to a static area which contains the requested item.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatLindex(Tcl_Interp *interp, const char *list, int index)
{
    static char *item = NULL;
    static int itemsize = 0;
    CONST84 char **argv = NULL;
    const char *act;
    int argc;

    if (TCL_OK != Tcl_SplitList(interp, list, &argc, &argv)) {
	if (0 != index) {
	    return NULL;
	}
	act = list;
    } else {
	if (index >= argc) {
	    ckfree(argv);
	    return NULL;
	}
	act = argv[index];
    }
    if (itemsize < (int)(strlen(act)+1)) {
	itemsize = strlen(act)+1;
	item = (char*)ckrealloc(item, itemsize);
    }
    strcpy(item, act);
    ckfree(argv);
    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * RatLL --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      The length of the given line.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatLL(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    CONST84 char *cPtr;
    int l;

    if (2 != objc) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), " line",
			 (char*) NULL);
	return TCL_ERROR;
    }

    for (l=0, cPtr = Tcl_GetString(objv[1]); *cPtr; cPtr = Tcl_UtfNext(cPtr)) {
	if ('\t' == *cPtr) {
	    l += 8-l%8;
	} else {
	    l++;
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(l));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatGen --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A string of spaces with the given length
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatGen(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *s;
    int i, l;

    if (2 != objc || TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &l)) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), " length",
		(char*) NULL);
	return TCL_ERROR;
    }

    s = Tcl_NewObj();
    for (i=0; i<l; i++) {
	Tcl_AppendToObj(s, " ", 1);
    }
    Tcl_SetObjResult(interp, s);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatWrapCited --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A wrapped text
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatWrapCited(ClientData dummy, Tcl_Interp *interp, int objc,
	Tcl_Obj *const objv[])
{
    if (2 != objc) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), " msg",
		(char*) NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, RatWrapMessage(interp, objv[1]));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDbaseCheck --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A list of strings to display to the user.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatDbaseCheck(ClientData dummy, Tcl_Interp *interp, int objc,
	      Tcl_Obj *const objv[])
{
    int fix;

    if (2 != objc || TCL_OK != Tcl_GetBooleanFromObj(interp, objv[1], &fix)) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), " fix",
			 (char*) NULL);
	return TCL_ERROR;
    }
    return RatDbCheck(interp, fix);
}


/*
 *----------------------------------------------------------------------
 *
 * RatOptionWatcher --
 *
 *	A trace function that gets called when the user modifies any of
 *	the options
 *
 * Results:
 *      NULL.
 *
 * Side effects:
 *      Depends on the optiosn set:-)
 *
 *
 *----------------------------------------------------------------------
 */

static char*
RatOptionWatcher(ClientData clientData, Tcl_Interp *interp,
		 CONST84 char *name1, CONST84 char *name2, int flags)
{
    char buf[32];
    Tcl_Obj *oPtr;
    int i;
    CONST84 char *v, *cPtr;

    if (NULL == (cPtr = strchr(name2, ','))) {
	cPtr = name2;
    }
    if (!strcmp(name2, "domain")
	|| !strcmp(name2, "charset")
	|| !strcmp(name2, "smtp_verbose")
	|| !strcmp(name2, "smtp_timeout")
	|| !strcmp(name2, "force_send")
	|| !strcmp(name2, "pgp_version")
	|| !strcmp(name2, "pgp_path")
	|| !strcmp(name2, "pgp_args")
	|| !strcmp(name2, "pgp_keyring")
	|| NULL != strchr(name2, ',')) {
	strlcpy(buf, "RatSend kill", sizeof(buf));
	Tcl_Eval(interp, buf);

    } else if (!strcmp(name2, "ssh_path")) {
	v = RatGetPathOption(interp, "ssh_path");
	if (v && *v) {
	    tcp_parameters(SET_SSHPATH, (void*)v);
	}

    } else if (!strcmp(name2, "ssh_timeout")) {
	oPtr = Tcl_GetVar2Ex(interp, "option", "ssh_timeout", TCL_GLOBAL_ONLY);
	if (oPtr && TCL_OK == Tcl_GetIntFromObj(interp, oPtr, &i) && i) {
	    tcp_parameters(SET_SSHTIMEOUT, (void*)i);
	}
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * KodHandler --
 *
 *	Handle the Kiss Of Death signal, actually there are three
 *	different functions implementing this. One is the signal handler
 *	KodHandlerSig() which schedules the asynchronous event handler
 *	KodHandlerAsync() which in turn schedules the final handler to run
 *	when the program is idle KodHandlerIdle().
 *	This final handler does the actual work of closing all open folders.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      All folders are closed
 *
 *
 *----------------------------------------------------------------------
 */

static void
KodHandlerSig(int s)
{
    Tcl_AsyncMark(kodhandler);
    signal(s, KodHandlerSig);
}

static int
KodHandlerAsync(ClientData interp, Tcl_Interp *notused, int code)
{
    Tcl_DoWhenIdle(KodHandlerIdle, interp);
    return code;
}

static void
KodHandlerIdle(ClientData clientData)
{
    Tcl_Interp *interp = (Tcl_Interp*)clientData;
    char buf[1024];

    while (ratFolderList) {
	snprintf(buf, sizeof(buf), "%s close 1", ratFolderList->cmdName);
	Tcl_GlobalEval(interp, buf);
    }
    RatLogF(interp, RAT_ERROR, "mailbox_stolen", RATLOG_TIME);
    strlcpy(buf, "foreach fh $folderWindowList {FolderWindowClear $fh}",
	    sizeof(buf));
    Tcl_GlobalEval(interp, buf);
}


/*
 *----------------------------------------------------------------------
 *
 * RatFormatDateCmd --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A list of strings to display to the user.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatFormatDateCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		 Tcl_Obj *const objv[])
{
    int month, day;
    
    if (7 != objc
	|| TCL_OK != Tcl_GetIntFromObj(interp, objv[2], &month)
	|| TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &day)) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), \
		" year month day hour min sec", (char*) NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, RatFormatDate(interp, month-1, day));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFormatDate --
 *
 *	Print the data in a short format.
 *
 * Results:
 *      A pointer to a static area.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatFormatDate(Tcl_Interp *interp, int month, int day)
{
    static char *months[12];
    static int initialized = 0;
    char buf[8];

    if (!initialized) {
	int i, argc;
	Tcl_Obj *oPtr, **argv;

	oPtr = Tcl_GetVar2Ex(interp, "t", "months", TCL_GLOBAL_ONLY);
	Tcl_ListObjGetElements(interp, oPtr, &argc, &argv);
	for (i=0; i<12; i++) {
	    months[i] = Tcl_GetString(argv[i]);
	}
	initialized = 1;
    }

    snprintf(buf, sizeof(buf), "%2d %s", day, months[month]);
    return Tcl_NewStringObj(buf, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetTimeZone --
 *
 *	Determines the current timezone.  The method varies wildly
 *	between different platform implementations, so its hidden in
 *	this function.
 *
 *	This function is shamelessy stolen from tcl8.0p2
 *
 * Results:
 *	The return value is the local time zone, measured in
 *	minutes away from GMT (-ve for east, +ve for west).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
RatGetTimeZone(unsigned long currentTime)
{
    /*
     * Determine how a timezone is obtained from "struct tm".  If there is no
     * time zone in this struct (very lame) then use the timezone variable.
     * This is done in a way to make the timezone variable the method of last
     * resort, as some systems have it in addition to a field in "struct tm".
     * The gettimeofday system call can also be used to determine the time
     * zone.
     */
    
#if defined(HAVE_TM_TZADJ)
#   define TCL_GOT_TIMEZONE
    time_t      curTime = (time_t) currentTime;
    struct tm  *timeDataPtr = localtime(&curTime);
    int         timeZone;

    timeZone = timeDataPtr->tm_tzadj  / 60;
    if (timeDataPtr->tm_isdst) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#if defined(HAVE_TM_GMTOFF) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    time_t     curTime = (time_t) currentTime;
    struct tm *timeDataPtr = localtime(&curTime);
    int        timeZone;

    timeZone = -(timeDataPtr->tm_gmtoff / 60);
    if (timeDataPtr->tm_isdst) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#if defined(USE_DELTA_FOR_TZ)
#define TCL_GOT_TIMEZONE 1
    /*
     * This hack replaces using global var timezone or gettimeofday
     * in situations where they are buggy such as on AIX when libbsd.a
     * is linked in.
     */

    int timeZone;
    time_t tt;
    struct tm *stm;
    tt = 849268800L;      /*    1996-11-29 12:00:00  GMT */
    stm = localtime(&tt); /* eg 1996-11-29  6:00:00  CST6CDT */
    /* The calculation below assumes a max of +12 or -12 hours from GMT */
    timeZone = (12 - stm->tm_hour)*60 + (0 - stm->tm_min);
    return timeZone;  /* eg +360 for CST6CDT */
#endif

    /*
     * Must prefer timezone variable over gettimeofday, as gettimeofday does
     * not return timezone information on many systems that have moved this
     * information outside of the kernel.
     */
    
#if defined(HAVE_TIMEZONE_VAR) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    static int setTZ = 0;
    int        timeZone;

    if (!setTZ) {
        tzset();
        setTZ = 1;
    }

    /*
     * Note: this is not a typo in "timezone" below!  See tzset
     * documentation for details.
     */

    timeZone = timezone / 60;

    return timeZone;
#endif

#if !defined(NO_GETTOD) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    struct timeval  tv;
    struct timezone tz;
    int timeZone;

    gettimeofday(&tv, &tz);
    timeZone = tz.tz_minuteswest;
    if (tz.tz_dsttime) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#ifndef TCL_GOT_TIMEZONE
    /*
     * Cause compile error, we don't know how to get timezone.
     */
    error: autoconf did not figure out how to determine the timezone. 
#endif

}

/*
 *----------------------------------------------------------------------
 *
 * RatSetCharset --
 *
 *	Set the system charset
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	The system character set is modified
 *
 *
 *----------------------------------------------------------------------
 */

static char*
RatSetCharset(ClientData clientData, Tcl_Interp *interp, CONST84 char *name1,
	      CONST84 char *name2, int flags)
{
    static char buf[1024];
    CONST84 char *charset;

    charset = Tcl_GetVar2(interp, "ratCurrent", "charset", TCL_GLOBAL_ONLY);
    if (TCL_OK != Tcl_SetSystemEncoding(interp, charset)) {
	strlcpy(buf, Tcl_GetStringResult(interp), sizeof(buf));
	return buf;
    } else {
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatExit --
 *
 *	Cleanup on program exit
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Frees allocated memory
 *
 *
 *----------------------------------------------------------------------
 */
void RatExit(ClientData clientData)
{
#ifdef MEM_DEBUG
    static int cleaned = 0;
    int i;

    if (cleaned) {
	return;
    }
    for (i=0; i<12 && mem_months; i++) {
	ckfree(mem_months[i]);
    }
    ratStdMessageCleanup();
    ratStdFolderCleanup();
    ratMessageCleanup();
    ratAddressCleanup();
    cleaned = 1;
#endif /* MEM_DEBUG */
}

/*
 *----------------------------------------------------------------------
 *
 * RatDStringApendNoCRLF --
 *
 *	A version of TCL_DStringAPpend which also converts CRLF-linenedings
 *	to single LF.
 *
 * Results:
 *      none
 *
 * Side effects:
 *	none
 *
 *
 *----------------------------------------------------------------------
 */

void
RatDStringApendNoCRLF(Tcl_DString *ds, const char *s, int length)
{
    int i;

    if (-1 == length) {
	length = strlen(s);
    }
    for (i=0; i<length; i++) {
	if (s[i] == '\r' && s[i+1] == '\n') i++;
	Tcl_DStringAppend(ds, s+i, 1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatReadFile --
 *
 *	Reads a file and stores it in a block of memeory. Optionally
 *	make sure that the stored data is CRLF-encoded.
 *
 * Results:
 *      A pointer to the block of memeory. It is the callers responsibility
 *	to later free this block of memory. If an error occurs it will
 *	return NULL and store an error message in the result.
 *
 * Side effects:
 *	none
 *
 *
 *----------------------------------------------------------------------
 */
unsigned char*
RatReadFile(Tcl_Interp *interp, const char *filename, unsigned long *length,
	     int convert_to_crlf)
{
    unsigned char *data;
    char buf[1024];
    struct stat statbuf;
    int allocated, ci;
    unsigned long len;
    FILE *fp;
    
    if (NULL == (fp = fopen(filename, "r"))) {
	snprintf(buf, sizeof(buf), "Failed to open file \"%s\": %s",
		 filename, Tcl_PosixError(interp));
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return NULL;
    }
    fstat(fileno(fp), &statbuf);
    allocated = statbuf.st_size/20 + statbuf.st_size + 1;
    data = (unsigned char*)ckalloc(allocated);
    len = 0;
    if (convert_to_crlf) {
	while (EOF != (ci = getc(fp))) {
	    if (len >= allocated-2) {
		allocated += 1024;
		data = (unsigned char*)ckrealloc(data, allocated);
	    }
	    if (ci == '\n' && (0==len || '\r' != data[len-1])) {
		data[len++] = '\r';
	    }
	    data[len++] = ci;
	}
    } else {
	fread(data, statbuf.st_size, 1, fp);
	len = statbuf.st_size;
    }
    data[len] = '\0';
    fclose(fp);
    
    if (length) {
	*length = len;
    }
    return data;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetPathOption --
 *
 *	Gets the value of an option and does ~-substitutions on it.
 *
 * Results:
 *      A pointer to the desired value (after expansion). The value is
 *	stored in a block of memeory managed by this module and the value
 *	is valid until the next call to this procedure.
 *
 * Side effects:
 *	none
 *
 *
 *----------------------------------------------------------------------
 */
CONST84 char*
RatGetPathOption(Tcl_Interp *interp, char *name)
{
    static Tcl_DString ds;
    static int dsUsed = 0;
    CONST84 char *value;
    
    if (NULL == (value = Tcl_GetVar2(interp, "option",name,TCL_GLOBAL_ONLY))) {
	return NULL;
    }
    if (dsUsed) {
	Tcl_DStringFree(&ds);
    }
    value = Tcl_TranslateFileName(interp, value, &ds);
    if (value) {
	dsUsed = 1;
    } else {
	dsUsed = 0;
    }
    return value;
}

/*
 *----------------------------------------------------------------------
 *
 * RatEncodeMutf7Cmd --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A string encoded in Mutf7
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatEncodeMutf7Cmd(ClientData dummy, Tcl_Interp *interp, int objc,
		  Tcl_Obj *const objv[])
{
    char *res;
    Tcl_Obj *oPtr;
    
    if (objc != 2) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), \
		" string_to_convert", (char*) NULL);
	return TCL_ERROR;
    }
    res = RatUtf8toMutf7(Tcl_GetString(objv[1]));
    oPtr = Tcl_NewStringObj(res, -1);
    Tcl_SetObjResult(interp, oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatLibSetOnlineMode --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      God online or offline
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatLibSetOnlineModeCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		       Tcl_Obj *const objv[])
{
    static Tcl_Obj *part1 = NULL;
    static Tcl_Obj *part2 = NULL;
    int online;
    
    if (objc != 2 || TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &online)) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]),
			 " online", (char*) NULL);
	return TCL_ERROR;
    }
    if (NULL == part1) {
	part1 = Tcl_NewStringObj("option", 6);
	part2 = Tcl_NewStringObj("online", 6);
    }
    Tcl_ObjSetVar2(interp, part1, part2, Tcl_NewBooleanObj(online),
		   TCL_GLOBAL_ONLY);
    if (TCL_ERROR == RatDisOnOffTrans(interp, online)) {
	Tcl_ObjSetVar2(interp, part1, part2, Tcl_NewBooleanObj(0),
		       TCL_GLOBAL_ONLY);
	return TCL_ERROR;
    }
    if (online) {
	RatSendDeferred(interp);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatTestCmd --
 *
 *	Command used for automated tests
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      Various
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatTestCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    char *s;
    int l;

    if (objc == 5
	&& !strcmp("encode_header", Tcl_GetString(objv[1]))
	&& TCL_OK == Tcl_GetIntFromObj(interp, objv[3], &l)) {
	/* RatTest encode_header charsets name-length text */
	s = RatEncodeHeaderLine(interp, objv[4], l);
	Tcl_SetResult(interp, s, TCL_VOLATILE);
	return TCL_OK;

    } else if (objc == 3
	       && !strcmp("decode_header", Tcl_GetString(objv[1]))) {
	/* RatTest decode_header header */
	s = RatDecodeHeader(interp, Tcl_GetString(objv[2]), 0);
	Tcl_SetResult(interp, s, TCL_VOLATILE);
	return TCL_OK;
	
    } else {
 	Tcl_AppendResult(interp, "Bad usage", TCL_STATIC);
	return TCL_ERROR;
    }
}
