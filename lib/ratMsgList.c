/*
 * ratMsgList.c --
 *
 *	This file contains code which handles message listing
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"


/*
 *----------------------------------------------------------------------
 *
 * RatParseList --
 *
 *      Parse a list expression (almost like a printf format string)
 *
 * Results:
 *	A structure representing the parsed expression, or null if
 *	there is a syntax error in the format string.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

ListExpression*
RatParseList(const char *format, char *error)
{
    ListExpression *expPtr;
    int i, w, expIndex, bufLen, num;
    char buf[1024];

    for(i=num=0; '\0' != format[i]; i++) {
	if ('%' == format[i] && format[i+1] && '%' != format[i+1]) {
	    while (format[++i] && ('-' == format[i]
		    || isdigit((unsigned char)format[i])));
	    if (!strchr("scnNmrRbBdDSitMu", format[i])) {
                if (error != NULL) {
                    *error = format[i];
                }
		return NULL;
	    }
	    num++;
	}
    }
    expPtr = (ListExpression*)ckalloc(sizeof(ListExpression));
    expPtr->size = num;
    expPtr->preString = (char**)ckalloc(num*sizeof(char*));
    expPtr->typeList =
	(RatFolderInfoType*)ckalloc(num*sizeof(RatFolderInfoType));
    expPtr->fieldWidth = (int*)ckalloc(num*sizeof(int));
    expPtr->leftJust = (int*)ckalloc(num*sizeof(int));
    for (i = expIndex = bufLen = 0; format[i]; i++) {
	if ('%' == format[i]) {
	    if ('%' == format[++i]) {
		buf[bufLen++] = format[i];
		continue;
	    }
	    buf[bufLen] = '\0';
	    expPtr->preString[expIndex] = cpystr(buf);
	    if ('-' == format[i]) {
		expPtr->leftJust[expIndex] = 1;
		i++;
	    } else {
		expPtr->leftJust[expIndex] = 0;
	    }
	    w=0;
	    while (isdigit((unsigned char)format[i])) {
		w = w*10+format[i++]-'0';
	    }
	    expPtr->fieldWidth[expIndex] = w;
	    switch(format[i]) {
	    case 's': expPtr->typeList[expIndex++] = RAT_FOLDER_SUBJECT; break;
	    case 'c': expPtr->typeList[expIndex++] = RAT_FOLDER_CANONSUBJECT; break;
	    case 'n': expPtr->typeList[expIndex++] = RAT_FOLDER_NAME; break;
	    case 'N': expPtr->typeList[expIndex++] = RAT_FOLDER_ANAME; break;
	    case 'm': expPtr->typeList[expIndex++] = RAT_FOLDER_MAIL; break;
	    case 'r': expPtr->typeList[expIndex++] = RAT_FOLDER_NAME_RECIPIENT;
		    break;
	    case 'R': expPtr->typeList[expIndex++] = RAT_FOLDER_MAIL_RECIPIENT;
		    break;
	    case 'b': expPtr->typeList[expIndex++] = RAT_FOLDER_SIZE; break;
	    case 'B': expPtr->typeList[expIndex++] = RAT_FOLDER_SIZE_F; break;
	    case 'd': expPtr->typeList[expIndex++] = RAT_FOLDER_DATE_F; break;
	    case 'D': expPtr->typeList[expIndex++] = RAT_FOLDER_DATE_N; break;
	    case 'S': expPtr->typeList[expIndex++] = RAT_FOLDER_STATUS; break;
	    case 'i': expPtr->typeList[expIndex++] = RAT_FOLDER_INDEX; break;
	    case 't': expPtr->typeList[expIndex++] =RAT_FOLDER_THREADING;break;
	    case 'M': expPtr->typeList[expIndex++] = RAT_FOLDER_MSGID; break;
	    case 'u': expPtr->typeList[expIndex++] = RAT_FOLDER_UID; break;
	    }
	    bufLen = 0;
	} else {
	    buf[bufLen++] = format[i];
	}
    }
    if (bufLen) {
	buf[bufLen] = '\0';
	expPtr->postString = cpystr(buf);
    } else {
	expPtr->postString = NULL;
    }
    return expPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatFreeListExpression --
 *
 *      Frees all memory associated with a list expression.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Some memory is freed.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatFreeListExpression(ListExpression *exPtr)
{
    int i;

    for (i=0; i<exPtr->size; i++) {
	ckfree(exPtr->preString[i]);
    }
    ckfree(exPtr->preString);
    ckfree(exPtr->typeList);
    ckfree(exPtr->fieldWidth);
    ckfree(exPtr->leftJust);
    ckfree(exPtr->postString);
    ckfree(exPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * RatDoList --
 *
 *      Print the list information about a message.
 *
 * Results:
 *	A tcl object
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatDoList(Tcl_Interp *interp, ListExpression *exprPtr, RatInfoProc *infoProc,
	ClientData clientData, int index)
{
    Tcl_Obj *oPtr = Tcl_NewObj(), *iPtr;
    char *str;
    unsigned char *s2 = NULL;
    int i, j, slen, length;
 
    for (i=0; i<exprPtr->size; i++) {
	if (exprPtr->preString[i]) {
	    Tcl_AppendToObj(oPtr, exprPtr->preString[i], -1);
	}
	iPtr = (*infoProc)(interp, clientData, exprPtr->typeList[i], index);
	if (!iPtr) {
	    for (j=0; j<exprPtr->fieldWidth[i]; j++) {
		Tcl_AppendToObj(oPtr, " ", 1);
	    }
	    continue;
	}
	str = Tcl_GetStringFromObj(iPtr, &slen);
	for (j=0; j<slen && str[j] > ' '; j++);
	if (j < slen) {
	    s2 = (unsigned char*)cpystr(str);
	    for (j=0; j<slen; j++) {
		if (s2[j] < ' ') {
		    s2[j] = ' ';
		}
	    }
	    str = (char*)s2;
	}
	if (exprPtr->fieldWidth[i]) {
	    length = Tcl_NumUtfChars(str, slen);
	    if (length > exprPtr->fieldWidth[i]) {
		j = Tcl_UtfAtIndex(str, exprPtr->fieldWidth[i]) - str;
		Tcl_AppendToObj(oPtr, str, j);
	    } else {
		if (exprPtr->leftJust[i]) {
		    Tcl_AppendToObj(oPtr, str, slen);
		    for (j=length; j<exprPtr->fieldWidth[i]; j++) {
			Tcl_AppendToObj(oPtr, " ", 1);
		    }
		} else {
		    for (j=length; j<exprPtr->fieldWidth[i]; j++) {
			Tcl_AppendToObj(oPtr, " ", 1);
		    }
		    Tcl_AppendToObj(oPtr, str, slen);
		}
	    }
	} else {
	    Tcl_AppendToObj(oPtr, str, slen);
	}
	if (s2) {
	    ckfree(s2);
	    s2 = NULL;
	}
    }
    if (exprPtr->postString) {
	Tcl_AppendToObj(oPtr, exprPtr->postString, -1);
    }
    return oPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCheckListFormatCmd --
 *
 *      CHeck if the given list format is correct
 *
 * Results:
 *	A tcl object
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
int
RatCheckListFormatCmd(ClientData dummy, Tcl_Interp *interp, int objc,
                     Tcl_Obj *const objv[])
{
    ListExpression *list;
    char error, buf[1024];
    Tcl_Obj *msg;

    if (objc != 2) {
 	Tcl_AppendResult(interp, "Missing parameter", TCL_STATIC);
	return TCL_ERROR;
    }

    list = RatParseList(Tcl_GetString(objv[1]), &error);
    if (list != NULL) {
 	Tcl_SetResult(interp, "ok", TCL_STATIC);
        RatFreeListExpression(list);
    } else {
        msg = Tcl_GetVar2Ex(interp, "t","illegal_list_format",TCL_GLOBAL_ONLY);
        snprintf(buf, sizeof(buf), Tcl_GetString(msg), error);
 	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    }
    return TCL_OK;
}
