/*
 * ratPGPprog.c --
 *
 *	This file contains compatibility functions.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"
#include "ratPGP.h"

#include <stdio.h>

/*
 * The contents of the first bodypart of nested bodyparts
 */
#define ENCFIRST "Version: 1\r\n"

/*
 * List of keys on keyring
 */
typedef struct {
    Tcl_Obj *keyid;
    Tcl_Obj *addresses;
    Tcl_Obj *subjects;
    Tcl_Obj *descr;
    Tcl_Obj *sign;
    Tcl_Obj *encrypt;
} RatPGPKey;  
typedef struct {
    RatPGPKey *keys;
    unsigned int keyCount;
    unsigned int keyAlloc;
    Tcl_Obj *title;
    char *name;
    time_t mtime;
    int secring;
} RatPGPKeyring;
static RatPGPKeyring *keyring = NULL;

/*
 * Maximum number of accepted fields in pgp key list output. Extra
 * fields will be ignored.
 */
#define MAX_FIELDS 20

/*
 * Local functions
 */
static int RatRunPGP(Tcl_Interp *interp, int nopass, char *cmd, char *args,
		     int *toPGP, char **outFile, int *errPGP);
static Tcl_DString *RatPGPRunOld(Tcl_Interp *interp, BodyInfo *bodyInfoPtr,
				 char *text, char *start, char *end);
static int RatUpdatePGPKeys(Tcl_Interp *interp, RatPGPKeyring *k);
static void ParsePGPListFormat(Tcl_Interp *interp, FILE *fp, Tcl_RegExp exp_id,
			       Tcl_RegExp axp_addr, int blankiscont,
			       RatPGPKeyring *k);
static void ParseGPGListFormat(Tcl_Interp *interp, FILE *fp, RatPGPKeyring *k);
static void AddKey(Tcl_Interp *interp, RatPGPKeyring *k, Tcl_Obj *ids,
		   Tcl_DString *descr);
static void AddKeyNew(Tcl_Interp *interp, RatPGPKeyring *k, Tcl_Obj *id,
		      Tcl_Obj *addresses, Tcl_Obj *subjects, Tcl_Obj *descr,
		      Tcl_Obj *sign, Tcl_Obj *encrypt);
static void RatPGPFreeKeyring(RatPGPKeyring *k);
static RatPGPKeyring* RatPGPNewKeyring(Tcl_Interp *interp, const char *name,
				       int secring);

/*
 *----------------------------------------------------------------------
 *
 * RatRunPGP --
 *
 *      Run the pgp command
 *
 * Results:
 *	Returns the pid of the pgp program on success and a negative
 *	value on failure. The toPGP and fromPGP integers will be modified.
 *
 * Side effects:
 *	forks.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatRunPGP(Tcl_Interp *interp, int nopass, char *command, char *args,
	int *toPGP, char **outFile, int *errPGP)
{
    int toPipe[2], errPipe[2], argc, pid, i, out;
    Tcl_DString cmd;
    char cmdbuf[1024];
    CONST84 char *opt_args, *pgp_path, **argv, *tmp;
    struct rlimit rlim;
    static char name[1024];

    /*
     * Setup command arrays
     */
    pgp_path = RatGetPathOption(interp, "pgp_path");
    opt_args = Tcl_GetVar2(interp, "option", "pgp_args", TCL_GLOBAL_ONLY);
    if (pgp_path && strlen(pgp_path)) {
	snprintf(cmdbuf, sizeof(cmdbuf), "%s/%s", pgp_path, command);
    } else {
	snprintf(cmdbuf, sizeof(cmdbuf), "%s", command);
    }
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, cmdbuf, -1);
    if (opt_args) {
	Tcl_DStringAppend(&cmd, " ", 1);
	Tcl_DStringAppend(&cmd, opt_args, -1);
    }
    Tcl_DStringAppend(&cmd, " ", 1);
    Tcl_DStringAppend(&cmd, args, -1);
    Tcl_SplitList(interp, Tcl_DStringValue(&cmd), &argc, &argv);
    /*fprintf(stderr, "Exec: %s %s\n", cmdbuf, Tcl_DStringValue(&cmd));*/
    Tcl_DStringFree(&cmd);

    /*
     * Open outfile and create the pgp subprocess.
     */
    tmp = Tcl_GetVar(interp, "rat_tmp", TCL_GLOBAL_ONLY);
    tmp = RatTranslateFileName(interp, tmp);
    snprintf(name, sizeof(name), "%s/pgptmp.%d", tmp, getpid());
    if ( 0 > (out = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0600))) {
	return 0;
    }
    pipe(toPipe);
    pipe(errPipe);
    if (0 == (pid = fork())) {
	getrlimit(RLIMIT_NOFILE, &rlim);
	for (i=0; i<rlim.rlim_cur; i++) {
	    if (i != toPipe[0] && i != out && i != errPipe[1]) {
		close(i);
	    }
	}
	dup2(toPipe[0], 0);
	dup2(out, 1);
	dup2(errPipe[1], 2);
	fcntl(0, F_SETFD, 0);
	fcntl(1, F_SETFD, 0);
	fcntl(2, F_SETFD, 0);
	if (!nopass) {
	    putenv("PGPPASSFD=0");
	}
	execvp(cmdbuf, (char**)argv);
	{
	    char buf[1024];
	    snprintf(buf, sizeof(buf), "ERROR executing '%s %s': %s\n",
		    cmdbuf, args, strerror(errno));
	    write(STDERR_FILENO, buf, strlen(buf));
	}
	exit(-1);
	/* notreached */
    }
    close(toPipe[0]);
    close(out);
    close(errPipe[1]);
    ckfree(argv);
    *toPGP = toPipe[1];
    *outFile = name;
    *errPGP = errPipe[0];
    return pid;
}

