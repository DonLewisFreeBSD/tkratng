/* 
 * ratSender.c --
 *
 *	Handles sendig of messages
 *
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratStdFolder.h"
#include <signal.h>
#include <smtp.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Stuff used to handle sender comm
 */
static int sender_created = 0;
static int to_sender[2];
static FILE *to_sender_fh;
static int from_sender[2];
typedef enum {
    EVENT_NONE,
    EVENT_LOG,
    EVENT_SEND_OK,
    EVENT_SEND_FAIL
} send_event_t;

/*
 * SMTP connection cache
 */
typedef struct smtp_conn_cache {
    char *host;
    time_t expires;
    SENDSTREAM *stream;
    struct smtp_conn_cache *next;
} smtp_conn_cache_t;
static smtp_conn_cache_t *conn_cache;

/*
 * Data to pass to output function
 */
typedef struct {
    int fd;
    int errfd;
    char *errbuf;
    int errbufsize;
    int errbufused;
} soutr_data_t;

/*
 * SMTP authentication passwd
 */
char *smtp_passwd = NULL;

/*
 * Current verboseness level
 */
static char cverboseness;

/*
 * The sending process if any
 */
static pid_t sender_pid;
static int sender_died = 0;
static int sender_status;

/*
 * Local functions
 */
static void RatSenderFileHandler(ClientData clientData, int mask);
static void RatSenderHandler(Tcl_Interp *interp, int check_sender);
static void RatReadString(int fd, Tcl_DString *ds);
static void RatSender(void);
static void RatAlarmHandler(int sig);
static void RatSigChldHandler(int sig);
static void RatFillinBody(BODY *b, char *bt);
static char *RatSendSMTP(int in_fd, int out_fd, char *host,
			 ENVELOPE *env, BODY *b);
static void RatSendLog(int fd, const char *msg);
static char *RatSendProg(int in_fd, int out_fd, char *host,
			 ENVELOPE *env, BODY *b);
static void RatAddAddresses(Tcl_DString *ds, ADDRESS *addr);
static void RatSendProgChild(char *cmd, int to, int err);
static long RatSendSoutr(void *stream, char *string);
static void RatReadData(int fd, char **buf, int *size, int *used);
static void RatWrapHeaderLines(ENVELOPE *env, BODY *b);
static void RatWrapHeaderLine(char *type, char **s);

