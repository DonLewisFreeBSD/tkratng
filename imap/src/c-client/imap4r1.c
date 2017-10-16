/*
 * Program:	Interactive Message Access Protocol 4rev1 (IMAP4R1) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	15 June 1988
 * Last Edited:	14 November 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 *
 * This original version of this file is
 * Copyright 1988 Stanford University
 * and was developed in the Symbolic Systems Resources Group of the Knowledge
 * Systems Laboratory at Stanford University in 1987-88, and was funded by the
 * Biomedical Research Technology Program of the NationalInstitutes of Health
 * under grant number RR-00785.
 */

#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include "mail.h"
#include "osdep.h"
#include "imap4r1.h"
#include "rfc822.h"
#include "misc.h"

/* Driver dispatch used by MAIL */

DRIVER imapdriver = {
  "imap",			/* driver name */
				/* driver flags */
  DR_MAIL|DR_NEWS|DR_NAMESPACE|DR_CRLF|DR_RECYCLE,
  (DRIVER *) NIL,		/* next driver */
  imap_valid,			/* mailbox is valid for us */
  imap_parameters,		/* manipulate parameters */
  imap_scan,			/* scan mailboxes */
  imap_list,			/* find mailboxes */
  imap_lsub,			/* find subscribed mailboxes */
  imap_subscribe,		/* subscribe to mailbox */
  imap_unsubscribe,		/* unsubscribe from mailbox */
  imap_create,			/* create mailbox */
  imap_delete,			/* delete mailbox */
  imap_rename,			/* rename mailbox */
  imap_status,			/* status of mailbox */
  imap_open,			/* open mailbox */
  imap_close,			/* close mailbox */
  imap_fast,			/* fetch message "fast" attributes */
  imap_flags,			/* fetch message flags */
  imap_overview,		/* fetch overview */
  imap_structure,		/* fetch message envelopes */
  NIL,				/* fetch message header */
  NIL,				/* fetch message body */
  imap_msgdata,			/* fetch partial message */
  imap_uid,			/* unique identifier */
  imap_msgno,			/* message number */
  imap_flag,			/* modify flags */
  NIL,				/* per-message modify flags */
  imap_search,			/* search for message based on criteria */
  imap_sort,			/* sort messages */
  imap_thread,			/* thread messages */
  imap_ping,			/* ping mailbox to see if still alive */
  imap_check,			/* check for new messages */
  imap_expunge,			/* expunge deleted messages */
  imap_copy,			/* copy messages to another mailbox */
  imap_append,			/* append string message to mailbox */
  imap_gc			/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM imapproto = {&imapdriver};

				/* driver parameters */
static unsigned long imap_maxlogintrials = MAXLOGINTRIALS;
static long imap_lookahead = IMAPLOOKAHEAD;
static long imap_uidlookahead = IMAPUIDLOOKAHEAD;
static long imap_defaultport = 0;
static long imap_sslport = 0;
static long imap_tryssl = NIL;
static long imap_prefetch = IMAPLOOKAHEAD;
static long imap_closeonerror = NIL;
static imapenvelope_t imap_envelope = NIL;
static imapreferral_t imap_referral = NIL;
static char *imap_extrahdrs = NIL;

				/* constants */
static char *hdrheader = "BODY.PEEK[HEADER.FIELDS (Path Message-ID Content-Type";
static char *hdrtrailer ="Newsgroups Followup-To References)]";


/* Append data */

typedef struct append_data {
  append_t af;
  void *data;
} APPENDDATA;

/* IMAP validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *imap_valid (char *name)
{
  return mail_valid_net (name,&imapdriver,NIL,NIL);
}


/* IMAP manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *imap_parameters (long function,void *value)
{
  switch ((int) function) {
  case SET_NAMESPACE:
    fatal ("SET_NAMESPACE not permitted");
  case GET_NAMESPACE:
    if (((IMAPLOCAL *) ((MAILSTREAM *) value)->local)->cap.namespace &&
	!((IMAPLOCAL *) ((MAILSTREAM *) value)->local)->namespace)
      imap_send (((MAILSTREAM *) value),"NAMESPACE",NIL);
    value = (void *) &((IMAPLOCAL *) ((MAILSTREAM *) value)->local)->namespace;
    break;
  case GET_THREADERS:
    value = (void *) ((IMAPLOCAL *) ((MAILSTREAM *) value)->local)->threader;
    break;
  case SET_MAXLOGINTRIALS:
    imap_maxlogintrials = (long) value;
    break;
  case GET_MAXLOGINTRIALS:
    value = (void *) imap_maxlogintrials;
    break;
  case SET_LOOKAHEAD:
    imap_lookahead = (long) value;
    break;
  case GET_LOOKAHEAD:
    value = (void *) imap_lookahead;
    break;
  case SET_UIDLOOKAHEAD:
    imap_uidlookahead = (long) value;
    break;
  case GET_UIDLOOKAHEAD:
    value = (void *) imap_uidlookahead;
    break;

  case SET_IMAPPORT:
    imap_defaultport = (long) value;
    break;
  case GET_IMAPPORT:
    value = (void *) imap_defaultport;
    break;
  case SET_SSLIMAPPORT:
    imap_sslport = (long) value;
    break;
  case GET_SSLIMAPPORT:
    value = (void *) imap_sslport;
    break;
  case SET_PREFETCH:
    imap_prefetch = (long) value;
    break;
  case GET_PREFETCH:
    value = (void *) imap_prefetch;
    break;
  case SET_CLOSEONERROR:
    imap_closeonerror = (long) value;
    break;
  case GET_CLOSEONERROR:
    value = (void *) imap_closeonerror;
    break;
  case SET_IMAPENVELOPE:
    imap_envelope = (imapenvelope_t) value;
    break;
  case GET_IMAPENVELOPE:
    value = (void *) imap_envelope;
    break;
  case SET_IMAPREFERRAL:
    imap_referral = (imapreferral_t) value;
    break;
  case GET_IMAPREFERRAL:
    value = (void *) imap_referral;
    break;
  case SET_IMAPEXTRAHEADERS:
    imap_extrahdrs = (char *) value;
    break;
  case GET_IMAPEXTRAHEADERS:
    value = (void *) imap_extrahdrs;
    break;
  case SET_IMAPTRYSSL:
    imap_tryssl = (long) value;
    break;
  case GET_IMAPTRYSSL:
    value = (void *) imap_tryssl;
    break;
  default:
    value = NIL;		/* error case */
    break;
  }
  return value;
}

/* IMAP scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void imap_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  imap_list_work (stream,"SCAN",ref,pat,contents);
}


/* IMAP list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void imap_list (MAILSTREAM *stream,char *ref,char *pat)
{
  imap_list_work (stream,"LIST",ref,pat,NIL);
}


/* IMAP list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void imap_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  void *sdb = NIL;
  char *s,mbx[MAILTMPLEN];
				/* do it on the server */
  imap_list_work (stream,"LSUB",ref,pat,NIL);
  if (*pat == '{') {		/* if remote pattern, must be IMAP */
    if (!imap_valid (pat)) return;
    ref = NIL;			/* good IMAP pattern, punt reference */
  }
				/* if remote reference, must be valid IMAP */
  if (ref && (*ref == '{') && !imap_valid (ref)) return;
				/* kludgy application of reference */
  if (ref && *ref) sprintf (mbx,"%s%s",ref,pat);
  else strcpy (mbx,pat);

  if (s = sm_read (&sdb)) do if (imap_valid (s) && pmatch (s,mbx))
    mm_lsub (stream,NIL,s,NIL);
  while (s = sm_read (&sdb));	/* until no more subscriptions */
}

/* IMAP find list of mailboxes
 * Accepts: mail stream
 *	    list command
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void imap_list_work (MAILSTREAM *stream,char *cmd,char *ref,char *pat,
		     char *contents)
{
  MAILSTREAM *st = stream;
  int pl;
  char *s,prefix[MAILTMPLEN],mbx[MAILTMPLEN];
  IMAPARG *args[4],aref,apat,acont;
  if (ref && *ref) {		/* have a reference? */
    if (!(imap_valid (ref) &&	/* make sure valid IMAP name and open stream */
	  ((stream && LOCAL && LOCAL->netstream) ||
	   (stream = mail_open (NIL,ref,OP_HALFOPEN|OP_SILENT))))) return;
				/* calculate prefix length */
    pl = strchr (ref,'}') + 1 - ref;
    strncpy (prefix,ref,pl);	/* build prefix */
    prefix[pl] = '\0';		/* tie off prefix */
    ref += pl;			/* update reference */
  }
  else {
    if (!(imap_valid (pat) &&	/* make sure valid IMAP name and open stream */
	  ((stream && LOCAL && LOCAL->netstream) ||
	   (stream = mail_open (NIL,pat,OP_HALFOPEN|OP_SILENT))))) return;
				/* calculate prefix length */
    pl = strchr (pat,'}') + 1 - pat;
    strncpy (prefix,pat,pl);	/* build prefix */
    prefix[pl] = '\0';		/* tie off prefix */
    pat += pl;			/* update reference */
  }
  LOCAL->prefix = prefix;	/* note prefix */
  if (contents) {		/* want to do a scan? */
    if (LEVELSCAN (stream)) {	/* make sure permitted */
      args[0] = &aref; args[1] = &apat; args[2] = &acont; args[3] = NIL;
      aref.type = ASTRING; aref.text = (void *) (ref ? ref : "");
      apat.type = LISTMAILBOX; apat.text = (void *) pat;
      acont.type = ASTRING; acont.text = (void *) contents;
      imap_send (stream,cmd,args);
    }
    else mm_log ("Scan not valid on this IMAP server",ERROR);
  }

  else if (LEVELIMAP4 (stream)){/* easy if IMAP4 */
    args[0] = &aref; args[1] = &apat; args[2] = NIL;
    aref.type = ASTRING; aref.text = (void *) (ref ? ref : "");
    apat.type = LISTMAILBOX; apat.text = (void *) pat;
				/* referrals armed? */
    if (LOCAL->cap.mbx_ref && mail_parameters (stream,GET_IMAPREFERRAL,NIL) &&
      ((cmd[0] == 'L') || (cmd[0] == 'l')) && !cmd[4]) {
				/* yes, convert LIST -> RLIST */
      if (((cmd[1] == 'I') || (cmd[1] == 'i')) &&
	  ((cmd[2] == 'S') || (cmd[1] == 's')) &&
	  ((cmd[3] == 'T') || (cmd[3] == 't'))) cmd = "RLIST";
				/* and convert LSUB -> RLSUB */
      else if (((cmd[1] == 'S') || (cmd[1] == 's')) &&
	       ((cmd[2] == 'U') || (cmd[1] == 'u')) &&
	       ((cmd[3] == 'B') || (cmd[3] == 'b'))) cmd = "RLSUB";
    }
    imap_send (stream,cmd,args);
  }
  else if (LEVEL1176 (stream)) {/* convert to IMAP2 format wildcard */
				/* kludgy application of reference */
    if (ref && *ref) sprintf (mbx,"%s%s",ref,pat);
    else strcpy (mbx,pat);
    for (s = mbx; *s; s++) if (*s == '%') *s = '*';
    args[0] = &apat; args[1] = NIL;
    apat.type = LISTMAILBOX; apat.text = (void *) mbx;
    if (!(strstr (cmd,"LIST") &&/* if list, try IMAP2bis, then RFC-1176 */
	  strcmp (imap_send (stream,"FIND ALL.MAILBOXES",args)->key,"BAD")) &&
	!strcmp (imap_send (stream,"FIND MAILBOXES",args)->key,"BAD"))
      LOCAL->rfc1176 = NIL;	/* must be RFC-1064 */
  }
  LOCAL->prefix = NIL;		/* no more prefix */
				/* close temporary stream if we made one */
  if (stream != st) mail_close (stream);
}

/* IMAP subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long imap_subscribe (MAILSTREAM *stream,char *mailbox)
{
  MAILSTREAM *st = stream;
  long ret = ((stream && LOCAL && LOCAL->netstream) ||
	      (stream = mail_open (NIL,mailbox,OP_HALFOPEN|OP_SILENT))) ?
		imap_manage (stream,mailbox,LEVELIMAP4 (stream) ?
			     "Subscribe" : "Subscribe Mailbox",NIL) : NIL;
				/* toss out temporary stream */
  if (st != stream) mail_close (stream);
  return ret;
}


/* IMAP unsubscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to delete from manage list
 * Returns: T on success, NIL on failure
 */

long imap_unsubscribe (MAILSTREAM *stream,char *mailbox)
{
  MAILSTREAM *st = stream;
  long ret = ((stream && LOCAL && LOCAL->netstream) ||
	      (stream = mail_open (NIL,mailbox,OP_HALFOPEN|OP_SILENT))) ?
		imap_manage (stream,mailbox,LEVELIMAP4 (stream) ?
			     "Unsubscribe" : "Unsubscribe Mailbox",NIL) : NIL;
				/* toss out temporary stream */
  if (st != stream) mail_close (stream);
  return ret;
}

/* IMAP create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long imap_create (MAILSTREAM *stream,char *mailbox)
{
  return imap_manage (stream,mailbox,"Create",NIL);
}


/* IMAP delete mailbox
 * Accepts: mail stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long imap_delete (MAILSTREAM *stream,char *mailbox)
{
  return imap_manage (stream,mailbox,"Delete",NIL);
}


/* IMAP rename mailbox
 * Accepts: mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long imap_rename (MAILSTREAM *stream,char *old,char *newname)
{
  return imap_manage (stream,old,"Rename",newname);
}

/* IMAP manage a mailbox
 * Accepts: mail stream
 *	    mailbox to manipulate
 *	    command to execute
 *	    optional second argument
 * Returns: T on success, NIL on failure
 */

long imap_manage (MAILSTREAM *stream,char *mailbox,char *command,char *arg2)
{
  MAILSTREAM *st = stream;
  IMAPPARSEDREPLY *reply;
  long ret = NIL;
  char mbx[MAILTMPLEN],mbx2[MAILTMPLEN];
  IMAPARG *args[3],ambx,amb2;
  imapreferral_t ir =
    (imapreferral_t) mail_parameters (stream,GET_IMAPREFERRAL,NIL);
  ambx.type = amb2.type = ASTRING; ambx.text = (void *) mbx;
  amb2.text = (void *) mbx2;
  args[0] = &ambx; args[1] = args[2] = NIL;
				/* require valid names and open stream */
  if (mail_valid_net (mailbox,&imapdriver,NIL,mbx) &&
      (arg2 ? mail_valid_net (arg2,&imapdriver,NIL,mbx2) : &imapdriver) &&
      ((stream && LOCAL && LOCAL->netstream) ||
       (stream = mail_open (NIL,mailbox,OP_HALFOPEN|OP_SILENT)))) {
    if (arg2) args[1] = &amb2;	/* second arg present? */
    if (!(ret = (imap_OK (stream,reply = imap_send (stream,command,args)))) &&
	ir && LOCAL->referral) {
      long code = -1;
      switch (*command) {	/* which command was it? */
      case 'S': code = REFSUBSCRIBE; break;
      case 'U': code = REFUNSUBSCRIBE; break;
      case 'C': code = REFCREATE; break;
      case 'D': code = REFDELETE; break;
      case 'R': code = REFRENAME; break;
      default:
	fatal ("impossible referral command");
      }
      if ((code >= 0) && (mailbox = (*ir) (stream,LOCAL->referral,code)))
	ret = imap_manage (NIL,mailbox,command,(*command == 'R') ?
			   (mailbox + strlen (mailbox) + 1) : NIL);
    }
    mm_log (reply->text,ret ? NIL : ERROR);
				/* toss out temporary stream */
    if (st != stream) mail_close (stream);
  }
  return ret;
}

/* IMAP status
 * Accepts: mail stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long imap_status (MAILSTREAM *stream,char *mbx,long flags)
{
  IMAPARG *args[3],ambx,aflg;
  char tmp[MAILTMPLEN];
  NETMBX mb;
  unsigned long i;
  long ret = NIL;
  MAILSTREAM *tstream = stream;
  imapreferral_t ir =
    (imapreferral_t) mail_parameters (stream,GET_IMAPREFERRAL,NIL);
  mail_valid_net_parse (mbx,&mb);
				/* can't use stream if not IMAP4rev1, STATUS,
				   or halfopen and right host */
  if (stream && (!(LEVELSTATUS (stream) || stream->halfopen)
		 || !mail_usable_network_stream (stream,mbx)))
    return imap_status (NIL,mbx,flags);
				/* make stream if don't have one */
  if (!(stream || (stream = mail_open (NIL,mbx,OP_HALFOPEN|OP_SILENT))))
    return NIL;
  args[0] = &ambx;args[1] = NIL;/* set up first argument as mailbox */
  ambx.type = ASTRING; ambx.text = (void *) mb.mailbox;
  if (LEVELSTATUS (stream)) {	/* have STATUS command? */
    aflg.type = FLAGS; aflg.text = (void *) tmp;
    args[1] = &aflg; args[2] = NIL;
    tmp[0] = tmp[1] = '\0';	/* build flag list */
    if (flags & SA_MESSAGES) strcat (tmp," MESSAGES");
    if (flags & SA_RECENT) strcat (tmp," RECENT");
    if (flags & SA_UNSEEN) strcat (tmp," UNSEEN");
    if (flags & SA_UIDNEXT) strcat (tmp,LEVELIMAP4rev1 (stream) ?
				    " UIDNEXT" : " UID-NEXT");
    if (flags & SA_UIDVALIDITY) strcat (tmp,LEVELIMAP4rev1 (stream) ?
					" UIDVALIDITY" : " UID-VALIDITY");
    tmp[0] = '(';
    strcat (tmp,")");
				/* send "STATUS mailbox flag" */
    if (imap_OK (stream,imap_send (stream,"STATUS",args))) ret = T;
    else if (ir && LOCAL->referral &&
	     (mbx = (*ir) (stream,LOCAL->referral,REFSTATUS)))
      ret = imap_status (NIL,mbx,flags);
  }

				/* IMAP2 way */
  else if (imap_OK (stream,imap_send (stream,"EXAMINE",args))) {
    MAILSTATUS status;
    status.flags = flags & ~ (SA_UIDNEXT | SA_UIDVALIDITY);
    status.messages = stream->nmsgs;
    status.recent = stream->recent;
    status.unseen = 0;
    if (flags & SA_UNSEEN) {	/* must search to get unseen messages */
				/* clear search vector */
      for (i = 1; i <= stream->nmsgs; ++i) mail_elt (stream,i)->searched = NIL;
      if (imap_OK (stream,imap_send (stream,"SEARCH UNSEEN",NIL)))
	for (i = 1,status.unseen = 0; i <= stream->nmsgs; i++)
	  if (mail_elt (stream,i)->searched) status.unseen++;
    }
    strcpy (strchr (strcpy (tmp,stream->mailbox),'}') + 1,mb.mailbox);
				/* pass status to main program */
    mm_status (stream,tmp,&status);
    ret = T;			/* note success */
  }
  if (stream != tstream) mail_close (stream);
  return ret;			/* success */
}

/* IMAP open
 * Accepts: stream to open
 * Returns: stream to use on success, NIL on failure
 */