/*
 *----------------------------------------------------------------------
 *
 * RatPGPEncrypt --
 *
 *      Encrypt a bodypart. Optionally also sign it.
 *
 * Results:
 *	A multipart/encrypted body
 *
 * Side effects:
 *	Will call pgp.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatPGPEncrypt(Tcl_Interp *interp, ENVELOPE *env, BODY **body,
	      char *signer, Tcl_Obj *rcpts)
{
    int toPGP, fromPGP, errPGP, length, all_ok, pid, result, status, i, j,
	objc;
    char *hdrPtr, *from, buf[MAILTMPLEN], *command;
    volatile char passPhrase[MAXPASSLENGTH];
    CONST84 char *version;
    Tcl_DString cmdDS, encDS;
    BODY *multiPtr;
    PARAMETER *parmPtr;
    PART *partPtr;
    char *recipSep;
    Tcl_Obj **objv;

    Tcl_DStringInit(&cmdDS);
    Tcl_DStringInit(&encDS);

    rfc822_encode_body_8bit(env, *body);
    /*
     * Create command to run
     */
    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);
    if (!strcmp("gpg-1", version)) {
	command = "gpg";
	Tcl_DStringAppend(&cmdDS,
		"-eatq --no-secmem-warning --passphrase-fd 0 --batch", -1);
	if (signer) {
	    Tcl_DStringAppend(&cmdDS, " -s ", -1);
	}
	recipSep = " -r ";
    } else if (!strcmp("2", version)) {
	command = "pgp";
	Tcl_DStringAppend(&cmdDS, "+BATCHMODE +VERBOSE=0 -eaf", -1);
	if (signer) {
	    Tcl_DStringAppend(&cmdDS, "s", 1);
	}
	recipSep=" ";
    } else if (!strcmp("5", version)) {
	command = "pgpe";
	if (signer) {
	    Tcl_DStringAppend(&cmdDS, "-s ", -1);
	}
	Tcl_DStringAppend(&cmdDS, "-at -f +batchmode=1 -r", -1);
	recipSep=" ";
    } else if (!strcmp("6", version)) {
	command = "pgp";
	Tcl_DStringAppend(&cmdDS, "+BATCHMODE +VERBOSE=0 +force -eaf", -1);
	if (signer) {
	    Tcl_DStringAppend(&cmdDS, "s", 1);
	}
	recipSep=" ";
    } else {
	Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	return TCL_ERROR;
    }
    if (signer) {
	Tcl_DStringAppend(&cmdDS, " -u ", 4);
	Tcl_DStringAppendElement(&cmdDS, signer);
    }
    Tcl_ListObjGetElements(interp, rcpts, &objc, &objv);
    for (i=0; i<objc; i++) {
	Tcl_DStringAppend(&cmdDS, recipSep, -1);
	Tcl_DStringAppend(&cmdDS, Tcl_GetString(objv[i]), -1);
    }

    /*
     * Run command
     */
    do {
	if (signer) {
	    if (NULL == RatPGPPhrase(interp, passPhrase, sizeof(passPhrase))) {
		Tcl_DStringFree(&encDS);
		Tcl_DStringFree(&cmdDS);
		return TCL_ERROR;
	    }
	}
	pid = RatRunPGP(interp, 0, command, Tcl_DStringValue(&cmdDS), &toPGP,
			&from, &errPGP);
	if (signer) {
	    write(toPGP, (char*)passPhrase, strlen((char*)passPhrase));
	    for (i=0; i<strlen((char*)passPhrase); i++) passPhrase[i] = '\0';
	}
        if (strcmp("6", version) || signer) {
	    write(toPGP, "\n", 1);
	}
	hdrPtr = buf;
	buf[0] = '\0';
	rfc822_write_body_header(&hdrPtr, *body);
	strlcat(buf, "\015\012", sizeof(buf));
	write(toPGP, buf, strlen(buf));
	RatInitDelayBuffer();
	rfc822_output_body(*body, RatDelaySoutr, (void*)toPGP);
	close(toPGP);
	do {
	    result = waitpid(pid, &status, 0);
	} while(-1 == result && EINTR == errno);

	/*
	 * Read result
	 */
	fromPGP = open(from, O_RDONLY);
	Tcl_DStringSetLength(&encDS, 0);
	do {
	    length = read(fromPGP, buf, sizeof(buf));
	    for (i=0; i < length; i += j) {
		for (j=0; buf[i+j] != '\n' && i+j<length; j++);
		Tcl_DStringAppend(&encDS, buf+i, j);
		if ('\n' == buf[i+j]) {
		    Tcl_DStringAppend(&encDS, "\r\n", 2);
		    j++;
		}
	    }
	} while (length > 0);
	close(fromPGP);
	unlink(from);

	/*
	 * Check for errors
	 */
	if (pid != result || WEXITSTATUS(status)) {
	    Tcl_DStringSetLength(&cmdDS, 0);
	    Tcl_DStringAppendElement(&cmdDS, "RatPGPError");
	    Tcl_DStringStartSublist(&cmdDS);
	    do {
		if (0 < (length = read(errPGP, buf, sizeof(buf)))) {
		    Tcl_DStringAppend(&cmdDS, buf, length);
		}
	    } while (length > 0);
	    Tcl_DStringEndSublist(&cmdDS);
	    Tcl_GlobalEval(interp, Tcl_DStringValue(&cmdDS));
	    if (!strncmp("ABORT", Tcl_GetStringResult(interp), 5)) {
		close(errPGP);
		Tcl_DStringFree(&encDS);
		return TCL_ERROR;
	    }
	    all_ok = 0;
	} else {
	    all_ok = 1;
	}
	close(errPGP);
    } while(0 == all_ok);
    Tcl_DStringFree(&cmdDS);
    mail_free_body(body);

    /*
     * Build encrypted multipart
     */
    multiPtr = mail_newbody();
    multiPtr->type = TYPEMULTIPART;
    multiPtr->subtype = cpystr("encrypted");
    multiPtr->parameter = parmPtr = mail_newbody_parameter();
    parmPtr->attribute = cpystr("protocol");
    parmPtr->value = cpystr("application/pgp-encrypted");
    parmPtr->next = mail_newbody_parameter();
    parmPtr = parmPtr->next;
    parmPtr->attribute = cpystr("BOUNDARY");
    snprintf(buf, sizeof(buf), "%ld-%ld-%ld=:%ld",(long)gethostid(),random(),
	     time(NULL), (long)getpid());
    parmPtr->value = cpystr(buf);
    parmPtr->next = NULL;
    multiPtr->encoding = ENC7BIT;
    multiPtr->id = NULL;
    multiPtr->description = NULL;
    multiPtr->nested.part = partPtr = mail_newbody_part();
    partPtr->body.type = TYPEAPPLICATION;
    partPtr->body.subtype = cpystr("pgp-encrypted");
    partPtr->body.encoding = ENC7BIT;
    partPtr->body.contents.text.data = (unsigned char*)cpystr(ENCFIRST);
    partPtr->body.size.bytes = strlen(ENCFIRST);
    partPtr->next = mail_newbody_part();
    partPtr = partPtr->next;
    partPtr->body.type = TYPEAPPLICATION;
    partPtr->body.subtype = cpystr("octet-stream");
    partPtr->body.encoding = ENC7BIT;
    partPtr->body.contents.text.data =
	    (unsigned char*)cpystr(Tcl_DStringValue(&encDS));
    partPtr->body.size.bytes = Tcl_DStringLength(&encDS);
    Tcl_DStringFree(&encDS);
    partPtr->next = NULL;

    *body = multiPtr;

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPSign --
 *
 *      Sign a bodypart.
 *
 * Results:
 *	A multipart/signed body
 *
 * Side effects:
 *	Will call pgp.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatPGPSign(Tcl_Interp *interp, ENVELOPE *env, BODY **body, const char *signer)
{
    int toPGP, fromPGP, errPGP, length, all_ok, pid, status, result, i, j;
    char *hdrPtr, *outfile, *cmd, buf[MAILTMPLEN];
    volatile char passPhrase[MAXPASSLENGTH];
    CONST84 char *version;
    Tcl_DString sigDS, cmdDS;
    BODY *multiPtr;
    PARAMETER *parmPtr;
    PART *partPtr;

    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);

    Tcl_DStringInit(&sigDS);
    Tcl_DStringInit(&cmdDS);
    do {
	/*
	 * Run command
	 */
	rfc822_encode_body_7bit(NIL, *body);
	Tcl_DStringSetLength(&cmdDS, 0);
	if (!strcmp("gpg-1", version)) {
	    cmd = "gpg";
	    Tcl_DStringAppend(&cmdDS, "--detach-sign --armor "
			      "--no-secmem-warning --passphrase-fd 0 "
			      "--batch", -1);
	} else if (!strcmp("2", version)) {
	    cmd = "pgp";
	    Tcl_DStringAppend(&cmdDS, "+BATCHMODE +VERBOSE=0 -satbf", -1);
	} else if (!strcmp("5", version)) {
	    cmd = "pgps";
	    Tcl_DStringAppend(&cmdDS, "-abf", -1);
	} else if (!strcmp("6", version)) {
	    cmd = "pgp";
	    Tcl_DStringAppend(&cmdDS,"+BATCHMODE +VERBOSE=0 +force -satbf",-1);
	} else {
	    Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&cmdDS, " -u ", -1);
	Tcl_DStringAppendElement(&cmdDS, signer);
	pid = RatRunPGP(interp, 0, cmd, Tcl_DStringValue(&cmdDS),
			&toPGP, &outfile, &errPGP);
	
        if (NULL == RatPGPPhrase(interp, passPhrase, sizeof(passPhrase))) {
	    return TCL_ERROR;
	}
	write(toPGP, (char*)passPhrase, strlen((char*)passPhrase));
	for (i=0; i<strlen((char*)passPhrase); i++) passPhrase[i] = '\0';
	write(toPGP, "\n", 1);
	hdrPtr = buf;
	buf[0] = '\0';
	rfc822_write_body_header(&hdrPtr, *body);
	strlcat(buf, "\015\012", sizeof(buf));
	write(toPGP, buf, strlen(buf));
	RatInitDelayBuffer();
	rfc822_output_body(*body, RatDelaySoutr, (void*)toPGP);
	close(toPGP);
	/*i = open("/tmp/sigdump", O_CREAT | O_TRUNC | O_WRONLY, FILEMODE);
	write(i, buf, strlen(buf));
	RatInitDelayBuffer();
	rfc822_output_body(*body, RatDelaySoutr, (void*)i);
	close(i);*/
	do {
	    result = waitpid(pid, &status, 0);
	} while(-1 == result && EINTR == errno);

	/*
	 * Read result
	 */
	fromPGP = open(outfile, O_RDONLY);
	Tcl_DStringSetLength(&sigDS, 0);
	do {
	    length = read(fromPGP, buf, sizeof(buf));
	    for (i=0; i < length; i += j) {
		for (j=0; buf[i+j] != '\n' && i+j<length; j++);
		Tcl_DStringAppend(&sigDS, buf+i, j);
		if ('\n' == buf[i+j]) {
		    Tcl_DStringAppend(&sigDS, "\r\n", 2);
		    j++;
		}
	    }
	} while (length > 0);
	close(fromPGP);
	unlink(outfile);

	/*
	 * Check for errors
	 */
	if (pid != result || WEXITSTATUS(status)) {
	    Tcl_DStringSetLength(&cmdDS, 0);
	    Tcl_DStringAppendElement(&cmdDS, "RatPGPError");
	    Tcl_DStringStartSublist(&cmdDS);
	    do {
		if (0 < (length = read(errPGP, buf, sizeof(buf)))) {
		    Tcl_DStringAppend(&cmdDS, buf, length);
		}
	    } while (length > 0);
	    Tcl_DStringEndSublist(&cmdDS);
	    Tcl_GlobalEval(interp, Tcl_DStringValue(&cmdDS));
	    if (!strncmp("ABORT", Tcl_GetStringResult(interp), 5)) {
		close(errPGP);
		Tcl_DStringFree(&sigDS);
		return TCL_ERROR;
	    }
	    all_ok = 0;
	} else {
	    all_ok = 1;
	}
	close(errPGP);
    } while(0 == all_ok);
    Tcl_DStringFree(&cmdDS);

    /*
     * Build signature multipart
     */
    multiPtr = mail_newbody();
    multiPtr->type = TYPEMULTIPART;
    multiPtr->subtype = cpystr("signed");
    multiPtr->parameter = parmPtr = mail_newbody_parameter();
    parmPtr->attribute = cpystr("micalg");
    if (!strcmp("gpg-1", version))
      parmPtr->value = cpystr("pgp-sha1");
    else
      parmPtr->value = cpystr("pgp-md5");
    parmPtr->next = mail_newbody_parameter();
    parmPtr = parmPtr->next;
    parmPtr->attribute = cpystr("protocol");
    parmPtr->value = cpystr("application/pgp-signature");
    parmPtr->next = mail_newbody_parameter();
    parmPtr = parmPtr->next;
    parmPtr->attribute = cpystr("BOUNDARY");
    snprintf(buf, sizeof(buf), "%ld-%ld-%ld=:%ld",(long)gethostid(),random(),
	     time(NULL), (long)getpid());
    parmPtr->value = cpystr(buf);
    parmPtr->next = NULL;
    multiPtr->encoding = ENC7BIT;
    multiPtr->id = NULL;
    multiPtr->description = NULL;
    multiPtr->nested.part = partPtr = mail_newbody_part();
    memcpy(&partPtr->body, *body, sizeof(partPtr->body));
    partPtr->next = mail_newbody_part();
    partPtr = partPtr->next;
    partPtr->body.type = TYPEAPPLICATION;
    partPtr->body.subtype = cpystr("pgp-signature");
    partPtr->body.encoding = ENC7BIT;
    partPtr->body.contents.text.data = 
	    (unsigned char*)cpystr(Tcl_DStringValue(&sigDS));
    partPtr->body.size.bytes = Tcl_DStringLength(&sigDS);
    Tcl_DStringFree(&sigDS);
    partPtr->next = NULL;

    *body = multiPtr;

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * FindBoundary --
 *
 *      Find the boundary in a message string.
 *
 * Results:
 *	Pointer to the start of the boundary.
 *
 * Side effects:
 *	None
 *
 *
 *----------------------------------------------------------------------
 */