/*
 *----------------------------------------------------------------------
 *
 * RatNudgeSender --
 *
 *      Nudges the sender. This makes the sender rescan the outgoing
 *      mailbox and send any messages found there. If we are online that is.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
RatNudgeSender(Tcl_Interp *interp)
{
    pid_t pid;

    if (0 == sender_created) {
	if (pipe(to_sender)) return;
	if (pipe(from_sender)) {
            close(to_sender[0]);
            close(to_sender[1]);
            return;
        }
	if (0 == (pid = fork())) {
	    RatSender();
	    /* Notreached */
	    exit(1);
	}
	close(to_sender[0]);
	close(from_sender[1]);
	to_sender_fh = fdopen(to_sender[1], "w");
	Tcl_CreateFileHandler(from_sender[0], TCL_READABLE,
			      RatSenderFileHandler, (ClientData)interp);
	sender_created = 1;
    }
    RatSenderHandler(interp, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * RatSenderFileHandler --
 *
 *      Handles file events from the sender.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatSenderFileHandler(ClientData clientData, int mask)
{
    RatSenderHandler((Tcl_Interp*)clientData, mask & TCL_READABLE);
}
/*
 *----------------------------------------------------------------------
 *
 * RatWriteString --
 *
 *      Writes the given string to the outgoing file
 *
 * Results:
 *      non zero on failure
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
RatWriteString(const char *str, FILE *out)
{
    unsigned int l = strlen(str);
    if (1 != fwrite(&l, sizeof(int), 1, out)
        || 1 != fwrite(str, l, 1, out)) {
        return -1;
    } else {
        return 0;
    }
}
/*
 *----------------------------------------------------------------------
 *
 * RatSenderHandler --
 *
 *      The handler which handles the sending. The actual
 *      sending is handled by RatSender.
 *      The sender thread may generate the following events (written as
 *      bytes on the pipe):
 *      EVENT_LOG       - Log a message
 *      EVENT_SEND_OK   - Message sent OK
 *      EVENT_SEND_FAIL - Failed to send message
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatSenderHandler(Tcl_Interp *interp, int check_sender)
{
    static Tcl_DString *log_ds = NULL;
    static RatFolderInfo *infoPtr = NULL;
    static char *name;    
    static int verboseness = 0;
    static int is_sending = 0;
    static Tcl_DString *save_to_cmd = NULL;
    Tcl_CmdInfo cmdInfo; 
    MessageInfo *msgPtr ;
    unsigned char c;
    char buf[2048], role[16], *header, *body;
    CONST84 char *s, *user;
    Tcl_Obj *oPtr, **objv, *eobjv[4];
    int i, objc, n=0, validate;

    if (!log_ds) {
	log_ds = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(log_ds);
    }
    if (!save_to_cmd) {
	save_to_cmd = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(save_to_cmd);
    }

    /*
     * Handle possible file event
     */
    if (check_sender) {
	if (1 != SafeRead(from_sender[0], &c, 1)) {
	    RatLog(interp, RAT_ERROR, "Sender died!", RATLOG_NOWAIT);
	    Tcl_DeleteFileHandler(from_sender[0]);
	    sender_created = 0;
	    is_sending = 0;
	    c = EVENT_NONE;
	}
	switch ((send_event_t)c) {
	case EVENT_LOG:
	    if (sizeof(i) != SafeRead(from_sender[0], &i, sizeof(i))) {
                return;
            }
	    Tcl_DStringSetLength(log_ds, i);
	    if (i != SafeRead(from_sender[0], Tcl_DStringValue(log_ds), i)) {
                return;
            }
	    Tcl_GlobalEval(interp, Tcl_DStringValue(log_ds));
	    return;
	    /* NOTREACHED */
	    
	case EVENT_SEND_OK:
	    if (verboseness == 1) {
		strlcpy(buf, "RatLog 2 {} explicit",sizeof(buf));
		Tcl_GlobalEval(interp, buf);
	    } else if (verboseness > 1) {
		strlcpy(buf, "RatLog 2 $t(sent_ok)", sizeof(buf));
		Tcl_GlobalEval(interp, buf);
	    }
	    is_sending = 0;

	    /* Save copy of outgoing, if so requested */
	    if (Tcl_DStringLength(save_to_cmd)) {
                if (TCL_OK !=
                    Tcl_GlobalEval(interp, Tcl_DStringValue(save_to_cmd))) {
                    RatLogF(interp, RAT_ERROR, "failed_save_sent",
                            RATLOG_NOWAIT, Tcl_GetStringResult(interp));
                }
	    }
	    
	    /* Remove message from outfolder */
	    i = 0;
	    RatFolderCmdSetFlag(interp, infoPtr, &i, 1, RAT_DELETED, 1);
	    break;
	    
	case EVENT_SEND_FAIL:
	    if (verboseness == 1) {
		strlcpy(buf, "RatLog 2 {} explicit", sizeof(buf));
		Tcl_GlobalEval(interp, buf);
	    }
	    if (sizeof(i) != SafeRead(from_sender[0], &i, sizeof(i))) {
                return;
            }
	    Tcl_DStringSetLength(log_ds, i);
	    if (i != SafeRead(from_sender[0], Tcl_DStringValue(log_ds), i)) {
                return;
            }
	    Tcl_GlobalEval(interp, Tcl_DStringValue(log_ds));
	    eobjv[n++] = Tcl_NewStringObj("RatSendFailed", -1);
	    eobjv[n++] = Tcl_NewStringObj(name, -1);
	    eobjv[n++] = Tcl_NewStringObj(Tcl_DStringValue(log_ds),
					  Tcl_DStringLength(log_ds));
	    for (i=0; i<n; i++) {
		Tcl_IncrRefCount(eobjv[i]);
	    }
	    if (TCL_OK != Tcl_EvalObjv(interp, n, eobjv, TCL_GLOBAL_ONLY)) {
		fprintf(stderr, "%s:%d Internal failure: %s\n", __FILE__,
			__LINE__, Tcl_GetStringResult(interp));
	    }
	    for (i=0; i<n; i++) {
		Tcl_DecrRefCount(eobjv[i]);
	    }
	    is_sending = 0;

	    /* Remove message from outfolder */
	    i = 0;
	    RatFolderCmdSetFlag(interp, infoPtr, &i, 1, RAT_DELETED, 1);
	    break;

	case EVENT_NONE:
	    break;
	}
    }

    /*
     * Are we already sending?
     */
    if (is_sending) {
	return;
    }

    /*
     * Check the outgoing folder
     * - First check that we have it open
     *     Open it if not
     * - Check if we have any messages
     * - Extract the first message
     */
    if (NULL == infoPtr) {
	CONST84 char *index = Tcl_GetVar(interp, "vFolderOutgoing",
					 TCL_GLOBAL_ONLY);

	oPtr = Tcl_GetVar2Ex(interp, "vFolderDef", index, TCL_GLOBAL_ONLY);
	infoPtr = RatOpenFolder(interp, 0, oPtr);
	if (NULL == infoPtr) {
	    return;
	}
    }
    RatUpdateFolder(interp, infoPtr, RAT_SYNC);
    if (0 == infoPtr->number) {
	return;
    }
    is_sending = 1;
    if (verboseness == 1) {
	strlcpy(buf, "RatLog 2 $t(sending_message) explicit", sizeof(buf));
	Tcl_GlobalEval(interp, buf);
    }
    name = RatFolderCmdGet(interp, infoPtr, 0);
    Tcl_GetCommandInfo(interp, name, &cmdInfo);
    msgPtr = (MessageInfo*)cmdInfo.objClientData;

    /*
     * Get role
     */
    if ((s = Std_GetHeadersProc(interp, msgPtr))
	&& (s = strstr(s,"X-TkRat-Internal-Role"))) {
	for (s += 23; isspace(*s); s++);
	for (i=0; !isspace(s[i]) && *s && i<sizeof(role)-1; i++) {
	    role[i] = s[i];
	}
	role[i] = '\0';
    } else {
	s = Tcl_GetVar2(interp, "option", "default_role", TCL_GLOBAL_ONLY);
	strlcpy(role, s, sizeof(role));
    }

    /*
     * Get save_to
     */
    Tcl_DStringSetLength(save_to_cmd, 0);
    if ((s = Std_GetHeadersProc(interp, msgPtr))
	&& (s = strstr(s,"X-TkRat-Internal-Save-To"))) {
	char *e;
	int len;
	
	for (s += 26; isspace(*s); s++);
	e = strchr(s, '\r');
	len = e-s;

	if (sizeof(buf) <len+1) {
	    len = sizeof(buf)-1;
	}
	memcpy(buf, s, len);
	buf[len] = '\0';
        Tcl_DStringAppendElement(save_to_cmd, "RatSaveOutgoing");
	Tcl_DStringAppendElement(save_to_cmd, msgPtr->name);
	Tcl_DStringAppendElement(save_to_cmd, buf);
    }
    
    /*
     * Send data to sender, data passed is:
     *  char type  - 0 = SMTP, 1 = prog
     *  char verboseness
     *  uint host_length
     *  char host
     *  uint hdr_length
     *  char hdr
     *  uint body_length
     *  char body
     * For smtp
     *  uint nhosts
     *   uint host_length
     *   char host
     *  char passwd
     *  uint from_length
     *  char from
     *  int cache - -1 = no cache, 0 = cache indefinitely, >0 = cache-time
     * For prog
     *  uint cmd_length
     *  char cmd
     *  char ok_8bit
     */
    snprintf(buf, sizeof(buf), "%s,sendprot", role);
    s = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
    buf[0] = strcmp("smtp", s) ? 0 : 1;
    oPtr = Tcl_GetVar2Ex(interp, "option", "smtp_verbose", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &verboseness);
    buf[1] = verboseness;
    if (1 != fwrite(buf, 2, 1, to_sender_fh)) goto fail;
    s = RatGetCurrent(interp, RAT_HOST, role);
    if (RatWriteString(s, to_sender_fh)) goto fail;
    RatMessageGetContent(interp, msgPtr, &header, &body);
    if (RatWriteString(header, to_sender_fh)) goto fail;
    if (RatWriteString(body, to_sender_fh)) goto fail;
    if (buf[0]) {
	snprintf(buf, sizeof(buf), "%s,smtp_hosts", role);
	oPtr = Tcl_GetVar2Ex(interp, "option", buf, TCL_GLOBAL_ONLY);
	Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);
	if (1 != fwrite(&objc, sizeof(int), 1, to_sender_fh)) goto fail;
	snprintf(buf, sizeof(buf), "%s,validate_cert", role);
	oPtr = Tcl_GetVar2Ex(interp, "option", buf, TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &validate);
	snprintf(buf, sizeof(buf), "%s,smtp_user", role);
	user = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
	for (i = 0; i<objc; i++) {
	    strlcpy(buf, Tcl_GetString(objv[i]), sizeof(buf));
#ifdef HAVE_OPENSSL
	    if (validate) {
		strlcat(buf, "/validate-cert", sizeof(buf));
	    } else {
		strlcat(buf, "/novalidate-cert", sizeof(buf));
	    }
#endif /* HAVE_OPENSSL */
	    if (user[0]) {
		strlcat(buf, "/user=", sizeof(buf));
		strlcat(buf, user, sizeof(buf));
	    }
	    if (RatWriteString(buf, to_sender_fh)) goto fail;
	}
	snprintf(buf, sizeof(buf), "%s,smtp_passwd", role);
	s = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
	if (RatWriteString(s, to_sender_fh)) goto fail;
	snprintf(buf, sizeof(buf), "%s,from", role);
	s = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
	if (RatWriteString(s, to_sender_fh)) goto fail;
	oPtr = Tcl_GetVar2Ex(interp, "option", "cache_conn", TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &i);
	if (0 == i) {
	    i = -1;
	} else {
	    oPtr = Tcl_GetVar2Ex(interp, "option", "cache_conn_timeout",
				 TCL_GLOBAL_ONLY);
	    Tcl_GetIntFromObj(interp, oPtr, &i);
	}
	if (1 != fwrite(&i, sizeof(int), 1, to_sender_fh)) goto fail;
    } else {
	snprintf(buf, sizeof(buf), "%s,sendprog", role);
	s = Tcl_GetVar2(interp, "option", buf, TCL_GLOBAL_ONLY);
	if (RatWriteString(s, to_sender_fh)) goto fail;
	snprintf(buf, sizeof(buf), "%s,sendprog_8bit", role);
	oPtr = Tcl_GetVar2Ex(interp, "option", buf, TCL_GLOBAL_ONLY);
	Tcl_GetBooleanFromObj(interp, oPtr, &i);
	c = i;
	if (1 != fwrite(&c, 1, 1, to_sender_fh)) goto fail;
    }
    fflush(to_sender_fh);
    return;

