/*
 * Program:	Dummy routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	9 May 1991
 * Last Edited:	19 December 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Function prototypes */

DRIVER *dummy_valid (char *name);
void *dummy_parameters (long function,void *value);
void dummy_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void dummy_list (MAILSTREAM *stream,char *ref,char *pat);
void dummy_lsub (MAILSTREAM *stream,char *ref,char *pat);
long dummy_subscribe (MAILSTREAM *stream,char *mailbox);
void dummy_list_work (MAILSTREAM *stream,char *dir,char *pat,char *contents,
		      long level);
long dummy_listed (MAILSTREAM *stream,char delimiter,char *name,
		   long attributes,char *contents);
long dummy_create (MAILSTREAM *stream,char *mailbox);
long dummy_create_path (MAILSTREAM *stream,char *path,long dirmode);
long dummy_delete (MAILSTREAM *stream,char *mailbox);
long dummy_rename (MAILSTREAM *stream,char *old,char *newname);
MAILSTREAM *dummy_open (MAILSTREAM *stream);
void dummy_close (MAILSTREAM *stream,long options);
long dummy_ping (MAILSTREAM *stream);
void dummy_check (MAILSTREAM *stream);
void dummy_expunge (MAILSTREAM *stream);
long dummy_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options);
long dummy_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);
char *dummy_file (char *dst,char *name);
long dummy_canonicalize (char *tmp,char *ref,char *pat);