static char*
FindBoundary(char *text, char*boundary)
{
    char *cPtr = text;
    int l = strlen(boundary);

    if (NULL == text) return NULL;
    do {
	if ('-' == cPtr[0] && '-' == cPtr[1] && !strncmp(cPtr+2, boundary, l)){
	    return cPtr;
	}
	cPtr = strchr(cPtr, '\n');
    } while (cPtr++);
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPChecksig --
 *
 *      Check the signature of a bodypart
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Updates the bodyInfoPtr->sigStatus
 *
 *
 *----------------------------------------------------------------------
 */

void
RatPGPChecksig(Tcl_Interp *interp, MessageProcInfo* procInfo,
	BodyInfo *bodyInfoPtr)
{
    unsigned char *text;
    unsigned long length;
    char *command;
    CONST84 char *version;
	
    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);
	
    /*
     * Check if PGP/MIME message or old style PGP.
     * The algorithms kind of differ:-)
     */
    if (bodyInfoPtr->secPtr) {
	char *boundary, *start, *end, *from;
	char buf[2048], textfile[1024], sigfile[1024];
	int fd, toPGP, fromPGP, errPGP, pid, status, result;
	CONST84 char *tmp;
	PARAMETER *parPtr;
	Tcl_DString *resultDS = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));

	/*
	 * Generate filenames
	 */
	tmp = Tcl_GetVar(interp, "rat_tmp", TCL_GLOBAL_ONLY);
	tmp = RatTranslateFileName(interp, tmp);
	RatGenIdCmd(NULL, interp, 0, NULL);
	snprintf(textfile, sizeof(textfile), "%s/rat.%s",
		tmp, Tcl_GetStringResult(interp));
	strlcpy(sigfile, textfile, sizeof(sigfile));
	strlcat(sigfile, ".sig", sizeof(sigfile));

	/*
	 * Save text and signature in files
	 */
	boundary = NULL;
	text = (unsigned char*)(*procInfo[bodyInfoPtr->type].fetchBodyProc)
		(bodyInfoPtr->secPtr, &length);
	for (parPtr = bodyInfoPtr->secPtr->bodyPtr->parameter; parPtr;
		parPtr = parPtr->next) {
	    if (!strcasecmp(parPtr->attribute, "boundary")) {
		boundary = parPtr->value;
		break;
	    }
	}
	if (!boundary || NULL == (start = FindBoundary((char*)text,boundary))){
	    bodyInfoPtr->sigStatus = RAT_SIG_BAD;
	    return;
	}
	start += strlen(boundary) + 4;
	if (NULL == (end = FindBoundary(start, boundary))) {
	    bodyInfoPtr->sigStatus = RAT_SIG_BAD;
	    return;
	}
	end -= 2;
	fd = open(textfile, O_CREAT | O_TRUNC | O_WRONLY, FILEMODE);
	write(fd, start, end-start);
	close(fd);
	text = (unsigned char*)(*procInfo[bodyInfoPtr->type].fetchBodyProc)
		(bodyInfoPtr->secPtr->firstbornPtr->nextPtr, &length);
	fd = open(sigfile, O_CREAT | O_TRUNC | O_WRONLY, FILEMODE);
	if (text) {
	    write(fd, text, length);
	}
	close(fd);

	/*
	 * Run PGP command
	 */
	if (!strcmp("gpg-1", version)) {
	    command = "gpg";
	    snprintf(buf, sizeof(buf),
		    "--verify --no-secmem-warning --batch %s %s",
		    sigfile, textfile);
	} else if (!strcmp("2", version)) {
	    command = "pgp";
	    snprintf(buf, sizeof(buf), "+batchmode +verbose=0 %s %s",
		    sigfile, textfile);
	} else if (!strcmp("5", version)) {
	    command = "pgpv";
	    snprintf(buf, sizeof(buf), "+batchmode=1 %s -o %s",
		    sigfile, textfile);
	} else if (!strcmp("6", version)) {
	    command = "pgp";
	    snprintf(buf, sizeof(buf), "+batchmode +verbose=0 +force %s %s",
		    sigfile, textfile);
	} else {
	    Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	    return;
	}
	pid = RatRunPGP(interp, 1, command, buf, &toPGP, &from, &errPGP);
	close(toPGP);
	do {
	    result = waitpid(pid, &status, 0);
	} while(-1 == result && EINTR == errno);
	fromPGP = open(from, O_RDONLY);
	Tcl_DStringInit(resultDS);
	while (0 < (length = read(errPGP, buf, sizeof(buf)))) {
	    Tcl_DStringAppend(resultDS, buf, length);
	}
	while (0 < (length = read(fromPGP, buf, sizeof(buf)))) {
	    Tcl_DStringAppend(resultDS, buf, length);
	}
	close(fromPGP);
	unlink(from);
	close(errPGP);
	if (pid != result || WEXITSTATUS(status)) {
	    bodyInfoPtr->sigStatus = RAT_SIG_BAD;
	} else {
	    bodyInfoPtr->sigStatus = RAT_SIG_GOOD;
	}

	/*
	 * pgp-6.5.1i does not produce usable exit codes :-(
	 */
	if (!strcmp("6", version)) {
	    bodyInfoPtr->sigStatus = RAT_UNCHECKED;
	}
	bodyInfoPtr->pgpOutput = resultDS;

	/*
	 * Clean up
	 */
	unlink(textfile);
	unlink(sigfile);
	/*fprintf(stderr, "textfile: %s\n", textfile);*/
	/*fprintf(stderr, " sigfile: %s\n", sigfile);*/
    } else {
	Tcl_DString *bodyDSPtr;
	char *start, *end;

	text = (unsigned char*)(*procInfo[bodyInfoPtr->type].fetchBodyProc)
		(bodyInfoPtr, &length);
	if (text) {
	    start = RatPGPStrFind((char*)text, length, "BEGIN PGP", 1);
	    if (NULL == start) {
		Tcl_ResetResult(interp);
		return;
	    }
	    end = RatPGPStrFind(start,length-(start-(char*)text),"END PGP ",1);
	    bodyDSPtr=RatPGPRunOld(interp,bodyInfoPtr,(char*)text,start,end+1);
	    Tcl_DStringFree(bodyDSPtr);
	    ckfree(bodyDSPtr);
	}
    }
    if (bodyInfoPtr->pgpOutput && 1<Tcl_DStringLength(bodyInfoPtr->pgpOutput)){
	Tcl_SetResult(interp, Tcl_DStringValue(bodyInfoPtr->pgpOutput),
		TCL_VOLATILE);
    } else {
	Tcl_ResetResult(interp);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPDecrypt --
 *
 *      Decryt a bodypart.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The BodyInfo structure will be modified if the decryption is ok.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatPGPDecrypt(Tcl_Interp *interp, MessageProcInfo *procInfo,
	      BodyInfo **bodyInfoPtrPtr)
{
    int toPGP, fromPGP, errPGP, result, pid, retry, status, i;
    char *text, buf[1024], *from;
    volatile char passPhrase[MAXPASSLENGTH];
    CONST84 char *version;
    BodyInfo *origPtr = *bodyInfoPtrPtr, *partInfoPtr;
    MessageInfo *msgPtr;
    unsigned long length;
    Tcl_DString bodyDS, *errDSPtr = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));

    RatLog(interp, RAT_PARSE, "decrypting", RATLOG_EXPLICIT);
    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);

    /*
     * Decode the bodypart
     */
    Tcl_DStringInit(&bodyDS);
    (*procInfo[(*bodyInfoPtrPtr)->type].makeChildrenProc)
	    (interp, *bodyInfoPtrPtr);
    text = (*procInfo[(*bodyInfoPtrPtr)->type].fetchBodyProc)
	    ((*bodyInfoPtrPtr)->firstbornPtr->nextPtr, &length);
    retry = 1;
    while (retry && text) {
	if (NULL == RatPGPPhrase(interp, passPhrase, sizeof(passPhrase))) {
	    goto failed;
	}
	if (!strcmp("gpg-1", version)) {
	    pid = RatRunPGP(interp, 0, "gpg",
 			    "--decrypt -atq --no-secmem-warning "
			    "--passphrase-fd 0 --batch",
			    &toPGP, &from, &errPGP);
	} else if (!strcmp("2", version)) {
	    pid = RatRunPGP(interp, 0, "pgp", "+BATCHMODE +VERBOSE=0 -f",
		    &toPGP, &from, &errPGP);
	} else if (!strcmp("5", version)) {
	    pid = RatRunPGP(interp, 0, "pgpv", "+batchmode=1 -f",
		    &toPGP, &from, &errPGP);
	} else if (!strcmp("6", version)) {
	    pid = RatRunPGP(interp, 0,
			    "pgp", "+BATCHMODE +VERBOSE=0 +force -f",
			    &toPGP, &from, &errPGP);
	} else {
	    Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	    for (i=0; i<strlen((char*)passPhrase); i++) passPhrase[i] = '\0';
	    break;
	}
	write(toPGP, (char*)passPhrase, strlen((char*)passPhrase));
	for (i=0; i<strlen((char*)passPhrase); i++) passPhrase[i] = '\0';
	write(toPGP, "\n", 1);
	write(toPGP, text, length);
	/*fprintf(stderr, "%s:%d Dumped data to '/tmp/msgdump'\n", __FILE__,
		__LINE__);
	i = open("/tmp/msgdump", O_CREAT|O_TRUNC|O_WRONLY, 0600);
	write(i, text, length);
	close(i);*/
	close(toPGP);
	do {
	    result = waitpid(pid, &status, 0);
	} while(-1 == result && EINTR == errno);

	/*
	 * Read result
	 */
	fromPGP = open(from, O_RDONLY);
	Tcl_DStringSetLength(&bodyDS, 0);
	Tcl_DStringAppend(&bodyDS, "MIME-Version: 1.0\r\n", -1);
	while (0 < (length = read(fromPGP, buf, sizeof(buf)))) {
	    Tcl_DStringAppend(&bodyDS, buf, length);
	}
	close(fromPGP);
	unlink(from);
	Tcl_DStringInit(errDSPtr);
	while (0 < (length = read(errPGP, buf, sizeof(buf)))) {
	    Tcl_DStringAppend(errDSPtr, buf, length);
	}
	close(errPGP);

	/*
	 * Check for errors?
	 */
	if (pid != result
		|| (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 1)) {
	    Tcl_DString error;

	    ClearPGPPass(NULL);
	    Tcl_DStringInit(&error);
	    Tcl_DStringAppend(&error, "RatPGPError", -1);
	    Tcl_DStringAppendElement(&error, Tcl_DStringValue(errDSPtr));
	    if (TCL_OK != Tcl_Eval(interp, Tcl_DStringValue(&error))
		    || !strcmp("ABORT", Tcl_GetStringResult(interp))) {
		close(errPGP);
		Tcl_DStringFree(&error);
		Tcl_DStringFree(&bodyDS);
		Tcl_DStringFree(errDSPtr);
		ckfree(errDSPtr);
		RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
		goto failed;
	    } else {
		retry = 1;
	    }
	} else {
	    retry = 0;
	}
    }

    /*
     * Now parse the bodypart
     */
    Tcl_DeleteCommand(interp, (*bodyInfoPtrPtr)->cmdName);
    (*bodyInfoPtrPtr)->containedEntity = RatFrMessageCreate(interp,
	    Tcl_DStringValue(&bodyDS), Tcl_DStringLength(&bodyDS),
	    &msgPtr);
    Tcl_DStringFree(&bodyDS);
    *bodyInfoPtrPtr = Fr_CreateBodyProc(interp, msgPtr);
    msgPtr->bodyInfoPtr = NULL;
    if (WEXITSTATUS(status)) {
	(*bodyInfoPtrPtr)->sigStatus = RAT_UNSIGNED;
    } else {
	(*bodyInfoPtrPtr)->sigStatus = RAT_SIG_GOOD;
    }
    (*bodyInfoPtrPtr)->pgpOutput = errDSPtr;
    (*bodyInfoPtrPtr)->altPtr = origPtr;
    RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);