fail:
    fclose(to_sender_fh);
    to_sender_fh = NULL;
    sender_created = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RatReadString --
 *
 *      Reads a string encoded with the length first from the given fd.
 *
 * Results:
 *      Stores the read string in the supplied DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatReadString(int fd, Tcl_DString *ds)
{
    unsigned int len;

    if (sizeof(int) != SafeRead(fd, &len, sizeof(int))) {
        exit(0); /* Master has died */
    }
    Tcl_DStringSetLength(ds, len);
    if (len != SafeRead(fd, Tcl_DStringValue(ds), len)) {
        exit(0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatSender --
 *
 *      The sender process
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatSender()
{
    Tcl_DString host, header, body;
    struct sigaction nact;
    char smtp, *errmsg, c, *s, buf[1024];
    smtp_conn_cache_t **cp, *ncp;
    int len, a, i;
    time_t now;
    ENVELOPE *env;
    BODY *b;
    STRING string;

    close(to_sender[1]);
    close(from_sender[0]);
    is_sender_child = 1;
    
    memset(&nact,0,sizeof (struct sigaction));
    sigemptyset(&nact.sa_mask);
    nact.sa_handler = RatAlarmHandler;
    sigaction(SIGALRM, &nact, NULL);
    nact.sa_handler = SIG_IGN;
    sigaction(SIGINT, &nact, NULL);
    
    Tcl_DStringInit(&host);
    Tcl_DStringInit(&header);
    Tcl_DStringInit(&body);
    while (1) {
	/*
	 * Read generic data from parent and handle cached connections
	 */
        now = time(NULL);
        for (cp = &conn_cache, a = 0; *cp;) {
            if ((*cp)->expires <= now) {
                smtp_close((*cp)->stream);
                ckfree((*cp)->host);
                ncp = (*cp)->next;
                ckfree(*cp);
                *cp = ncp;
            } else if (0 == a || a > (*cp)->expires) {
                a = (*cp)->expires;
                cp = &(*cp)->next;
            }
        }
        if (a) {
            alarm(a - time(NULL));
        }
        if (1 != SafeRead(to_sender[0], &smtp, 1)) {
            exit(0); /* Master has died */
        }
        alarm(0);
	if (1 != SafeRead(to_sender[0], &cverboseness, 1)) {
            exit(0); /* Master has died */
        }
	RatReadString(to_sender[0], &host);
	RatReadString(to_sender[0], &header);
	RatReadString(to_sender[0], &body);

	/*
	 * Parse message
	 */
	INIT(&string, mail_string, (void*)Tcl_DStringValue(&body),
	     Tcl_DStringLength(&body));
	rfc822_parse_msg(&env, &b, Tcl_DStringValue(&header),
			 Tcl_DStringLength(&header), &string,
			 Tcl_DStringValue(&host), 0);
	RatFillinBody(b, Tcl_DStringValue(&body));
	if (NULL != (s = strstr(Tcl_DStringValue(&header),
				"X-TkRat-Internal-Bcc"))
            || NULL != (s = strstr(Tcl_DStringValue(&header),
                                   "X-TkRat-Original-bcc"))) {
	    for (s += 22; isspace(*s); s++);
	    for (i=0; '\r' != s[i] && *s && i<sizeof(buf)-1; i++) {
		buf[i] = s[i];
	    }
	    buf[i] = '\0';
	    rfc822_parse_adrlist(&env->bcc, buf, Tcl_DStringValue(&host));
	}
	RatWrapHeaderLines(env, b);

	/*
	 * Call the appropriate sender routine
	 */
	if (smtp) {
	    errmsg = RatSendSMTP(to_sender[0], from_sender[1],
				 Tcl_DStringValue(&host), env, b);
	} else {
	    errmsg = RatSendProg(to_sender[0], from_sender[1],
				 Tcl_DStringValue(&host), env, b);
	}

	/*
	 * Report to master
	 */
	if (errmsg) {
	    c = EVENT_SEND_FAIL;
	    len = strlen(errmsg);
	    if (0 > safe_write(from_sender[1], &c, 1)
                || 0 > safe_write(from_sender[1], &len, 4)
                || 0 > safe_write(from_sender[1], errmsg, len)) {
                exit(1);
            }
	} else {
	    c = EVENT_SEND_OK;
	    if (0 > safe_write(from_sender[1], &c, 1)) {
                exit(1);
            }
	}

	/*
	 * Housekeeping
	 */
	mail_free_envelope(&env);
	mail_free_body(&b);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatAlarmHandler --
 *
 *      Signal hander which handles the ALRM signal. This function does
 *      nothing in itself but the existence makes system-calls be
 *      interrupted.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
RatAlarmHandler(int sig)
{
    /* Do nothing */
}

/*
 *----------------------------------------------------------------------
 *
 * RatSigChldHandler --
 *
 *      Signal hander which handles the CHLD signal. Thsi sets a flag
 *      that the sending child has dies (if that pid died).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
RatSigChldHandler(int sig)
{
    pid_t pid;
    int status;

    while (0 < (pid = waitpid(-1, &status, WNOHANG))) {
        if (pid == sender_pid) {
            sender_died = 1;
            sender_status = status;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatFillinBody --
 *
 *      Fills in the body->contents.text.data pointers
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Modifies the body-structure.
 *
 *----------------------------------------------------------------------
 */
static void
RatFillinBody(BODY *b, char *bt)
{
    PART *part;
    
    if (TYPEMULTIPART == b->type) {
	for (part = b->nested.part; part; part = part->next) {
	    RatFillinBody(&part->body, bt);
	}
    } else {
	b->contents.text.data =
            (unsigned char*)ckalloc(b->contents.text.size+1);
	b->contents.text.data[b->contents.text.size] = '\0';
	memcpy(b->contents.text.data, bt+b->contents.offset,
	       b->contents.text.size);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendSMTP --
 *
 *      Sends a message via SMTP
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static char*
RatSendSMTP(int in_fd, int out_fd, char *host, ENVELOPE *env, BODY *b)
{
    static char *err = NULL;
    smtp_conn_cache_t **cp, *ncp;
    SENDSTREAM *stream = NULL;
    long debug;
    char buf[1024], **smtp_hosts;
    unsigned int nhosts, len, i;
    Tcl_DString from, passwd;
    int cache;

    if (err) {
        ckfree(err);
        err = NULL;
    }
    
    /* Read data from master */
    if (sizeof(nhosts) != SafeRead(in_fd, &nhosts, sizeof(nhosts))) {
        exit(0); /* Master has died */
    }
    smtp_hosts = (char**)ckalloc((nhosts+1)*sizeof(char*));
    for (i=0; i<nhosts; i++) {
	if (sizeof(len) != SafeRead(in_fd, &len, sizeof(len))) {
            exit(0); /* Master has died */
        }
	smtp_hosts[i] = (char*)ckalloc(len+1);
	smtp_hosts[i][len] = '\0';
	if (len != SafeRead(in_fd, smtp_hosts[i], len)) {
            exit(0); /* Master has died */
        }
    }
    smtp_hosts[i] = NULL;
    Tcl_DStringInit(&passwd);
    RatReadString(in_fd, &passwd);
    smtp_passwd = Tcl_DStringValue(&passwd);
    Tcl_DStringInit(&from);
    RatReadString(in_fd, &from);
    rfc822_parse_adrlist(&env->return_path, Tcl_DStringValue(&from), host);
    if (sizeof(int) != SafeRead(in_fd, &cache, sizeof(int))) {
        exit(0); /* Master has died */
    }

    /* Get cached */
    for (cp = &conn_cache; *cp; cp = &(*cp)->next) {
	if (!strcmp((*cp)->host, smtp_hosts[0])) {
	    stream = (*cp)->stream;
	    ckfree((*cp)->host);
	    ncp = (*cp)->next;
	    ckfree(*cp);
	    *cp = ncp;

	    if (250 != smtp_send(stream,"RSET",NIL)) {
		smtp_close(stream);
		stream = NULL;
	    }
	    break;
	}
    }

    /* Open new stream if needed */
    if (!stream) {
	if (cverboseness > 1) {
	    strlcpy(buf,"RatLog 2 $t(opening_smtp_conn) explicit",sizeof(buf));
	    RatSendLog(out_fd, buf);
	}
	if (cverboseness >= 3) {
	    debug = 1;
	} else {
	    debug = 0;
	}
	stream = smtp_open(smtp_hosts, debug);
	if (NULL == stream) {
	    return "";
	}
    }

    /* Send message */
    if (cverboseness > 1) {
	strlcpy(buf, "RatLog 2 $t(sending_message) explicit", sizeof(buf));
	RatSendLog(out_fd, buf);
    }
    if (T != smtp_mail(stream, "MAIL", env, b)) {
        err = cpystr(stream->reply);
        /* Do not cache connection if we got an error */
        cache = -1;
    }

    /* Add to cache or close */
    if (cache >= 0) {
	ncp = (smtp_conn_cache_t*)ckalloc(sizeof(*ncp));
	ncp->host = cpystr(smtp_hosts[0]);
	if (cache) {
	    ncp->expires = time(NULL)+cache;
	} else {
	    /* We define infinity as a year :-) */
	    ncp->expires = time(NULL)+256*24*60*60;
	}
	ncp->stream = stream;
	ncp->next = conn_cache;
	conn_cache = ncp;
    } else {
	smtp_close(stream);
    }

    if (cverboseness > 0) {
	strlcpy(buf, "RatLog 2 {} explicit", sizeof(buf));
	RatSendLog(out_fd, buf);
    }

    /* Free stuff */
    for (i=0; smtp_hosts[i]; i++) {
	ckfree(smtp_hosts[i]);
    }
    ckfree(smtp_hosts);
    Tcl_DStringFree(&from);
    memset(smtp_passwd, '\0', strlen(smtp_passwd));
    Tcl_DStringFree(&passwd);
    
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendLog --
 *
 *      Sends a log message from the sender thread
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatSendLog(int fd, const char *msg)
{
    char c = EVENT_LOG;
    unsigned int len = strlen(msg);
    int ignored;

    ignored = safe_write(fd, &c, 1);
    ignored = safe_write(fd, &len, sizeof(int));
    ignored = safe_write(fd, msg, len);
}

/*
 *----------------------------------------------------------------------
 *
 * RatSendProg --
 *
 *      Sends a message via an external program
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static char*
RatSendProg(int in_fd, int out_fd, char *host, ENVELOPE *env, BODY *b)
{
    static Tcl_DString *cmd = NULL;
    static char *buf = NULL;
    static int bufsize = 0;
    static soutr_data_t sd = {0, 0, NULL, 0, 0};
    struct sigaction nact;
    int to[2], err[2], len, r;
    pid_t child;
    char ok_8bit;
    long arg;

    /*
     * First create the command
     */
    if (!cmd) {
	cmd = (Tcl_DString*)ckalloc(sizeof(*cmd));
	Tcl_DStringInit(cmd);
    }
    if (bufsize < 1024) {
	bufsize = 1024;
	buf = (char*)ckalloc(bufsize);
    }
    if (cverboseness > 1) {
	strlcpy(buf, "RatLog 2 $t(launching_send_cmd) explicit", bufsize);
	RatSendLog(out_fd, buf);
    }
    RatReadString(in_fd, cmd);
    if (1 != SafeRead(in_fd, &ok_8bit, 1)) {
        exit(0); /* Master has died */
    }
    RatAddAddresses(cmd, env->to);
    RatAddAddresses(cmd, env->cc);
    RatAddAddresses(cmd, env->bcc);

    /*
     * Execute send command
     */
    if (pipe(to) || pipe(err)) {
	snprintf(buf, bufsize, "$t(prog_send_failed): %s",strerror(errno));
	return buf;
    }
    if (0 == (child = fork())) {
	/* Child */
	RatSendProgChild(Tcl_DStringValue(cmd), to[0], err[1]);
	/* Notreached */
	exit(1);
    }
    if (-1 == child) {
	snprintf(buf, bufsize, "$t(prog_send_failed): %s",strerror(errno));
	return buf;
    }
    close(to[0]);
    close(err[1]);
    sender_pid = child;
    sender_died = 0;
    memset(&nact,0,sizeof (struct sigaction));
    sigemptyset(&nact.sa_mask);
    nact.sa_handler = RatSigChldHandler;
    sigaction(SIGCHLD, &nact, NULL);


    arg = O_NONBLOCK;
    fcntl(err[0], F_SETFL, arg);
    
    if (cverboseness > 1) {
	strlcpy(buf, "RatLog 2 $t(writing_message) explicit", bufsize);
	RatSendLog(out_fd, buf);
    }

    /*
     * Write message
     */
    len = RatHeaderSize(env, b);
    if (len > bufsize) {
	bufsize = len+1024;
	buf = (char*)ckrealloc(buf, bufsize);
    }
    sd.fd = to[1];
    sd.errfd = err[0];
    sd.errbufused = 0;
    rfc822_output(buf, env, b, RatSendSoutr, (void*)&sd, ok_8bit);
    close(sd.fd);
    
    if (cverboseness > 1) {
	strlcpy(buf, "RatLog 2 $t(waiting_on_send_cmd) explicit", bufsize);
	RatSendLog(out_fd, buf);
    }

    /*
     * Wait for command to complete
     */
    nact.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &nact, NULL);
    while (!sender_died && 0 == (r = waitpid(child, &sender_status,WNOHANG))) {
	RatReadData(sd.errfd, &sd.errbuf, &sd.errbufsize, &sd.errbufused);
	usleep(100000); /* 0.1 second */
    }
    RatReadData(sd.errfd, &sd.errbuf, &sd.errbufsize, &sd.errbufused);

    /*
     * Check status
     */
    if (child == r) {
	if (!WIFEXITED(sender_status) || WEXITSTATUS(sender_status)) {
	    if (!sd.errbufused) {
		snprintf(sd.errbuf, sd.errbufsize,
			 "Child terminated with code %d\n",
			 WEXITSTATUS(sender_status));
		sd.errbufused = strlen(sd.errbuf);
	    }
	} else {
	    sd.errbufused = 0;
	}
    }
    
    close(to[1]);
    close(err[0]);

    /*
     * Report
     */
    if (sd.errbufused) {
	sd.errbuf[sd.errbufused] = '\0';
	return sd.errbuf;
    } else {
	return NULL;
    }
}
/*
 *----------------------------------------------------------------------
 *
 * RatAddAddresses --
 *
 *      Adds addresses to a buffer. This function reallocates the buffer
 *      if needed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatAddAddresses(Tcl_DString *ds, ADDRESS *addr)
{
    char buf[1024];
    ADDRESS *a;
    int r;

    for (a = addr; a; a = a->next) {
	if (!a->mailbox) {
	    continue;
	}
	r = strlen(a->mailbox)*2+1;
	if (a->host) {
	    r += strlen(a->host) + 1;
	}
	if (r >= sizeof(buf)) {
	    /* Ridiculosuly long address */
	    continue;
	}
	buf[0] = '\0';
	rfc822_address(buf, a);
	Tcl_DStringAppend(ds, " ", 1);
	Tcl_DStringAppend(ds, buf, -1);
    }
}
/*
 *----------------------------------------------------------------------
 *
 * RatSendProgChild --
 *
 *      Child process responsible for executing the sender prog
 *      if needed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Will modify 'cmd'.
 *
 *----------------------------------------------------------------------
 */
static void
RatSendProgChild(char *cmd, int to, int err)
{
    struct rlimit rlim;
    int i, oi, fd, n;
    char **argv, buf[1024];

    /*
     * Fix file descriptors
     */
    getrlimit(RLIMIT_NOFILE, &rlim);	
    for (i=0; i<rlim.rlim_cur; i++) {
	if (i != to && i != err) {
	    close(i);
	}
    }
    dup2(to, 0);
    fd = open("/dev/null", O_WRONLY);
    if (fd != 1) {
	dup2(fd, 1);
    }
    dup2(err, 2);
    fcntl(0, F_SETFD, 0);
    fcntl(1, F_SETFD, 0);
    fcntl(2, F_SETFD, 0);

    /*
     * Split cmd
     */
    for (i=0,n=2; cmd[i]; i++) {
	if (isspace(cmd[i])) {
	    n++;
	}
    }
    argv = (char**)ckalloc(n*sizeof(char*));
    for (i=0; cmd[i] && isspace(cmd[i]); i++)
	;
    argv[0] = cmd+i;
    for (n=1; cmd[i]; i++) {
	if (isspace(cmd[i])) {
	    oi = i;
	    while(cmd[i] && isspace(cmd[i]))
		i++;
	    cmd[oi] = '\0';
	    argv[n++] = cmd+i;
	}
    }
    argv[n] = NULL;

    /*
     * Do exec
     */
    execv(argv[0], argv);

    snprintf(buf, sizeof(buf), "Failed to exec '%s': %s\n", argv[0],
	     strerror(errno));
    i = safe_write(err, buf, strlen(buf));
    exit(1);
}
/*
 *----------------------------------------------------------------------
 *
 * RatSendSoutr --
 *
 *      Output function used by c-client library. This function will
 *      write data to the given fd, read data from the given error fd and
 *      give up writing if the child has died before we are done.
 *      if needed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static long
RatSendSoutr(void *stream, char *string)
{
    soutr_data_t *sd = (soutr_data_t*)stream;
    char *data = string;
    int r, len = strlen(string);

    /*
     * This is kind of complicated. We write data to the child, but
     * the write may come up short for various reasons:
     * - The child is currently busy and the queue is full
     * - The child is waiting to write to stderr
     * - The child has died
     *
     */
    while (len != (r = safe_write(sd->fd, data, len))) {
	if (0 > r || sender_died) {
	    /* If failed the child must be dead */
	    return NIL;
	}

	/*
	 * Read error output
	 */
	RatReadData(sd->errfd, &sd->errbuf, &sd->errbufsize, &sd->errbufused);

	usleep(100000); /* 0.1 second */
	data += r;
	len -= r;
    }
    if (sender_died) {
        return NIL;
    }
    return T;
}
/*
 *----------------------------------------------------------------------
 *
 * RatReadData --
 *
 *      Reads all available data from the given descriptor and stores
 *      it in the given buffer (which may be reallocated).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RatReadData(int fd, char **buf, int *size, int *used)
{
    int len;
    
    do {
	if (*used+1024 >= *size) {
	    *size += 1024;
	    *buf = (char*)ckrealloc(*buf, *size);
	    (*buf)[*used] = '\0';
	}
	len = SafeRead(fd, *buf + *used, *size - *used);
	if (len <= 0) {
	    return;
	}
	*used += len;
    } while(len > 0);
}
/*
 *----------------------------------------------------------------------
 *
 * RatSenderLog --
 *
 *      Used by RatLog to send a log message when running in the sender
 *      thread.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Sends the log message to the master
 *
 *----------------------------------------------------------------------
 */
void
RatSenderLog(const char *logcmd)
{
    RatSendLog(from_sender[1], logcmd);
}

/*
 *----------------------------------------------------------------------
 *
 * RatWrapHeaderLines --
 *
 *      Wraps all header-liens which are too long to fit on a line.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Modifies to provided envelope and body
 *
 *----------------------------------------------------------------------
 */

static void
RatWrapHeaderLines(ENVELOPE *env, BODY *body)
{
    RatWrapHeaderLine("Newsgroups", &env->newsgroups);
    RatWrapHeaderLine("Subject", &env->subject);
    RatWrapHeaderLine("In-Reply-To", &env->in_reply_to);
    RatWrapHeaderLine("Followup-to", &env->followup_to);
    RatWrapHeaderLine("References", &env->references);
    RatWrapHeaderLine("Content-Description: %s", &body->description);
}
/*
 *----------------------------------------------------------------------
 *
 * RatWrapHeaderLine --
 *
 *      Wraps a header-line if needed. Assumes that the input is a
 *      single line without newlines etc.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Modifies to provided pointer
 *
 *----------------------------------------------------------------------
 */

static void
RatWrapHeaderLine(char *type, char **s)
{
    int l, extra = 0, offset, ls;

    /*
     * Do we need to wrap this entry?
     */
    l = strlen(type) + 2;
    if (!*s || l + strlen(*s) <= 78) {
	return;
    }

    /*
     * Wrap line
     */
    ls = 0;
    while (strlen(*s+ls)+l > 78) {
	offset = 78-l;
	while (offset > 0 && !isspace((*s)[ls+offset])) offset--;
	if (0 == offset) {
	    offset = 78-l;
	    while ((*s)[ls+offset] && !isspace((*s)[ls+offset])) offset++;
	    if (!(*s)[ls+offset]) {
		/*
		 * We have reached the end of the line
		 */
		return;
	    }
	}
	l = 1; /* Header length of continuation-lines is 1 */

	/*
	 * Make more room if needed
	 */
	if (extra < 2) {
	    extra += 10;
	    *s = ckrealloc(*s, strlen(*s)+extra+1);
	}

	/*
	 * Move remaining text and insert line-break
	 */
	memmove(*s+ls+offset+2, *s+ls+offset, strlen(*s+ls+offset)+1);
	memcpy(*s+ls+offset, "\015\012", 2);
	extra -= 2;
	ls += offset+2;
    }
}


void mm_smtptrace(long smtpstate, char *string) {
    char buf[1024];
    const char *key, *local;

    if (cverboseness < 2) {
        return;
    }
    
    switch (smtpstate) {
    case SMTPSTATE_MAIL_FROM:
        key = "sending_mail_from";
        break;
    case SMTPSTATE_RCPT_TO:
        key = "sending_rcpt";
        break;
    case SMTPSTATE_SENDING_DATA:
        key = "sending_data";
        break;
    default:
        key = NULL; /* To keep the compiler happy */
        break;
    }
    local = Tcl_GetVar2(timerInterp, "t", (CONST84 char*)key, TCL_GLOBAL_ONLY);
    snprintf(buf, sizeof(buf), local, string);
    RatLog(timerInterp, RAT_INFO, buf, RATLOG_EXPLICIT);
}