MAILSTREAM *imap_open (MAILSTREAM *stream)
{
  unsigned long i,j;
  char *s,tmp[MAILTMPLEN],usr[MAILTMPLEN];
  NETMBX mb;
  IMAPPARSEDREPLY *reply = NIL;
  imapreferral_t ir =
    (imapreferral_t) mail_parameters (stream,GET_IMAPREFERRAL,NIL);
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &imapproto;
  mail_valid_net_parse (stream->mailbox,&mb);
  usr[0] = '\0';		/* initially no user name */
  if (LOCAL) {			/* if stream opened earlier by us */
    if (LOCAL->netstream) {	/* recycle if still alive */
      i = stream->silent;	/* temporarily mark silent */
      stream->silent = T;	/* don't give mm_exists() events */
      j = imap_ping (stream);	/* learn if stream still alive */
      stream->silent = i;	/* restore prior state */
      if (j) {			/* was stream still alive? */
	sprintf (tmp,"Reusing connection to %s",net_host (LOCAL->netstream));
	if (LOCAL->user) sprintf (tmp + strlen (tmp),"/user=\"%s\"",
				  LOCAL->user);
	if (!stream->silent) mm_log (tmp,(long) NIL);
      }
      else imap_close (stream,NIL);
    }
    else imap_close (stream,NIL);
  }
				/* copy flags from name */
  if (mb.dbgflag) stream->debug = T;
  if (mb.readonlyflag) stream->rdonly = T;
  if (mb.anoflag) stream->anonymous = T;
  if (mb.secflag) stream->secure = T;
  if (mb.trysslflag || imap_tryssl) stream->tryssl = T;

  if (!LOCAL) {			/* open new connection if no recycle */
    NETDRIVER *ssld = (NETDRIVER *) mail_parameters (NIL,GET_SSLDRIVER,NIL);
    unsigned long defprt = imap_defaultport ? imap_defaultport : IMAPTCPPORT;
    unsigned long sslport = imap_sslport ? imap_sslport : IMAPSSLPORT;
    stream->local =		/* instantiate localdata */
      (void *) memset (fs_get (sizeof (IMAPLOCAL)),0,sizeof (IMAPLOCAL));
				/* assume IMAP2bis server */
    LOCAL->imap2bis = LOCAL->rfc1176 = T;
				/* desirable authenticators */
    LOCAL->authflags = (stream->secure ? AU_SECURE : NIL) |
      (mb.authuser[0] ? AU_AUTHUSER : NIL);
    /* IMAP connection open logic is more complex than net_open() normally
     * deals with, because of the simap and rimap hacks.
     * If the session is anonymous, a specific port is given, or if /ssl or
     * /tls is set, do net_open() since those conditions override everything
     * else.
     */
    if (stream->anonymous || mb.port || mb.sslflag || mb.tlsflag)
      reply = (LOCAL->netstream = net_open (&mb,NIL,defprt,ssld,"*imaps",
					    sslport)) ?
	imap_reply (stream,NIL) : NIL;
    /* 
     * No overriding conditions, so get the best connection that we can.  In
     * order, attempt to open via simap, tryssl, rimap, and finally TCP.
     */
				/* try simap */
    else if (reply = imap_rimap (stream,"*imap",&mb,usr,tmp));
    else if (ssld &&		/* try tryssl if enabled */
	     (stream->tryssl || mail_parameters (NIL,GET_TRYSSLFIRST,NIL)) &&
	     (LOCAL->netstream =
	      net_open_work (ssld,mb.host,"*imaps",sslport,mb.port,
			     (mb.novalidate ? NET_NOVALIDATECERT : 0) |
			     NET_SILENT | NET_TRYSSL))) {
      if (net_sout (LOCAL->netstream,"",0)) {
	mb.sslflag = T;
	reply = imap_reply (stream,NIL);
      }
      else {			/* flush fake SSL stream */
	net_close (LOCAL->netstream);
	LOCAL->netstream = NIL;
      }
    }
				/* try rimap first, then TCP */
    else if (!(reply = imap_rimap (stream,"imap",&mb,usr,tmp)) &&
	     (LOCAL->netstream = net_open (&mb,NIL,defprt,NIL,NIL,NIL)))
      reply = imap_reply (stream,NIL);
				/* make sure greeting is good */
    if (!reply || strcmp (reply->tag,"*") ||
	(strcmp (reply->key,"OK") && strcmp (reply->key,"PREAUTH"))) {
      if (reply) mm_log (reply->text,ERROR);
      return NIL;		/* lost during greeting */
    }

				/* if connected and not preauthenticated */
    if (LOCAL->netstream && strcmp (reply->key,"PREAUTH")) {
      sslstart_t stls = (sslstart_t) mail_parameters (NIL,GET_SSLSTART,NIL);
				/* get server capabilities */
      if (!LOCAL->gotcapability) imap_capability (stream);
      if (LOCAL->netstream &&	/* does server support STARTTLS? */
	  stls && LOCAL->cap.starttls && !mb.sslflag && !mb.notlsflag &&
	  imap_OK (stream,imap_send (stream,"STARTTLS",NIL))) {
	mb.tlsflag = T;		/* TLS OK, get into TLS at this end */
	LOCAL->netstream->dtb = ssld;
	if (!(LOCAL->netstream->stream =
	      (*stls) (LOCAL->netstream->stream,mb.host,NET_TLSCLIENT |
		       (mb.novalidate ? NET_NOVALIDATECERT : NIL)))) {
				/* drat, drop this connection */
	  if (LOCAL->netstream) net_close (LOCAL->netstream);
	  LOCAL->netstream = NIL;
	}
				/* get capabilities now that TLS in effect */
	if (LOCAL->netstream) imap_capability (stream);
      }
      else if (mb.tlsflag) {	/* user specified /tls but can't do it */
	mm_log ("Unable to negotiate TLS with this server",ERROR);
	return NIL;
      }
      if (LOCAL->netstream) {	/* still in the land of the living? */
	if ((int) mail_parameters (NIL,GET_TRUSTDNS,NIL)) {
				/* remote name for authentication */
	  strncpy (mb.host,(int) mail_parameters (NIL,GET_SASLUSESPTRNAME,NIL)?
		   net_remotehost (LOCAL->netstream) :
		   net_host (LOCAL->netstream),NETMAXHOST-1);
	  mb.host[NETMAXHOST-1] = '\0';
	}
				/* need new capabilities after login */
	LOCAL->gotcapability = NIL;
	if (!(stream->anonymous ? imap_anon (stream,tmp) :
	      (LOCAL->cap.auth ? imap_auth (stream,&mb,tmp,usr) :
	       imap_login (stream,&mb,tmp,usr)))) {
				/* failed, is there a referral? */
	  if (ir && LOCAL->referral &&
	      (s = (*ir) (stream,LOCAL->referral,REFAUTHFAILED))) {
	    imap_close (stream,NIL);
	    fs_give ((void **) &stream->mailbox);
				/* set as new mailbox name to open */
	    stream->mailbox = s;
	    return imap_open (stream);
	  }
	  return NIL;		/* authentication failed */
	}
      }
    }
				/* get server capabilities again */
    if (LOCAL->netstream && !LOCAL->gotcapability) imap_capability (stream);
				/* save state for future recycling */
    if (mb.tlsflag) LOCAL->tlsflag = T;
    if (mb.notlsflag) LOCAL->notlsflag = T;
    if (mb.sslflag) LOCAL->sslflag = T;
    if (mb.novalidate) LOCAL->novalidate = T;
  }

  if (LOCAL->netstream) {	/* still have a connection? */
    if (ir && LOCAL->referral &&
	(s = (*ir) (stream,LOCAL->referral,REFAUTH))) {
      imap_close (stream,NIL);
      fs_give ((void **) &stream->mailbox);
      stream->mailbox = s;	/* set as new mailbox name to open */
      return imap_open (stream);/* recurse to log in on real site */
    }
    stream->perm_seen = stream->perm_deleted = stream->perm_answered =
      stream->perm_draft = LEVELIMAP4 (stream) ? NIL : T;
    stream->perm_user_flags = LEVELIMAP4 (stream) ? NIL : 0xffffffff;
    stream->sequence++;		/* bump sequence number */
    sprintf (tmp,"{%s",(int) mail_parameters (NIL,GET_TRUSTDNS,NIL) ?
	     net_host (LOCAL->netstream) : mb.host);
    if (!((i = net_port (LOCAL->netstream)) & 0xffff0000))
      sprintf (tmp + strlen (tmp),":%lu",i);
    strcat (tmp,"/imap");
    if (LOCAL->tlsflag) strcat (tmp,"/tls");
    if (LOCAL->notlsflag) strcat (tmp,"/notls");
    if (LOCAL->sslflag) strcat (tmp,"/ssl");
    if (LOCAL->novalidate) strcat (tmp,"/novalidate-cert");
    if (stream->secure) strcat (tmp,"/secure");
    if (stream->rdonly) strcat (tmp,"/readonly");
    if (stream->anonymous) strcat (tmp,"/anonymous");
    else {			/* record user name */
      if (!LOCAL->user && usr[0]) LOCAL->user = cpystr (usr);
      if (LOCAL->user) sprintf (tmp + strlen (tmp),"/user=\"%s\"",
				LOCAL->user);
    }
    strcat (tmp,"}");

    if (!stream->halfopen) {	/* wants to open a mailbox? */
      IMAPARG *args[2];
      IMAPARG ambx;
      ambx.type = ASTRING;
      ambx.text = (void *) mb.mailbox;
      args[0] = &ambx; args[1] = NIL;
      if (imap_OK (stream,reply = imap_send (stream,stream->rdonly ?
					     "EXAMINE": "SELECT",args))) {
	strcat (tmp,mb.mailbox);/* mailbox name */
	if (!stream->nmsgs && !stream->silent)
	  mm_log ("Mailbox is empty",(long) NIL);
				/* note if an INBOX or not */
	stream->inbox = !compare_cstring (mb.mailbox,"INBOX");
      }
      else if (ir && LOCAL->referral &&
	       (s = (*ir) (stream,LOCAL->referral,REFSELECT))) {
	imap_close (stream,NIL);
	fs_give ((void **) &stream->mailbox);
	stream->mailbox = s;	/* set as new mailbox name to open */
	return imap_open (stream);
      }
      else {
	mm_log (reply->text,ERROR);
	if (imap_closeonerror) return NIL;
	stream->halfopen = T;	/* let him keep it half-open */
      }
    }
    if (stream->halfopen) {	/* half-open connection? */
      strcat (tmp,"<no_mailbox>");
				/* make sure dummy message counts */
      mail_exists (stream,(long) 0);
      mail_recent (stream,(long) 0);
    }
    fs_give ((void **) &stream->mailbox);
    stream->mailbox = cpystr (tmp);
  }
				/* success if stream open */
  return LOCAL->netstream ? stream : NIL;
}

/* IMAP rimap connect
 * Accepts: MAIL stream
 *	    NETMBX specification
 *	    service to use
 *	    user name
 *	    scratch buffer
 * Returns: parsed reply if success, else NIL
 */

IMAPPARSEDREPLY *imap_rimap (MAILSTREAM *stream,char *service,NETMBX *mb,
			     char *usr,char *tmp)
{
  unsigned long i;
  char c[2];
  NETSTREAM *tstream;
  IMAPPARSEDREPLY *reply = NIL;
				/* try rimap open */
  if (!mb->norsh && (tstream = net_aopen (NIL,mb,service,usr))) {
				/* if success, see if reasonable banner */
    if (net_getbuffer (tstream,(long) 1,c) && (*c == '*')) {
      i = 0;			/* copy to buffer */
      do tmp[i++] = *c;
      while (net_getbuffer (tstream,(long) 1,c) && (*c != '\015') &&
	     (*c != '\012') && (i < (MAILTMPLEN-1)));
      tmp[i] = '\0';		/* tie off */
				/* snarfed a valid greeting? */
      if ((*c == '\015') && net_getbuffer (tstream,(long) 1,c) &&
	  (*c == '\012') &&
	  !strcmp ((reply = imap_parse_reply (stream,cpystr (tmp)))->tag,"*")){
				/* parse line as IMAP */
	imap_parse_unsolicited (stream,reply);
				/* make sure greeting is good */
	if (!strcmp (reply->key,"OK") || !strcmp (reply->key,"PREAUTH")) {
	  LOCAL->netstream = tstream;
	  return reply;		/* return success */
	}
      }
    }
    net_close (tstream);	/* failed, punt the temporary netstream */
  }
  return NIL;
}

/* IMAP log in as anonymous
 * Accepts: stream to authenticate
 *	    scratch buffer
 * Returns: T on success, NIL on failure
 */

long imap_anon (MAILSTREAM *stream,char *tmp)
{
  IMAPPARSEDREPLY *reply;
  char *s = net_localhost (LOCAL->netstream);
  if (LOCAL->cap.authanon) {
    char tag[16];
    unsigned long i;
    char *broken = "[CLOSED] IMAP connection broken (anonymous auth)";
    sprintf (tag,"%08lx",stream->gensym++);
				/* build command */
    sprintf (tmp,"%s AUTHENTICATE ANONYMOUS",tag);
    if (!imap_soutr (stream,tmp)) {
      mm_log (broken,ERROR);
      return NIL;
    }
    if (imap_challenge (stream,&i)) imap_response (stream,s,strlen (s));
				/* get response */
    if (!(reply = &LOCAL->reply)->tag) reply = imap_fake (stream,tag,broken);
				/* what we wanted? */
    if (strcmp (reply->tag,tag)) {
				/* abort if don't have tagged response */
      while (strcmp ((reply = imap_reply (stream,tag))->tag,tag))
	imap_soutr (stream,"*");
    }
  }
  else {
    IMAPARG *args[2];
    IMAPARG ausr;
    ausr.type = ASTRING;
    ausr.text = (void *) s;
    args[0] = &ausr; args[1] = NIL;
				/* send "LOGIN anonymous <host>" */
    reply = imap_send (stream,"LOGIN ANONYMOUS",args);
  }
				/* success if reply OK */
  if (imap_OK (stream,reply)) return T;
  mm_log (reply->text,ERROR);
  return NIL;
}

/* IMAP authenticate
 * Accepts: stream to authenticate
 *	    parsed network mailbox structure
 *	    scratch buffer
 *	    place to return user name
 * Returns: T on success, NIL on failure
 */

long imap_auth (MAILSTREAM *stream,NETMBX *mb,char *tmp,char *usr)
{
  unsigned long trial,ua;
  int ok;
  char tag[16];
  char *lsterr = NIL;
  AUTHENTICATOR *at;
  IMAPPARSEDREPLY *reply;
  for (ua = LOCAL->cap.auth; LOCAL->netstream && ua &&
       (at = mail_lookup_auth (find_rightmost_bit (&ua) + 1));) {
    if (lsterr) {		/* previous authenticator failed? */
      sprintf(tmp,"Retrying using %s authentication after %s",at->name,lsterr);
      mm_log (tmp,NIL);
      fs_give ((void **) &lsterr);
    }
    trial = 0;			/* initial trial count */
    tmp[0] = '\0';		/* no error */
    do {			/* gensym a new tag */
      if (tmp[0]) mm_log (tmp,WARN);
      sprintf (tag,"%08lx",stream->gensym++);
				/* build command */
      sprintf (tmp,"%s AUTHENTICATE %s",tag,at->name);
      if (imap_soutr (stream,tmp)) {
				/* hide client authentication responses */
	if (!(at->flags & AU_SECURE)) LOCAL->sensitive = T;
	ok = (*at->client) (imap_challenge,imap_response,"imap",mb,stream,
			    &trial,usr);
	LOCAL->sensitive = NIL;	/* unhide */
				/* get response */
	if (!(reply = &LOCAL->reply)->tag)
	  reply = imap_fake (stream,tag,
			     "[CLOSED] IMAP connection broken (authenticate)");
				/* what we wanted? */
	if (strcmp (reply->tag,tag)) {
				/* abort if don't have tagged response */
	  while (strcmp ((reply = imap_reply (stream,tag))->tag,tag))
	    imap_soutr (stream,"*");
	}
				/* cancel any last error */
	if (lsterr) fs_give ((void **) &lsterr);
	if (ok) {		/* done if got success response */
	  if (imap_OK (stream,reply)) return T;
	  if (!trial) {		/* if main program requested cancellation */
	    mm_log ("IMAP Authentication cancelled",ERROR);
	    return NIL;
	  }
	}
	lsterr = cpystr (reply->text);
	sprintf (tmp,"Retrying %s authentication after %s",at->name,lsterr);
      }
    }
    while (LOCAL->netstream && !LOCAL->byeseen && trial &&
	   (trial < imap_maxlogintrials));
  }
  if (lsterr) {			/* previous authenticator failed? */
    sprintf (tmp,"Can not authenticate to IMAP server: %s",lsterr);
    mm_log (tmp,ERROR);
    fs_give ((void **) &lsterr);
  }
  return NIL;			/* ran out of authenticators */
}

/* IMAP login
 * Accepts: stream to login
 *	    parsed network mailbox structure
 *	    scratch buffer of length MAILTMPLEN
 *	    place to return user name
 * Returns: T on success, NIL on failure
 */

long imap_login (MAILSTREAM *stream,NETMBX *mb,char *pwd,char *usr)
{
  unsigned long trial = 0;
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[3];
  IMAPARG ausr,apwd;
  long ret = NIL;
  if (stream->secure)		/* never do LOGIN if want security */
    mm_log ("Can't do secure authentication with this server",ERROR);
  else if (LOCAL->logindisabled)/* never do LOGIN if server disabled it */
    mm_log ("Server disables LOGIN, no recognized SASL authenticator",ERROR);
  else if (mb->authuser[0])	/* never do LOGIN with /authuser */
    mm_log ("Can't do /authuser with this server",ERROR);
  else {			/* OK to try login */
    ausr.type = apwd.type = ASTRING;
    ausr.text = (void *) usr;
    apwd.text = (void *) pwd;
    args[0] = &ausr; args[1] = &apwd; args[2] = NIL;
    do {
      pwd[0] = 0;		/* prompt user for password */
      mm_login (mb,usr,pwd,trial++);
      if (pwd[0]) {		/* send login command if have password */
	LOCAL->sensitive = T;	/* hide this command */
				/* send "LOGIN usr pwd" */
	if (imap_OK (stream,reply = imap_send (stream,"LOGIN",args)))
	  ret = LONGT;		/* success */
	else {
	  mm_log (reply->text,WARN);
	  if (!LOCAL->referral && (trial == imap_maxlogintrials))
	    mm_log ("Too many login failures",ERROR);
	}
	LOCAL->sensitive = NIL;	/* unhide */
      }
				/* user refused to give password */
      else mm_log ("Login aborted",ERROR);
    } while (!ret && pwd[0] && (trial < imap_maxlogintrials) &&
	     LOCAL->netstream && !LOCAL->byeseen && !LOCAL->referral);
  }
  memset (pwd,0,MAILTMPLEN);	/* erase password */
  return ret;
}

/* Get challenge to authenticator in binary
 * Accepts: stream
 *	    pointer to returned size
 * Returns: challenge or NIL if not challenge
 */

void *imap_challenge (void *s,unsigned long *len)
{
  MAILSTREAM *stream = (MAILSTREAM *) s;
  IMAPPARSEDREPLY *reply;
  while (LOCAL->netstream) {	/* parse reply from server */
    if (reply = imap_parse_reply (stream,net_getline (LOCAL->netstream))) {
				/* received challenge? */
      if (!strcmp (reply->tag,"+"))
	return rfc822_base64 ((unsigned char *) reply->text,
			      strlen (reply->text),len);
				/* untagged data? */
      else if (!strcmp (reply->tag,"*")) imap_parse_unsolicited (stream,reply);
      else break;		/* tagged response */
    }
  }
  return NIL;			/* tagged response, bogon, or lost stream */
}


/* Send authenticator response in BASE64
 * Accepts: MAIL stream
 *	    string to send
 *	    length of string
 * Returns: T if successful, else NIL
 */

long imap_response (void *s,char *response,unsigned long size)
{
  MAILSTREAM *stream = (MAILSTREAM *) s;
  unsigned long i,j,ret;
  char *t,*u;
  if (response) {		/* make CRLFless BASE64 string */
    if (size) {
      for (t = (char *) rfc822_binary ((void *) response,size,&i),u = t,j = 0;
	   j < i; j++) if (t[j] > ' ') *u++ = t[j];
      *u = '\0';		/* tie off string for mm_dlog() */
      if (stream->debug) mail_dlog (t,LOCAL->sensitive);
				/* append CRLF */
      *u++ = '\015'; *u++ = '\012';
      ret = net_sout (LOCAL->netstream,t,u - t);
      fs_give ((void **) &t);
    }
    else ret = imap_soutr (stream,"");
  }
				/* abort requested */
  else ret = imap_soutr (stream,"*");
  return ret;
}

/* IMAP close
 * Accepts: MAIL stream
 *	    option flags
 */

void imap_close (MAILSTREAM *stream,long options)
{
  THREADER *thr,*t;
  IMAPPARSEDREPLY *reply;
  if (stream && LOCAL) {	/* send "LOGOUT" */
    if (!LOCAL->byeseen) {	/* don't even think of doing it if saw a BYE */
				/* expunge silently if requested */
      if (options & CL_EXPUNGE) imap_send (stream,"EXPUNGE",NIL);
      if (LOCAL->netstream &&
	  !imap_OK (stream,reply = imap_send (stream,"LOGOUT",NIL)))
	mm_log (reply->text,WARN);
    }
				/* close NET connection if still open */
    if (LOCAL->netstream) net_close (LOCAL->netstream);
    LOCAL->netstream = NIL;
				/* free up memory */
    if (LOCAL->sortdata) fs_give ((void **) &LOCAL->sortdata);
    if (LOCAL->namespace) {
      mail_free_namespace (&LOCAL->namespace[0]);
      mail_free_namespace (&LOCAL->namespace[1]);
      mail_free_namespace (&LOCAL->namespace[2]);
      fs_give ((void **) &LOCAL->namespace);
    }
    if (LOCAL->threaddata) mail_free_threadnode (&LOCAL->threaddata);
				/* flush threaders */
    if (thr = LOCAL->threader) while (t = thr) {
      fs_give ((void **) &t->name);
      thr = t->next;
      fs_give ((void **) &t);
    }
    if (LOCAL->referral) fs_give ((void **) &LOCAL->referral);
    if (LOCAL->user) fs_give ((void **) &LOCAL->user);
    if (LOCAL->reply.line) fs_give ((void **) &LOCAL->reply.line);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
  }
}

/* IMAP fetch fast information
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 *
 * Generally, imap_fetchstructure is preferred
 */

void imap_fast (MAILSTREAM *stream,char *sequence,long flags)
{
  IMAPPARSEDREPLY *reply = imap_fetch (stream,sequence,flags & FT_UID);
  if (!imap_OK (stream,reply)) mm_log (reply->text,ERROR);
}


/* IMAP fetch flags
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 */

void imap_flags (MAILSTREAM *stream,char *sequence,long flags)
{				/* send "FETCH sequence FLAGS" */
  char *cmd = (LEVELIMAP4 (stream) && (flags & FT_UID)) ? "UID FETCH":"FETCH";
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[3],aseq,aatt;
  aseq.type = SEQUENCE; aseq.text = (void *) sequence;
  aatt.type = ATOM; aatt.text = (void *) "FLAGS";
  args[0] = &aseq; args[1] = &aatt; args[2] = NIL;
  if (!imap_OK (stream,reply = imap_send (stream,cmd,args)))
    mm_log (reply->text,ERROR);
}

/* IMAP fetch overview
 * Accepts: MAIL stream, sequence bits set
 *	    pointer to overview return function
 * Returns: T if successful, NIL otherwise
 */

long imap_overview (MAILSTREAM *stream,overview_t ofn)
{
  MESSAGECACHE *elt;
  ENVELOPE *env;
  OVERVIEW ov;
  char *s,*t;
  unsigned long i,j,start,last,len;
  if (!LOCAL->netstream) return NIL;
				/* build overview sequence */
  for (i = 1,len = start = last = 0,s = t = NIL; i <= stream->nmsgs; ++i)
    if ((elt = mail_elt (stream,i))->sequence) {
      if (!elt->private.msg.env) {
	if (s) {		/* continuing a sequence */
	  if (i == last + 1) last = i;
	  else {		/* end of range */
	    if (last != start) sprintf (t,":%lu,%lu",last,i);
	    else sprintf (t,",%lu",i);
	    start = last = i;	/* begin a new range */
	    if ((j = ((t += strlen (t)) - s)) > (MAILTMPLEN - 20)) {
	      fs_resize ((void **) s,len += MAILTMPLEN);
	      t = s + j;	/* relocate current pointer */
	    }
	  }
	}
	else {			/* first time, start new buffer */
	  s = (char *) fs_get (len = MAILTMPLEN);
	  sprintf (s,"%lu",start = last = i);
	  t = s + strlen (s);	/* end of buffer */
	}
      }
    }
				/* last sequence */
  if (last != start) sprintf (t,":%lu",last);
  if (s) {			/* prefetch as needed */
    imap_fetch (stream,s,FT_NEEDENV);
    fs_give ((void **) &s);
  }
  ov.optional.lines = 0;	/* now overview each message */
  ov.optional.xref = NIL;
  if (ofn) for (i = 1; i <= stream->nmsgs; i++)
    if (((elt = mail_elt (stream,i))->sequence) &&
	(env = mail_fetch_structure (stream,i,NIL,NIL)) && ofn) {
      ov.subject = env->subject;
      ov.from = env->from;
      ov.date = env->date;
      ov.message_id = env->message_id;
      ov.references = env->references;
      ov.optional.octets = elt->rfc822_size;
      (*ofn) (stream,mail_uid (stream,i),&ov);
    }
  return LONGT;
}

/* IMAP fetch structure
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to return body
 *	    option flags
 * Returns: envelope of this message, body returned in body value
 *
 * Fetches the "fast" information as well
 */

