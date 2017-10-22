/* 
 * ratAppInit.c --
 *
 *	Provides a default version of the Tcl_AppInit procedure for
 *	use in wish and similar Tk-based applications.
 *
 *
 * TkRat software and its included text is Copyright 1996-2004 by
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
#define LIBVERSION	"2.3.0"
#define LIBDATE		"20171022"

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
 * How often we should touch the file in the tmp directory (in milliseconds)
 */
#define TOUCH_INTERVAL (24*60*60*1000)

/*
 * Names of days and months as per rfc822.
 */
char *dayName[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char *monthName[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


/*
 * This is used by the sender child process to indicate that logging should
 * be done though its own special function
 */
int is_sender_child = 0;

/*
 * Buffer for delayed output
 */
static char ratDelayBuffer[3];

/*
 * KOD-handler (Kiss Of Death)
 */
static Tcl_AsyncHandler kodhandler;

/*
 * Interpreter for timer procedures
 */
Tcl_Interp *timerInterp;

/*
 * Local functions
 */
static Tcl_TimerProc RatChildHandler;
static Tcl_TimerProc RatTmpdirToucher;
static Tcl_VarTraceProc RatReject;
static Tcl_AppInitProc RatAppInit;
static Tcl_VarTraceProc RatOptionWatcher;
static Tcl_ObjCmdProc RatGetCurrentCmd;
static Tcl_ObjCmdProc RatBgExecCmd;
static Tcl_ObjCmdProc RatGetCTECmd;
static Tcl_ObjCmdProc RatCleanupCmd;
static Tcl_ObjCmdProc RatTildeSubstCmd;
static Tcl_ObjCmdProc RatTimeCmd;
static Tcl_ObjCmdProc RatLockCmd;
static Tcl_ObjCmdProc RatIsLockedCmd;
static Tcl_ObjCmdProc RatEncodingCmd;
static Tcl_ObjCmdProc RatDSECmd;
static Tcl_ObjCmdProc RatExpireCmd;
static Tcl_ObjCmdProc RatLLCmd;
static Tcl_ObjCmdProc RatGenCmd;
static Tcl_ObjCmdProc RatWrapCitedCmd;
static Tcl_ObjCmdProc RatDbaseCheckCmd;
static Tcl_ObjCmdProc RatMangleNumberCmd;
static void KodHandlerSig(int s);
static Tcl_AsyncProc KodHandlerAsync;
static Tcl_IdleProc KodHandlerIdle;
static void RatPopulateStruct(char *base, BODY *bodyPtr);
static Tcl_VarTraceProc RatSetCharset;
static Tcl_ExitProc RatExit;
static Tcl_ObjCmdProc RatEncodeMutf7Cmd;
static Tcl_ObjCmdProc RatLibSetOnlineModeCmd;
static Tcl_ObjCmdProc RatTestCmd;
static Tcl_ObjCmdProc RatNudgeSenderCmd;
static Tcl_ObjCmdProc RatExtractAddressesCmd;
static Tcl_ObjCmdProc RatGenerateDateCmd;
static Tcl_ObjCmdProc RatGenerateMsgIdCmd;

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
    struct passwd *pwPtr = NULL;
    double tcl_version;
    Tcl_Obj *oPtr;
    char *c, tmp[1024];
    CONST84 char *v;
    int i;

    setlocale(LC_TIME, "");
    setlocale(LC_CTYPE, "");
    setlocale(LC_COLLATE, "");
    timerInterp = interp;

    /*
     * Check tcl version
     * But do it softly and ignore unexpected errors
     */
    oPtr = Tcl_GetVar2Ex(interp, "tcl_version", NULL, TCL_GLOBAL_ONLY);
    if (TCL_OK == Tcl_GetDoubleFromObj(interp, oPtr, &tcl_version)
	&& tcl_version < 8.3) {
	fprintf(stderr,
		"TkRat requires tcl/tk 8.3 or later (detected %4.1f)\n",
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
	    fprintf(stderr, "Failed to create tmp-directory '%s': %s\n", tmp,
		    strerror(errno));
	    exit(1);
	}
    }
    if (100 == i) {
	fprintf(stderr, "Failed to create temporary directory '%s'\n", tmp);
    }
    Tcl_SetVar(interp, "rat_tmp", tmp, TCL_GLOBAL_ONLY);
    RatReleaseWatchdog(tmp);
    Tcl_CreateTimerHandler(TOUCH_INTERVAL, RatTmpdirToucher, NULL);

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
    i = 1;
    mail_parameters(NIL, SET_USERHASNOLIFE, (void*)i);

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
	if (pwPtr == NULL) {
	    pwPtr = GetPw();
	}
	Tcl_SetVar2(interp, "env", "USER", pwPtr->pw_name, TCL_GLOBAL_ONLY);
    }
    if (!Tcl_GetVar2(interp, "env", "GECOS", TCL_GLOBAL_ONLY)) {
	if (pwPtr == NULL) {
	    pwPtr = GetPw();
	}
	strlcpy(tmp, pwPtr->pw_gecos, sizeof(tmp));
	if ((c = strchr(tmp, ','))) {
	    *c = '\0';
	}
	Tcl_SetVar2(interp, "env", "GECOS", tmp, TCL_GLOBAL_ONLY);
    }
    if (!Tcl_GetVar2(interp, "env", "HOME", TCL_GLOBAL_ONLY)) {
	if (pwPtr == NULL) {
	    pwPtr = GetPw();
	}
	Tcl_SetVar2(interp, "env", "HOME", pwPtr->pw_dir, TCL_GLOBAL_ONLY);
    }
    if (!Tcl_GetVar2(interp, "env", "MAIL", TCL_GLOBAL_ONLY)) {
	char buf[1024];

	if (pwPtr == NULL) {
	    pwPtr = GetPw();
	}
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

    RatInitAddressHandling(interp);
    
    /*
     * Call Tcl_CreateObjCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */
    Tcl_CreateObjCommand(interp, "RatGetCurrent", RatGetCurrentCmd, NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatBgExec", RatBgExecCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGenId", RatGenIdCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGetCTE", RatGetCTECmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCleanup", RatCleanupCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatTildeSubst", RatTildeSubstCmd, NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatTime", RatTimeCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatLock", RatLockCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatIsLocked", RatIsLockedCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDaysSinceExpire", RatDSECmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatExpire", RatExpireCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatLL", RatLLCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGen", RatGenCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatWrapCited", RatWrapCitedCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDbaseCheck", RatDbaseCheckCmd, NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatMailcapReload", RatMailcapReloadCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatPGP", RatPGPCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatMangleNumber", RatMangleNumberCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCheckEncodings", RatCheckEncodingsCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatPurgePwChache", RatPasswdCachePurgeCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatPrettyPrintMsg", RatPrettyPrintMsgCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatEncodeMutf7", RatEncodeMutf7Cmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatLibSetOnlineMode", RatLibSetOnlineModeCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatEncoding", RatEncodingCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatBusy", RatBusyCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatTest", RatTestCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatEncodeQP", RatEncodeQPCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDecodeQP", RatDecodeQPCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCreateMessage", RatCreateMessageCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatNudgeSender",RatNudgeSenderCmd,NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatExtractAddresses", RatExtractAddressesCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatGenerateDate", RatGenerateDateCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDbaseInfo", RatDbaseInfoCmd, NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatGetMatchingAddrsImpl",
                         RatGetMatchingAddrsImplCmd, NULL,NULL);
    Tcl_CreateObjCommand(interp, "RatGenerateMsgId", RatGenerateMsgIdCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCreateSequence", RatCreateSequenceCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatCheckListFormat", RatCheckListFormatCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDecodeUrlc", RatDecodeUrlcCmd,
			 NULL, NULL);
    Tcl_CreateObjCommand(interp, "RatDbaseKeywords", RatDbaseKeywordsCmd,
                         NULL, NULL);

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
 *	  if option($role,from) is set and contains a domain then
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
    ADDRESS *address = NULL;
    static char buf[1024];
    char *result = NULL, *personal, hostbuf[1024];
    CONST84 char *host, *from, *uqdom, *helo, *mailbox;
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
	} else {
	    strlcpy(buf, Tcl_GetVar2(interp, "env", "USER", TCL_GLOBAL_ONLY),
		    sizeof(buf));
	}
	result = buf;
	break;

     case RAT_EMAILADDRESS:
        if (address && address->host) {
            host = address->host;
        } else {
            snprintf(buf, sizeof(buf), "%s,uqa_domain", role);
            uqdom = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
            if (uqdom && 0 < strlen(uqdom)) {
                host = uqdom;
            } /* else use previous host value */
        }
        if (address && address->mailbox) {
            mailbox = address->mailbox;
        } else {
            mailbox = Tcl_GetVar2(interp, "env", "USER", TCL_GLOBAL_ONLY);
        }
        snprintf(buf, sizeof(buf), "%s@%s", mailbox, host);
        result = buf;
        break;

    case RAT_PERSONAL:
        if (address && address->personal) {
            oPtr = Tcl_NewStringObj(address->personal, -1);
        } else {
            oPtr = Tcl_GetVar2Ex(interp, "env", "GECOS", TCL_GLOBAL_ONLY),
            Tcl_IncrRefCount(oPtr);
        }
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
    RatCurrentType what;
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
	} else {
	    goto usage;
	}
    } else {
      usage:
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
    static char *buf = NULL;
    static int bufsize = 0;
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
    if (bufsize < 16 + strlen(parsedMsg) + 9) {
	bufsize = 1024 + strlen(parsedMsg);
	buf = (char*)ckrealloc(buf, bufsize);
    }
    snprintf(buf, bufsize, "RatLog %d %s %s", levelNumber, parsedMsg, typeStr);
    if (is_sender_child) {
	RatSenderLog(buf);
    } else {
        if (TCL_OK != Tcl_GlobalEval(interp, buf)) {
	    Tcl_AppendResult(interp, "Error: '", Tcl_GetStringResult(interp),
		    "'\nWhile executing '", buf, "'\n", NULL);
        }
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
 * RatMangleNumberCmdCmd --
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
 * RatBgExecCmd --
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
RatBgExecCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	     Tcl_Obj *CONST objv[])
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
 * RatTmpdirToucher --
 *
 *	This timer touches a file in the tmp-directory. This should
 *      prevent the directory to be removed by and tmp-removal programs.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatTmpdirToucher(ClientData clientData)
{
    char buf[1024];
    const char *tmp = Tcl_GetVar(timerInterp, "rat_tmp", TCL_GLOBAL_ONLY);
    int fd, l;

    snprintf(buf, sizeof(buf), "%s/mark", tmp);
    if (0 <= (fd = open(buf, O_RDWR|O_TRUNC|O_CREAT, 0644))) {
	l = safe_write(fd, "mark", 4); /* Ignore result */
	lseek(fd, 0, SEEK_SET);
	SafeRead(fd, buf, 4);
	close(fd);
    }
    
    Tcl_CreateTimerHandler(TOUCH_INTERVAL, RatTmpdirToucher, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * RatGenIdCmd --
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

char*
RatGenId()
{
    static long lastid = 0;
    static char buf[64];

    long t = time(NULL);
    if (t <= lastid)
        lastid++;
    else
        lastid = t;
    snprintf(buf, sizeof(buf), "%lx.%x", lastid, (int)getpid());
    return buf;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGenIdCmd --
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
RatGenIdCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	    Tcl_Obj *const objv[])
{
    Tcl_SetResult(interp, RatGenId(), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetCTECmd --
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
RatGetCTECmd(ClientData dummy, Tcl_Interp *interp, int objc,
	     Tcl_Obj *CONST objv[])
{
    CONST84 char *fileName;
    FILE *fp;
    int seen8bit = 0;
    int seenZero = 0;
    int c;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " filename\"", (char *) NULL);
	return TCL_ERROR;
    }

    fileName = RatTranslateFileName(interp, Tcl_GetString(objv[1]));
    if (NULL == (fp = fopen(fileName, "r"))) {
	RatLogF(interp, RAT_ERROR, "failed_to_open_file", RATLOG_TIME,
		Tcl_PosixError(interp));
	Tcl_SetResult(interp, "binary", TCL_STATIC);
	return TCL_OK;
    }

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
RatCleanupCmd(ClientData dummy, Tcl_Interp *interp,int objc,
	      Tcl_Obj *CONST objv[])
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
RatTildeSubstCmd(ClientData dummy, Tcl_Interp *interp, int objc,
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
RatTimeCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	   Tcl_Obj *CONST objv[])
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
	    buf = (unsigned char*)ckrealloc(buf, bufLength);
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
		    || Tcl_UtfNcasecmp((char*)buf+j, searchIn+i+j, 1)) {
		    break;
		}
		j = Tcl_UtfNext((char*)buf+j)-(char*)buf-1;
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
RatLockCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	   Tcl_Obj *CONST objv[])
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
 * RatIsLockedCmd --
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
RatIsLockedCmd(ClientData dummy, Tcl_Interp *interp, int objc,
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
 * RatEncodingCmd --
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
RatEncodingCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
    char *encodingName;
    CONST84 char *fileName;
    unsigned char c;
    int length, encoding;
    FILE *fp;

    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " filename\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Determine encoding
     */
    fileName = RatTranslateFileName(interp, Tcl_GetString(objv[1]));
    if (NULL == (fp = fopen(fileName, "r"))) {
	Tcl_AppendResult(interp, "error opening file \"", fileName, "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
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
    int len1, len2, l;
    len1 = strlen(ratDelayBuffer);
    len2 = strlen(string);

    if (len1+len2 <= 2) {
	strlcat(ratDelayBuffer, string, sizeof(ratDelayBuffer));
	return 1;
    }
    l = safe_write((int)stream_x, ratDelayBuffer, len1); /* Ignore result */
    l = safe_write((int)stream_x, string, len2-2);       /* Ignore result */
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

    for (s=e=l=0; e<len; e++) {
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
 * RatDSECmd --
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
RatDSECmd(ClientData dummy, Tcl_Interp *interp, int objc,
	  Tcl_Obj *const objv[])
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(RatDbDaysSinceExpire(interp)));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatExpireCmd --
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
RatExpireCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	     Tcl_Obj *const objv[])
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
 * RatLLCmd --
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
RatLLCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
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
 * RatGenCmd --
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
RatGenCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	  Tcl_Obj *const objv[])
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
 * RatWrapCitedCmd --
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
RatWrapCitedCmd(ClientData dummy, Tcl_Interp *interp, int objc,
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
 * RatDbaseCheckCmd --
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
RatDbaseCheckCmd(ClientData dummy, Tcl_Interp *interp, int objc,
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
    Tcl_Obj *oPtr;
    int i;
    CONST84 char *v, *cPtr;

    if (NULL == (cPtr = strchr(name2, ','))) {
	cPtr = name2;
    }

    if (!strcmp(name2, "ssh_path")) {
	v = RatGetPathOption(interp, "ssh_path");
	if (v && *v) {
	    tcp_parameters(SET_SSHPATH, (void*)v);
	}

    } else if (!strcmp(name2, "ssh_timeout")) {
	oPtr = Tcl_GetVar2Ex(interp, "option", "ssh_timeout", TCL_GLOBAL_ONLY);
	if (oPtr && TCL_OK == Tcl_GetIntFromObj(interp, oPtr, &i) && i) {
	    tcp_parameters(SET_SSHTIMEOUT, (void*)i);
	}
    } else if (!strcmp(name2, "watcher_time")) {
	RatFolderUpdateTime((ClientData)interp);
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
RatFormatDate(Tcl_Interp *interp, struct tm *tm)
{
    char buf[1024];
    const char *format;

    format = Tcl_GetVar2(interp, "option", "date_format", TCL_GLOBAL_ONLY);
    strftime(buf, sizeof(buf), format, tm);
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
 * RatGetPathOption --
 *
 *	Gets the value of an option and does ~-substitutions on it.
 *
 * Results:
 *      A pointer to the desired value (after expansion). The value is
 *	stored in a block of memory managed by this module and the value
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
    CONST84 char *value;
    
    if (NULL == (value = Tcl_GetVar2(interp, "option",name,TCL_GLOBAL_ONLY))) {
	return NULL;
    }
    return RatTranslateFileName(interp, value);
}

/*
 *----------------------------------------------------------------------
 *
 * RatTranslateFileName --
 *
 *	Translates a filename to local conventions (including charset).
 *
 * Results:
 *      A pointer to the desired value (after expansion). The value is
 *	stored in a block of memory managed by this module and the value
 *	is valid until the next call to this procedure.
 *
 * Side effects:
 *	none
 *
 *
 *----------------------------------------------------------------------
 */
CONST84 char*
RatTranslateFileName(Tcl_Interp *interp, CONST84 char *name)
{
    static Tcl_DString ds;
    static int first = 1;
    CONST84 char *tmp;
    Tcl_DString tds;
    
    if (!first) {
	Tcl_DStringFree(&ds);
    }
    tmp = Tcl_TranslateFileName(interp, name, &tds);
    if (!tmp) {
	return NULL;
    }
    Tcl_UtfToExternalDString(NULL, tmp, -1, &ds);
    Tcl_DStringFree(&tds);
    first = 0;
    return Tcl_DStringValue(&ds);
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
 *      Goes online or offline
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
    if (TCL_ERROR == RatDisOnOffTrans(interp, online)) {
	Tcl_ObjSetVar2(interp, part1, part2, Tcl_NewBooleanObj(0),
		       TCL_GLOBAL_ONLY);
	return TCL_ERROR;
    }
    Tcl_ObjSetVar2(interp, part1, part2, Tcl_NewBooleanObj(online),
		   TCL_GLOBAL_ONLY);
    if (online) {
	RatNudgeSender(interp);
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

    } else if (objc == 3
	       && !strcmp("encode_parameters", Tcl_GetString(objv[1]))) {
	/* RatTest encode_parameters {param list} */
	PARAMETER *p, *f;
	int i, plc, pc;
	Tcl_Obj **plv, **pv, *oPtr, *ov[2];

	Tcl_ListObjGetElements(interp, objv[2], &plc, &plv);
	for (i=0, p = f = NULL; i<plc; i++) {
	    Tcl_ListObjGetElements(interp, plv[i], &pc, &pv);
	    if (p) {
		p->next = mail_newbody_parameter();
		p = p->next;
	    } else {
		f = p = mail_newbody_parameter();
	    }
	    p->attribute = (char*)ucase(
                (unsigned char*)cpystr(Tcl_GetString(pv[0])));
	    p->value = cpystr(Tcl_GetString(pv[1]));
	}

	RatEncodeParameters(interp, f);

	oPtr = Tcl_NewObj();
	for (p=f; p; p = p->next) {
	    ov[0] = Tcl_NewStringObj(p->attribute, -1);
	    ov[1] = Tcl_NewStringObj(p->value, -1);
	    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewListObj(2, ov));
	}

	Tcl_SetObjResult(interp, oPtr);
	return TCL_OK;

    } else {
 	Tcl_AppendResult(interp, "Bad usage", TCL_STATIC);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatReadAndCanonify --
 *
 *	Read a file and canonalize the line endings, from system local
 *      to CRLF.
 *
 * Results:
 *      Returns a pointer to a memory area containing the data. It is
 *      the callers responsibility to eventually free this data with
 *      a call to ckfree().
 *
 * Side effects:
 *      Various
 *
 *
 *----------------------------------------------------------------------
 */
char*
RatReadAndCanonify(Tcl_Interp *interp, char *filename_utf,
		   unsigned long *size, int canonify)
{
    char *buf;
    CONST84 char *filename;
    struct stat sbuf;
    FILE *fp;

    Tcl_ResetResult(interp);
    filename = RatTranslateFileName(interp, filename_utf);
    fp = fopen(filename, "r");
    if (NULL == fp) {
	return NULL;
	/* XXX Handle error better */
    }
    fstat(fileno(fp), &sbuf);
    if (canonify) {
	int c, allocated, used;

	allocated = sbuf.st_size + sbuf.st_size/40;
	used = 0;
	buf = (char*)ckalloc(allocated+1);

	while (c=fgetc(fp), !feof(fp)) {
	    if (used >= allocated-1) {
		allocated += 1024;
		buf = (char*)ckrealloc(buf, allocated);
	    }
	    if ('\n' == c) {
		buf[used++] = '\r';
	    }
	    buf[used++] = c;
	}
	buf[used] = '\0';
	*size = used;
    } else {
	buf = (char*)ckalloc(sbuf.st_size+1);
	if (1 != fread(buf, sbuf.st_size, 1, fp)) {
            sbuf.st_size = 0;
        }
	buf[sbuf.st_size] = '\0';
	*size = sbuf.st_size;
    }
    fclose(fp);
    return buf;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCanonalize --
 *
 *	Canonalizes a give DString. That is it makes sure that every
 *      newline is expressed as CRLF.
 *
 * Results:
 *      Modifies the given DString.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */
void
RatCanonalize(Tcl_DString *ds)
{
    char *s, *c, *t = cpystr(Tcl_DStringValue(ds));

    Tcl_DStringSetLength(ds, 0);
    for (s=t; (c=strchr(s, '\n')); s=c+1) {
	Tcl_DStringAppend(ds, s, c-s);
	if ('\r' == c[-1]) {
	    Tcl_DStringAppend(ds, "\n", 1);
	} else {
	    Tcl_DStringAppend(ds, "\r\n", 2);
	}
    }
    Tcl_DStringAppend(ds, s, strlen(s));
    ckfree(t);
}

/*
 *----------------------------------------------------------------------
 *
 * RatNudgeSenderCmd --
 *
 *	Command used to nudge the sender
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatNudgeSenderCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		  Tcl_Obj *const objv[])
{
    RatNudgeSender(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatExtractAddressesCmd --
 *
 *	Se ../doc/interface
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatExtractAddressesCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		       Tcl_Obj *const objv[])
{
    Tcl_Obj *oPtr = Tcl_NewObj();
    ADDRESS *alist = NULL, *a;
    char *host, buf[1024];
    int i;

    if (objc < 2) {
 	Tcl_AppendResult(interp, "Bad usage", TCL_STATIC);
	return TCL_ERROR;
    }
    host = RatGetCurrent(interp, RAT_HOST, Tcl_GetString(objv[1]));
    for (i=2; i<objc; i++) {
	strlcpy(buf, Tcl_GetString(objv[i]), sizeof(buf));
	rfc822_parse_adrlist(&alist, buf, host);
	for (a = alist; a; a = a->next) {
	    buf[0] = '\0';
	    rfc822_address(buf, a);
	    Tcl_ListObjAppendElement(interp, oPtr, Tcl_NewStringObj(buf, -1));
	}
	mail_free_address(&alist);
    }
    Tcl_SetObjResult(interp, oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatGenerateDateCmd --
 *
 *	Se ../doc/interface
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatGenerateDateCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		   Tcl_Obj *const objv[])
{
    char buf[1024];

    rfc822_date(buf);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
    return TCL_OK;
}

/*
 * These are from auth_md5.c of c-client
 */
#define MD5BLKLEN 64		/* MD5 block length */
#define MD5DIGLEN 16		/* MD5 digest length */
typedef struct {
  unsigned long chigh;		/* high 32bits of byte count */
  unsigned long clow;		/* low 32bits of byte count */
  unsigned long state[4];	/* state (ABCD) */
  unsigned char buf[MD5BLKLEN];	/* input buffer */
  unsigned char *ptr;		/* buffer position */
} MD5CONTEXT;


/* Prototypes */
void md5_init (MD5CONTEXT *ctx);
void md5_update (MD5CONTEXT *ctx,unsigned char *data,unsigned long len);
void md5_final (unsigned char *digest,MD5CONTEXT *ctx);

/*
 *----------------------------------------------------------------------
 *
 * RatGenerateMsgIdCmd --
 *
 *	Se ../doc/interface
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatGenerateMsgIdCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		   Tcl_Obj *const objv[])
{
    char *domain, digest_hex[MD5DIGLEN*2+1], buf[1024];
    unsigned char digest[MD5DIGLEN];
    int i;
    MD5CONTEXT ctx;
    
    if (objc < 2) {
 	Tcl_AppendResult(interp, "Bad usage", TCL_STATIC);
	return TCL_ERROR;
    }

    domain = RatGetCurrent(interp, RAT_HOST, Tcl_GetString(objv[1]));

    snprintf(buf, sizeof(buf), "tkrat.%s.%ld.%d", domain, time(NULL),getpid());
    md5_init(&ctx);
    md5_update(&ctx, (unsigned char*)buf, strlen(buf));
    md5_final(digest, &ctx);
    for(i=0; i<MD5DIGLEN; i++) {
        snprintf(digest_hex+i*2, MD5DIGLEN+1-i*2, "%02x", digest[i]);
    }

    snprintf(buf, sizeof(buf), "<tkrat.%s@%s>", digest_hex, domain);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SafeRead --
 *
 *      Read a specified number of bytes from a socket. This function will
 *      continue until it has received an EOF, error or the requested
 *      number of bytes.
 *
 * Results:
 *      Returns the number of bytes actually read or a negative value
 *      on error.
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */
ssize_t
SafeRead(int fd, void *buf, size_t count)
{
    ssize_t got = 0;
    ssize_t l;

    while (got < count) {
        l = read(fd, buf+got, count-got);
        if (l < 0 && errno == EINTR) {
            continue;
        } else if (l <= 0) {
            return got;
        }
        got += l;
    }
    return got;
}


/*
 *----------------------------------------------------------------------
 *
 * GetPw --
 *
 *      Get the passwd struct for the current user. This function will
 *      never return NULL, but may exit if the call fails.
 *
 * Results:
 *      Returns a pointer to a passwd struct. The function will exit
 *      if the getpwuid call fails.
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */
struct passwd*
GetPw()
{
    struct passwd *pwPtr = getpwuid(getuid());

    if (!pwPtr) {
        fprintf(stderr, "You don't exist, go away!\n");
        exit(1);
    }
    return pwPtr;
}
