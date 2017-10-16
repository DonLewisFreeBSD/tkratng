/*
 * ratAddrlist.c --
 *
 *	This file contains the native support for searching in the address list
 *
 * TkRat software and its included text is Copyright 1996-2005 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"


/*
 *----------------------------------------------------------------------
 *
 * GetMatchingAddrsImplCmd --
 *
 *      This routine creates an address command by an address given
 *	as argument
 *
 * Results:
 *	A list of address entity names is appended to the result
 *
 * Side effects:
 *	New address entities are created,
 *
 *
 *----------------------------------------------------------------------
 */

int
RatGetMatchingAddrsImplCmd(ClientData clientData, Tcl_Interp *interp, int objc,
                           Tcl_Obj *CONST objv[])
{
    int i, listc, max, matchlen, found=0;
    Tcl_Obj **listv, *ret, *o;
    char *match, *name, *email, buf[1024];
    ADDRESS adr;

    if (4 != objc
        || TCL_OK != Tcl_ListObjGetElements(interp, objv[1], &listc, &listv)
        || TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &max)) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]),
                         " addrlist match max", (char*)NULL);
	return TCL_ERROR;
    }
    match = Tcl_GetStringFromObj(objv[2], &matchlen);

    ret = Tcl_NewObj();
    for (i=0; i<listc && found<max; i+=2) {
        email = Tcl_GetString(listv[i]);
        name = Tcl_GetString(listv[i+1]);
        if (0 == strncasecmp(match, email, matchlen)
            || 0 == strncasecmp(match, name, matchlen)) {
            if (strlen(name)) {
                strlcpy(buf, email, sizeof(buf));
                adr.personal = name;
                adr.adl = NULL;
                adr.mailbox = buf;
                adr.host = strchr(buf, '@');
                if (adr.host) {
                    *adr.host++ = '\0';
                } else {
                    adr.host = NODOMAIN;
                }
                adr.error = NULL;
                adr.next = NULL;
                o = Tcl_NewStringObj(RatAddressFull(interp, &adr, NULL), -1);
            } else {
                o = listv[i];
            }
            if (!strcmp(Tcl_GetString(o), match)) {
                /* Free it if refcount equals zero */
                Tcl_IncrRefCount(o);
                Tcl_DecrRefCount(o);
                continue;
            }
            Tcl_ListObjAppendElement(interp, ret, o);
            found++;
        }
    }

    Tcl_SetObjResult(interp, ret);
    return TCL_OK;
}
