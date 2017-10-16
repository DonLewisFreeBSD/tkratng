/*
 * ratStdFolder.h --
 *
 *      Declarations of functions used in the Std folder and messages
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forss�n
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#ifndef _RATPGP
#define _RATPGP

/* ratPGP.c */
extern char *RatPGPPhrase(Tcl_Interp *interp);
extern char *RatSenderPGPPhrase(Tcl_Interp *interp);
extern Tcl_TimerProc ClearPGPPass;
extern void RatPGPBodyCheck(Tcl_Interp *interp, MessageProcInfo *procInfo,
			    BodyInfo **bodyInfoPtrPtr);
extern Tcl_ObjCmdProc RatPGPCmd;
extern char *RatPGPStrFind(char *haystack, long straws, char *needle,
			   int linestart);

/* ratPGPprog.c */
extern BODY *RatPGPEncrypt(Tcl_Interp *interp, ENVELOPE *env, BODY *body,
			   int sign);
extern BODY *RatPGPSign(Tcl_Interp *interp, ENVELOPE *env, BODY *body,
			const char *keyid);
extern void RatPGPChecksig(Tcl_Interp *interp, MessageProcInfo *procInfo,
			   BodyInfo *bodyInfoPtr);
extern void RatPGPDecrypt(Tcl_Interp *interp, MessageProcInfo *procInfo,
			  BodyInfo **bodyInfoPtrPtr);
extern int RatPGPListKeys(Tcl_Interp *interp, char *keyring);
extern int RatPGPExtractKey(Tcl_Interp *interp, char *id, char *keyring);
extern int RatPGPAddKeys(Tcl_Interp *interp, char *keys, char *keyring);
extern void RatPGPHandleOld(Tcl_Interp *interp, BodyInfo *bodyInfoPtr,
			     char *text, char *start, char *end);

#endif /* _RATPGP */