ENVELOPE *imap_structure (MAILSTREAM *stream,unsigned long msgno,BODY **body,
			  long flags)
{
  unsigned long i,j,k;
  char *s,seq[128],tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  ENVELOPE **env;
  BODY **b;
  IMAPPARSEDREPLY *reply = NIL;
  IMAPARG *args[3],aseq,aatt;
  args[0] = &aseq; args[1] = &aatt; args[2] = NIL;
  aseq.type = SEQUENCE; aseq.text = (void *) seq;
  aatt.type = ATOM; aatt.text = NIL;
  if (flags & FT_UID)		/* see if can find msgno from UID */
    for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->private.uid == msgno) {
	msgno = i;		/* found msgno, use it from now on */
	flags &= ~FT_UID;	/* no longer a UID fetch */
      }
  sprintf (seq,"%lu",msgno);	/* initial sequence */
  if (LEVELIMAP4 (stream) && (flags & FT_UID)) {
    /* UID fetching is requested and we can't map the UID to a message sequence
     * number.  Assume that the message isn't cached at all.
     */
    if (!imap_OK (stream,reply = imap_fetch (stream,seq,FT_NEEDENV +
					     (body ? FT_NEEDBODY : NIL) +
					     (flags & (FT_UID + FT_NOHDRS)))))
      mm_log (reply->text,ERROR);
				/* now hunt for this UID */
    for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->private.uid == msgno) {
	if (body) *body = elt->private.msg.body;
	return elt->private.msg.env;
      }
    if (body) *body = NIL;	/* can't find the UID */
    return NIL;
  }

  elt = mail_elt (stream,msgno);/* get cache pointer */
  if (stream->scache) {		/* short caching? */
    env = &stream->env;		/* use temporaries on the stream */
    b = &stream->body;
    if (msgno != stream->msgno){/* flush old poop if a different message */
      mail_free_envelope (env);
      mail_free_body (b);
      stream->msgno = msgno;	/* this is now the current short cache msg */
    }
  }
  else {			/* normal cache */
    env = &elt->private.msg.env;/* get envelope and body pointers */
    b = &elt->private.msg.body;
				/* prefetch if don't have envelope */
    if ((k = imap_lookahead) && (!*env || (*env)->incomplete))
				/* build message number list */
      for (i = msgno + 1, s = seq; k && (i <= stream->nmsgs); i++)
	if (!mail_elt (stream,i)->private.msg.env) {
	  s += strlen (s);	/* find string end, see if nearing end */
	  if ((s - seq) > (MAILTMPLEN - 20)) break;
	  sprintf (s,",%lu",i);	/* append message */
 	  for (j = i + 1, k--;	/* hunt for last message without an envelope */
	       k && (j <= stream->nmsgs) &&
	       !mail_elt (stream,j)->private.msg.env; j++, k--);
				/* if different, make a range */
	  if (i != --j) sprintf (s + strlen (s),":%lu",i = j);
	}
  }

  if (!stream->lock) {		/* no-op if stream locked */
    /* Build the fetch attributes.  Unlike imap_fetch(), this tries not to
     * fetch data that is already cached.  However, since it is based on the
     * message requested and not on any of the prefetched messages, it can
     * goof, either by fetching data already cached or not prefetching data
     * that isn't cached (but was cached in the message requested).
     * Fortunately, no great harm is done.  If it doesn't prefetch the data,
     * it will get it when the affected message(s) are requested.
     */
    if (!elt->private.uid && LEVELIMAP4 (stream)) strcpy (tmp," UID");
    else tmp[0] = '\0';		/* initialize command */
				/* need envelope? */
    if (!*env || (*env)->incomplete) {
      strcat (tmp," ENVELOPE");	/* yes, get it and possible extra poop */
      if (!(flags & FT_NOHDRS) && LEVELIMAP4rev1 (stream)) {
	if (imap_extrahdrs) sprintf (tmp + strlen (tmp)," %s %s %s",
				     hdrheader,imap_extrahdrs,hdrtrailer);
	else sprintf (tmp + strlen (tmp)," %s %s",hdrheader,hdrtrailer);
      }
    }
				/* need body? */
    if (body && !*b && LEVELIMAP2bis (stream))
      strcat (tmp,LEVELIMAP4 (stream) ? " BODYSTRUCTURE" : " BODY");
    if (!elt->day) strcat (tmp," INTERNALDATE");
    if (!elt->rfc822_size) strcat (tmp," RFC822.SIZE");
    if (tmp[0]) {		/* anything to do? */
      tmp[0] = '(';		/* make into a list */
      strcat (tmp," FLAGS)");	/* always get current flags */
      aatt.text = (void *) tmp;	/* do the built command */
      if (!imap_OK (stream,reply = imap_send (stream,"FETCH",args))) {
				/* failed, probably RFC-1176 server */
	if (!LEVELIMAP4 (stream) && LEVELIMAP2bis (stream) && body && !*b){
	  aatt.text = (void *) "ALL";
	  if (imap_OK (stream,reply = imap_send (stream,"FETCH",args)))
	    LOCAL->imap2bis = NIL;/* doesn't have body capabilities */
	  else mm_log (reply->text,ERROR);
	}
	else mm_log (reply->text,ERROR);
      }
    }
  }
  if (body) *body = *b;		/* return the body */
  return *env;			/* return the envelope */
}

/* IMAP fetch message data
 * Accepts: MAIL stream
 *	    message number
 *	    section specifier
 *	    offset of first designated byte or 0 to start at beginning
 *	    maximum number of bytes or 0 for all bytes
 *	    lines to fetch if header
 *	    flags
 * Returns: T on success, NIL on failure
 */

long imap_msgdata (MAILSTREAM *stream,unsigned long msgno,char *section,
		   unsigned long first,unsigned long last,STRINGLIST *lines,
		   long flags)
{
  char *t,tmp[MAILTMPLEN],part[40];
  char *cmd = (LEVELIMAP4 (stream) && (flags & FT_UID)) ? "UID FETCH":"FETCH";
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[5],aseq,aatt,alns,acls;
  aseq.type = NUMBER; aseq.text = (void *) msgno;
  aatt.type = ATOM;		/* assume atomic attribute */
  alns.type = LIST; alns.text = (void *) lines;
  acls.type = BODYCLOSE; acls.text = (void *) part;
  args[0] = &aseq; args[1] = &aatt; args[2] = args[3] = args[4] = NIL;
  part[0] = '\0';		/* initially no partial specifier */
  if (!(flags & FT_PREFETCHTEXT) &&
      LEVELIMAP4rev1 (stream)) {/* easy if IMAP4rev1 server and no prefetch */
    aatt.type = (flags & FT_PEEK) ? BODYPEEK : BODYTEXT;
    if (lines) {		/* want specific header lines? */
      sprintf (tmp,"%s.FIELDS%s",section,(flags & FT_NOT) ? ".NOT" : "");
      aatt.text = (void *) tmp;
      args[2] = &alns; args[3] = &acls;
    }
    else {
      aatt.text = (void *) section;
      args[2] = &acls;
    }
    if (first || last) sprintf (part,"<%lu.%lu>",first,last ? last:-1);
  }
#if 0
  /* I don't think that this is a good idea.  If partial fetch isn't available,
   * the application is going to have to do a full fetch anyway if it wants to
   * get the data.  The mailgets call will indicate what happened.
   */
  else if (first || last) {	/* partial fetching is only on IMAP4rev1 */
    mm_notify (stream,"[NOTIMAP4REV1] Can't do partial fetch",WARN);
    return NIL;
  }
#endif

				/* BODY.PEEK[HEADER] becomes RFC822.HEADER */
  else if (!strcmp (section,"HEADER")) {
    if (flags & FT_PEEK) aatt.text = (void *)
      ((flags & FT_PREFETCHTEXT) ? "(RFC822.HEADER RFC822.TEXT)" :
       "RFC822.HEADER");
    else {
      mm_notify (stream,"[NOTIMAP4] Can't do non-peeking header fetch",WARN);
      return NIL;
    }
  }
				/* other peeking was introduced in RFC-1730 */
  else if ((flags & FT_PEEK) && !LEVEL1730 (stream)) {
    mm_notify (stream,"[NOTIMAP4] Can't do peeking fetch",WARN);
    return NIL;
  }
				/* BODY[TEXT] becomes RFC822.TEXT */
  else if (!strcmp (section,"TEXT")) aatt.text = (void *)
    ((flags & FT_PEEK) ? "RFC822.TEXT.PEEK" : "RFC822.TEXT");
				/* BODY[] becomes RFC822 */
  else if (!section[0]) aatt.text = (void *)
    ((flags & FT_PEEK) ? "RFC822.PEEK" : "RFC822");
				/* nested header */
  else if (t = strstr (section,".HEADER")) {
    if (!LEVEL1730 (stream)) {	/* this was introduced in RFC-1730 */
      mm_notify (stream,"[NOTIMAP4] Can't do nested header fetch",WARN);
      return NIL;
    }
    aatt.type = (flags & FT_PEEK) ? BODYPEEK : BODYTEXT;
    args[2] = &acls;		/* will need to close section */
    aatt.text = (void *) tmp;	/* convert .HEADER to .0 for RFC-1730 server */
    strncpy (tmp,section,t-section);
    strcpy (tmp+(t-section),".0");
  }
				/* extended nested text */
  else if (strstr (section,".MIME") || strstr (section,".TEXT")) {
    mm_notify (stream,"[NOTIMAP4REV1] Can't do extended body part fetch",WARN);
    return NIL;
  }
				/* nested message */
  else if (LEVELIMAP2bis (stream)) {
    aatt.type = (flags & FT_PEEK) ? BODYPEEK : BODYTEXT;
    args[2] = &acls;		/* will need to close section */
    aatt.text = (void *) section;
  }
  else {			/* ancient server */
    mm_notify (stream,"[NOTIMAP2BIS] Can't do body part fetch",WARN);
    return NIL;
  }
				/* send the fetch command */
  if (!imap_OK (stream,reply = imap_send (stream,cmd,args))) {
    mm_log (reply->text,ERROR);
    return NIL;			/* failure */
  }
  return T;
}

/* IMAP fetch UID
 * Accepts: MAIL stream
 *	    message number
 * Returns: UID
 */

unsigned long imap_uid (MAILSTREAM *stream,unsigned long msgno)
{
  MESSAGECACHE *elt;
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[3],aseq,aatt;
  char *s,seq[MAILTMPLEN];
  unsigned long i,j,k;
				/* IMAP2 didn't have UIDs */
  if (!LEVELIMAP4 (stream)) return msgno;
				/* do we know its UID yet? */
  if (!(elt = mail_elt (stream,msgno))->private.uid) {
    aseq.type = SEQUENCE; aseq.text = (void *) seq;
    aatt.type = ATOM; aatt.text = (void *) "UID";
    args[0] = &aseq; args[1] = &aatt; args[2] = NIL;
    sprintf (seq,"%lu",msgno);
    if (k = imap_uidlookahead) {/* build UID list */
      for (i = msgno + 1, s = seq; k && (i <= stream->nmsgs); i++)
	if (!mail_elt (stream,i)->private.uid) {
	  s += strlen (s);	/* find string end, see if nearing end */
	  if ((s - seq) > (MAILTMPLEN - 20)) break;
	  sprintf (s,",%lu",i);	/* append message */
	  for (j = i + 1, k--;	/* hunt for last message without a UID */
	       k && (j <= stream->nmsgs) && !mail_elt (stream,j)->private.uid;
	       j++, k--);
				/* if different, make a range */
	  if (i != --j) sprintf (s + strlen (s),":%lu",i = j);
	}
    }
				/* send "FETCH msgno UID" */
    if (!imap_OK (stream,reply = imap_send (stream,"FETCH",args)))
      mm_log (reply->text,ERROR);
  }
  return elt->private.uid;	/* return our UID now */
}

/* IMAP fetch message number from UID
 * Accepts: MAIL stream
 *	    UID
 * Returns: message number
 */

unsigned long imap_msgno (MAILSTREAM *stream,unsigned long uid)
{
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[3],aseq,aatt;
  char seq[MAILTMPLEN];
  unsigned long msgno;
				/* IMAP2 didn't have UIDs */
  if (!LEVELIMAP4 (stream)) return uid;
				/* have server hunt for UID */
  aseq.type = SEQUENCE; aseq.text = (void *) seq;
  aatt.type = ATOM; aatt.text = (void *) "UID";
  args[0] = &aseq; args[1] = &aatt; args[2] = NIL;
  sprintf (seq,"%lu",uid);
				/* send "UID FETCH uid UID" */
  if (!imap_OK (stream,reply = imap_send (stream,"UID FETCH",args)))
    mm_log (reply->text,ERROR);
  for (msgno = 1; msgno <= stream->nmsgs; msgno++)
    if (mail_elt (stream,msgno)->private.uid == uid) return msgno;
  return 0;			/* didn't find the UID anywhere */
}

/* IMAP modify flags
 * Accepts: MAIL stream
 *	    sequence
 *	    flag(s)
 *	    option flags
 */

void imap_flag (MAILSTREAM *stream,char *sequence,char *flag,long flags)
{
  char *cmd = (LEVELIMAP4 (stream) && (flags & ST_UID)) ? "UID STORE":"STORE";
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[4],aseq,ascm,aflg;
  aseq.type = SEQUENCE; aseq.text = (void *) sequence;
  ascm.type = ATOM; ascm.text = (void *)
    ((flags & ST_SET) ?
     ((LEVELIMAP4 (stream) && (flags & ST_SILENT)) ?
      "+Flags.silent" : "+Flags") :
     ((LEVELIMAP4 (stream) && (flags & ST_SILENT)) ?
      "-Flags.silent" : "-Flags"));
  aflg.type = FLAGS; aflg.text = (void *) flag;
  args[0] = &aseq; args[1] = &ascm; args[2] = &aflg; args[3] = NIL;
				/* send "STORE sequence +Flags flag" */
  if (!imap_OK (stream,reply = imap_send (stream,cmd,args)))
    mm_log (reply->text,ERROR);
}

/* IMAP search for messages
 * Accepts: MAIL stream
 *	    character set
 *	    search program
 *	    option flags
 */
void imap_search (MAILSTREAM *stream,char *charset,SEARCHPGM *pgm,long flags)
{
  char *cmd = (LEVELIMAP4 (stream) && (flags & SE_UID)) ?"UID SEARCH":"SEARCH";
  unsigned long i,j,k;
  char *s;
  IMAPPARSEDREPLY *reply;
  MESSAGECACHE *elt;
  IMAPARG *args[4],apgm,aseq,aatt,achs;
  args[1] = args[2] = args[3] = NIL;
  apgm.type = SEARCHPROGRAM; apgm.text = (void *) pgm;
  aseq.type = SEQUENCE;
  aatt.type = ATOM;
  achs.type = ASTRING;
  if (charset) {		/* optional charset argument requested */
    args[0] = &aatt; args[1] = &achs; args[2] = &apgm;
    aatt.text = (void *) "CHARSET";
    achs.text = (void *) charset;
  }
  else args[0] = &apgm;
				/* be sure that receiver understands */
  LOCAL->uidsearch = (flags & SE_UID) ? T : NIL;
  if (!LEVELIMAP4 (stream) &&	/* if old server but new functions... */
      (charset || LOCAL->uidsearch || pgm->msgno || pgm->uid || pgm->or ||
       pgm->not || pgm->header || pgm->larger || pgm->smaller ||
       pgm->sentbefore || pgm->senton || pgm->sentsince || pgm->draft ||
       pgm->undraft || pgm->return_path || pgm->sender || pgm->reply_to ||
       pgm->message_id || pgm->in_reply_to || pgm->newsgroups ||
       pgm->followup_to || pgm->references))
    mail_search_default (stream,charset,pgm,flags);
				/* do the SEARCH */
  else if (!imap_OK (stream,reply = imap_send (stream,cmd,args)))
    mm_log (reply->text,ERROR);
				/* can never pre-fetch with a short cache */
  else if ((k = imap_prefetch) && !(flags & (SE_NOPREFETCH | SE_UID)) &&
	   !stream->scache) {	/* only if prefetching permitted */
    s = LOCAL->tmp;		/* build sequence in temporary buffer */
    *s = '\0';			/* initially nothing */
				/* search through mailbox */
    for (i = 1; k && (i <= stream->nmsgs); ++i) 
				/* for searched messages with no envelope */
      if ((elt = mail_elt (stream,i)) && elt->searched &&
	  !mail_elt (stream,i)->private.msg.env) {
				/* prepend with comma if not first time */
	if (LOCAL->tmp[0]) *s++ = ',';
	sprintf (s,"%lu",j = i);/* output message number */
	s += strlen (s);	/* point at end of string */
	k--;			/* count one up */
				/* search for possible end of range */
	while (k && (i < stream->nmsgs) &&
	       (elt = mail_elt (stream,i+1))->searched &&
	       !elt->private.msg.env) i++,k--;
	if (i != j) {		/* if a range */
	  sprintf (s,":%lu",i);	/* output delimiter and end of range */
	  s += strlen (s);	/* point at end of string */
	}
      }
    if (LOCAL->tmp[0]) {	/* anything to pre-fetch? */
      /* pre-fetch envelopes for the first imap_prefetch number of messages */
      if (!imap_OK (stream,reply =
		    imap_fetch (stream,s = cpystr (LOCAL->tmp),FT_NEEDENV +
				((flags & SE_NOHDRS) ? FT_NOHDRS : NIL) +
				((flags & SE_NEEDBODY) ? FT_NEEDBODY : NIL))))
	mm_log (reply->text,ERROR);
      fs_give ((void **) &s);	/* flush copy of sequence */
    }
  }
}

/* IMAP sort messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    sort program
 *	    option flags
 * Returns: vector of sorted message sequences or NIL if error
 */

unsigned long *imap_sort (MAILSTREAM *stream,char *charset,SEARCHPGM *spg,
			  SORTPGM *pgm,long flags)
{
  unsigned long i,j,start,last;
  unsigned long *ret = NIL;
  pgm->nmsgs = 0;		/* start off with no messages */
				/* can use server-based sort? */
  if (LEVELSORT (stream) && !(flags & SO_NOSERVER)) {
    char *cmd = (flags & SE_UID) ? "UID SORT" : "SORT";
    IMAPARG *args[4],apgm,achs,aspg;
    IMAPPARSEDREPLY *reply;
    SEARCHSET *ss = NIL;
    apgm.type = SORTPROGRAM; apgm.text = (void *) pgm;
    achs.type = ASTRING; achs.text = (void *) (charset ? charset : "US-ASCII");
    aspg.type = SEARCHPROGRAM;
				/* did he provide a searchpgm? */
    if (!(aspg.text = (void *) spg)) {
      for (i = 1,start = last = 0; i <= stream->nmsgs; ++i)
	if (mail_elt (stream,i)->searched) {
	  if (ss) {		/* continuing a sequence */
	    if (i == last + 1) last = i;
	    else {		/* end of range */
	      if (last != start) ss->last = last;
	      (ss = ss->next = mail_newsearchset ())->first = i;
	      start = last = i;	/* begin a new range */
	    }
	  }
	  else {		/* first time, start new searchpgm */
	    (spg = mail_newsearchpgm ())->msgno = ss = mail_newsearchset ();
	    ss->first = start = last = i;
	  }
	}
				/* nothing to sort if no messages */
      if (!(aspg.text = (void *) spg)) return NIL;
				/* else install last sequence */
      if (last != start) ss->last = last;
    }
    args[0] = &apgm; args[1] = &achs; args[2] = &aspg; args[3] = NIL;
    if (imap_OK (stream,reply = imap_send (stream,cmd,args))) {
      pgm->nmsgs = LOCAL->sortsize;
      ret = LOCAL->sortdata;
      LOCAL->sortdata = NIL;	/* mail program is responsible for flushing */
    }
    else mm_log (reply->text,ERROR);
				/* free any temporary searchpgm */
    if (ss) mail_free_searchpgm (&spg);
  }

				/* not much can do if short caching */
  else if (stream->scache) ret = mail_sort_msgs (stream,charset,spg,pgm,flags);
  else {			/* try to be a bit more clever */
    char *s,*t;
    unsigned long len;
    MESSAGECACHE *elt;
    SORTCACHE **sc;
    SORTPGM *sp;
    long ftflags = 0;
				/* see if need envelopes */
    for (sp = pgm; sp && !ftflags; sp = sp->next) switch (sp->function) {
    case SORTDATE: case SORTFROM: case SORTSUBJECT: case SORTTO: case SORTCC:
      ftflags = FT_NEEDENV + ((flags & SE_NOHDRS) ? FT_NOHDRS : NIL);
    }
    if (spg) {			/* only if a search needs to be done */
      int silent = stream->silent;
      stream->silent = T;	/* don't pass up mm_searched() events */
				/* search for messages */
      mail_search_full (stream,charset,spg,NIL);
      stream->silent = silent;	/* restore silence state */
    }
				/* initialize progress counters */
    pgm->nmsgs = pgm->progress.cached = 0;
				/* pass 1: count messages to sort */
    for (i = 1,len = start = last = 0,s = t = NIL; i <= stream->nmsgs; ++i)
      if ((elt = mail_elt (stream,i))->searched) {
	pgm->nmsgs++;
	if (ftflags ? !elt->private.msg.env : !elt->day) {
	  if (s) {		/* continuing a sequence */
	    if (i == last + 1) last = i;
	    else {		/* end of range */
	      if (last != start) sprintf (t,":%lu,%lu",last,i);
	      else sprintf (t,",%lu",i);
	      start = last = i;	/* begin a new range */
	      if ((j = ((t += strlen (t)) - s)) > (MAILTMPLEN - 20)) {
		fs_resize ((void **) s,len += MAILTMPLEN);
		t = s + j;	/* relocate current pointer */
	      }
	    }
	  }
	  else {		/* first time, start new buffer */
	    s = (char *) fs_get (len = MAILTMPLEN);
	    sprintf (s,"%lu",start = last = i);
	    t = s + strlen (s);	/* end of buffer */
	  }
	}
      }
				/* last sequence */
    if (last != start) sprintf (t,":%lu",last);
    if (s) {			/* load cache for all messages being sorted */
      imap_fetch (stream,s,ftflags);
      fs_give ((void **) &s);
    }
    if (pgm->nmsgs) {		/* pass 2: sort cache */
      sortresults_t sr = (sortresults_t)
	mail_parameters (NIL,GET_SORTRESULTS,NIL);
      sc = mail_sort_loadcache (stream,pgm);
				/* pass 3: sort messages */
      if (!pgm->abort) ret = mail_sort_cache (stream,pgm,sc,flags);
      fs_give ((void **) &sc);	/* don't need sort vector any more */
				/* also return via callback if requested */
      if (sr) (*sr) (stream,ret,pgm->nmsgs);
    }
  }
  return ret;
}

