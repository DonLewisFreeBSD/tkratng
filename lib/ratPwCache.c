/*
 * RatPwCache.c --
 *
 *	This file contains password caching routines
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include <sys/stat.h>
#include <unistd.h>
#include "rat.h"
#include "env_unix.h"

/*
 * For the memory cache
 */
typedef struct CachedPasswd {
    int onDisk;
    char *spec;
    char *passwd;
    struct CachedPasswd *next;
    Tcl_TimerToken token;
} CachedPasswd;
static CachedPasswd *cache = NULL;
static int initialized = 0;
static char *filename = NULL;

/*
 * Local functions
 */
static char *Canonify(const char *spec);
static void ReadDisk(Tcl_Interp *interp);
static void WriteDisk(Tcl_Interp *interp);
static void TouchEntry(Tcl_Interp *interp, CachedPasswd *cp);
static Tcl_TimerProc ErasePasswd;


/*
 *----------------------------------------------------------------------
 *
 * Canonify --
 *
 *      Convert foler specification to canonical form
 *
 * Results:
 *	A pointer to a static buffer containing the canonic form.
 *	This buffer will be rewritten by the next call.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static char*
Canonify(const char *spec)
{
    static char *cSpec = NULL;
    static int size = 0;
    char *c;

    if (strlen(spec)+1 > size) {
	size = strlen(spec)+64;
	cSpec = (char*)realloc(cSpec, size);
    }
    strlcpy(cSpec, spec, size);
    if (NULL != (c = strstr(cSpec, "/debug"))) {
	memmove(c, c+6, strlen(c+6)+1);
    }
    if (NULL != (c = strchr(cSpec, '}'))) {
	c[1] = '\0';
    }
    return cSpec;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadDisk --
 *
 *      Read cached passwords from disk
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates the local cached list
 *
 *
 *----------------------------------------------------------------------
 */

void
ReadDisk(Tcl_Interp *interp)
{
    CONST84 char **argv, *spec = NULL, *passwd = NULL;
    const char *name;
    CachedPasswd *cp;
    char buf[1024];
    int argc;
    FILE *fp;

    if (NULL == (name = RatGetPathOption(interp, "pwcache_file"))) {
	return;
    }
    filename = cpystr(name);
    initialized = 1;
    if (NULL == (fp = fopen(filename, "r"))) {
	return;
    }
    while (fgets(buf, sizeof(buf), fp), !feof(fp)) {
	if (TCL_OK != Tcl_SplitList(interp, buf, &argc, &argv)
	    || (argc != 2 && argc != 5)) {
	    continue;
	}	
	if (2 == argc) {
	    /* {spec passwd} */
	    spec = argv[0];
	    passwd = argv[1];
	} else if (5 == argc) {
	    /* {host port user service passwd} */
	    snprintf(buf, sizeof(buf), "{%s:%s/user=%s%s}",
		     argv[0], argv[1], argv[2],
		     (strcmp("imap", argv[3]) ? "/pop3" : ""));
	    spec = buf;
	    passwd = argv[4];
	}
	cp = (CachedPasswd*)ckalloc(sizeof(CachedPasswd)
				    +strlen(spec)+1+strlen(passwd)+1);
	cp->onDisk = 1;
	cp->spec = (char*)cp + sizeof(CachedPasswd);
	strcpy(cp->spec, spec);
	cp->passwd = cp->spec + strlen(cp->spec)+1;
	strcpy(cp->passwd, passwd);
	cp->next = cache;
	cache = cp;
	ckfree(argv);
    }
    fclose(fp);
    return;
}


/*
 *----------------------------------------------------------------------
 *
 * WriteDisk --
 *
 *      Write the cache to disk. Only those entries marked to be stored on
 *	disk are actually output.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Rewrites the disk cache.
 *
 *
 *----------------------------------------------------------------------
 */

void
WriteDisk(Tcl_Interp *interp)
{
    CachedPasswd *cp;
    char c;
    FILE *fp;
    struct stat sbuf;
    int i, fd;
    Tcl_DString ds;

    if (-1 < (fd = open(filename, O_WRONLY))) {
	fstat(fd, &sbuf);
	c = 0;
	for (i=0; i<sbuf.st_size; i++) {
	    write(fd, &c, 1);
	}
	close(fd);
	unlink(filename);
    }
    if (NULL == (fp = fopen(filename, "w"))) {
	return;
    }
    fchmod(fileno(fp), 0600);
    Tcl_DStringInit(&ds);
    for (cp = cache; cp; cp = cp->next) {
	if (cp->onDisk) {
	    Tcl_DStringAppendElement(&ds, cp->spec);
	    Tcl_DStringAppendElement(&ds, cp->passwd);
	    fprintf(fp, "%s\n", Tcl_DStringValue(&ds));
	    Tcl_DStringSetLength(&ds, 0);
	}
    }
    fclose(fp);
    Tcl_DStringFree(&ds);
    return;
}


