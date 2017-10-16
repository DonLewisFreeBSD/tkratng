/* 
 * ratWatchdog.c --
 *
 *	Provides a small forked copy of tkrat which cleans up
 *	when the parent dies.
 *
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"
#include <signal.h>

static void RatWatchdogCleanup(const char *tmp);


/*
 *----------------------------------------------------------------------
 *
 * RatReleaseWatchdog --
 *
 *      Release the watchdog which eventually will cleanup the tmp-
 *	directory.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	forks.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatReleaseWatchdog(const char *tmpdir)
{
    struct rlimit rlim;
    int i, leash[2];
    char c;

    /*
     * The leash is used to release the watchdog (child) when the parent
     * dies.
     */
    if (pipe(leash)) return;
    
    if (0 == fork()) {
	/*
	 * Install signal handlers
	 */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGABRT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	
	/*
	 * The watchdog starts by closing all decriptors except our
	 * end of the leash.
	 */
	getrlimit(RLIMIT_NOFILE, &rlim);
	for (i=0; i<rlim.rlim_cur; i++) {
	    if (i != leash[0]) {
		close(i);
	    }
	}

	/*
	 * Try reading from the leash. This will hang until the server
	 * dies (since the server never will write to it).
	 */
	do {
	    i = SafeRead(leash[0], &c, 1);
	} while (0 != i);

	/*
	 * Do the cleanup and exit
	 */
	RatWatchdogCleanup(tmpdir);
	exit(0);
    }
    close(leash[0]);
}

/*
 *----------------------------------------------------------------------
 *
 * RatWatchdogCleanup --
 *
 *      Actually do the cleanup
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static void
RatWatchdogCleanup(const char *tmpdir)
{
    DIR *dir;
    struct dirent *d;
    char buf[1024];

    dir = opendir(tmpdir);
    while (NULL != dir && NULL != (d = readdir(dir))) {
	if (!strcmp(".", d->d_name) || !strcmp("..", d->d_name)) {
	    continue;
	}
	snprintf(buf, sizeof(buf), "%s/%s", tmpdir, d->d_name);
	unlink(buf);
    }
    closedir(dir);
    rmdir(tmpdir);
}