/* IMAP thread messages
 * Accepts: mail stream
 *	    thread type
 *	    character set
 *	    search program
 *	    option flags
 * Returns: thread node tree
 */

THREADNODE *imap_thread (MAILSTREAM *stream,char *type,char *charset,
			 SEARCHPGM *spg,long flags)
{
  THREADER *thr;
				/* does server have this threader type? */
  for (thr = LOCAL->threader; thr;thr = thr->next)
    if (!compare_cstring (thr->name,type)) {
      unsigned long i,start,last;
      char *cmd = (flags & SE_UID) ? "UID THREAD" : "THREAD";
      IMAPARG *args[4],apgm,achs,aspg;
      IMAPPARSEDREPLY *reply;
      THREADNODE *ret = NIL;
      SEARCHSET *ss = NIL;
      apgm.type = ATOM; apgm.text = (void *) thr->name;
      achs.type = ASTRING;
      achs.text = (void *) (charset ? charset : "US-ASCII");
      aspg.type = SEARCHPROGRAM;
				/* did he provide a searchpgm? */
      if (!(aspg.text = (void *) spg)) {
	for (i = 1,start = last = 0; i <= stream->nmsgs; ++i)
	  if (mail_elt (stream,i)->searched) {
	    if (ss) {		/* continuing a sequence */
	      if (i == last + 1) last = i;
	      else {		/* end of range */
		if (last != start) ss->last = last;
		(ss = ss->next = mail_newsearchset ())->first = i;
		start = last =i;/* begin a new range */
	      }
	    }
	    else {		/* first time, start new searchpgm */
	      (spg = mail_newsearchpgm ())->msgno = ss = mail_newsearchset ();
	      ss->first = start = last = i;
	    }
	  }
				/* nothing to sort if no messages */
	if (!(aspg.text = (void *) spg)) return NIL;
				/* else install last sequence */
	if (last != start) ss->last = last;
      }
      args[0] = &apgm; args[1] = &achs; args[2] = &aspg; args[3] = NIL;
      if (imap_OK (stream,reply = imap_send (stream,cmd,args))) {
	ret = LOCAL->threaddata;
	LOCAL->threaddata = NIL;/* mail program is responsible for flushing */
      }
      else mm_log (reply->text,ERROR);
				/* free any temporary searchpgm */
      if (ss) mail_free_searchpgm (&spg);
      return ret;
    }
  return mail_thread_msgs (stream,type,charset,spg,flags | SO_NOSERVER,
			   imap_sort);
}

/* IMAP ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream still alive, else NIL
 */

long imap_ping (MAILSTREAM *stream)
{
  return (LOCAL->netstream &&	/* send "NOOP" */
	  imap_OK (stream,imap_send (stream,"NOOP",NIL))) ? T : NIL;
}


/* IMAP check mailbox
 * Accepts: MAIL stream
 */

void imap_check (MAILSTREAM *stream)
{
				/* send "CHECK" */
  IMAPPARSEDREPLY *reply = imap_send (stream,"CHECK",NIL);
  mm_log (reply->text,imap_OK (stream,reply) ? (long) NIL : ERROR);
}


/* IMAP expunge mailbox
 * Accepts: MAIL stream
 */

void imap_expunge (MAILSTREAM *stream)
{
				/* send "EXPUNGE" */
  IMAPPARSEDREPLY *reply = imap_send (stream,"EXPUNGE",NIL);
  mm_log (reply->text,imap_OK (stream,reply) ? (long) NIL : ERROR);
}

/* IMAP copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    option flags
 * Returns: T if successful else NIL
 */

long imap_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long flags)
{
  char *cmd = (LEVELIMAP4 (stream) && (flags & CP_UID)) ? "UID COPY" : "COPY";
  char *s;
  IMAPPARSEDREPLY *reply;
  IMAPARG *args[3],aseq,ambx;
  imapreferral_t ir =
    (imapreferral_t) mail_parameters (stream,GET_IMAPREFERRAL,NIL);
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  aseq.type = SEQUENCE; aseq.text = (void *) sequence;
  ambx.type = ASTRING; ambx.text = (void *) mailbox;
  args[0] = &aseq; args[1] = &ambx; args[2] = NIL;
				/* send "COPY sequence mailbox" */
  if (!imap_OK (stream,reply = imap_send (stream,cmd,args))) {
    if (ir && pc && LOCAL->referral && mail_sequence (stream,sequence) &&
	(s = (*ir) (stream,LOCAL->referral,REFCOPY)))
      return (*pc) (stream,sequence,s,flags);
    mm_log (reply->text,ERROR);
    return NIL;
  }
				/* delete the messages if the user said to */
  if (flags & CP_MOVE) imap_flag (stream,sequence,"\\Deleted",
				  ST_SET + ((flags & CP_UID) ? ST_UID : NIL));
  return T;
}

/* IMAP mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long imap_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  MAILSTREAM *st = stream;
  IMAPARG *args[3],ambx,amap;
  IMAPPARSEDREPLY *reply;
  APPENDDATA map;
  STRING *message;
  char *flags,*date,tmp[MAILTMPLEN];
  long ret = NIL;
  imapreferral_t ir =
    (imapreferral_t) mail_parameters (stream,GET_IMAPREFERRAL,NIL);
				/* barf if bad mailbox */
  if (mail_valid_net (mailbox,&imapdriver,NIL,tmp)) {
				/* create a stream if given one no good */
    if (!(stream && LOCAL && LOCAL->netstream) &&
	!(stream =  mail_open (NIL,mailbox,OP_HALFOPEN|OP_SILENT)))
      mm_log ("Can't access server for append",ERROR);
				/* use multi-append? */
    else if (LEVELMULTIAPPEND (stream)) {
      ambx.type = ASTRING; ambx.text = (void *) tmp;
      amap.type = MULTIAPPEND; amap.text = (void *) &map;
      map.af = af; map.data = data;
      args[0] = &ambx; args[1] = &amap; args[2] = NIL;
				/* success if OK */
      if (imap_OK (stream,reply = imap_send (stream,"APPEND",args)))
	ret = LONGT;
				/* failed, is there a referral? */
      else if (ir && LOCAL->referral &&
	       (mailbox = (*ir) (stream,LOCAL->referral,REFAPPEND)))
	ret = imap_append (NIL,mailbox,af,data);
      else mm_log (reply->text,ERROR);
    }
				/* do multiple single appends */
    else while ((ret = ((*af) (stream,data,&flags,&date,&message))) &&
		message &&
		(ret = imap_append_single (stream,tmp,flags,date,message,ir)));
				/* toss out temporary stream */
    if (st != stream) mail_close (stream);
  }
  return ret;			/* return */
}

/* IMAP append single message
 * Accepts: mail stream
 *	    destination mailbox
 *	    initial flags
 *	    internal date
 *	    stringstruct of message to append
 *	    referral callback
 * Returns: T on success, NIL on failure
 */

long imap_append_single (MAILSTREAM *stream,char *mailbox,char *flags,
			 char *date,STRING *message,imapreferral_t ir)
{
  MESSAGECACHE elt;
  IMAPARG *args[5],ambx,aflg,adat,amsg;
  IMAPPARSEDREPLY *reply;
  char tmp[MAILTMPLEN],datetmp[MAILTMPLEN];
  int i;
  long ret = NIL;
  ambx.type = ASTRING; ambx.text = (void *) mailbox;
  args[i = 0] = &ambx;
  if (flags) {
    aflg.type = FLAGS; aflg.text = (void *) flags;
    args[++i] = &aflg;
  }
  if (date) {		/* ensure date in INTERNALDATE format */
    if (!mail_parse_date (&elt,date)) {
      sprintf (tmp,"Bad date in append: %.80s",date);
      mm_log (tmp,ERROR);
      return NIL;
    }
    adat.type = ASTRING;
    adat.text = (void *) (date = mail_date (datetmp,&elt));
    args[++i] = &adat;
  }
  amsg.type = LITERAL; amsg.text = (void *) message;
  args[++i] = &amsg;
  args[++i] = NIL;

  if (!strcmp ((reply = imap_send (stream,"APPEND",args))->key,"BAD") &&
      (flags || date)) {	/* full form and got a BAD? */
				/* yes, retry with old IMAP2bis form */
    args[1] = &amsg; args[2] = NIL;
    reply = imap_send (stream,"APPEND",args);
  }
				/* success if OK */
  if (imap_OK (stream,reply)) ret = LONGT;
				/* failed, is there a referral? */
  else if (!(ir && LOCAL->referral &&
	     (mailbox = (*ir) (stream,LOCAL->referral,REFAPPEND))))
    mm_log (reply->text,ERROR);	/* no referral, babble the error */
  else if (stream = mail_open (NIL,mailbox,OP_HALFOPEN|OP_SILENT)) {
				/* try again with referral */
    ret = imap_append_single (stream,mailbox,flags,date,message,ir);
    mail_close (stream);	/* flush temporary stream */
  }
  else {
    sprintf (tmp,"Can't access referral server: %.80s",mailbox);
    mm_log (tmp,ERROR);
  }
  return ret;
}

/* IMAP garbage collect stream
 * Accepts: Mail stream
 *	    garbage collection flags
 */

void imap_gc (MAILSTREAM *stream,long gcflags)
{
  unsigned long i;
  MESSAGECACHE *elt;
  mailcache_t mc = (mailcache_t) mail_parameters (NIL,GET_CACHE,NIL);
				/* make sure the cache is large enough */
  (*mc) (stream,stream->nmsgs,CH_SIZE);
  if (gcflags & GC_TEXTS) {	/* garbage collect texts? */
    if (!stream->scache) for (i = 1; i <= stream->nmsgs; ++i)
      if (elt = (MESSAGECACHE *) (*mc) (stream,i,CH_ELT))
	imap_gc_body (elt->private.msg.body);
    imap_gc_body (stream->body);
  }
				/* gc cache if requested and unlocked */
  if (gcflags & GC_ELT) for (i = 1; i <= stream->nmsgs; ++i)
    if ((elt = (MESSAGECACHE *) (*mc) (stream,i,CH_ELT)) &&
	(elt->lockcount == 1)) (*mc) (stream,i,CH_FREE);
}

/* IMAP garbage collect body texts
 * Accepts: body to GC
 */

void imap_gc_body (BODY *body)
{
  PART *part;
  if (body) {			/* have a body? */
    if (body->mime.text.data)	/* flush MIME data */
      fs_give ((void **) &body->mime.text.data);
				/* flush text contents */
    if (body->contents.text.data)
      fs_give ((void **) &body->contents.text.data);
    body->mime.text.size = body->contents.text.size = 0;
				/* multipart? */
    if (body->type == TYPEMULTIPART)
      for (part = body->nested.part; part; part = part->next)
	imap_gc_body (&part->body);
				/* MESSAGE/RFC822? */
    else if ((body->type == TYPEMESSAGE) && !strcmp (body->subtype,"RFC822")) {
      imap_gc_body (body->nested.msg->body);
      if (body->nested.msg->full.text.data)
	fs_give ((void **) &body->nested.msg->full.text.data);
      if (body->nested.msg->header.text.data)
	fs_give ((void **) &body->nested.msg->header.text.data);
      if (body->nested.msg->text.text.data)
	fs_give ((void **) &body->nested.msg->text.text.data);
      body->nested.msg->full.text.size = body->nested.msg->header.text.size =
	body->nested.msg->text.text.size = 0;
    }
  }
}

/* IMAP get capabilities
 * Accepts: mail stream
 */

void imap_capability (MAILSTREAM *stream)
{
  THREADER *thr,*t;
				/* zap all old capabilities */
  memset (&LOCAL->cap,0,sizeof (LOCAL->cap));
  if (thr = LOCAL->threader) {	/* and threaders */
    while (t = thr) {
      fs_give ((void **) &t->name);
      thr = t->next;
      fs_give ((void **) &t);
    }
    LOCAL->threader = NIL;	/* no more threaders */
  }
				/* send new capabilities */
  imap_send (stream,"CAPABILITY",NIL);
}

/* IMAP set ACL
 * Accepts: mail stream
 *	    mailbox name
 *	    authentication identifer
 *	    new access rights
 * Returns: T on success, NIL on failure
 */

long imap_setacl (MAILSTREAM *stream,char *mailbox,char *id,char *rights)
{
  IMAPARG *args[4],ambx,aid,art;
  ambx.type = aid.type = art.type = ASTRING;
  ambx.text = (void *) mailbox; aid.text = (void *) id;
  art.text = (void *) rights;
  args[0] = &ambx; args[1] = &aid; args[2] = &art; args[3] = NIL;
  return imap_acl_work (stream,"SETACL",args);
}


/* IMAP delete ACL
 * Accepts: mail stream
 *	    mailbox name
 *	    authentication identifer
 * Returns: T on success, NIL on failure
 */

long imap_deleteacl (MAILSTREAM *stream,char *mailbox,char *id)
{
  IMAPARG *args[3],ambx,aid;
  ambx.type = aid.type = ASTRING;
  ambx.text = (void *) mailbox; aid.text = (void *) id;
  args[0] = &ambx; args[1] = &aid; args[2] = NIL;
  return imap_acl_work (stream,"DELETEACL",args);
}


/* IMAP get ACL
 * Accepts: mail stream
 *	    mailbox name
 * Returns: T on success with data returned via callback, NIL on failure
 */

long imap_getacl (MAILSTREAM *stream,char *mailbox)
{
  IMAPARG *args[2],ambx;
  ambx.type = ASTRING; ambx.text = (void *) mailbox;
    args[0] = &ambx; args[1] = NIL;
  return imap_acl_work (stream,"GETACL",args);
}

/* IMAP list rights
 * Accepts: mail stream
 *	    mailbox name
 *	    authentication identifer
 * Returns: T on success with data returned via callback, NIL on failure
 */

long imap_listrights (MAILSTREAM *stream,char *mailbox,char *id)
{
  IMAPARG *args[3],ambx,aid;
  ambx.type = aid.type = ASTRING;
  ambx.text = (void *) mailbox; aid.text = (void *) id;
  args[0] = &ambx; args[1] = &aid; args[2] = NIL;
  return imap_acl_work (stream,"LISTRIGHTS",args);
}


/* IMAP my rights
 * Accepts: mail stream
 *	    mailbox name
 * Returns: T on success with data returned via callback, NIL on failure
 */

long imap_myrights (MAILSTREAM *stream,char *mailbox)
{
  IMAPARG *args[2],ambx;
  ambx.type = ASTRING; ambx.text = (void *) mailbox;
  args[0] = &ambx; args[1] = NIL;
  return imap_acl_work (stream,"MYRIGHTS",args);
}


/* IMAP ACL worker routine
 * Accepts: mail stream
 *	    command
 *	    command arguments
 * Returns: T on success, NIL on failure
 */

long imap_acl_work (MAILSTREAM *stream,char *command,IMAPARG *args[])
{
  long ret = NIL;
  if (LEVELACL (stream)) {	/* send command */
    IMAPPARSEDREPLY *reply;
    if (imap_OK (stream,reply = imap_send (stream,command,args)))
      ret = LONGT;
    else mm_log (reply->text,ERROR);
  }
  else mm_log ("ACL not available on this IMAP server",ERROR);
  return ret;
}

/* IMAP set quota
 * Accepts: mail stream
 *	    quota root name
 *	    resource limit list as a stringlist
 * Returns: T on success with data returned via callback, NIL on failure
 */

long imap_setquota (MAILSTREAM *stream,char *qroot,STRINGLIST *limits)
{
  long ret = NIL;
  if (LEVELQUOTA (stream)) {	/* send "SETQUOTA" */
    IMAPPARSEDREPLY *reply;
    IMAPARG *args[3],aqrt,alim;
    aqrt.type = ASTRING; aqrt.text = (void *) qroot;
    alim.type = SNLIST; alim.text = (void *) limits;
    args[0] = &aqrt; args[1] = &alim; args[2] = NIL;
    if (imap_OK (stream,reply = imap_send (stream,"SETQUOTA",args)))
      ret = LONGT;
    else mm_log (reply->text,ERROR);
  }
  else mm_log ("Quota not available on this IMAP server",ERROR);
  return ret;
}

/* IMAP get quota
 * Accepts: mail stream
 *	    quota root name
 * Returns: T on success with data returned via callback, NIL on failure
 */

long imap_getquota (MAILSTREAM *stream,char *qroot)
{
  long ret = NIL;
  if (LEVELQUOTA (stream)) {	/* send "GETQUOTA" */
    IMAPPARSEDREPLY *reply;
    IMAPARG *args[2],aqrt;
    aqrt.type = ASTRING; aqrt.text = (void *) qroot;
    args[0] = &aqrt; args[1] = NIL;
    if (imap_OK (stream,reply = imap_send (stream,"GETQUOTA",args)))
      ret = LONGT;
    else mm_log (reply->text,ERROR);
  }
  else mm_log ("Quota not available on this IMAP server",ERROR);
  return ret;
}


/* IMAP get quota root
 * Accepts: mail stream
 *	    mailbox name
 * Returns: T on success with data returned via callback, NIL on failure
 */

long imap_getquotaroot (MAILSTREAM *stream,char *mailbox)
{
  long ret = NIL;
  if (LEVELQUOTA (stream)) {	/* send "GETQUOTAROOT" */
    IMAPPARSEDREPLY *reply;
    IMAPARG *args[2],ambx;
    ambx.type = ASTRING; ambx.text = (void *) mailbox;
    args[0] = &ambx; args[1] = NIL;
    if (imap_OK (stream,reply = imap_send (stream,"GETQUOTAROOT",args)))
      ret = LONGT;
    else mm_log (reply->text,ERROR);
  }
  else mm_log ("Quota not available on this IMAP server",ERROR);
  return ret;
}

/* Internal routines */


/* IMAP send command
 * Accepts: MAIL stream
 *	    command
 *	    argument list
 * Returns: parsed reply
 */

#define MAXSEQUENCE 1000

IMAPPARSEDREPLY *imap_send (MAILSTREAM *stream,char *cmd,IMAPARG *args[])
{
  IMAPPARSEDREPLY *reply;
  IMAPARG *arg,**arglst;
  SORTPGM *spg;
  STRINGLIST *list;
  SIZEDTEXT st;
  APPENDDATA *map;
  STRING *message;
  char c,*s,*t,*date,tag[10];
  				/* gensym a new tag */
  sprintf (tag,"%08lx",stream->gensym++);
  if (!LOCAL->netstream) return imap_fake (stream,tag,
					   "[CLOSED] No-op dead stream");
  mail_lock (stream);		/* lock up the stream */
				/* ignored referral from previous command */
  if (LOCAL->referral) fs_give ((void **) &LOCAL->referral);
  sprintf (LOCAL->tmp,"%s ",tag);
  for (t = cmd, s = LOCAL->tmp + 9; *t; *s++ = *t++);
  if (arglst = args) while (arg = *arglst++) {
    *s++ = ' ';			/* delimit argument with space */
    switch (arg->type) {
    case ATOM:			/* atom */
      for (t = (char *) arg->text; *t; *s++ = *t++);
      break;
    case NUMBER:		/* number */
      sprintf (s,"%lu",(unsigned long) arg->text);
      s += strlen (s);
      break;
    case FLAGS:			/* flag list as a single string */
      if (*(t = (char *) arg->text) != '(') {
	*s++ = '(';		/* wrap parens around string */
	while (*t) *s++ = *t++;
	*s++ = ')';		/* wrap parens around string */
      }
      else while (*t) *s++ = *t++;
      break;
    case ASTRING:		/* atom or string, must be literal? */
      st.size = strlen ((char *) (st.data = (unsigned char *) arg->text));
      if (reply = imap_send_astring (stream,tag,&s,&st,NIL)) return reply;
      break;
    case LITERAL:		/* literal, as a stringstruct */
      if (reply = imap_send_literal (stream,tag,&s,arg->text)) return reply;
      break;

    case LIST:			/* list of strings */
      list = (STRINGLIST *) arg->text;
      c = '(';			/* open paren */
      do {			/* for each list item */
	*s++ = c;		/* write prefix character */
	if (reply = imap_send_astring (stream,tag,&s,&list->text,NIL))
	  return reply;
	c = ' ';		/* prefix character for subsequent strings */
      }
      while (list = list->next);
      *s++ = ')';		/* close list */
      break;
    case SEARCHPROGRAM:		/* search program */
      if (reply = imap_send_spgm (stream,tag,&s,arg->text)) return reply;
      break;
    case SORTPROGRAM:		/* search program */
      c = '(';			/* open paren */
      for (spg = (SORTPGM *) arg->text; spg; spg = spg->next) {
	*s++ = c;		/* write prefix */
	if (spg->reverse) for (t = "REVERSE "; *t; *s++ = *t++);
	switch (spg->function) {
	case SORTDATE:
	  for (t = "DATE"; *t; *s++ = *t++);
	  break;
	case SORTARRIVAL:
	  for (t = "ARRIVAL"; *t; *s++ = *t++);
	  break;
	case SORTFROM:
	  for (t = "FROM"; *t; *s++ = *t++);
	  break;
	case SORTSUBJECT:
	  for (t = "SUBJECT"; *t; *s++ = *t++);
	  break;
	case SORTTO:
	  for (t = "TO"; *t; *s++ = *t++);
	  break;
	case SORTCC:
	  for (t = "CC"; *t; *s++ = *t++);
	  break;
	case SORTSIZE:
	  for (t = "SIZE"; *t; *s++ = *t++);
	  break;
	default:
	  fatal ("Unknown sort program function in imap_send()!");
	}
	c = ' ';		/* prefix character for subsequent items */
      }
      *s++ = ')';		/* close list */
      break;

    case BODYTEXT:		/* body section */
      for (t = "BODY["; *t; *s++ = *t++);
      for (t = (char *) arg->text; *t; *s++ = *t++);
      break;
    case BODYPEEK:		/* body section */
      for (t = "BODY.PEEK["; *t; *s++ = *t++);
      for (t = (char *) arg->text; *t; *s++ = *t++);
      break;
    case BODYCLOSE:		/* close bracket and possible length */
      s[-1] = ']';		/* no leading space */
      for (t = (char *) arg->text; *t; *s++ = *t++);
      break;
    case SEQUENCE:		/* sequence */
      while (t = (strlen ((char *) arg->text) > (size_t) MAXSEQUENCE) ?
	     strchr (((char *) arg->text) + MAXSEQUENCE - 20,',') : NIL) {
	*t = '\0';		/* tie off sequence */
	mail_unlock (stream);	/* unlock stream, recurse to do part */
	imap_send (stream,cmd,args);
	mail_lock (stream);	/* lock up stream again */
				/* rewrite the tag */
	memcpy (LOCAL->tmp,tag,8);
	*t++ = ',';		/* restore the comma */
	arg->text = (void *) t;	/* now do rest of data */
      }
				/* falls through */
    case LISTMAILBOX:		/* astring with wildcards */
      st.size = strlen ((char *) (st.data = (unsigned char *) arg->text));
      if (reply = imap_send_astring (stream,tag,&s,&st,T)) return reply;
      break;

    case MULTIAPPEND:
      map = (APPENDDATA *) arg->text;
      if (!(*map->af) (stream,map->data,&t,&date,&message) || !message) {
	STRING es;
	INIT (&es,mail_string,"",0);
	return imap_send_literal (stream,tag,&s,&es);
      }
      do {			/* make sure date valid if given */
	char datetmp[MAILTMPLEN];
	MESSAGECACHE elt;
	STRING es;
	if (!date || mail_parse_date (&elt,date)) {
	  if (t) {		/* flags given? */
	    if (*t != '(') {
	      *s++ = '(';	/* wrap parens around string */
	      while (*t) *s++ = *t++;
	      *s++ = ')';	/* wrap parens around string */
	    }
	    else while (*t) *s++ = *t++;
	    *s++ = ' ';		/* delimit with space */
	  }
	  if (date) {		/* date given? */
	    st.size = strlen ((char *) (st.data = (unsigned char *)
					mail_date (datetmp,&elt)));
	    if (reply = imap_send_astring (stream,tag,&s,&st,NIL))
	      return reply;
	    *s++ = ' ';		/* delimit with space */
	  }
	  if (reply = imap_send_literal(stream,tag,&s,message)) return reply;
				/* get next message */
	  if ((*map->af) (stream,map->data,&t,&date,&message)) {
				/* have a message, delete next in command */
	    if (message) *s++ = ' ';
	    continue;		/* loop back for next message */
	  }
	}
				/* bad date or need to abort */
	INIT (&es,mail_string,"",0);
	return imap_send_literal (stream,tag,&s,&es);
	break;			/* exit the loop */
      } while (message);
      break;

    case SNLIST:		/* list of string/number pairs */
      list = (STRINGLIST *) arg->text;
      c = '(';			/* open paren */
      do {			/* for each list item */
	*s++ = c;		/* write prefix character */
	if (list) {		/* sigh, QUOTA has bizarre syntax! */
	  for (t = (char *) list->text.data; *t; *s++ = *t++);
	  sprintf (s," %lu",list->text.size);
	  s += strlen (s);
	  c = ' ';		/* prefix character for subsequent strings */
	}
      }
      while (list = list->next);
      *s++ = ')';		/* close list */
      break;
    default:
      fatal ("Unknown argument type in imap_send()!");
    }
  }
				/* send the command */
  reply = imap_sout (stream,tag,LOCAL->tmp,&s);
  mail_unlock (stream);		/* unlock stream */
  return reply;
}