/*
 *----------------------------------------------------------------------
 *
 * TouchEntry --
 *
 *     Touch an entry in the cache. This only affects entries that are
 *	going to timeout.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May rewrite the cached password file
 *
 *
 *----------------------------------------------------------------------
 */

void
TouchEntry(Tcl_Interp *interp, CachedPasswd *cp)
{
    int timeout;
    Tcl_Obj *oPtr;

    if (cp->onDisk) {
	return;
    }
    Tcl_DeleteTimerHandler(cp->token);
    oPtr = Tcl_GetVar2Ex(interp, "option", "cache_passwd_timeout",
			 TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &timeout);
    if (timeout) {
	cp->token =
	    Tcl_CreateTimerHandler(timeout*1000, ErasePasswd, (ClientData)cp);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * ErasePasswd --
 *
 *      Earase a password from the cache
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the in-memory cache
 *
 *
 *----------------------------------------------------------------------
 */

static void
ErasePasswd(ClientData clientData)
{
    CachedPasswd **tcp, *cp = (CachedPasswd*)clientData;

    Tcl_DeleteTimerHandler(cp->token);
    memset(cp->passwd, 0, strlen(cp->passwd));
    for (tcp = &cache; *tcp != cp; tcp = &(*tcp)->next);
    *tcp = cp->next;
    ckfree(cp);
}


/*
 *----------------------------------------------------------------------
 *
 * RatGetCachedPassword --
 *
 *      get a cached password
 *
 * Results:
 *	Returns a pointer to a static area containing the password,
 *	or NULL if no suitable cached password was found.
 *
 * Side effects:
 *	may read the cached password file
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatGetCachedPassword(Tcl_Interp *interp, const char *spec)
{
    CachedPasswd *cp;
    char *cSpec = Canonify(spec);

    if (0 == initialized) {
	ReadDisk(interp);
    }
    for (cp = cache; cp; cp = cp->next) {
	if (!strcmp(cp->spec, cSpec)) {
	    TouchEntry(interp, cp);
	    return cp->passwd;
	}
    }
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * RatCachePassword --
 *
 *      Cache a password
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May rewrite the cached password file
 *
 *
 *----------------------------------------------------------------------
 */

void
RatCachePassword(Tcl_Interp *interp, const char *spec, const char *passwd,
		 int store)
{
    CachedPasswd *cp;
    char *cSpec = Canonify(spec);

    if (0 == initialized) {
	ReadDisk(interp);
    }
    cp = (CachedPasswd*)ckalloc(sizeof(CachedPasswd) +
				strlen(cSpec) + 1 + strlen(passwd) + 1);
    cp->onDisk = store;
    cp->spec = (char*)cp + sizeof(CachedPasswd);
    strcpy(cp->spec, cSpec);
    cp->passwd = cp->spec+strlen(cSpec)+1;
    strcpy(cp->passwd, passwd);
    cp->next = cache;
    cp->token = NULL;
    cache = cp;
    if (store) {
	WriteDisk(interp);
    } else {
	TouchEntry(interp, cp);
    }
    return;
}


/*
 *----------------------------------------------------------------------
 *
 * RatPasswdCachePurge --
 *
 *      Purge the password cache. If disk_also is true the both the disk and
 *	memory caches are purged. If disk_also is false, the only the
 *	memory cache is purged.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May rewrite the cached password file
 *
 *
 *----------------------------------------------------------------------
 */

void
RatPasswdCachePurge(Tcl_Interp *interp, int disk_also)
{
    CachedPasswd *cp, *cpn;

    if (0 == initialized) {
	ReadDisk(interp);
    }
    for (cp = cache; cp; cp = cpn) {
	cpn = cp->next;
	memset(cp->passwd, 0, strlen(cp->passwd));
	Tcl_DeleteTimerHandler(cp->token);
	ckfree(cp);
    }
    cache = NULL;
    if (disk_also) {
	WriteDisk(interp);
    }
    return;
}

int
RatPasswdCachePurgeCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    RatPasswdCachePurge(interp, 1);
    return TCL_OK;
}
