/*
 * ratSequence.c --
 *
 *      This file contains code to build seqences of message numbers
 *
 * TkRat software and its included text is Copyright 1996-2005 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"

#define CHUNKSIZE 256

typedef struct {
    int used;
    int allocated;
    unsigned long *elems;
    Tcl_DString string;
} rat_int_sequence_t;

rat_sequence_t
RatSequenceInit(void)
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)ckalloc(sizeof(*rs));

    rs->used = 0;
    rs->allocated = 0;
    rs->elems = NULL;
    Tcl_DStringInit(&rs->string);
    
    return (rat_sequence_t)rs;
}

void
RatSequenceAdd(rat_sequence_t seq, unsigned long elem)
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)seq;
    int i;
    
    if (rs->used == rs->allocated) {
        rs->allocated += CHUNKSIZE;
        rs->elems = (unsigned long*)ckrealloc(
            rs->elems, rs->allocated * sizeof(unsigned long));
    }

    i=0;
    while (rs->elems[i] < elem && i < rs->used) {
        i++;
    }

    if (i == rs->used) {
        rs->elems[rs->used] = elem;
    } else {
        if (rs->elems[i] == elem) {
            return;
        }
        memmove(&rs->elems[i+1], &rs->elems[i],
                sizeof(unsigned long)*(rs->used-i));
        rs->elems[i] = elem;
    }
    rs->used++;
}

int
RatSequenceNotempty(rat_sequence_t seq)
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)seq;
    return rs->used;
}

char*
RatSequenceGet(rat_sequence_t seq)
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)seq;
    char buf[32];
    int i, j;
    
    if (Tcl_DStringLength(&rs->string)) {
        Tcl_DStringSetLength(&rs->string, 0);
    }

    for (i=0; i<rs->used; i++) {
        if (Tcl_DStringLength(&rs->string)) {
            Tcl_DStringAppend(&rs->string, ",", 1);
        }
        snprintf(buf, sizeof(buf), "%lu", rs->elems[i]);
        Tcl_DStringAppend(&rs->string, buf, -1);
        j = i;
        while(j < rs->used && rs->elems[j]+1 == rs->elems[j+1]) {
            j++;
        }
        if (j > i+1) {
            snprintf(buf, sizeof(buf), ":%lu", rs->elems[j]);
            Tcl_DStringAppend(&rs->string, buf, -1);
            i = j;
        }
    }
    
    return Tcl_DStringValue(&rs->string);
}

void
RatSequenceFree(rat_sequence_t seq)
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)seq;
    Tcl_DStringFree(&rs->string);
    ckfree(rs->elems);
    ckfree(rs);
}

/*
 * These functions are used for unit-testing
 */
static Tcl_ObjCmdProc RatSequenceCmd;
static Tcl_CmdDeleteProc RatSequenceDelProc;

int
RatCreateSequenceCmd(ClientData dummy, Tcl_Interp *interp, int objc,
                     Tcl_Obj *const objv[])
{
    static int unique = 0;
    char name[32];

    snprintf(name, sizeof(name), "seq%d", unique++);
    Tcl_CreateObjCommand(interp,  name, RatSequenceCmd, RatSequenceInit(),
                         RatSequenceDelProc);
    Tcl_SetResult(interp, name, TCL_VOLATILE);
    return TCL_OK;
}

static int
RatSequenceCmd(ClientData clientData, Tcl_Interp *interp, int objc,
               Tcl_Obj *const objv[])
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)clientData;
    long no;

    if (objc == 3
        && !strcmp("add", Tcl_GetString(objv[1]))
        && TCL_OK == Tcl_GetLongFromObj(interp, objv[2], &no)) {
        RatSequenceAdd(rs, no);
    } else if (objc == 2 && !strcmp("get", Tcl_GetString(objv[1]))) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(RatSequenceGet(rs), -1));
    } else if (objc == 2 && !strcmp("notempty", Tcl_GetString(objv[1]))) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(RatSequenceNotempty(rs)));
    } else {
        return TCL_ERROR;
    }
    return TCL_OK;
}

static void
RatSequenceDelProc(ClientData clientData)
{
    rat_int_sequence_t *rs = (rat_int_sequence_t*)clientData;
    RatSequenceFree(rs);
}