/* IMAP send atom-string
 * Accepts: MAIL stream
 *	    reply tag
 *	    pointer to current position pointer of output bigbuf
 *	    atom-string to output
 *	    flag if list_wildcards allowed
 * Returns: error reply or NIL if success
 */

IMAPPARSEDREPLY *imap_send_astring (MAILSTREAM *stream,char *tag,char **s,
				    SIZEDTEXT *as,long wildok)
{
  unsigned long j;
  char c;
  STRING st;
				/* default to not quoted unless empty */
  int qflag = as->size ? NIL : T;
  for (j = 0; j < as->size; j++) switch (c = as->data[j]) {
  default:			/* all other characters */
    if (!(c & 0x80)) {		/* must not be 8bit */
      if (c <= ' ') qflag = T;	/* must quote if a CTL */
      break;
    }
  case '\0':			/* not a CHAR */
  case '\012': case '\015':	/* not a TEXT-CHAR */
  case '"': case '\\':		/* quoted-specials (IMAP2 required this) */
    INIT (&st,mail_string,(void *) as->data,as->size);
    return imap_send_literal (stream,tag,s,&st);
  case '*': case '%':		/* list_wildcards */
    if (wildok) break;		/* allowed if doing the wild thing */
				/* atom_specials */
  case '(': case ')': case '{': case ' ': case 0x7f:
#if 0
  case '"': case '\\':		/* quoted-specials (could work in IMAP4) */
#endif
    qflag = T;			/* must use quoted string format */
    break;
  }
  if (qflag) *(*s)++ = '"';	/* write open quote */
  for (j = 0; j < as->size; j++) *(*s)++ = as->data[j];
  if (qflag) *(*s)++ = '"';	/* write close quote */
  return NIL;
}

/* IMAP send literal
 * Accepts: MAIL stream
 *	    reply tag
 *	    pointer to current position pointer of output bigbuf
 *	    literal to output as stringstruct
 * Returns: error reply or NIL if success
 */

IMAPPARSEDREPLY *imap_send_literal (MAILSTREAM *stream,char *tag,char **s,
				    STRING *st)
{
  IMAPPARSEDREPLY *reply;
  unsigned long i = SIZE (st);
  sprintf (*s,"{%lu}",i);	/* write literal count */
  *s += strlen (*s);		/* size of literal count */
				/* send the command */
  reply = imap_sout (stream,tag,LOCAL->tmp,s);
  if (strcmp (reply->tag,"+")) {/* prompt for more data? */
    mail_unlock (stream);	/* no, give up */
    return reply;
  }
  while (i) {			/* dump the text */
    if (!net_sout (LOCAL->netstream,st->curpos,st->cursize)) {
      mail_unlock (stream);
      return imap_fake (stream,tag,"[CLOSED] IMAP connection broken (data)");
    }
    i -= st->cursize;		/* note that we wrote out this much */
    st->curpos += (st->cursize - 1);
    st->cursize = 0;
    (*st->dtb->next) (st);	/* advance to next buffer's worth */
  }
  return NIL;			/* success */
}

/* IMAP send search program
 * Accepts: MAIL stream
 *	    reply tag
 *	    pointer to current position pointer of output bigbuf
 *	    search program to output
 * Returns: error reply or NIL if success
 */

IMAPPARSEDREPLY *imap_send_spgm (MAILSTREAM *stream,char *tag,char **s,
				 SEARCHPGM *pgm)
{
  IMAPPARSEDREPLY *reply;
  SEARCHHEADER *hdr;
  SEARCHOR *pgo;
  SEARCHPGMLIST *pgl;
  char *t = "ALL";
  while (*t) *(*s)++ = *t++;	/* default initial text */
				/* message sequences */
  if (pgm->msgno) imap_send_sset (s,pgm->msgno,NIL);
  if (pgm->uid) imap_send_sset (s,pgm->uid," UID");
				/* message sizes */
  if (pgm->larger) {
    sprintf (*s," LARGER %lu",pgm->larger);
    *s += strlen (*s);
  }
  if (pgm->smaller) {
    sprintf (*s," SMALLER %lu",pgm->smaller);
    *s += strlen (*s);
  }
				/* message flags */
  if (pgm->answered) for (t = " ANSWERED"; *t; *(*s)++ = *t++);
  if (pgm->unanswered) for (t =" UNANSWERED"; *t; *(*s)++ = *t++);
  if (pgm->deleted) for (t =" DELETED"; *t; *(*s)++ = *t++);
  if (pgm->undeleted) for (t =" UNDELETED"; *t; *(*s)++ = *t++);
  if (pgm->draft) for (t =" DRAFT"; *t; *(*s)++ = *t++);
  if (pgm->undraft) for (t =" UNDRAFT"; *t; *(*s)++ = *t++);
  if (pgm->flagged) for (t =" FLAGGED"; *t; *(*s)++ = *t++);
  if (pgm->unflagged) for (t =" UNFLAGGED"; *t; *(*s)++ = *t++);
  if (pgm->recent) for (t =" RECENT"; *t; *(*s)++ = *t++);
  if (pgm->old) for (t =" OLD"; *t; *(*s)++ = *t++);
  if (pgm->seen) for (t =" SEEN"; *t; *(*s)++ = *t++);
  if (pgm->unseen) for (t =" UNSEEN"; *t; *(*s)++ = *t++);
  if ((pgm->keyword &&		/* keywords */
       (reply = imap_send_slist (stream,tag,s,"KEYWORD",pgm->keyword))) ||
      (pgm->unkeyword &&
       (reply = imap_send_slist (stream,tag,s,"UNKEYWORD",pgm->unkeyword))))
    return reply;

				/* sent date ranges */
  if (pgm->sentbefore) imap_send_sdate (s,"SENTBEFORE",pgm->sentbefore);
  if (pgm->senton) imap_send_sdate (s,"SENTON",pgm->senton);
  if (pgm->sentsince) imap_send_sdate (s,"SENTSINCE",pgm->sentsince);
				/* internal date ranges */
  if (pgm->before) imap_send_sdate (s,"BEFORE",pgm->before);
  if (pgm->on) imap_send_sdate (s,"ON",pgm->on);
  if (pgm->since) imap_send_sdate (s,"SINCE",pgm->since);
				/* search texts */
  if ((pgm->bcc && (reply = imap_send_slist (stream,tag,s,"BCC",pgm->bcc))) ||
      (pgm->cc && (reply = imap_send_slist (stream,tag,s,"CC",pgm->cc))) ||
      (pgm->from && (reply = imap_send_slist(stream,tag,s,"FROM",pgm->from)))||
      (pgm->to && (reply = imap_send_slist (stream,tag,s,"TO",pgm->to))))
    return reply;
  if ((pgm->subject &&
       (reply = imap_send_slist (stream,tag,s,"SUBJECT",pgm->subject))) ||
      (pgm->body && (reply = imap_send_slist(stream,tag,s,"BODY",pgm->body)))||
      (pgm->text && (reply = imap_send_slist (stream,tag,s,"TEXT",pgm->text))))
    return reply;

  /* Note that these criteria are not supported by IMAP and have to be
     emulated */
  if ((pgm->return_path &&
       (reply = imap_send_slist (stream,tag,s,"HEADER Return-Path",
				 pgm->return_path))) ||
      (pgm->sender &&
       (reply = imap_send_slist (stream,tag,s,"HEADER Sender",pgm->sender))) ||
      (pgm->reply_to &&
       (reply = imap_send_slist (stream,tag,s,"HEADER Reply-To",
				 pgm->reply_to))) ||
      (pgm->in_reply_to &&
       (reply = imap_send_slist (stream,tag,s,"HEADER In-Reply-To",
				 pgm->in_reply_to))) ||
      (pgm->message_id &&
       (reply = imap_send_slist (stream,tag,s,"HEADER Message-ID",
				 pgm->message_id))) ||
      (pgm->newsgroups &&
       (reply = imap_send_slist (stream,tag,s,"HEADER Newsgroups",
				 pgm->newsgroups))) ||
      (pgm->followup_to &&
       (reply = imap_send_slist (stream,tag,s,"HEADER Followup-To",
				 pgm->followup_to))) ||
      (pgm->references &&
       (reply = imap_send_slist (stream,tag,s,"HEADER References",
				 pgm->references)))) return reply;
				/* all other headers */
  if (hdr = pgm->header) do {
    for (t = " HEADER "; *t; *(*s)++ = *t++);
    if (reply = imap_send_astring (stream,tag,s,&hdr->line,NIL)) return reply;
    *(*s)++ = ' ';
    if (reply = imap_send_astring (stream,tag,s,&hdr->text,NIL)) return reply;
  }
  while (hdr = hdr->next);
  for (pgo = pgm->or; pgo; pgo = pgo->next) {
    for (t = " OR ("; *t; *(*s)++ = *t++);
    if (reply = imap_send_spgm (stream,tag,s,pgo->first)) return reply;
    for (t = ") ("; *t; *(*s)++ = *t++);
    if (reply = imap_send_spgm (stream,tag,s,pgo->second)) return reply;
    *(*s)++ = ')';
  }
  for (pgl = pgm->not; pgl; pgl = pgl->next) {
    for (t = " NOT ("; *t; *(*s)++ = *t++);
    if (reply = imap_send_spgm (stream,tag,s,pgl->pgm)) return reply;
    *(*s)++ = ')';
  }
  return NIL;			/* search program written OK */
}

/* IMAP send search set
 * Accepts: pointer to current position pointer of output bigbuf
 *	    search set to output
 */

void imap_send_sset (char **s,SEARCHSET *set,char *prefix)
{
  char c;
				/* write prefix */
  if (prefix) while (*prefix) *(*s)++ = *prefix++;
				/* run down search list */
  for (c = ' '; set; set = set->next, c = ',') {
    *(*s)++ = c;		/* write delimiter and first value */
    if (set->first != 0xffffffff) {
      sprintf (*s,"%lu",set->first);
      *s += strlen (*s);
    }
    else *(*s)++ = '*';		/* last message */
    if (set->last) {		/* have a second value? */
      *(*s)++ = ':';		/* write delimiter and second value */
      if (set->last != 0xffffffff) {
	sprintf (*s,"%lu",set->last);
	*s += strlen (*s);
      }
      else *(*s)++ = '*';	/* last message */
    }
  }
}

/* IMAP send search list
 * Accepts: MAIL stream
 *	    reply tag
 *	    pointer to current position pointer of output bigbuf
 *	    name of search list
 *	    search list to output
 * Returns: NIL if success, error reply if error
 */

IMAPPARSEDREPLY *imap_send_slist (MAILSTREAM *stream,char *tag,char **s,
				  char *name,STRINGLIST *list)
{
  char *t;
  IMAPPARSEDREPLY *reply;
  do {
    *(*s)++ = ' ';		/* output name of search list */
    for (t = name; *t; *(*s)++ = *t++);
    *(*s)++ = ' ';
    reply = imap_send_astring (stream,tag,s,&list->text,NIL);
  }
  while (!reply && (list = list->next));
  return reply;
}


/* IMAP send search date
 * Accepts: pointer to current position pointer of output bigbuf
 *	    field name
 *	    search date to output
 */

void imap_send_sdate (char **s,char *name,unsigned short date)
{
  sprintf (*s," %s %d-%s-%d",name,date & 0x1f,
	   months[((date >> 5) & 0xf) - 1],BASEYEAR + (date >> 9));
  *s += strlen (*s);
}

/* IMAP send buffered command to sender
 * Accepts: MAIL stream
 *	    reply tag
 *	    string
 *	    pointer to string tail pointer
 * Returns: reply
 */

IMAPPARSEDREPLY *imap_sout (MAILSTREAM *stream,char *tag,char *base,char **s)
{
  IMAPPARSEDREPLY *reply;
  if (stream->debug) {		/* output debugging telemetry */
    **s = '\0';
    mail_dlog (base,LOCAL->sensitive);
  }
  *(*s)++ = '\015';		/* append CRLF */
  *(*s)++ = '\012';
  **s = '\0';
  reply = net_sout (LOCAL->netstream,base,*s - base) ?
    imap_reply (stream,tag) :
      imap_fake (stream,tag,"[CLOSED] IMAP connection broken (command)");
  *s = base;			/* restart buffer */
  return reply;
}


/* IMAP send null-terminated string to sender
 * Accepts: MAIL stream
 *	    string
 * Returns: T if success, else NIL
 */

long imap_soutr (MAILSTREAM *stream,char *string)
{
  long ret;
  unsigned long i;
  char *s;
  if (stream->debug) mm_dlog (string);
  sprintf (s = (char *) fs_get ((i = strlen (string) + 2) + 1),
	   "%s\015\012",string);
  ret = net_sout (LOCAL->netstream,s,i);
  fs_give ((void **) &s);
  return ret;
}

/* IMAP get reply
 * Accepts: MAIL stream
 *	    tag to search or NIL if want a greeting
 * Returns: parsed reply, never NIL
 */

IMAPPARSEDREPLY *imap_reply (MAILSTREAM *stream,char *tag)
{
  IMAPPARSEDREPLY *reply;
  while (LOCAL->netstream) {	/* parse reply from server */
    if (reply = imap_parse_reply (stream,net_getline (LOCAL->netstream))) {
				/* continuation ready? */
      if (!strcmp (reply->tag,"+")) return reply;
				/* untagged data? */
      else if (!strcmp (reply->tag,"*")) {
	imap_parse_unsolicited (stream,reply);
	if (!tag) return reply;	/* return if just wanted greeting */
      }
      else {			/* tagged data */
	if (tag && !strcmp (tag,reply->tag)) return reply;
				/* report bogon */
	sprintf (LOCAL->tmp,"Unexpected tagged response: %.80s %.80s %.80s",
		 reply->tag,reply->key,reply->text);
	mm_log (LOCAL->tmp,WARN);
      }
    }
  }
  return imap_fake (stream,tag,
		    "[CLOSED] IMAP connection broken (server response)");
}

/* IMAP parse reply
 * Accepts: MAIL stream
 *	    text of reply
 * Returns: parsed reply, or NIL if can't parse at least a tag and key
 */


IMAPPARSEDREPLY *imap_parse_reply (MAILSTREAM *stream,char *text)
{
  if (LOCAL->reply.line) fs_give ((void **) &LOCAL->reply.line);
				/* init fields in case error */
  LOCAL->reply.key = LOCAL->reply.text = LOCAL->reply.tag = NIL;
  if (!(LOCAL->reply.line = text)) {
				/* NIL text means the stream died */
    if (LOCAL->netstream) net_close (LOCAL->netstream);
    LOCAL->netstream = NIL;
    return NIL;
  }
  if (stream->debug) mm_dlog (LOCAL->reply.line);
  if (!(LOCAL->reply.tag = (char *) strtok (LOCAL->reply.line," "))) {
    mm_log ("IMAP server sent a blank line",WARN);
    return NIL;
  }
				/* non-continuation replies */
  if (strcmp (LOCAL->reply.tag,"+")) {
				/* parse key */
    if (!(LOCAL->reply.key = (char *) strtok (NIL," "))) {
				/* determine what is missing */
      sprintf (LOCAL->tmp,"Missing IMAP reply key: %.80s",LOCAL->reply.tag);
      mm_log (LOCAL->tmp,WARN);	/* pass up the barfage */
      return NIL;		/* can't parse this text */
    }
    ucase (LOCAL->reply.key);	/* make sure key is upper case */
				/* get text as well, allow empty text */
    if (!(LOCAL->reply.text = (char *) strtok (NIL,"\n")))
      LOCAL->reply.text = LOCAL->reply.key + strlen (LOCAL->reply.key);
  }
  else {			/* special handling of continuation */
    LOCAL->reply.key = "BAD";	/* so it barfs if not expecting continuation */
    if (!(LOCAL->reply.text = (char *) strtok (NIL,"\n")))
      LOCAL->reply.text = "";
  }
  return &LOCAL->reply;		/* return parsed reply */
}

/* IMAP fake reply when stream determined to be dead
 * Accepts: MAIL stream
 *	    tag
 *	    text of fake reply (must start with "[CLOSED]")
 * Returns: parsed reply
 */

IMAPPARSEDREPLY *imap_fake (MAILSTREAM *stream,char *tag,char *text)
{
  mm_notify (stream,text,BYE);	/* send bye alert */
  if (LOCAL->netstream) net_close (LOCAL->netstream);
  LOCAL->netstream = NIL;	/* farewell, dear NET stream... */
				/* flush previous reply */
  if (LOCAL->reply.line) fs_give ((void **) &LOCAL->reply.line);
				/* build new fake reply */
  LOCAL->reply.tag = LOCAL->reply.line = cpystr (tag ? tag : "*");
  LOCAL->reply.key = "NO";
  LOCAL->reply.text = text;
  return &LOCAL->reply;		/* return parsed reply */
}


/* IMAP check for OK response in tagged reply
 * Accepts: MAIL stream
 *	    parsed reply
 * Returns: T if OK else NIL
 */

long imap_OK (MAILSTREAM *stream,IMAPPARSEDREPLY *reply)
{
  long ret = NIL;
				/* OK - operation succeeded */
  if (!strcmp (reply->key,"OK")) {
    imap_parse_response (stream,reply->text,NIL,NIL);
    ret = T;
  }
				/* NO - operation failed */
  else if (!strcmp (reply->key,"NO"))
    imap_parse_response (stream,reply->text,WARN,NIL);
  else {			/* BAD - operation rejected */
    if (!strcmp (reply->key,"BAD")) {
      imap_parse_response (stream,reply->text,ERROR,NIL);
      sprintf (LOCAL->tmp,"IMAP protocol error: %.80s",reply->text);
    }
				/* bad protocol received */
    else sprintf (LOCAL->tmp,"Unexpected IMAP response: %.80s %.80s",
		  reply->key,reply->text);
    mm_log (LOCAL->tmp,ERROR);	/* either way, this is not good */
  }
  return ret;
}

/* IMAP parse and act upon unsolicited reply
 * Accepts: MAIL stream
 *	    parsed reply
 */

