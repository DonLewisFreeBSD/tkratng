/*
 * Program:	Operating-system dependent routines -- SCO Unix version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 August 1988
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
#include "tcp_unix.h"		/* must be before osdep includes tcp.h */
#include <sys/time.h>		/* must be before osdep.h */
#include "mail.h"
#include <stdio.h>
#include "osdep.h"
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include "misc.h"
#define SecureWare		/* protected subsystem */
#include <sys/security.h>
#include <sys/audit.h>
#include <prot.h>
#include <pwd.h>

char *bigcrypt (char *key,char *salt);

#define DIR_SIZE(d) d->d_reclen

#include "fs_unix.c"
#include "ftl_unix.c"
#include "nl_unix.c"
#include "env_unix.c"
#include "tcp_unix.c"
#include "gr_waitp.c"
#include "flocksim.c"
#include "scandir.c"
#include "tz_sv4.c"
#include "gethstid.c"
#include "fsync.c"
#undef setpgrp
#include "setpgrp.c"
#include "rename.c"
#include "utime.c"