failed:
    /*
     * Create ordinary parts for body
     */
    for (partInfoPtr = (*bodyInfoPtrPtr)->firstbornPtr; partInfoPtr;
	    partInfoPtr = partInfoPtr->nextPtr) {
	Tcl_CreateObjCommand(interp, partInfoPtr->cmdName, RatBodyCmd,
		(ClientData) partInfoPtr, NULL);
    }
    RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
}

/*
 *----------------------------------------------------------------------
 *
 * RatPGPListKeys --
 *
 *      Lists the keys on a keyring
 *
 * Results:
 *	See ../doc/interface.
 *
 * Side effects:
 *	Runs the pgp command
 *
 *
 *----------------------------------------------------------------------
 */

int
RatPGPListKeys(Tcl_Interp *interp, char *keyringName)
{
    struct stat sbuf;
    Tcl_DString ck;
    RatPGPKeyring *k = NULL;
    Tcl_Obj **list, **l = NULL, *l3[6];
    int i;
    CONST84 char *value, *name;
    int secring = 0;

    Tcl_DStringInit(&ck);
    if (keyringName && !strcmp("PubRing", keyringName)) {
	Tcl_DStringAppend(&ck, "", 0);
    } else if (keyringName && !strcmp("SecRing", keyringName)) {
	Tcl_DStringAppend(&ck, "", 0);
	secring = 1;
    } else if (keyringName) {
	switch (keyringName[0]) {
	case '/':
	    Tcl_DStringAppend(&ck, keyringName, -1);
	    break;
	case '~':
	    name = RatTranslateFileName(interp, keyringName);
	    Tcl_DStringAppend(&ck, name, -1);	    
	    break;
	default:
	    Tcl_DStringAppend(&ck,
			      Tcl_GetVar2(interp, "env", "HOME",
					  TCL_GLOBAL_ONLY), -1);
	    Tcl_DStringAppend(&ck, "/.pgp/", -1);
	    Tcl_DStringAppend(&ck, keyringName, -1);
	    break;
	}
    } else {
	if (NULL == (value = RatGetPathOption(interp, "pgp_keyring"))) {
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&ck, value, -1);
    }

    /*
     * Check that we really need to do this
     */
    if ((keyring && !strcmp(keyring->name, Tcl_DStringValue(&ck)))) {
	k = keyring;
	if (0 != stat(k->name, &sbuf) || sbuf.st_mtime != k->mtime) {
	    RatPGPFreeKeyring(keyring);
	    k = NULL;
	    keyring = k = RatPGPNewKeyring(interp, Tcl_DStringValue(&ck),
					   secring);
	    if (TCL_OK != RatUpdatePGPKeys(interp, k)) {
		return TCL_ERROR;
	    }
	}
    }
    if (!k) {
	k = RatPGPNewKeyring(interp, Tcl_DStringValue(&ck), secring);
	if (TCL_OK != RatUpdatePGPKeys(interp, k)) {
	    return TCL_ERROR;
	}
    }
    if (!keyringName) {
	keyring = k;
    }
    Tcl_DStringFree(&ck);

    if (0 < k->keyCount) {
	list = (Tcl_Obj**)ckalloc(sizeof(Tcl_Obj*)*k->keyCount);
	for (i=0; i<k->keyCount; i++) {
	    l3[0] = k->keys[i].keyid;
	    l3[1] = k->keys[i].addresses;
	    l3[2] = k->keys[i].descr;
	    l3[3] = k->keys[i].subjects;
	    l3[4] = k->keys[i].sign;
	    l3[5] = k->keys[i].encrypt;
	    list[i] = Tcl_NewListObj(6, l3);
	}
	l3[0] = k->title;
	l3[1] = Tcl_NewListObj(k->keyCount, list);
	Tcl_SetObjResult(interp, Tcl_NewListObj(2, l3));
	ckfree(list);
	ckfree(l);
    } else {
	Tcl_ResetResult(interp);
    }

    if (keyring != k) {
	RatPGPFreeKeyring(k);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatPGPExtractKey --
 *
 *      Extracts a key from a keyring
 *
 * Results:
 *	See ../doc/interface.
 *
 * Side effects:
 *	Runs the pgp command
 *
 *
 *----------------------------------------------------------------------
 */

int
RatPGPExtractKey(Tcl_Interp *interp, char *id, char *keyringName)
{
    int toPGP, fromPGP, errPGP, pid, status, length, ret;
    Tcl_DString cmd, ck;
    char buf[1024], *cPtr, *command, *from;
    CONST84 char *value, *version, *name, *keyring_arg;
    Tcl_Obj *rPtr;

    Tcl_DStringInit(&ck);
    if (keyringName) {
	switch (keyringName[0]) {
	case '/':
	    Tcl_DStringAppend(&ck, keyringName, -1);
	    break;
	case '~':
	    name = RatTranslateFileName(interp, keyringName);
	    Tcl_DStringAppend(&ck, name, -1);
	    break;
	default:
	    Tcl_DStringAppend(&ck,
			      Tcl_GetVar2(interp, "env", "HOME",
					  TCL_GLOBAL_ONLY), -1);
	    Tcl_DStringAppend(&ck, "/.pgp/", -1);
	    Tcl_DStringAppend(&ck, keyringName, -1);
	    break;
	}
    } else {
	if (NULL != (value = RatGetPathOption(interp, "pgp_keyring"))) {
	    Tcl_DStringAppend(&ck, value, -1);
	}
    }

    Tcl_DStringInit(&cmd);
    rPtr = Tcl_NewObj();
    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);
    if (!strcmp("gpg-1", version)) {
	command = "gpg";
	keyring_arg = "--keyring ";
	Tcl_DStringAppend(&cmd, "--no-secmem-warning --export -aqt ", -1);
    } else if (!strcmp("2", version)) {
	command = "pgp";
	keyring_arg = "+PubRing=";
	Tcl_DStringAppend(&cmd, "-kxaf +BATCHMODE +VERBOSE=0 ", -1);
    } else if (!strcmp("5", version)) {
	command = "pgpk";
	keyring_arg = "+PubRing=";
	Tcl_DStringAppend(&cmd, "+batchmode=1 -x ", -1);
    } else if (!strcmp("6", version)) {
	command = "pgp";
	keyring_arg = "+PubRing=";
	Tcl_DStringAppend(&cmd, "-kxaf +BATCHMODE +VERBOSE=0 +force ", -1);
    } else {
	Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	return TCL_ERROR;
    }
    if (Tcl_DStringLength(&ck)) {
	Tcl_DStringAppend(&cmd, keyring_arg, -1);
	Tcl_DStringAppend(&cmd, Tcl_DStringValue(&ck), Tcl_DStringLength(&ck));
    }
    Tcl_DStringAppend(&cmd, " \"", 2);
    for (cPtr = id; *cPtr; cPtr++) {
      if ('"' == *cPtr) {
          Tcl_DStringAppend(&cmd, "\\\"", 2);
      } else {
          Tcl_DStringAppend(&cmd, cPtr, 1);
      }
    }
    Tcl_DStringAppend(&cmd, "\"", 1);
    pid = RatRunPGP(interp, 1, command, Tcl_DStringValue(&cmd),
	    &toPGP, &from, &errPGP);
    Tcl_DStringFree(&cmd);
    close(toPGP);
    do {
	ret = waitpid(pid, &status, 0);
    } while(-1 == ret && EINTR == errno);

    /*
     * Read output
     */
    fromPGP = open(from, O_RDONLY);
    do {
	if (0 < (length = read(fromPGP, buf, sizeof(buf)))) {
	    Tcl_AppendToObj(rPtr, buf, length);
	}
    } while (length > 0);
    close(fromPGP);
    unlink(from);

    /*
     * Check for errors?
     */
    if (pid != ret
	    || (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 1)) {
	Tcl_SetStringObj(rPtr, NULL, 0);
	do {
	    if (0 < (length = read(errPGP, buf, sizeof(buf)))) {
		Tcl_AppendToObj(rPtr, buf, length);
	    }
	} while (length > 0);
	close(errPGP);
	Tcl_SetObjResult(interp, rPtr);
	return TCL_ERROR;
    }
    close(errPGP);
    Tcl_SetObjResult(interp, rPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatPGPAddKeys --
 *
 *      Adds keys to a keyring
 *
 * Results:
 *	See ../doc/interface.
 *
 * Side effects:
 *	Runs the pgp command
 *
 *
 *----------------------------------------------------------------------
 */

int
RatPGPAddKeys(Tcl_Interp *interp, char *keys, char *keyring)
{
    Tcl_DString cmd;
    int result;

    /*
     * Setup and execute command
     */
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppendElement(&cmd, "RatPGPAddKeys");
    Tcl_DStringAppendElement(&cmd, keys);
    if (keyring) {
	Tcl_DStringAppendElement(&cmd, keyring);
    }
    result = Tcl_Eval(interp, Tcl_DStringValue(&cmd));
    Tcl_DStringFree(&cmd);
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPRunOld --
 *
 *      Handle an bodypart generated by pgp (which is not PGP/MIME).
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The BodyInfo structure will be modified.
 *
 *
 *----------------------------------------------------------------------
 */

static Tcl_DString*
RatPGPRunOld(Tcl_Interp *interp, BodyInfo *bodyInfoPtr, char *text,
		char *start, char *end)
{
    Tcl_DString *errDSPtr = (Tcl_DString*)ckalloc(sizeof(Tcl_DString)),
	*bodyDSPtr = (Tcl_DString*)ckalloc(sizeof(Tcl_DString)),
	*dsPtr = NULL;
    int needPhrase, toPGP, fromPGP, errPGP, length, preamble, pid, status,
	result, retry, i;
    char *ePtr, *cPtr, buf[1024], *from;
    volatile char passPhrase[MAXPASSLENGTH];
    CONST84 char *version;
    FILE *fp;

    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);

    /*
     * Prepare text part
     */
    Tcl_DStringInit(bodyDSPtr);
    RatDStringApendNoCRLF(bodyDSPtr, text, start-text);
    preamble = start-text;

    /*
     * Setup and run pgp command
     */
    needPhrase = strncmp(start, "-----BEGIN PGP SIGNED", 21);
    do {
	if (needPhrase) {
	    if (NULL == RatPGPPhrase(interp, passPhrase, sizeof(passPhrase))) {
		RatDStringApendNoCRLF(bodyDSPtr, start, end-start);
		ckfree(errDSPtr);
		bodyInfoPtr->pgpOutput = NULL;
		return bodyDSPtr;
	    }
	} else {
	    passPhrase[0] = '\0';
	}
	if (!strcmp("gpg-1", version)) {
	    pid = RatRunPGP(interp, 0, "gpg", "--decrypt -atq "
		    "--passphrase-fd 0 --no-secmem-warning --batch",
		    &toPGP, &from, &errPGP);
	} else if (!strcmp("2", version)) {
	    pid = RatRunPGP(interp, 0, "pgp", "+BATCHMODE +VERBOSE=0 -f",
		    &toPGP, &from, &errPGP);
	} else if (!strcmp("5", version)) {
	    pid = RatRunPGP(interp, 0, "pgpv", "+batchmode=1 -f",
		    &toPGP, &from, &errPGP);
	} else if (!strcmp("6", version)) {
	    pid = RatRunPGP(interp, 0,
			    "pgp", "+BATCHMODE +VERBOSE=0 +force -f",
			    &toPGP, &from, &errPGP);
	} else {
	    Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	    for (i=0; i<strlen((char*)passPhrase); i++) passPhrase[i] = '\0';
	    return NULL;
	}
	write(toPGP, (char*)passPhrase, strlen((char*)passPhrase));
	for (i=0; i<strlen((char*)passPhrase); i++) passPhrase[i] = '\0';
	write(toPGP, "\n", 1);
	fp = fdopen(toPGP, "w");
	/*
	 * Undo any encoding ant buggy MTA may have applied. Since we have this
	 * code here it means that the "BEGIN PGP" stuff must be visible in
	 * the raw (undecoded) text, so the only encoding we actually can
	 * handle is quoted-printable. I can live with this restriction and
	 * it is better than the performance penalty to decode all bodyparts.
	 */
	if (ENC7BIT != bodyInfoPtr->bodyPtr->encoding) {
	
	    dsPtr = RatDecode(interp, bodyInfoPtr->bodyPtr->encoding,
				  start, end-start, NULL);
	    start = Tcl_DStringValue(dsPtr);
	    ePtr = start + Tcl_DStringLength(dsPtr);
	} else {
	    if (!(ePtr = strchr(end, '\n'))) {
		ePtr = end + strlen(end);
	    }
	}
	
	/* Convert to local newline conventions */
	for (cPtr = start; cPtr < ePtr; cPtr++) {
	    if ('\r' == cPtr[0] && '\n' == cPtr[1]) cPtr++;
	    fputc(*cPtr, fp);
	}
	fclose(fp);

	if (dsPtr) {
	    Tcl_DStringFree(dsPtr);
	}
	do {
	    result = waitpid(pid, &status, 0);
	} while(-1 == result && EINTR == errno);

	/*
	 * Read result
	 */
	fromPGP = open(from, O_RDONLY);
	while (0 < (length = read(fromPGP, buf, sizeof(buf)))) {
	    Tcl_DStringAppend(bodyDSPtr, buf, length);
	}
	close(fromPGP);
	unlink(from);
	Tcl_DStringInit(errDSPtr);
	while (0 < (length = read(errPGP, buf, sizeof(buf)))) {
	    Tcl_DStringAppend(errDSPtr, buf, length);
	}
	close(errPGP);

	/*
	 * Check for errors?
	 */
	if (pid != result
		|| (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 1)) {
	    Tcl_DString error;

	    ClearPGPPass(NULL);
	    Tcl_DStringInit(&error);
	    Tcl_DStringAppend(&error, "RatPGPError", -1);
	    Tcl_DStringAppendElement(&error, Tcl_DStringValue(errDSPtr));
	    if (TCL_OK != Tcl_Eval(interp, Tcl_DStringValue(&error))
		    || !strcmp("ABORT", Tcl_GetStringResult(interp))) {
		close(errPGP);
		Tcl_DStringFree(&error);
		Tcl_DStringFree(errDSPtr);
		ckfree(errDSPtr);
		RatLog(interp, RAT_PARSE, "", RATLOG_EXPLICIT);
		RatDStringApendNoCRLF(bodyDSPtr, start, ePtr-start);
		bodyInfoPtr->pgpOutput = NULL;
		return bodyDSPtr;
	    } else {
		retry = 1;
	    }
	} else {
	    retry = 0;
	}
    } while (0 != retry);
    if (WEXITSTATUS(status)) {
	if (needPhrase) {
	    bodyInfoPtr->sigStatus = RAT_UNSIGNED;
	} else {
	    bodyInfoPtr->sigStatus = RAT_SIG_BAD;
	}
    } else {
	bodyInfoPtr->sigStatus = RAT_UNCHECKED;
    }
    bodyInfoPtr->pgpOutput = errDSPtr;

    return bodyDSPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPHandleOld --
 *
 *      Handle an bodypart generated by pgp (which is not PGP/MIME).
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The BodyInfo structure will be modified.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatPGPHandleOld(Tcl_Interp *interp, BodyInfo *bodyInfoPtr, char *text,
		char *start, char *end)
{
    if (strncmp(start, "-----BEGIN PGP SIGNED", 21)) {
	char *cPtr;
	CONST84 char *t;

	bodyInfoPtr->decodedTextPtr = RatPGPRunOld(interp, bodyInfoPtr, text,
						start, end);
	if (!(cPtr = strchr(end, '\n'))) {
	    cPtr = end + strlen(end);
	}
	if (*cPtr) {
	    RatDStringApendNoCRLF(bodyInfoPtr->decodedTextPtr, cPtr, -1);
	}
	if (bodyInfoPtr->pgpOutput
		&& 1 < Tcl_DStringLength(bodyInfoPtr->pgpOutput)) {
	    Tcl_DString cmd;

	    Tcl_DStringInit(&cmd);
	    Tcl_DStringAppendElement(&cmd, "RatText");
	    t = Tcl_GetVar2(interp, "t", "pgp_output", TCL_GLOBAL_ONLY);
	    Tcl_DStringAppendElement(&cmd, t);
	    Tcl_DStringAppendElement(&cmd,
		    Tcl_DStringValue(bodyInfoPtr->pgpOutput));
	    Tcl_Eval(interp, Tcl_DStringValue(&cmd));
	    Tcl_DStringFree(&cmd);
	}

    } else {
	bodyInfoPtr->sigStatus = RAT_UNCHECKED;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatUpdatePGPKeys --
 *
 *      Update the internal list of pgp keys (if needed)
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static int
RatUpdatePGPKeys(Tcl_Interp *interp, RatPGPKeyring *k)
{
    int toPGP, errPGP, pid, status, length, ret, blankiscont = 0;
    char buf[1024], *command, *from;
    CONST84 char *version, *keyring_arg;
    Tcl_DString cmd;
    struct stat sbuf;
    Tcl_RegExp exp_id = NULL, exp_addr = NULL;
    FILE *fp;
    Tcl_Obj *rPtr;

    stat(k->name, &sbuf);
    k->mtime = sbuf.st_mtime;

    /*
     * Launch PGP command
     */
    Tcl_DStringInit(&cmd);
    version = Tcl_GetVar2(interp, "option", "pgp_version", TCL_GLOBAL_ONLY);
    if (!strcmp("gpg-1", version)) {
        command = "gpg";
	keyring_arg = "--keyring ";
	if (k->secring) {
	    Tcl_DStringAppend(&cmd, "--list-secret-keys ", -1);
	} else {
	    Tcl_DStringAppend(&cmd, "--list-public-keys ", -1);
	}
	Tcl_DStringAppend(&cmd, "--no-secmem-warning "
			  "--with-colons --fixed-list-mode ", -1);

    } else if (!strcmp("2", version)) {
	/* XXX Handle private-public keyrings */
	command = "pgp";
	keyring_arg = "+PubRing=";
	Tcl_DStringAppend(&cmd, "-kv +BATCHMODE +VERBOSE=0 ", -1);
	exp_id = Tcl_RegExpCompile(interp, "[0-9]/([0-9A-F]{8})");
	exp_addr = Tcl_RegExpCompile(interp, "<[a-zA-Z.+@-]+>");
	blankiscont = 1;

    } else if (!strcmp("5", version)) {
	/* XXX Handle private-public keyrings */
	command = "pgpk";
	keyring_arg = "+PubRing=";
	Tcl_DStringAppend(&cmd, "-l +batchmode=1", -1);
	exp_id = Tcl_RegExpCompile(interp, ".ub.+0x([0-9A-F]{8}) ");
	exp_addr = Tcl_RegExpCompile(interp, "<[a-zA-Z.+@-]+>");
	blankiscont = 0;

    } else if (!strcmp("6", version)) {
	/* XXX Handle private-public keyrings */
	command = "pgp";
	keyring_arg = "+PubRing=";
	Tcl_DStringAppend(&cmd, "-kv +BATCHMODE +VERBOSE=0 +force ", -1);
	exp_id = Tcl_RegExpCompile(interp, "0x([0-9A-F]{8})");
	exp_addr = Tcl_RegExpCompile(interp, "<[a-zA-Z.+@-]+>");
	blankiscont = 1;

    } else {
	Tcl_SetResult(interp, "Unkown pgp version", TCL_STATIC);
	return TCL_ERROR;
    }
    if (k->name && *k->name) {
	Tcl_DStringAppend(&cmd, keyring_arg, -1);
	Tcl_DStringAppend(&cmd, k->name, -1);
    }
    pid = RatRunPGP(interp, 1, command, Tcl_DStringValue(&cmd),
	    &toPGP, &from, &errPGP);
    Tcl_DStringFree(&cmd);
    close(toPGP);
    do {
	ret = waitpid(pid, &status, 0);
    } while(-1 == ret && EINTR == errno);

    fp = fopen(from, "r");
    if (exp_id) {
	ParsePGPListFormat(interp, fp, exp_id, exp_addr, blankiscont, k);
    } else {
	ParseGPGListFormat(interp, fp, k);
    }
    fclose(fp);
    unlink(from);

    /*
     * Check for errors?
     */
    if (pid != ret
	    || (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 1)) {
	rPtr = Tcl_NewObj();
	do {
	    if ( 0 < (length = read(errPGP, buf, sizeof(buf)))) {
		Tcl_AppendToObj(rPtr, buf, length);
	    }
	} while (length > 0);
	close(errPGP);
	Tcl_SetObjResult(interp, rPtr);
	return TCL_ERROR;
    }
    close(errPGP);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParsePGPListFormat --
 *
 *      Parse the old format key list
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
ParsePGPListFormat(Tcl_Interp *interp, FILE *fp, Tcl_RegExp exp_id,
		   Tcl_RegExp exp_addr, int blankiscont, RatPGPKeyring *k)
{
    int preamble;
    char buf[1024], title[1024], idbuf[1024];
    CONST84 char *start, *end, *last;
    Tcl_DString tmp;
    Tcl_Obj *ids = NULL;

    Tcl_DStringInit(&tmp);
    preamble = 1;
    buf[sizeof(buf)-1] = '\0';
    while (fgets(buf, sizeof(buf)-1, fp), !feof(fp)) {
	if (buf[0]) {
	    buf[strlen(buf)-1] = '\0';
	}
	if (blankiscont && !isspace(buf[0]) && ids) {
	    AddKey(interp, k, ids, &tmp);
	    Tcl_DStringSetLength(&tmp, 0);
	    ids = NULL;
	}
	if (Tcl_RegExpExec(interp, exp_id, buf, buf)
	    || Tcl_RegExpExec(interp, exp_addr, buf, buf)) {
	    if (preamble) {
		preamble = 0;
		if (k->title) {
		    Tcl_DecrRefCount(k->title);
		}
		k->title = Tcl_NewStringObj(title, -1);
		Tcl_IncrRefCount(k->title);
	    }
	    if (Tcl_DStringLength(&tmp)) {
		Tcl_DStringAppend(&tmp, "\n", 1);
	    }
	    Tcl_DStringAppend(&tmp, buf, -1);
	    last = buf;
	    do {
		if (NULL == ids) {
		    ids = Tcl_NewObj();
		}
		if (Tcl_RegExpExec(interp, exp_id, buf, buf)) {
		    Tcl_RegExpRange(exp_id, 1, &start, &end);
		    last = end;
		    strlcpy(idbuf, "0x", sizeof(idbuf));
		    strlcpy(idbuf+2, start, end-start+1);
		    Tcl_ListObjAppendElement(
			interp, ids, Tcl_NewStringObj(idbuf, 2+end-start));
		}
		if (Tcl_RegExpExec(interp, exp_addr, buf, buf)) {
		    Tcl_RegExpRange(exp_addr, 0, &start, &end);
		    last = end;
		    Tcl_ListObjAppendElement(
			interp, ids, Tcl_NewStringObj(start, end-start));
		}
	    } while (Tcl_RegExpExec(interp, exp_id, last, buf)
		     || Tcl_RegExpExec(interp, exp_addr, last, buf));
	} else {
	    if (ids) {
		AddKey(interp, k, ids, &tmp);
		Tcl_DStringSetLength(&tmp, 0);
		ids = NULL;
	    }
	    if (preamble && buf[0] && buf[0] != '-') {
		strlcpy(title, buf, sizeof(title));
	    }
	}
    }
    if (ids) {
	AddKey(interp, k, ids, &tmp);
    }
    Tcl_DStringFree(&tmp);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseGPGListFormat --
 *
 *      Parse the gpg format key list
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
ParseGPGListFormat(Tcl_Interp *interp, FILE *fp, RatPGPKeyring *k)
{
    char buf[1024], buf2[1024], buf3[16], *fields[MAX_FIELDS], *s, *e, *alg;
    int num_fields, expect_uid = 0, is_subkey, is_key;
    Tcl_Obj *id = NULL;
    Tcl_Obj *addresses = NULL;
    Tcl_Obj *subjects = NULL;
    Tcl_Obj *descr = NULL;
    int sign = 0, encrypt = 0;
    Tcl_Obj *oPtr;
    struct tm *tm;
    time_t t;

    buf[sizeof(buf)-1] = '\0';
    do {
	fgets(buf, sizeof(buf)-1, fp);
	if (feof(fp)) {
	    buf[0] = '\0';
	} else if (buf[0]) {
	    buf[strlen(buf)-1] = '\0';
	}
	for (num_fields=0, s=buf; s && *s && num_fields < MAX_FIELDS;) {
	    fields[num_fields++] = s;
	    if (NULL != (s = strchr(s, ':'))) {
		*s++ = '\0';
	    }
	}

	if (expect_uid) {
	    if (num_fields && !strcmp("uid", fields[0])) {
		oPtr = Tcl_NewStringObj(fields[9], -1);		
		Tcl_ListObjAppendElement(interp, subjects, oPtr);
		if (NULL != (s = strchr(fields[9], '<'))
		    && NULL != (e = strchr(++s, '>'))) {
		    oPtr = Tcl_NewStringObj(s, e-s);
		}
		Tcl_ListObjAppendElement(interp, addresses, oPtr);
		continue;
	    } else {
		AddKeyNew(interp, k, id, addresses, subjects, descr,
			  Tcl_NewBooleanObj(sign), Tcl_NewBooleanObj(encrypt));
		expect_uid = 0;
	    }
	}

	is_subkey = is_key = 0;
	if (num_fields
	    && (!strcmp("sub", fields[0]) || !strcmp("ssb", fields[0]))) {
	    is_subkey = 1;
	} else if (num_fields
	    && (!strcmp("pub", fields[0]) || !strcmp("sec", fields[0]))) {
	    is_key = 1;
	}

	if (is_subkey || is_key) {
	    id = Tcl_NewStringObj(fields[4], -1);
	    switch (atoi(fields[3])) {
	    case 1:
		alg = "RSA";
		sign = 1;
		encrypt = 1;
		break;
	    case 16:
		alg = "ElGamal (encrypt only)";
		sign = 0;
		encrypt = 1;
		break;
	    case 17:
		alg = "DSA (sign only)";
		sign = 1;
		encrypt = 0;
		break;
	    case 20:
		alg = "ElGamal (sign and encrypt)";
		sign = 1;
		encrypt = 1;
		break;
	    default:
		alg = "unknown";
		sign = 1;
		encrypt = 1;
		break;
	    }
	    snprintf(buf2, sizeof(buf2), "%s %s %s",
		     fields[0], fields[2], alg);
	    if (fields[6][0]) { /* Expire date */
		strlcat(buf2, " expires ", sizeof(buf));
		t = atol(fields[6]);
		tm = localtime(&t);
		strftime(buf3, sizeof(buf3), "%x", tm);
		strlcat(buf2, buf3, sizeof(buf2));
	    }
	    descr = Tcl_NewStringObj(buf2, -1);
	    if (fields[11][0]) {
		sign = (int)strchr(fields[11], 's');
		encrypt = (int)strchr(fields[11], 'e');
	    }
	    if (is_subkey) {
		AddKeyNew(interp, k, id, addresses, subjects, descr,
			  Tcl_NewBooleanObj(sign), Tcl_NewBooleanObj(encrypt));
	    } else {
		expect_uid = 1;
		addresses = Tcl_NewObj();
		subjects = Tcl_NewObj();
	    }
	}
    } while (!feof(fp));
/*
  0x986343A5 <support@appgate.com> {pub  2048R/986343A5 2002-03-26 AppGate support <support@appgate.com>}
  0xA2646D35 <maf@appgate.com> {pub  1024D/A2646D35 2003-11-10 Martin Forssen <maf@appgate.com>
sub  2048g/63A7B927 2003-11-10 [expires: 2004-11-09]}
*/

    /* Fields to handle, pub, uid, sub, sec, ssb */
}

/*
 *----------------------------------------------------------------------
 *
 * AddKey --
 *
 *      Add key(s) to current keyring
 *
 *      TODO, usage, trust, expiration date, alg/size
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
AddKey(Tcl_Interp *interp, RatPGPKeyring *k, Tcl_Obj *ids,
       Tcl_DString *descr)
{
    unsigned int i;
    int idc;
    Tcl_Obj **idv;
    RatPGPKey *key;
    Tcl_Obj *addresses = Tcl_NewObj();
    char *s;

    if (TCL_OK != Tcl_ListObjGetElements(interp, ids, &idc, &idv)
	|| idc == 0
	|| !strcmp("0x00000000", Tcl_GetString(idv[0]))
	|| strncmp("0x", Tcl_GetString(idv[0]), 2)) {
	return;
    }
    Tcl_IncrRefCount(ids);

    for (i = 0; i < idc; i++) {
	if ('<' == *Tcl_GetString(idv[i])) {
	    s = Tcl_GetString(idv[i])+1;
	    Tcl_ListObjAppendElement(interp, addresses,
				     Tcl_NewStringObj(s, strlen(s)-1));
	}
    }

    for(i = 0; i<idc; i++) {
	if ('<' != *Tcl_GetStringFromObj(idv[i], NULL)) {
	    if (k->keyCount == k->keyAlloc) {
		k->keyAlloc += 256;
		k->keys = (RatPGPKey*)
		    ckrealloc(k->keys, sizeof(RatPGPKey)*k->keyAlloc);
	    }
	    key = &k->keys[k->keyCount++];
	    key->keyid = idv[i];
	    Tcl_IncrRefCount(key->keyid);
	    key->addresses = addresses;
	    key->subjects = NULL;
	    Tcl_IncrRefCount(key->subjects);
	    key->descr = Tcl_NewStringObj(
		    Tcl_DStringValue(descr), Tcl_DStringLength(descr));
	    Tcl_IncrRefCount(key->descr);
	    key->sign = Tcl_NewBooleanObj(1);
	    key->encrypt = key->sign;
	    Tcl_IncrRefCount(key->sign);
	    Tcl_IncrRefCount(key->encrypt);
	}
    }
    Tcl_DecrRefCount(ids);
}

/*
 *----------------------------------------------------------------------
 *
 * AddKeyNew --
 *
 *      Add key(s) to current keyring. Thsi is the new interface.
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
AddKeyNew(Tcl_Interp *interp, RatPGPKeyring *k, Tcl_Obj *id,
	  Tcl_Obj *addresses, Tcl_Obj *subjects, Tcl_Obj *descr,
	  Tcl_Obj *sign, Tcl_Obj *encrypt)
{
    RatPGPKey *key;

    if (k->keyCount == k->keyAlloc) {
	k->keyAlloc += 256;
	k->keys = (RatPGPKey*)
	    ckrealloc(k->keys, sizeof(RatPGPKey)*k->keyAlloc);
    }
    key = &k->keys[k->keyCount++];
    key->keyid = id;
    key->addresses = addresses;
    key->subjects = subjects;
    key->descr = descr;
    key->sign = sign;
    key->encrypt = encrypt;
    Tcl_IncrRefCount(key->keyid);
    Tcl_IncrRefCount(key->addresses);
    Tcl_IncrRefCount(key->subjects);
    Tcl_IncrRefCount(key->descr);
    Tcl_IncrRefCount(key->sign);
    Tcl_IncrRefCount(key->encrypt);
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPFreeKeyring --
 *
 *      Deallocates a keyring
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
RatPGPFreeKeyring(RatPGPKeyring *k)
{
    int i;

    for (i=0; i < k->keyCount; i++) {
	Tcl_DecrRefCount(k->keys[i].keyid);
	Tcl_DecrRefCount(k->keys[i].addresses);
	Tcl_DecrRefCount(k->keys[i].subjects);
	Tcl_DecrRefCount(k->keys[i].descr);
	Tcl_DecrRefCount(k->keys[i].sign);
	Tcl_DecrRefCount(k->keys[i].encrypt);
    }
    ckfree(k->keys);
    if (k->title) {
	Tcl_DecrRefCount(k->title);
    }
    ckfree(k->name);
    k->keyCount = 0;
    ckfree(k);
}


/*
 *----------------------------------------------------------------------
 *
 * RatPGPNewKeyring --
 *
 *      Allocate a new keyring
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
static RatPGPKeyring*
RatPGPNewKeyring(Tcl_Interp *interp, const char *name, int secring)
{
    RatPGPKeyring *k;

    k = (RatPGPKeyring*)ckalloc(sizeof(RatPGPKeyring));
    k->keys = NULL;
    k->keyCount = 0;
    k->keyAlloc = 0;
    k->title = Tcl_GetVar2Ex(interp, "t",
			     (secring ? "secring" : "pubring"),
			     TCL_GLOBAL_ONLY);
    Tcl_IncrRefCount(k->title);
    k->name = cpystr(name);
    k->mtime = 0;
    k->secring = secring;

    return k;
}