void imap_parse_unsolicited (MAILSTREAM *stream,IMAPPARSEDREPLY *reply)
{
  unsigned long i = 0;
  unsigned long msgno;
  char *s,*t;
  if (isdigit (*reply->key)) {	/* see if key is a number */
    msgno = strtoul (reply->key,&s,10);
    if (*s) {			/* better be nothing after number */
      sprintf (LOCAL->tmp,"Unexpected untagged message: %.80s",reply->key);
      mm_log (LOCAL->tmp,WARN);
      return;
    }
    if (!reply->text) {		/* better be some data */
      mm_log ("Missing message data",WARN);
      return;
    }
				/* get keyword */
    s = ucase ((char *) strtok (reply->text," "));
				/* and locate the text after it */
    t = (char *) strtok (NIL,"\n");
				/* now take the action */
				/* change in size of mailbox */
    if (!strcmp (s,"EXISTS")) mail_exists (stream,msgno);
    else if (!strcmp (s,"RECENT")) mail_recent (stream,msgno);
    else if (!strcmp (s,"EXPUNGE") && msgno && (msgno <= stream->nmsgs)) {
      mailcache_t mc = (mailcache_t) mail_parameters (NIL,GET_CACHE,NIL);
      MESSAGECACHE *elt = (MESSAGECACHE *) (*mc) (stream,msgno,CH_ELT);
      if (elt) imap_gc_body (elt->private.msg.body);
				/* notify upper level */
      mail_expunged (stream,msgno);
    }

    else if ((!strcmp (s,"FETCH") || !strcmp (s,"STORE")) &&
	     msgno && (msgno <= stream->nmsgs)) {
      char *prop;
      GETS_DATA md;
      ENVELOPE **e;
      MESSAGECACHE *elt = mail_elt (stream,msgno);
      ENVELOPE *env = NIL;
      imapenvelope_t ie =
	(imapenvelope_t) mail_parameters (stream,GET_IMAPENVELOPE,NIL);
      ++t;			/* skip past open parenthesis */
				/* parse Lisp-form property list */
      while (prop = ((char *) strtok (t," )"))) {
	t = (char *) strtok (NIL,"\n");
	INIT_GETS (md,stream,elt->msgno,NIL,0,0);
	e = NIL;		/* not pointing at any envelope yet */
				/* parse the property and its value */
	if (!strcmp (ucase (prop),"FLAGS")) imap_parse_flags (stream,elt,&t);
	else if (!strcmp (prop,"INTERNALDATE") &&
		 (s = imap_parse_string (stream,&t,reply,NIL,NIL,LONGT))) {
	  if (!mail_parse_date (elt,s)) {
	    sprintf (LOCAL->tmp,"Bogus date: %.80s",s);
	    mm_log (LOCAL->tmp,WARN);
				/* slam in default so we don't try again */
	    mail_parse_date (elt,"01-Jan-1970 00:00:00 +0000");
	  }
	  fs_give ((void **) &s);
	}
	else if (!strcmp (prop,"UID")) /* unique identifier */
	  elt->private.uid = strtoul (t,&t,10);
	else if (!strcmp (prop,"ENVELOPE")) {
	  if (stream->scache) {	/* short cache, flush old stuff */
	    mail_free_body (&stream->body);
	    stream->msgno = elt->msgno;
	    e = &stream->env;	/* get pointer to envelope */
	  }
	  else e = &elt->private.msg.env;
	  imap_parse_envelope (stream,e,&t,reply);
	}
	else if (!strncmp (prop,"BODY",4)) {
	  if (!prop[4] || !strcmp (prop+4,"STRUCTURE")) {
	    BODY **body;
	    if (stream->scache){/* short cache, flush old stuff */
	      if (stream->msgno != msgno) {
		mail_free_envelope (&stream->env);
		sprintf (LOCAL->tmp,"Body received for %lu but current is %lu",
			 msgno,stream->msgno);
		stream->msgno = msgno;
	      }
				/* get pointer to body */
	      body = &stream->body;
	    }
	    else body = &elt->private.msg.body;
				/* flush any prior body */
	    mail_free_body (body);
				/* instantiate and parse a new body */
	    imap_parse_body_structure (stream,*body = mail_newbody(),&t,reply);
	  }

	  else if (prop[4] == '[') {
	    STRINGLIST *stl = NIL;
	    SIZEDTEXT text;
				/* will want to return envelope data */
	    if (!strcmp (md.what = cpystr (prop + 5),"HEADER]") ||
		!strcmp (md.what,"0]"))
	      e = stream->scache ? &stream->env : &elt->private.msg.env;
	    LOCAL->tmp[0] ='\0';/* no errors yet */
				/* found end of section? */
	    if (!(s = strchr (md.what,']'))) {
				/* skip leading nesting */
	      for (s = md.what; *s && (isdigit (*s) || (*s == '.')); s++);
				/* better be one of these */
	      if (strncmp (s,"HEADER.FIELDS",13) &&
		  (!s[13] || strcmp (s+13,".NOT")))
		sprintf (LOCAL->tmp,"Unterminated section: %.80s",md.what);
				/* get list of headers */
	      else if (!(stl = imap_parse_stringlist (stream,&t,reply)))
		sprintf (LOCAL->tmp,"Bogus header field list: %.80s",t);
	      else if (*t != ']')
		sprintf (LOCAL->tmp,"Unterminated header section: %.80s",t);
				/* point after the text */
	      else if (t = strchr (s = t,' ')) *t++ = '\0';
	    }
	    if (s && !LOCAL->tmp[0]) {
	      *s++ = '\0';	/* tie off section specifier */
	      if (*s == '<') {	/* partial specifier? */
		md.first = strtoul (s+1,&s,10) + 1;
		if (*s++ != '>')	/* make sure properly terminated */
		  sprintf (LOCAL->tmp,"Unterminated partial data: %.80s",s-1);
	      }
	      if (!LOCAL->tmp[0] && *s)
		sprintf (LOCAL->tmp,"Junk after section: %.80s",s);
	    }
	    if (LOCAL->tmp[0]) { /* got any errors? */
	      mm_log (LOCAL->tmp,WARN);
	      mail_free_stringlist (&stl);
	    }
	    else {		/* parse text from server */
	      text.data = (unsigned char *)
		imap_parse_string (stream,&t,reply,
				   ((md.what[0] && (md.what[0] != 'H')) ||
				    md.first || md.last) ? &md : NIL,
				   &text.size,NIL);
				/* all done if partial */
	      if (md.first || md.last) mail_free_stringlist (&stl);
				/* otherwise register it in the cache */
	      else imap_cache (stream,msgno,md.what,stl,&text);
	    }
	    fs_give ((void **) &md.what);
	  }
	  else {
	    sprintf (LOCAL->tmp,"Unknown body message property: %.80s",prop);
	    mm_log (LOCAL->tmp,WARN);
	  }
	}

				/* one of the RFC822 props? */
	else if (!strncmp (prop,"RFC822",6) && (!prop[6] || (prop[6] == '.'))){
	  SIZEDTEXT text;
	  if (!prop[6]) {	/* cache full message */
	    md.what = "";
	    text.data = (unsigned char *)
	      imap_parse_string (stream,&t,reply,&md,&text.size,NIL);
	    imap_cache (stream,msgno,md.what,NIL,&text);
	  }
	  else if (!strcmp (prop+7,"SIZE"))
	    elt->rfc822_size = strtoul (t,&t,10);
				/* legacy properties */
	  else if (!strcmp (prop+7,"HEADER")) {
	    text.data = (unsigned char *)
	      imap_parse_string (stream,&t,reply,NIL,&text.size,NIL);
	    imap_cache (stream,msgno,"HEADER",NIL,&text);
	    e = stream->scache ? &stream->env : &elt->private.msg.env;
	  }
	  else if (!strcmp (prop+7,"TEXT")) {
	    md.what = "TEXT";
	    text.data = (unsigned char *)
	      imap_parse_string (stream,&t,reply,&md,&text.size,NIL);
	    imap_cache (stream,msgno,md.what,NIL,&text);
	  }
	  else {
	    sprintf (LOCAL->tmp,"Unknown RFC822 message property: %.80s",prop);
	    mm_log (LOCAL->tmp,WARN);
	  }
	}
	else {
	  sprintf (LOCAL->tmp,"Unknown message property: %.80s",prop);
	  mm_log (LOCAL->tmp,WARN);
	}
	if (e && *e) env = *e;	/* note envelope if we got one */
      }
				/* do callback if requested */
      if (ie && env) (*ie) (stream,msgno,env);
    }
				/* obsolete response to COPY */
    else if (strcmp (s,"COPY")) {
      sprintf (LOCAL->tmp,"Unknown message data: %lu %.80s",msgno,s);
      mm_log (LOCAL->tmp,WARN);
    }
  }

  else if (!strcmp (reply->key,"FLAGS")) {
				/* flush old user flags if any */
    while ((i < NUSERFLAGS) && stream->user_flags[i])
      fs_give ((void **) &stream->user_flags[i++]);
    i = 0;			/* add flags */
    if (reply->text && (s = (char *) strtok (reply->text+1," )"))) do
      if (*s != '\\') stream->user_flags[i++] = cpystr (s);
    while (s = (char *) strtok (NIL," )"));
  }
  else if (!strcmp (reply->key,"SEARCH")) {
				/* only do something if have text */
    if (reply->text && (t = (char *) strtok (reply->text," "))) do {
				/* UIDs always passed to main program */
      if (LOCAL->uidsearch) mm_searched (stream,atol (t));
				/* should be a msgno then */
      else if ((i = atol (t)) <= stream->nmsgs) {
	mail_elt (stream,i)->searched = T;
	if (!stream->silent) mm_searched (stream,i);
      }
    } while (t = (char *) strtok (NIL," "));
  }

  else if (!strcmp (reply->key,"ACL") && (s = reply->text) &&
	   (t = imap_parse_astring (stream,&s,reply,NIL))) {
    getacl_t ar = (getacl_t) mail_parameters (NIL,GET_ACL,NIL);
    if (s && (*s++ == ' ')) {
      ACLLIST *al = mail_newacllist ();
      ACLLIST *ac = al;
      do if ((ac->identifier = imap_parse_astring (stream,&s,reply,NIL)) &&
	     s && (*s++ == ' '))
	ac->rights = imap_parse_astring (stream,&s,reply,NIL);
      while (ac->rights && s && (*s == ' ') && s++ &&
	     (ac = ac->next = mail_newacllist ()));
      if (!ac->rights || (s && *s)) {
	sprintf (LOCAL->tmp,"Invalid ACL identifer/rights for %.80s",t);
	mm_log (LOCAL->tmp,WARN);
      }
      else if (ar) (*ar) (stream,t,al);
      mail_free_acllist (&al);	/* clean up */
    }
				/* no optional rights */
    else if (ar) (*ar) (stream,t,NIL);
    fs_give ((void **) &t);	/* free mailbox name */
  }

  else if (!strcmp (reply->key,"LISTRIGHTS") && (s = reply->text) &&
	   (t = imap_parse_astring (stream,&s,reply,NIL))) {
    listrights_t lr = (listrights_t) mail_parameters (NIL,GET_LISTRIGHTS,NIL);
    char *id,*r;
    if (s && (*s++ == ' ') && (id = imap_parse_astring (stream,&s,reply,NIL))){
      if (s && (*s++ == ' ') &&
	  (r = imap_parse_astring (stream,&s,reply,NIL))) {
	if (s && (*s++ == ' ')) {
	  STRINGLIST *rl = mail_newstringlist ();
	  STRINGLIST *rc = rl;
	  do rc->text.data = (unsigned char *)
	    imap_parse_astring (stream,&s,reply,&rc->text.size);
	  while (rc->text.data && s && (*s == ' ') && s++ &&
		 (rc = rc->next = mail_newstringlist ()));
	  if (!rc->text.data || (s && *s)) {
	    sprintf (LOCAL->tmp,"Invalid optional LISTRIGHTS for %.80s",t);
	    mm_log (LOCAL->tmp,WARN);
	  }
	  else if (lr) (*lr) (stream,t,id,r,rl);
				/* clean up */
	  mail_free_stringlist (&rl);
	}
				/* no optional rights */
	else if (lr) (*lr) (stream,t,id,r,NIL);
	fs_give ((void **) &r);	/* free rights */
      }
      else {
	sprintf (LOCAL->tmp,"Missing LISTRIGHTS rights for %.80s",t);
	mm_log (LOCAL->tmp,WARN);
      }
      fs_give ((void **) &id);	/* free identifier */
    }
    else {
      sprintf (LOCAL->tmp,"Missing LISTRIGHTS identifer for %.80s",t);
      mm_log (LOCAL->tmp,WARN);
    }
    fs_give ((void **) &t);	/* free mailbox name */
  }

  else if (!strcmp (reply->key,"MYRIGHTS") && (s = reply->text) &&
	   (t = imap_parse_astring (stream,&s,reply,NIL))) {
    myrights_t mr = (myrights_t) mail_parameters (NIL,GET_MYRIGHTS,NIL);
    char *r;
    if (s && (*s++ == ' ') && (r = imap_parse_astring (stream,&s,reply,NIL))) {
      if (s && *s) {
	sprintf (LOCAL->tmp,"Junk after MYRIGHTS for %.80s",t);
	mm_log (LOCAL->tmp,WARN);
      }
      else if (mr) (*mr) (stream,t,r);
      fs_give ((void **) &r);	/* free rights */
    }
    else {
      sprintf (LOCAL->tmp,"Missing MYRIGHTS for %.80s",t);
      mm_log (LOCAL->tmp,WARN);
    }
    fs_give ((void **) &t);	/* free mailbox name */
  }

				/* this response has a bizarre syntax! */
  else if (!strcmp (reply->key,"QUOTA") && (s = reply->text) &&
	   (t = imap_parse_astring (stream,&s,reply,NIL))) {
				/* in case error */
    sprintf (LOCAL->tmp,"Bad quota resource list for %.80s",t);
    if (s && (*s++ == ' ') && (*s++ == '(') && *s && ((*s != ')') || !s[1])) {
      quota_t qt = (quota_t) mail_parameters (NIL,GET_QUOTA,NIL);
      QUOTALIST *ql = NIL;
      QUOTALIST *qc;
				/* parse non-empty quota resource list */
      if (*s != ')') for (ql = qc = mail_newquotalist (); T;
			  qc = qc->next = mail_newquotalist ()) {
	if ((qc->name = imap_parse_astring (stream,&s,reply,NIL)) && s &&
	    (*s++ == ' ') && isdigit (*s)) {
	  qc->usage = strtoul (s,&s,10);
	  if ((*s++ == ' ') && isdigit (*s)) {
	    qc->limit = strtoul (s,&s,10);
				/* another resource follows? */
	    if (*s == ' ') continue;
				/* end of resource list? */
	    if ((*s == ')') && !s[1]) {
	      if (qt) (*qt) (stream,t,ql);
	      break;		/* all done */
	    }
	  }
	}
				/* something bad happened */
	mm_log (LOCAL->tmp,WARN);
	break;			/* parse failed */
      }
				/* all done with quota resource list now */
      if (ql) mail_free_quotalist (&ql);
    }
    else mm_log (LOCAL->tmp,WARN);
    fs_give ((void **) &t);	/* free root name */
  }
  else if (!strcmp (reply->key,"QUOTAROOT") && (s = reply->text) &&
	   (t = imap_parse_astring (stream,&s,reply,NIL))) {
    sprintf (LOCAL->tmp,"Bad quota root list for %.80s",t);
    if (s && (*s++ == ' ')) {
      quotaroot_t qr = (quotaroot_t) mail_parameters (NIL,GET_QUOTAROOT,NIL);
      STRINGLIST *rl = mail_newstringlist ();
      STRINGLIST *rc = rl;
      do rc->text.data = (unsigned char *)
	imap_parse_astring (stream,&s,reply,&rc->text.size);
      while (rc->text.data && s && (*s++ == ' ') &&
	       (rc = rc->next = mail_newstringlist ()));
      if (!rc->text.data || (s && *s)) mm_log (LOCAL->tmp,WARN);
      else if (qr) (*qr) (stream,t,rl);
				/* clean up */
      mail_free_stringlist (&rl);
    }
    else mm_log (LOCAL->tmp,WARN);
    fs_give ((void **) &t);
  }

  else if (!strcmp (reply->key,"NAMESPACE")) {
    if (LOCAL->namespace) {
      mail_free_namespace (&LOCAL->namespace[0]);
      mail_free_namespace (&LOCAL->namespace[1]);
      mail_free_namespace (&LOCAL->namespace[2]);
    }
    else LOCAL->namespace = (NAMESPACE **) fs_get (3 * sizeof (NAMESPACE *));
    if (s = reply->text) {	/* parse namespace results */
      LOCAL->namespace[0] = imap_parse_namespace (stream,&s,reply);
      LOCAL->namespace[1] = imap_parse_namespace (stream,&s,reply);
      LOCAL->namespace[2] = imap_parse_namespace (stream,&s,reply);
      if (s && *s) {
	sprintf (LOCAL->tmp,"Junk after namespace list: %.80s",s);
	mm_log (LOCAL->tmp,WARN);
      }
    }
    else mm_log ("Missing namespace list",WARN);
  }
  else if (!strcmp (reply->key,"SORT")) {
    sortresults_t sr = (sortresults_t)
      mail_parameters (NIL,GET_SORTRESULTS,NIL);
    LOCAL->sortsize = 0;	/* initialize sort data */
    if (LOCAL->sortdata) fs_give ((void **) &LOCAL->sortdata);
    LOCAL->sortdata = (unsigned long *)
      fs_get ((stream->nmsgs + 1) * sizeof (unsigned long));
				/* only do something if have text */
    if (reply->text && (t = (char *) strtok (reply->text," "))) do
      LOCAL->sortdata[LOCAL->sortsize++] = atol (t);
    while ((t = (char *) strtok (NIL," ")) && (LOCAL->sortsize<stream->nmsgs));
    LOCAL->sortdata[LOCAL->sortsize] = 0;
				/* also return via callback if requested */
    if (sr) (*sr) (stream,LOCAL->sortdata,LOCAL->sortsize);
  }
  else if (!strcmp (reply->key,"THREAD")) {
    threadresults_t tr = (threadresults_t)
      mail_parameters (NIL,GET_THREADRESULTS,NIL);
    if (LOCAL->threaddata) mail_free_threadnode (&LOCAL->threaddata);
    if (s = reply->text) {
      LOCAL->threaddata = imap_parse_thread (&s);
      if (tr) (*tr) (stream,LOCAL->threaddata);
      if (s && *s) {
	sprintf (LOCAL->tmp,"Junk at end of thread: %.80s",s);
	mm_log (LOCAL->tmp,WARN);
      }
    }
  }

  else if (!strcmp (reply->key,"STATUS") && reply->text) {
    MAILSTATUS status;
    char *txt = reply->text;
    if ((t = imap_parse_astring (stream,&txt,reply,NIL)) && txt &&
	(*txt++ == ' ') && (*txt++ == '(') && (s = strchr (txt,')')) &&
	(s - txt) && !s[1]) {
      *s = '\0';		/* tie off status data */
				/* initialize data block */
      status.flags = status.messages = status.recent = status.unseen =
	status.uidnext = status.uidvalidity = 0;
      ucase (txt);		/* do case-independent match */
      while (*txt && (s = strchr (txt,' '))) {
	*s++ = '\0';		/* tie off status attribute name */
	i = strtoul (s,&s,10);	/* get attribute value */
	if (!strcmp (txt,"MESSAGES")) {
	  status.flags |= SA_MESSAGES;
	  status.messages = i;
	}
	else if (!strcmp (txt,"RECENT")) {
	  status.flags |= SA_RECENT;
	  status.recent = i;
	}
	else if (!strcmp (txt,"UNSEEN")) {
	  status.flags |= SA_UNSEEN;
	  status.unseen = i;
	}
	else if (!strcmp (txt,"UIDNEXT") || !strcmp (txt,"UID-NEXT")) {
	  status.flags |= SA_UIDNEXT;
	  status.uidnext = i;
	}
	else if (!strcmp (txt,"UIDVALIDITY")|| !strcmp (txt,"UID-VALIDITY")){
	  status.flags |= SA_UIDVALIDITY;
	  status.uidvalidity = i;
	}
				/* next attribute */
	txt = (*s == ' ') ? s + 1 : s;
      }
      strcpy (strchr (strcpy (LOCAL->tmp,stream->mailbox),'}') + 1,t);
				/* pass status to main program */
      mm_status (stream,LOCAL->tmp,&status);
    }
    if (t) fs_give ((void **) &t);
  }

  else if ((!strcmp (reply->key,"LIST") || !strcmp (reply->key,"LSUB")) &&
	   reply->text && (*reply->text == '(') &&
	   (s = strchr (reply->text,')')) && (s[1] == ' ')) {
    char delimiter = '\0';
    *s++ = '\0';		/* tie off attribute list */
				/* parse attribute list */
    if (t = (char *) strtok (reply->text+1," ")) do {
      if (!strcmp (ucase (t),"\\NOINFERIORS")) i |= LATT_NOINFERIORS;
      else if (!strcmp (t,"\\NOSELECT")) i |= LATT_NOSELECT;
      else if (!strcmp (t,"\\MARKED")) i |= LATT_MARKED;
      else if (!strcmp (t,"\\UNMARKED")) i |= LATT_UNMARKED;
				/* ignore extension flags */
    }
    while (t = (char *) strtok (NIL," "));
    switch (*++s) {		/* process delimiter */
    case 'N':			/* NIL */
    case 'n':
      s += 4;			/* skip over NIL<space> */
      break;
    case '"':			/* have a delimiter */
      delimiter = (*++s == '\\') ? *++s : *s;
      s += 3;			/* skip over <delimiter><quote><space> */
    }
				/* parse the mailbox name */
    s = t = imap_parse_astring (stream,&s,reply,NIL);
				/* prepend prefix if requested */
    if (LOCAL->prefix) sprintf (s = LOCAL->tmp,"%s%s",LOCAL->prefix,t);
				/* pass data to main program */
    if (reply->key[1] == 'S') mm_lsub (stream,delimiter,s,i);
    else mm_list (stream,delimiter,s,i);
    fs_give ((void **) &t);	/* flush mailbox name */
  }
  else if (!strcmp (reply->key,"MAILBOX") && reply->text) {
    if (LOCAL->prefix)
      sprintf (t = LOCAL->tmp,"%s%s",LOCAL->prefix,reply->text);
    else t = reply->text;
    mm_list (stream,NIL,t,NIL);
  }
  else if (!strcmp (reply->key,"OK") || !strcmp (reply->key,"PREAUTH"))
    imap_parse_response (stream,reply->text,NIL,T);
  else if (!strcmp (reply->key,"NO"))
    imap_parse_response (stream,reply->text,WARN,T);
  else if (!strcmp (reply->key,"BAD"))
    imap_parse_response (stream,reply->text,ERROR,T);
  else if (!strcmp (reply->key,"BYE")) {
    LOCAL->byeseen = T;		/* note that a BYE seen */
    imap_parse_response (stream,reply->text,BYE,T);
  }
  else if (!strcmp (reply->key,"CAPABILITY") && reply->text)
    imap_parse_capabilities (stream,reply->text);
  else {
    sprintf (LOCAL->tmp,"Unexpected untagged message: %.80s",reply->key);
    mm_log (LOCAL->tmp,WARN);
  }
}

/* Parse human-readable response text
 * Accepts: mail stream
 *	    text
 *	    error level for mm_notify()
 *	    non-NIL if want mm_notify() event even if no response code
 */

void imap_parse_response (MAILSTREAM *stream,char *text,long errflg,long ntfy)
{
  char *s,*t;
  size_t i;
  if (text && (*text == '[') && (t = strchr (s = text + 1,']')) &&
      ((i = t - s) < IMAPTMPLEN)) {
    LOCAL->tmp[i] = '\0';	/* make mungable copy of text code */
    if (s = strchr (strncpy (LOCAL->tmp,s,i),' ')) *s++ = '\0';
    ucase (LOCAL->tmp);		/* make code uppercase */
    if (s) {			/* have argument? */
      ntfy = NIL;		/* suppress mm_notify if normal SELECT data */
      if (!strcmp (LOCAL->tmp,"UIDVALIDITY"))
	stream->uid_validity = strtoul (s,NIL,10);
      else if (!strcmp (LOCAL->tmp,"UIDNEXT"))
	stream->uid_last = strtoul (s,NIL,10) - 1;
      else if (!strcmp (LOCAL->tmp,"PERMANENTFLAGS") && (*s == '(') &&
	       (LOCAL->tmp[i-1] == ')')) {
	LOCAL->tmp[i-1] = '\0';	/* tie off flags */
	stream->perm_seen = stream->perm_deleted = stream->perm_answered =
	  stream->perm_draft = stream->kwd_create = NIL;
	stream->perm_user_flags = NIL;
	if (s = strtok (s+1," ")) do {
	  if (*ucase (s) == '\\') {	/* system flags */
	    if (!strcmp (s,"\\SEEN")) stream->perm_seen = T;
	    else if (!strcmp (s,"\\DELETED")) stream->perm_deleted = T;
	    else if (!strcmp (s,"\\FLAGGED")) stream->perm_flagged = T;
	    else if (!strcmp (s,"\\ANSWERED")) stream->perm_answered = T;
	    else if (!strcmp (s,"\\DRAFT")) stream->perm_draft = T;
	    else if (!strcmp (s,"\\*")) stream->kwd_create = T;
	  }
	  else stream->perm_user_flags |= imap_parse_user_flag (stream,s);
	}
	while (s = strtok (NIL," "));
      }
      else if (!strcmp (LOCAL->tmp,"CAPABILITY"))
	imap_parse_capabilities (stream,s);
      else {			/* all other response code events */
	ntfy = T;		/* must mm_notify() */
	if (!strcmp (LOCAL->tmp,"REFERRAL"))
	  LOCAL->referral = cpystr (LOCAL->tmp + 9);
      }
    }
    else {			/* no arguments */
      if (!strcmp (LOCAL->tmp,"UIDNOTSTICKY")) {
	ntfy = NIL;
	stream->uid_nosticky = T;
      }
      else if (!strcmp (LOCAL->tmp,"READ-ONLY")) stream->rdonly = T;
      else if (!strcmp (LOCAL->tmp,"READ-WRITE")) stream->rdonly = NIL;
    }
  }
				/* give event to main program */
  if (ntfy && !stream->silent) mm_notify (stream,text ? text : "",errflg);
}

/* Parse a namespace
 * Accepts: mail stream
 *	    current text pointer
 *	    parsed reply
 * Returns: namespace list, text pointer updated
 */

NAMESPACE *imap_parse_namespace (MAILSTREAM *stream,char **txtptr,
				 IMAPPARSEDREPLY *reply)
{
  NAMESPACE *ret = NIL;
  NAMESPACE *nam = NIL;
  NAMESPACE *prev = NIL;
  PARAMETER *par = NIL;
  if (*txtptr) {		/* only if argument given */
				/* ignore leading space */
    while (**txtptr == ' ') ++*txtptr;
    switch (**txtptr) {
    case 'N':			/* if NIL */
    case 'n':
      ++*txtptr;		/* bump past "N" */
      ++*txtptr;		/* bump past "I" */
      ++*txtptr;		/* bump past "L" */
      break;
    case '(':
      ++*txtptr;		/* skip past open paren */
      while (**txtptr == '(') {
	++*txtptr;		/* skip past open paren */
	prev = nam;		/* note previous if any */
	nam = (NAMESPACE *) memset (fs_get (sizeof (NAMESPACE)),0,
				  sizeof (NAMESPACE));
	if (!ret) ret = nam;	/* if first time note first namespace */
				/* if previous link new block to it */
	if (prev) prev->next = nam;
	nam->name = imap_parse_string (stream,txtptr,reply,NIL,NIL,NIL);
				/* ignore whitespace */
	while (**txtptr == ' ') ++*txtptr;
	switch (**txtptr) {	/* parse delimiter */
	case 'N':
	case 'n':
	  *txtptr += 3;		/* bump past "NIL" */
	  break;
	case '"':
	  if (*++*txtptr == '\\') nam->delimiter = *++*txtptr;
	  else nam->delimiter = **txtptr;
	  *txtptr += 2;		/* bump past character and closing quote */
	  break;
	default:
	  sprintf (LOCAL->tmp,"Missing delimiter in namespace: %.80s",*txtptr);
	  mm_log (LOCAL->tmp,WARN);
	  *txtptr = NIL;	/* stop parse */
	  return ret;
	}

	while (**txtptr == ' '){/* append new parameter to tail */
	  if (nam->param) par = par->next = mail_newbody_parameter ();
	  else nam->param = par = mail_newbody_parameter ();
	  if (!(par->attribute = imap_parse_string (stream,txtptr,reply,NIL,
						    NIL,NIL))) {
	    mm_log ("Missing namespace extension attribute",WARN);
	    par->attribute = cpystr ("UNKNOWN");
	  }
				/* skip space */
	  while (**txtptr == ' ') ++*txtptr;
	  if (**txtptr == '(') {/* have value list?  */
	    char *att = par->attribute;
	    ++*txtptr;		/* yes */
	    do {		/* parse each value */
	      if (!(par->value = imap_parse_string (stream,txtptr,reply,NIL,
						    NIL,LONGT))) {
		sprintf (LOCAL->tmp,
			 "Missing value for namespace attribute %.80s",att);
		mm_log (LOCAL->tmp,WARN);
		par->value = cpystr ("UNKNOWN");
	      }
				/* is there another value? */
	      if (**txtptr == ' ') par = par->next = mail_newbody_parameter ();
	    } while (!par->value);
	  }
	  else {
	    sprintf (LOCAL->tmp,"Missing values for namespace attribute %.80s",
		     par->attribute);
	    mm_log (LOCAL->tmp,WARN);
	    par->value = cpystr ("UNKNOWN");
	  }
	}
	if (**txtptr == ')') ++*txtptr;
	else {			/* missing trailing paren */
	  sprintf (LOCAL->tmp,"Junk at end of namespace: %.80s",*txtptr);
	  mm_log (LOCAL->tmp,WARN);
	  return ret;
	}
      }
      if (**txtptr == ')') {	/* expected trailing paren? */
	++*txtptr;		/* got it! */
	break;
      }
    default:
      sprintf (LOCAL->tmp,"Not a namespace: %.80s",*txtptr);
      mm_log (LOCAL->tmp,WARN);
      *txtptr = NIL;		/* stop parse now */
      break;
    }
  }
  return ret;
}

/* Parse a thread node list
 * Accepts: current text pointer
 * Returns: thread node list, text pointer updated
 */

THREADNODE *imap_parse_thread (char **txtptr)
{
  THREADNODE *ret = NIL;	/* returned tree */
  THREADNODE *last = NIL;	/* last branch in this tree */
  THREADNODE *parent = NIL;	/* parent of current node */
  THREADNODE *cur;		/* current node */
  while (**txtptr == '(') {	/* see a thread? */
    ++*txtptr;			/* skip past open paren */
    while (**txtptr != ')') {	/* parse thread */
      if (**txtptr == '(') {	/* thread branch */
	cur = imap_parse_thread (txtptr);
				/* add to parent */
	if (parent) parent = parent->next = cur;
	else {			/* no parent, create dummy */
	  if (last) last = last->branch = mail_newthreadnode (NIL);
				/* new tree */
	  else ret = last = mail_newthreadnode (NIL);
				/* add to dummy parent */
	  last->next = parent = cur;
	}
      }
				/* threaded message number */
      else if (isdigit (**txtptr)) {
	(cur = mail_newthreadnode (NIL))->num = strtoul (*txtptr,txtptr,10);
				/* add to parent */
	if (parent) parent = parent->next = cur;
				/* no parent, start new thread */
	else if (last) last = last->branch = parent = cur;
				/* create new tree */
	else ret = last = parent = cur;
      }
      else {			/* anything else is a bogon */
	char tmp[MAILTMPLEN];
	sprintf (tmp,"Bogus thread member: %.80s",*txtptr);
	mm_log (tmp,WARN);
	return ret;
      }
				/* skip past any space */
      if (**txtptr == ' ') ++*txtptr;
    }
    ++*txtptr;			/* skip pase end of thread */
    parent = NIL;		/* close this thread */
  }
  return ret;			/* return parsed thread */
}

/* Parse RFC822 message header
 * Accepts: MAIL stream
 *	    envelope to parse into
 *	    header as sized text
 *	    stringlist if partial header
 */

void imap_parse_header (MAILSTREAM *stream,ENVELOPE **env,SIZEDTEXT *hdr,
			STRINGLIST *stl)
{
  ENVELOPE *nenv;
  BODY *body = NIL;
				/* parse what we can from this header */
  rfc822_parse_msg (&nenv,&body,(char *) hdr->data,hdr->size,NIL,
		    net_host (LOCAL->netstream),stream->dtb->flags);
  if (*env) {			/* need to merge this header into envelope? */
    if (!(*env)->newsgroups) {	/* need Newsgroups? */
      (*env)->newsgroups = nenv->newsgroups;
      (*env)->ngbogus = nenv->ngbogus;
      nenv->newsgroups = NIL;
    }
    if (!(*env)->followup_to) {	/* need Followup-To? */
      (*env)->followup_to = nenv->followup_to;
      nenv->followup_to = NIL;
    }
    if (!(*env)->references) {	/* need References? */
      (*env)->references = nenv->references;
      nenv->references = NIL;
    }
    mail_free_envelope (&nenv);
  }
				/* otherwise set it to this envelope */
  else (*env = nenv)->incomplete = stl ? T : NIL;

  /* Fill in optional mime content-type part /MaF */
  (*env)->optional.type = body->type;
  (*env)->optional.subtype = body->subtype;
  (*env)->optional.parameter = body->parameter;
  body->subtype = NIL;
  body->parameter = NIL;
  mail_free_body(&body);
}

/* IMAP parse envelope
 * Accepts: MAIL stream
 *	    pointer to envelope pointer
 *	    current text pointer
 *	    parsed reply
 *
 * Updates text pointer
 */

void imap_parse_envelope (MAILSTREAM *stream,ENVELOPE **env,char **txtptr,
			  IMAPPARSEDREPLY *reply)
{
  ENVELOPE *oenv = *env;
  char c = *((*txtptr)++);	/* grab first character */
				/* ignore leading spaces */
  while (c == ' ') c = *((*txtptr)++);
  switch (c) {			/* dispatch on first character */
  case '(':			/* if envelope S-expression */
    *env = mail_newenvelope ();	/* parse the new envelope */
    (*env)->date = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
    (*env)->subject = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
    (*env)->from = imap_parse_adrlist (stream,txtptr,reply);
    (*env)->sender = imap_parse_adrlist (stream,txtptr,reply);
    (*env)->reply_to = imap_parse_adrlist (stream,txtptr,reply);
    (*env)->to = imap_parse_adrlist (stream,txtptr,reply);
    (*env)->cc = imap_parse_adrlist (stream,txtptr,reply);
    (*env)->bcc = imap_parse_adrlist (stream,txtptr,reply);
    (*env)->in_reply_to = imap_parse_string (stream,txtptr,reply,NIL,NIL,
					     LONGT);
    (*env)->message_id = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
    if (oenv) {			/* need to merge old envelope? */
      (*env)->newsgroups = oenv->newsgroups;
      oenv->newsgroups = NIL;
      (*env)->ngbogus = oenv->ngbogus;
      (*env)->followup_to = oenv->followup_to;
      oenv->followup_to = NIL;
      (*env)->references = oenv->references;
      oenv->references = NIL;
      mail_free_envelope(&oenv);/* free old envelope */
    }
    if (**txtptr != ')') {
      sprintf (LOCAL->tmp,"Junk at end of envelope: %.80s",*txtptr);
      mm_log (LOCAL->tmp,WARN);
    }
    else ++*txtptr;		/* skip past delimiter */
    break;
  case 'N':			/* if NIL */
  case 'n':
    ++*txtptr;			/* bump past "I" */
    ++*txtptr;			/* bump past "L" */
    break;
  default:
    sprintf (LOCAL->tmp,"Not an envelope: %.80s",*txtptr);
    mm_log (LOCAL->tmp,WARN);
    break;
  }
}

/* IMAP parse address list
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 * Returns: address list, NIL on failure
 *
 * Updates text pointer
 */

ADDRESS *imap_parse_adrlist (MAILSTREAM *stream,char **txtptr,
			     IMAPPARSEDREPLY *reply)
{
  ADDRESS *adr = NIL;
  char c = **txtptr;		/* sniff at first character */
				/* ignore leading spaces */
  while (c == ' ') c = *++*txtptr;
  ++*txtptr;			/* skip past open paren */
  switch (c) {
  case '(':			/* if envelope S-expression */
    adr = imap_parse_address (stream,txtptr,reply);
    if (**txtptr != ')') {
      sprintf (LOCAL->tmp,"Junk at end of address list: %.80s",*txtptr);
      mm_log (LOCAL->tmp,WARN);
    }
    else ++*txtptr;		/* skip past delimiter */
    break;
  case 'N':			/* if NIL */
  case 'n':
    ++*txtptr;			/* bump past "I" */
    ++*txtptr;			/* bump past "L" */
    break;
  default:
    sprintf (LOCAL->tmp,"Not an address: %.80s",*txtptr);
    mm_log (LOCAL->tmp,WARN);
    break;
  }
  return adr;
}

/* IMAP parse address
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 * Returns: address, NIL on failure
 *
 * Updates text pointer
 */

ADDRESS *imap_parse_address (MAILSTREAM *stream,char **txtptr,
			     IMAPPARSEDREPLY *reply)
{
  ADDRESS *adr = NIL;
  ADDRESS *ret = NIL;
  ADDRESS *prev = NIL;
  char c = **txtptr;		/* sniff at first address character */
  switch (c) {
  case '(':			/* if envelope S-expression */
    while (c == '(') {		/* recursion dies on small stack machines */
      ++*txtptr;		/* skip past open paren */
      if (adr) prev = adr;	/* note previous if any */
      adr = mail_newaddr ();	/* instantiate address and parse its fields */
      adr->personal = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
      adr->adl = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
      adr->mailbox = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
      adr->host = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
      if (**txtptr != ')') {	/* handle trailing paren */
	sprintf (LOCAL->tmp,"Junk at end of address: %.80s",*txtptr);
	mm_log (LOCAL->tmp,WARN);
      }
      else ++*txtptr;		/* skip past close paren */
      c = **txtptr;		/* set up for while test */
				/* ignore leading spaces in front of next */
      while (c == ' ') c = *++*txtptr;
      if (!ret) ret = adr;	/* if first time note first adr */
				/* if previous link new block to it */
      if (prev) prev->next = adr;
    }
    break;

  case 'N':			/* if NIL */
  case 'n':
    *txtptr += 3;		/* bump past NIL */
    break;
  default:
    sprintf (LOCAL->tmp,"Not an address: %.80s",*txtptr);
    mm_log (LOCAL->tmp,WARN);
    break;
  }
  return ret;
}

/* IMAP parse flags
 * Accepts: current message cache
 *	    current text pointer
 *
 * Updates text pointer
 */

void imap_parse_flags (MAILSTREAM *stream,MESSAGECACHE *elt,char **txtptr)
{
  char *flag;
  char c = '\0';
  struct {			/* old flags */
    unsigned int valid : 1;
    unsigned int seen : 1;
    unsigned int deleted : 1;
    unsigned int flagged : 1;
    unsigned int answered : 1;
    unsigned int draft : 1;
    unsigned long user_flags;
  } old;
  old.valid = elt->valid; old.seen = elt->seen; old.deleted = elt->deleted;
  old.flagged = elt->flagged; old.answered = elt->answered;
  old.draft = elt->draft; old.user_flags = elt->user_flags;
  elt->valid = T;		/* mark have valid flags now */
  elt->user_flags = NIL;	/* zap old flag values */
  elt->seen = elt->deleted = elt->flagged = elt->answered = elt->draft =
    elt->recent = NIL;
  while (c != ')') {		/* parse list of flags */
				/* point at a flag */
    while (*(flag = ++*txtptr) == ' ');
				/* scan for end of flag */
    while (**txtptr != ' ' && **txtptr != ')') ++*txtptr;
    c = **txtptr;		/* save delimiter */
    **txtptr = '\0';		/* tie off flag */
    if (!*flag) break;		/* null flag */
				/* if starts with \ must be sys flag */
    else if (*ucase (flag) == '\\') {
      if (!strcmp (flag,"\\SEEN")) elt->seen = T;
      else if (!strcmp (flag,"\\DELETED")) elt->deleted = T;
      else if (!strcmp (flag,"\\FLAGGED")) elt->flagged = T;
      else if (!strcmp (flag,"\\ANSWERED")) elt->answered = T;
      else if (!strcmp (flag,"\\RECENT")) elt->recent = T;
      else if (!strcmp (flag,"\\DRAFT")) elt->draft = T;
    }
				/* otherwise user flag */
    else elt->user_flags |= imap_parse_user_flag (stream,flag);
  }
  ++*txtptr;			/* bump past delimiter */
  if (!old.valid || (old.seen != elt->seen) ||
      (old.deleted != elt->deleted) || (old.flagged != elt->flagged) ||
      (old.answered != elt->answered) || (old.draft != elt->draft) ||
      (old.user_flags != elt->user_flags)) mm_flags (stream,elt->msgno);
}


/* IMAP parse user flag
 * Accepts: MAIL stream
 *	    flag name
 * Returns: flag bit position
 */

unsigned long imap_parse_user_flag (MAILSTREAM *stream,char *flag)
{
  long i;
				/* sniff through all user flags */
  for (i = 0; i < NUSERFLAGS; ++i) if (stream->user_flags[i])
    if (!compare_cstring (flag,stream->user_flags[i])) return (1 << i);
  return (unsigned long) 0;	/* not found */
}

/* IMAP parse atom-string
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 *	    returned string length
 * Returns: string
 *
 * Updates text pointer
 */

char *imap_parse_astring (MAILSTREAM *stream,char **txtptr,
			  IMAPPARSEDREPLY *reply,unsigned long *len)
{
  unsigned long i;
  char c,*s,*ret;
				/* ignore leading spaces */
  for (c = **txtptr; c == ' '; c = *++*txtptr);
  switch (c) {
  case '"':			/* quoted string? */
  case '{':			/* literal? */
    ret = imap_parse_string (stream,txtptr,reply,NIL,len,NIL);
    break;
  default:			/* must be atom */
				/* find end of atom */
    for (c = *(s = *txtptr); c; c = *++*txtptr)
      if ((c <= ' ') || (c == '(') || (c == ')') || (c == '{') || (c == '%') ||
	  (c == '*') || (c == '"') || (c == '\\') || (c & 0x80)) break;
    if (c) i = *txtptr - s;	/* atom ends at atom_special */
				/* else atom is entire string */
    else *txtptr = s + (i = strlen (s));
    if (len) *len = i;		/* return length of atom */
    ret = strncpy ((char *) fs_get (i + 1),s,i);
    ret[i] = '\0';		/* tie off string */
    break;
  }
  return ret;
}

/* IMAP parse string
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 *	    mailgets data
 *	    returned string length
 *	    filter newline flag
 * Returns: string
 *
 * Updates text pointer
 */

char *imap_parse_string (MAILSTREAM *stream,char **txtptr,
			 IMAPPARSEDREPLY *reply,GETS_DATA *md,
			 unsigned long *len,long flags)
{
  char *st;
  char *string = NIL;
  unsigned long i,j,k;
  unsigned char c = **txtptr;	/* sniff at first character */
  mailgets_t mg = (mailgets_t) mail_parameters (NIL,GET_GETS,NIL);
  readprogress_t rp =
    (readprogress_t) mail_parameters (NIL,GET_READPROGRESS,NIL);
				/* ignore leading spaces */
  while (c == ' ') c = *++*txtptr;
  st = ++*txtptr;		/* remember start of string */
  switch (c) {
  case '"':			/* if quoted string */
    i = 0;			/* initial byte count */
				/* search for end of string */
    for (c = **txtptr; c != '"'; ++i,c = *++*txtptr) {
      if (c == '\\') c = *++*txtptr;
      if (!c || (c & 0x80)) {	/* server bogon */
	sprintf (LOCAL->tmp,"Invalid CHAR in quoted string: %x",
		 (unsigned int) c);
	mm_log (LOCAL->tmp,WARN);
	do {			/* search for end with no further check */
	  ++i;			/* count this character */
	  c = *++*txtptr;	/* get next character */
	  if (c == '\\') c = *++*txtptr;
	} while (c != '"');	/* until find delimiter */
	break;			/* exit loop */
      }
    }
    ++*txtptr;			/* bump past delimiter */
    string = (char *) fs_get ((size_t) i + 1);
    for (j = 0; j < i; j++) {	/* copy the string */
      if (*st == '\\') ++st;	/* quoted character */
      string[j] = *st++;
    }
    string[j] = '\0';		/* tie off string */
    if (len) *len = i;		/* set return value too */
    if (md && mg) {		/* have special routine to slurp string? */
      STRING bs;
      if (md->first) {		/* partial fetch? */
	md->first--;		/* restore origin octet */
	md->last = i;		/* number of octets that we got */
      }
      INIT (&bs,mail_string,string,i);
      (*mg) (mail_read,&bs,i,md);
    }
    break;
  case 'N':			/* if NIL */
  case 'n':
    ++*txtptr;			/* bump past "I" */
    ++*txtptr;			/* bump past "L" */
    if (len) *len = 0;
    break;

  case '{':			/* if literal string */
				/* get size of string */ 
    i = strtoul (*txtptr,txtptr,10);
    if (len) *len = i;		/* set return value */
    if (md && mg) {		/* have special routine to slurp string? */
      if (md->first) {		/* partial fetch? */
	md->first--;		/* restore origin octet */
	md->last = i;		/* number of octets that we got */
      }
      else md->flags |= MG_COPY;/* otherwise flag need to copy */
      string = (*mg) (net_getbuffer,LOCAL->netstream,i,md);
    }
    else {			/* must slurp into free storage */
      string = (char *) fs_get ((size_t) i + 1);
      *string = '\0';		/* init in case getbuffer fails */
				/* get the literal */
      if (rp) for (k = 0; j = min ((long) MAILTMPLEN,(long) i); i -= j) {
	net_getbuffer (LOCAL->netstream,j,string + k);
	(*rp) (md,k += j);
      }
      else net_getbuffer (LOCAL->netstream,i,string);
    }
    fs_give ((void **) &reply->line);
    if (flags && string)	/* need to filter newlines? */
      for (st = string; st = strpbrk (st,"\015\012\011"); *st++ = ' ');
				/* get new reply text line */
    reply->line = net_getline (LOCAL->netstream);
    if (stream->debug) mm_dlog (reply->line);
    *txtptr = reply->line;	/* set text pointer to point at it */
    break;
  default:
    sprintf (LOCAL->tmp,"Not a string: %c%.80s",c,*txtptr);
    mm_log (LOCAL->tmp,WARN);
    if (len) *len = 0;
    break;
  }
  return string;
}

/* Register text in IMAP cache
 * Accepts: MAIL stream
 *	    message number
 *	    IMAP segment specifier
 *	    header string list (if a HEADER section specifier)
 *	    sized text to register
 * Returns: non-zero if cache non-empty
 */

long imap_cache (MAILSTREAM *stream,unsigned long msgno,char *seg,
		 STRINGLIST *stl,SIZEDTEXT *text)
{
  char *t,tmp[MAILTMPLEN];
  unsigned long i;
  BODY *b;
  SIZEDTEXT *ret;
  STRINGLIST *stc;
  MESSAGECACHE *elt = mail_elt (stream,msgno);
				/* top-level header never does mailgets */
  if (!strcmp (seg,"HEADER") || !strcmp (seg,"0") ||
      !strcmp (seg,"HEADER.FIELDS") || !strcmp (seg,"HEADER.FIELDS.NOT")) {
    ret = &elt->private.msg.header.text;
    if (text) {			/* don't do this if no text */
      if (ret->data) fs_give ((void **) &ret->data);
      mail_free_stringlist (&elt->private.msg.lines);
      elt->private.msg.lines = stl;
				/* prevent cache reuse of .NOT */
      if ((seg[0] == 'H') && (seg[6] == '.') && (seg[13] == '.'))
	for (stc = stl; stc; stc = stc->next) stc->text.size = 0;
      if (stream->scache) {	/* short caching puts it in the stream */
	if (stream->msgno != msgno) {
				/* flush old stuff */
	  mail_free_envelope (&stream->env);
	  mail_free_body (&stream->body);
	  stream->msgno = msgno;
	}
	imap_parse_header (stream,&stream->env,text,stl);
      }
				/* regular caching */
      else imap_parse_header (stream,&elt->private.msg.env,text,stl);
    }
  }
				/* top level text */
  else if (!strcmp (seg,"TEXT")) {
    ret = &elt->private.msg.text.text;
    if (text && ret->data) fs_give ((void **) &ret->data);
  }
  else if (!*seg) {		/* full message */
    ret = &elt->private.msg.full.text;
    if (text && ret->data) fs_give ((void **) &ret->data);
  }

  else {			/* nested, find non-contents specifier */
    for (t = seg; *t && !((*t == '.') && (isalpha(t[1]) || !atol (t+1))); t++);
    if (*t) *t++ = '\0';	/* tie off section from data specifier */
    if (!(b = mail_body (stream,msgno,seg))) {
      sprintf (tmp,"Unknown section number: %.80s",seg);
      mm_log (tmp,WARN);
      return NIL;
    }
    if (*t) {			/* if a non-numberic subpart */
      if ((i = (b->type == TYPEMESSAGE) && (!strcmp (b->subtype,"RFC822"))) &&
	  (!strcmp (t,"HEADER") || !strcmp (t,"0") ||
	   !strcmp (t,"HEADER.FIELDS") || !strcmp (t,"HEADER.FIELDS.NOT"))) {
	ret = &b->nested.msg->header.text;
	if (text) {
	  if (ret->data) fs_give ((void **) &ret->data);
	  mail_free_stringlist (&b->nested.msg->lines);
	  b->nested.msg->lines = stl;
				/* prevent cache reuse of .NOT */
	  if ((t[0] == 'H') && (t[6] == '.') && (t[13] == '.'))
	    for (stc = stl; stc; stc = stc->next) stc->text.size = 0;
	  imap_parse_header (stream,&b->nested.msg->env,text,stl);
	}
      }
      else if (i && !strcmp (t,"TEXT")) {
	ret = &b->nested.msg->text.text;
	if (text && ret->data) fs_give ((void **) &ret->data);
      }
				/* otherwise it must be MIME */
      else if (!strcmp (t,"MIME")) {
	ret = &b->mime.text;
	if (text && ret->data) fs_give ((void **) &ret->data);
      }
      else {
	sprintf (tmp,"Unknown section specifier: %.80s.%.80s",seg,t);
	mm_log (tmp,WARN);
	return NIL;
      }
    }
    else {			/* ordinary contents */
      ret = &b->contents.text;
      if (text && ret->data) fs_give ((void **) &ret->data);
    }
  }
  if (text) {			/* update cache if requested */
    ret->data = text->data;
    ret->size = text->size;
  }
  return ret->data ? LONGT : NIL;
}

/* IMAP parse body structure
 * Accepts: MAIL stream
 *	    body structure to write into
 *	    current text pointer
 *	    parsed reply
 *
 * Updates text pointer
 */

void imap_parse_body_structure (MAILSTREAM *stream,BODY *body,char **txtptr,
				IMAPPARSEDREPLY *reply)
{
  int i;
  char *s;
  PART *part = NIL;
  char c = *((*txtptr)++);	/* grab first character */
				/* ignore leading spaces */
  while (c == ' ') c = *((*txtptr)++);
  switch (c) {			/* dispatch on first character */
  case '(':			/* body structure list */
    if (**txtptr == '(') {	/* multipart body? */
      body->type= TYPEMULTIPART;/* yes, set its type */
      do {			/* instantiate new body part */
	if (part) part = part->next = mail_newbody_part ();
	else body->nested.part = part = mail_newbody_part ();
				/* parse it */
	imap_parse_body_structure (stream,&part->body,txtptr,reply);
      } while (**txtptr == '(');/* for each body part */
      if (body->subtype = imap_parse_string (stream,txtptr,reply,NIL,NIL,
					     LONGT)) ucase (body->subtype);
      else {
	mm_log ("Missing multipart subtype",WARN);
	body->subtype = cpystr (rfc822_default_subtype (body->type));
      }
      if (**txtptr == ' ')	/* multipart parameters */
	body->parameter = imap_parse_body_parameter (stream,txtptr,reply);
      if (**txtptr == ' ')	/* disposition */
	imap_parse_disposition (stream,body,txtptr,reply);
      if (**txtptr == ' ')	/* language */
	body->language = imap_parse_language (stream,txtptr,reply);
      while (**txtptr == ' ') imap_parse_extension (stream,txtptr,reply);
      if (**txtptr != ')') {	/* validate ending */
	sprintf (LOCAL->tmp,"Junk at end of multipart body: %.80s",*txtptr);
	mm_log (LOCAL->tmp,WARN);
      }
      else ++*txtptr;		/* skip past delimiter */
    }

    else {			/* not multipart, parse type name */
      if (**txtptr == ')') {	/* empty body? */
	++*txtptr;		/* bump past it */
	break;			/* and punt */
      }
      body->type = TYPEOTHER;	/* assume unknown type */
      body->encoding = ENCOTHER;/* and unknown encoding */
				/* parse type */
      if (s = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT)) {
	ucase (s);
	for (i=0;(i<=TYPEMAX) && body_types[i] && strcmp(s,body_types[i]);i++);
	if (i <= TYPEMAX) {	/* only if found a slot */
	  body->type = i;	/* set body type */
	  if (body_types[i]) fs_give ((void **) &s);
	  else body_types[i]=s;	/* assign empty slot */
	}
      }
				/* parse subtype */
      if (body->subtype = imap_parse_string (stream,txtptr,reply,NIL,NIL,
					     LONGT)) ucase (body->subtype);
      else {
	mm_log ("Missing body subtype",WARN);
	body->subtype = cpystr (rfc822_default_subtype (body->type));
      }
      body->parameter = imap_parse_body_parameter (stream,txtptr,reply);
      body->id = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
      body->description = imap_parse_string (stream,txtptr,reply,NIL,NIL,
					     LONGT);
      if (s = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT)) {
	ucase (s);		/* search for body encoding */
	for (i = 0; (i <= ENCMAX) && body_encodings[i] &&
	     strcmp (s,body_encodings[i]); i++);
	if (i > ENCMAX) body->type = ENCOTHER;
	else {			/* only if found a slot */
	  body->encoding = i;	/* set body encoding */
	  if (body_encodings[i]) fs_give ((void **) &s);
				/* assign empty slot */
	  else body_encodings[i] = s;
	}
      }
				/* parse size of contents in bytes */
      body->size.bytes = strtoul (*txtptr,txtptr,10);
      switch (body->type) {	/* possible extra stuff */
      case TYPEMESSAGE:		/* message envelope and body */
	if (strcmp (body->subtype,"RFC822")) break;
	body->nested.msg = mail_newmsg ();
	imap_parse_envelope (stream,&body->nested.msg->env,txtptr,reply);
	body->nested.msg->body = mail_newbody ();
	imap_parse_body_structure (stream,body->nested.msg->body,txtptr,reply);
				/* drop into text case */
      case TYPETEXT:		/* size in lines */
	body->size.lines = strtoul (*txtptr,txtptr,10);
	break;
      default:			/* otherwise nothing special */
	break;
      }
      if (**txtptr == ' ')	/* if extension data */
	body->md5 = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT);
      if (**txtptr == ' ')	/* disposition */
	imap_parse_disposition (stream,body,txtptr,reply);
      if (**txtptr == ' ')	/* language */
	body->language = imap_parse_language (stream,txtptr,reply);
      while (**txtptr == ' ') imap_parse_extension (stream,txtptr,reply);
      if (**txtptr != ')') {	/* validate ending */
	sprintf (LOCAL->tmp,"Junk at end of body part: %.80s",*txtptr);
	mm_log (LOCAL->tmp,WARN);
      }
      else ++*txtptr;		/* skip past delimiter */
    }
    break;
  case 'N':			/* if NIL */
  case 'n':
    ++*txtptr;			/* bump past "I" */
    ++*txtptr;			/* bump past "L" */
    break;
  default:			/* otherwise quite bogus */
    sprintf (LOCAL->tmp,"Bogus body structure: %.80s",*txtptr);
    mm_log (LOCAL->tmp,WARN);
    break;
  }
}

/* IMAP parse body parameter
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 * Returns: body parameter
 * Updates text pointer
 */

PARAMETER *imap_parse_body_parameter (MAILSTREAM *stream,char **txtptr,
				      IMAPPARSEDREPLY *reply)
{
  PARAMETER *ret = NIL;
  PARAMETER *par = NIL;
  char c,*s;
				/* ignore leading spaces */
  while ((c = *(*txtptr)++) == ' ');
				/* parse parameter list */
  if (c == '(') while (c != ')') {
				/* append new parameter to tail */
    if (ret) par = par->next = mail_newbody_parameter ();
    else ret = par = mail_newbody_parameter ();
    if(!(par->attribute=imap_parse_string (stream,txtptr,reply,NIL,NIL,
					   LONGT))) {
      mm_log ("Missing parameter attribute",WARN);
      par->attribute = cpystr ("UNKNOWN");
    }
    if (!(par->value = imap_parse_string (stream,txtptr,reply,NIL,NIL,LONGT))){
      sprintf (LOCAL->tmp,"Missing value for parameter %.80s",par->attribute);
      mm_log (LOCAL->tmp,WARN);
      par->value = cpystr ("UNKNOWN");
    }
    switch (c = **txtptr) {	/* see what comes after */
    case ' ':			/* flush whitespace */
      while ((c = *++*txtptr) == ' ');
      break;
    case ')':			/* end of attribute/value pairs */
      ++*txtptr;		/* skip past closing paren */
      break;
    default:
      sprintf (LOCAL->tmp,"Junk at end of parameter: %.80s",*txtptr);
      mm_log (LOCAL->tmp,WARN);
      break;
    }
  }
				/* empty parameter, must be NIL */
  else if (((c == 'N') || (c == 'n')) &&
	   ((*(s = *txtptr) == 'I') || (*s == 'i')) &&
	   ((s[1] == 'L') || (s[1] == 'l'))) *txtptr += 2;
  else {
    sprintf (LOCAL->tmp,"Bogus body parameter: %c%.80s",c,(*txtptr) - 1);
    mm_log (LOCAL->tmp,WARN);
  }
  return ret;
}

/* IMAP parse body disposition
 * Accepts: MAIL stream
 *	    body structure to write into
 *	    current text pointer
 *	    parsed reply
 */

void imap_parse_disposition (MAILSTREAM *stream,BODY *body,char **txtptr,
			     IMAPPARSEDREPLY *reply)
{
  switch (*++*txtptr) {
  case '(':
    ++*txtptr;			/* skip open paren */
    body->disposition.type = imap_parse_string (stream,txtptr,reply,NIL,NIL,
						LONGT);
    body->disposition.parameter =
      imap_parse_body_parameter (stream,txtptr,reply);
    if (**txtptr != ')') {	/* validate ending */
      sprintf (LOCAL->tmp,"Junk at end of disposition: %.80s",*txtptr);
      mm_log (LOCAL->tmp,WARN);
    }
    else ++*txtptr;		/* skip past delimiter */
    break;
  case 'N':			/* if NIL */
  case 'n':
    ++*txtptr;			/* bump past "N" */
    ++*txtptr;			/* bump past "I" */
    ++*txtptr;			/* bump past "L" */
    break;
  default:
    sprintf (LOCAL->tmp,"Unknown body disposition: %.80s",*txtptr);
    mm_log (LOCAL->tmp,WARN);
				/* try to skip to next space */
    while ((*++*txtptr != ' ') && (**txtptr != ')') && **txtptr);
    break;
  }
}

/* IMAP parse body language
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 * Returns: string list or NIL if empty or error
 */

STRINGLIST *imap_parse_language (MAILSTREAM *stream,char **txtptr,
				 IMAPPARSEDREPLY *reply)
{
  unsigned long i;
  char *s;
  STRINGLIST *ret = NIL;
				/* language is a list */
  if (*++*txtptr == '(') ret = imap_parse_stringlist (stream,txtptr,reply);
  else if (s = imap_parse_string (stream,txtptr,reply,NIL,&i,LONGT)) {
    (ret = mail_newstringlist ())->text.data = (unsigned char *) s;
    ret->text.size = i;
  }
  return ret;
}

/* IMAP parse string list
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 * Returns: string list or NIL if empty or error
 */

STRINGLIST *imap_parse_stringlist (MAILSTREAM *stream,char **txtptr,
				   IMAPPARSEDREPLY *reply)
{
  STRINGLIST *stl = NIL;
  STRINGLIST *stc = NIL;
  char *t = *txtptr;
				/* parse the list */
  if (*t++ == '(') while (*t != ')') {
    if (stl) stc = stc->next = mail_newstringlist ();
    else stc = stl = mail_newstringlist ();
				/* parse astring */
    if (!(stc->text.data = (unsigned char *)
	  imap_parse_astring (stream,&t,reply,&stc->text.size))) {
      sprintf (LOCAL->tmp,"Bogus string list member: %.80s",t);
      mm_log (LOCAL->tmp,WARN);
      mail_free_stringlist (&stl);
      break;
    }
    else if (*t == ' ') ++t;	/* another token follows */
  }
  if (stl) *txtptr = ++t;	/* update return string */
  return stl;
}

/* IMAP parse unknown body extension data
 * Accepts: MAIL stream
 *	    current text pointer
 *	    parsed reply
 *
 * Updates text pointer
 */

void imap_parse_extension (MAILSTREAM *stream,char **txtptr,
			   IMAPPARSEDREPLY *reply)
{
  unsigned long i,j;
  switch (*++*txtptr) {		/* action depends upon first character */
  case '(':
    while (**txtptr != ')') imap_parse_extension (stream,txtptr,reply);
    ++*txtptr;			/* bump past closing parenthesis */
    break;
  case '"':			/* if quoted string */
    while (*++*txtptr != '"') if (**txtptr == '\\') ++*txtptr;
    ++*txtptr;			/* bump past closing quote */
    break;
  case 'N':			/* if NIL */
  case 'n':
    ++*txtptr;			/* bump past "N" */
    ++*txtptr;			/* bump past "I" */
    ++*txtptr;			/* bump past "L" */
    break;
  case '{':			/* get size of literal */
    ++*txtptr;			/* bump past open squiggle */
    if (i = strtoul (*txtptr,txtptr,10)) do
      net_getbuffer (LOCAL->netstream,j = max (i,(long) IMAPTMPLEN - 1),
		     LOCAL->tmp);
    while (i -= j);
				/* get new reply text line */
    reply->line = net_getline (LOCAL->netstream);
    if (stream->debug) mm_dlog (reply->line);
    *txtptr = reply->line;	/* set text pointer to point at it */
    break;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    strtoul (*txtptr,txtptr,10);
    break;
  default:
    sprintf (LOCAL->tmp,"Unknown extension token: %.80s",*txtptr);
    mm_log (LOCAL->tmp,WARN);
				/* try to skip to next space */
    while ((*++*txtptr != ' ') && (**txtptr != ')') && **txtptr);
    break;
  }
}

/* IMAP parse capabilities
 * Accepts: MAIL stream
 *	    capability list
 */

void imap_parse_capabilities (MAILSTREAM *stream,char *t)
{
  unsigned long i;
  LOCAL->gotcapability = T;	/* flag that capabilities arrived */
  for (t = (char *) strtok (ucase (t)," "); t; t = (char *) strtok (NIL," ")) {
    if (!strcmp (t,"IMAP4")) LOCAL->cap.imap4 = T;
    else if (!strcmp (t,"IMAP4REV1")) LOCAL->cap.imap4rev1 = T;
    else if (!strcmp (t,"ACL")) LOCAL->cap.acl = T;
    else if (!strcmp (t,"QUOTA")) LOCAL->cap.quota = T;
    else if (!strcmp (t,"NAMESPACE")) LOCAL->cap.namespace = T;
    else if (!strcmp (t,"MAILBOX-REFERRALS")) LOCAL->cap.mbx_ref = T;
    else if (!strcmp (t,"LOGIN-REFERRALS")) LOCAL->cap.log_ref = T;
    else if (!strcmp (t,"STARTTLS")) LOCAL->cap.starttls = T;
    else if (!strcmp (t,"LOGINDISABLED")) LOCAL->logindisabled = T;
    else if (!strcmp (t,"MULTIAPPEND")) LOCAL->cap.multiappend = T;
    else if (!strcmp (t,"SCAN")) LOCAL->cap.scan = T;
    else if (!strncmp (t,"SORT",4)) LOCAL->cap.sort = T;
    else if (!strncmp (t,"THREAD=",7)) {
      THREADER *thread = (THREADER *) fs_get (sizeof (THREADER));
      thread->name = cpystr (t+7);
      thread->dispatch = NIL;
      thread->next = LOCAL->threader;
      LOCAL->threader = thread;
    }
    else if (!strncmp (t,"AUTH",4) && ((t[4] == '=') || (t[4] == '-'))) {
      if ((i = mail_lookup_auth_name (t+5,LOCAL->authflags)) &&
	  (--i < MAXAUTHENTICATORS)) LOCAL->cap.auth |= (1 << i);
      else if (!strcmp (t+5,"ANONYMOUS")) LOCAL->cap.authanon = T;
    }
				/* unsupported IMAP4 extension */
    else if (!strcmp (t,"STATUS")) LOCAL->cap.status = T;
				/* ignore other capabilities */
  }
				/* disable LOGIN if PLAIN also advertised */
  if ((i = mail_lookup_auth_name ("PLAIN",NIL)) && (--i < MAXAUTHENTICATORS) &&
      (LOCAL->cap.auth & (1 << i)) &&
      (i = mail_lookup_auth_name ("LOGIN",NIL)) && (--i < MAXAUTHENTICATORS))
    LOCAL->cap.auth &= ~(1 << i);
}

/* IMAP return host name
 * Accepts: MAIL stream
 * Returns: host name
 */

char *imap_host (MAILSTREAM *stream)
{
				/* return host name on stream if open */
  return (LOCAL && LOCAL->netstream) ? net_host (LOCAL->netstream) :
    ".NO-IMAP-CONNECTION.";
}

/* IMAP load cache
 * Accepts: MAIL stream
 *	    sequence
 *	    flags
 * Returns: parsed reply from fetch
 */

IMAPPARSEDREPLY *imap_fetch (MAILSTREAM *stream,char *sequence,long flags)
{
  int i = 2;
  char *cmd = (LEVELIMAP4 (stream) && (flags & FT_UID)) ?
    "UID FETCH" : "FETCH";
  IMAPARG *args[9],aseq,aarg,aenv,ahhr,axtr,ahtr,abdy,atrl;
  args[0] = &aseq; aseq.type = SEQUENCE; aseq.text = (void *) sequence;
  args[1] = &aarg; aarg.type = ATOM;
  aenv.type = ATOM; aenv.text = (void *) "ENVELOPE";
  ahhr.type = ATOM; ahhr.text = (void *) hdrheader;
  axtr.type = ATOM; axtr.text = (void *) imap_extrahdrs;
  ahtr.type = ATOM; ahtr.text = (void *) hdrtrailer;
  abdy.type = ATOM; abdy.text = (void *) "BODYSTRUCTURE";
  atrl.type = ATOM; atrl.text = (void *) "INTERNALDATE RFC822.SIZE FLAGS)";
  if (LEVELIMAP4 (stream)) {	/* include UID if IMAP4 or IMAP4rev1 */
    aarg.text = (void *) "(UID";
    if (flags & FT_NEEDENV) {	/* if need envelopes */
      args[i++] = &aenv;	/* include envelope */
				/* extra header poop if IMAP4rev1 */
      if (!(flags & FT_NOHDRS) && LEVELIMAP4rev1 (stream)) {
	args[i++] = &ahhr;	/* header header */
	if (axtr.text) args[i++] = &axtr;
	args[i++] = &ahtr;	/* header trailer */
      }
				/* fetch body if requested */
      if (flags & FT_NEEDBODY) args[i++] = &abdy;
    }
    args[i++] = &atrl;		/* fetch trailer */
  }
				/* easy if IMAP2 */
  else aarg.text = (void *) (flags & FT_NEEDENV) ?
    ((flags & FT_NEEDBODY) ? "FULL" : "ALL") : "FAST";
  args[i] = NIL;		/* tie off command */
  return imap_send (stream,cmd,args);
}
