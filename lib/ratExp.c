/*
 * ratExp.c --
 *
 *	This file handles the expressions.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of my legal notices is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"

/*
 * Private types
 */
typedef enum {TT_Field, TT_Operator, TT_Boolean,TT_Grouping,TT_Spec} TokenType;
typedef enum {T_To, T_From, T_Subject, T_Sender, T_Cc, T_Reply_To, T_Size,
	      T_Has, T_Is, T_Gt, T_Lt,
	      T_And, T_Or, T_Not,
	      T_Lparen, T_Rparen} Token;

typedef struct {
    Token		token;
    TokenType		type;
    char               *string;
    RatFolderInfoType	info;
} TokenList;

typedef struct Expression {
    int		negate;
    Token	op;
    union {
	struct Expression      *expPtr;
	RatFolderInfoType	info;
    } arg1;
    union {
	struct Expression      *expPtr;
	char                   *string;
    } arg2;
} Expression;

typedef struct ExpList {
    int		    id;
    Expression     *expPtr;
    struct ExpList *next;
} ExpList;

/*
 * Static data
 */
static int expCounter = 0;
static ExpList *expListPtr = NULL;
static TokenList tokenList[] = {
    {T_To,	TT_Field,	"to",		RAT_FOLDER_TO},
    {T_From,	TT_Field,	"from",		RAT_FOLDER_FROM},
    {T_Subject,	TT_Field,	"subject",	RAT_FOLDER_SUBJECT},
    {T_Sender,	TT_Field,	"sender",	RAT_FOLDER_SENDER},
    {T_Cc,	TT_Field,	"cc",		RAT_FOLDER_CC},
    {T_Reply_To,TT_Field,	"reply-to",	RAT_FOLDER_REPLY_TO},
    {T_Size,	TT_Field,	"size",		RAT_FOLDER_SIZE},
    {T_Has,	TT_Operator,	"has",		0},
    {T_Is,	TT_Operator,	"is",		0},
    {T_Gt,	TT_Operator,	">",		0},
    {T_Lt,	TT_Operator,	"<",		0},
    {T_And,	TT_Boolean,	"and",		0},
    {T_Or,	TT_Boolean,	"or",		0},
    {T_Not,	TT_Spec,	"not",		0},
    {T_Lparen,	TT_Grouping,	"(",		0},
    {T_Rparen,	TT_Grouping,	")",		0},
    {0,		0,		NULL,		0}
};

/*
 * Local functions
 */
static TokenList   *GetToken(char **sPtr);
static char 	   *GetString(char **sPtr);
static void	    FreeExp(Expression *expPtr);
static Expression  *ParseExpression(char **sPtr, char **errPtr, int inParen);
static void	    GetExpression(Tcl_Interp *interp, Tcl_Obj *ePtr,
				  Expression *expPtr);
static int	    RatExpMatchDo(Tcl_Interp *interp, Expression *expPtr,
		    RatInfoProc *infoProc, ClientData clientData, int index);


/*
 *----------------------------------------------------------------------
 *
 * GetToken --
 *
 *      Extract the next token from the string.
 *	to the interpreter.
 *
 * Results:
 *	A pointer to a TokenList entity or nULL if no valid token is
 *	found
 *
 * Side effects:
 *	*sPtr will most probably be modified.
 *
 *
 *----------------------------------------------------------------------
 */

static TokenList*
GetToken(char **sPtr)
{
    char *cPtr = *sPtr;
    int i;

    while (isspace((unsigned char)*cPtr)) {
	cPtr++;
    }
    *sPtr = cPtr;
    if (!*cPtr) {
	return NULL;
    }
    for (i=0; tokenList[i].string; i++) {
	if (!strncasecmp(cPtr, tokenList[i].string,
			 strlen(tokenList[i].string))) {
	    cPtr += strlen(tokenList[i].string);
	    *sPtr = cPtr;
	    return &tokenList[i];
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * GetString --
 *
 *      Extract the next string.
 *
 * Results:
 *	A pointer to a copy of the found string. It is the callers
 *	resposibility to eventually free this area.
 *
 * Side effects:
 *	*sPtr will most probably be modified.
 *
 *
 *----------------------------------------------------------------------
 */

static char*
GetString(char **sPtr)
{
    char quote = '\0', *cPtr = *sPtr, *result;
    int i;


    while (isspace((unsigned char)*cPtr)) {
	cPtr++;
    }
    if ('\'' == *cPtr || '"' == *cPtr || '{' == *cPtr) {
	quote = *cPtr++;
    }
    *sPtr = cPtr;
    if ('{' == quote) {
	quote = '}';
    }
    result = (char*)ckalloc(strlen(cPtr)+1);
    i=0;
    while (*cPtr && !(quote == *cPtr
		      || (!quote && isspace((unsigned char)*cPtr)))) {
	if ('\\' == *cPtr && cPtr[1]) {
	    cPtr++;
	}
	if (isupper((unsigned char)*cPtr)) {
	    result[i] = tolower((unsigned char)*cPtr);
	} else {
	    result[i] = *cPtr;
	}
	i++;
	cPtr++;
    }
    result[i] = '\0';
    if (quote && quote == *cPtr) {
	cPtr++;
    }
    *sPtr = cPtr;
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * FreeExp --
 *
 *      Free an expression
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The given expression will be free'ed.
 *
 *
 *----------------------------------------------------------------------
 */

static void
FreeExp(Expression *expPtr)
{
    if (!expPtr) {
	return;
    }
    if (expPtr->op == T_And || expPtr->op == T_Or) {
	FreeExp(expPtr->arg1.expPtr);
	FreeExp(expPtr->arg2.expPtr);
    } else {
	ckfree(expPtr->arg2.string);
    }
    ckfree(expPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseExpression --
 *
 *      Parse a given search expression
 *
 * Results:
 *	A pointer to an expression, or NULL if an error was encountered.
 *
 * Side effects:
 *	*sPtr will most probably be modified.
 *
 *
 *----------------------------------------------------------------------
 */

static Expression*
ParseExpression(char **sPtr, char **errPtr, int inParen)
{
    TokenList *tokPtr;
    Expression *expPtr = NULL, *exp2Ptr;
    char *newString;
    int negated, l;

    while (**sPtr) {
	negated = 0;
	while (tokPtr = GetToken(sPtr), (tokPtr && tokPtr->token == T_Not)) {
	    negated = negated ? 0 : 1;
	}
	if (!tokPtr) {
	    if (**sPtr) {
		*errPtr = "Unparseable text";
	    }
	    return expPtr;
	}
	switch (tokPtr->type) {
	case TT_Field:
		if (expPtr && expPtr->op != T_And && expPtr->op != T_Or) {
		    *errPtr = "Expected boolean or ')'";
		    return expPtr;
		}
		exp2Ptr = (Expression*)ckalloc(sizeof(Expression));
		exp2Ptr->negate = negated;
		exp2Ptr->arg1.info = tokPtr->info;
		tokPtr = GetToken(sPtr);
		if (!tokPtr || tokPtr->type != TT_Operator) {
		    *errPtr = "Expected operator";
		    ckfree(exp2Ptr);
		    return expPtr;
		}
		exp2Ptr->op = tokPtr->token;
		exp2Ptr->arg2.string = GetString(sPtr);
		if (!exp2Ptr->arg2.string) {
		    *errPtr = "String expected";
		    ckfree(exp2Ptr);
		    return expPtr;
		} else if (T_Is == exp2Ptr->op) {
		    exp2Ptr->op = T_Has;
		    l = strlen(exp2Ptr->arg2.string)+3;
		    newString = (char*)ckalloc(l);
		    strlcpy(newString, "^", l);
		    strlcat(newString, exp2Ptr->arg2.string, l);
		    strlcat(newString, "$", l);
		    ckfree(exp2Ptr->arg2.string);
		    exp2Ptr->arg2.string = newString;
		}
		if (expPtr) {
		    expPtr->arg2.expPtr = exp2Ptr;
		} else {
		    expPtr = exp2Ptr;
		}
		break;
	case TT_Boolean:
		if (!expPtr) {
		    *errPtr = "Must have a valid expression before a boolean";
		    return expPtr;
		}
		exp2Ptr = (Expression*)ckalloc(sizeof(Expression));
		exp2Ptr->negate = negated;
		exp2Ptr->op = tokPtr->token;
		exp2Ptr->arg1.expPtr = expPtr;
		exp2Ptr->arg2.expPtr = NULL;
		expPtr = exp2Ptr;
		break;
	case TT_Grouping:
		if (T_Lparen == tokPtr->token) {
		    if (expPtr && expPtr->op != T_And && expPtr->op != T_Or) {
			*errPtr = "Expected boolean, field or ')'";
			return expPtr;
		    }
		    exp2Ptr = ParseExpression(sPtr, errPtr, 1);
		    if (expPtr) {
			expPtr->arg2.expPtr = exp2Ptr;
		    } else {
			expPtr = exp2Ptr;
		    }
		    if (*errPtr) {
			return expPtr;
		    }
		} else {
		    if (!inParen) {
			*errPtr = "Unexpected ')'.";
			return expPtr;
		    }
		    return expPtr;
		}
		break;
	case TT_Operator:	/* fallthrough */
	case TT_Spec:
		*errPtr = "Expected field or (";
		return expPtr;
	}
    }
    if (!expPtr) {
	*errPtr = "Empty expression";
	return NULL;
    }
    return expPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * RatParseExp --
 *
 *      See ../doc/interface
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	Will probably add an expression to the local list.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatParseExpCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	       Tcl_Obj *const objv[])
{
    char *error = NULL, *cPtr, *exp;
    Expression *expPtr;
    ExpList *elemPtr;

    if (objc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]), " expression\"",
			 (char *) NULL);
	return TCL_ERROR;
    }

    exp = cPtr = Tcl_GetString(objv[1]);
    expPtr = ParseExpression(&cPtr, &error, 0);
    if (error) {
	char buf[32];

	FreeExp(expPtr);
	sprintf(buf, "%d", cPtr-exp);
	Tcl_AppendElement(interp, buf);
	Tcl_AppendElement(interp, error);
	return TCL_ERROR;
    }

    elemPtr = (ExpList*)ckalloc(sizeof(ExpList));
    elemPtr->id = expCounter;
    elemPtr->expPtr = expPtr;
    elemPtr->next = expListPtr;
    expListPtr = elemPtr;
    Tcl_SetObjResult(interp, Tcl_NewIntObj(expCounter++));
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * GetExp --
 *
 *      Print one expression into the given DString.
 *
 * Results:
 *	The given DString will be modified.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

static void
GetExpression(Tcl_Interp *interp, Tcl_Obj *ePtr, Expression *expPtr)
{
    int opIndex, fIndex;
    Tcl_Obj *oPtr;

    for (opIndex=0; tokenList[opIndex].token != expPtr->op; opIndex++);

    if (expPtr->negate) {
	Tcl_ListObjAppendElement(interp, ePtr, Tcl_NewStringObj("not", 3));
    }
    if (tokenList[opIndex].type == TT_Boolean) {
	oPtr = Tcl_NewObj();
	GetExpression(interp, oPtr, expPtr->arg1.expPtr);
	Tcl_ListObjAppendElement(interp, ePtr, oPtr);
	Tcl_ListObjAppendElement(interp, ePtr,
			      Tcl_NewStringObj(tokenList[opIndex].string, -1));
	oPtr = Tcl_NewObj();
	GetExpression(interp, oPtr, expPtr->arg2.expPtr);
	Tcl_ListObjAppendElement(interp, ePtr, oPtr);
    } else {
	for (fIndex=0; tokenList[fIndex].info != expPtr->arg1.info; fIndex++);
	Tcl_ListObjAppendElement(interp, ePtr,
			      Tcl_NewStringObj(tokenList[fIndex].string, -1));
	Tcl_ListObjAppendElement(interp, ePtr,
			      Tcl_NewStringObj(tokenList[opIndex].string, -1));
	Tcl_ListObjAppendElement(interp, ePtr,
			      Tcl_NewStringObj(expPtr->arg2.string, -1));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetExp --
 *
 *      See ../doc/interface
 *
 * Results:
 *	The identified expression is returned as a string in the result area.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatGetExpCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	     Tcl_Obj *const objv[])
{
    ExpList *elemPtr;
    Tcl_Obj *rPtr;
    int id;

    if (objc < 2
	|| TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &id)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 Tcl_GetString(objv[0]), " id\"", (char *) NULL);
	return TCL_ERROR;
    }

    for (elemPtr = expListPtr; elemPtr; elemPtr = elemPtr->next) {
	if (elemPtr->id == id) {
	    rPtr = Tcl_NewObj();
	    GetExpression(interp, rPtr, elemPtr->expPtr);
	    Tcl_SetObjResult(interp, rPtr);
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "No expression with id \"",
		     Tcl_GetString(objv[1]), "\"", (char *) NULL);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * RatFreeExp --
 *
 *      See ../doc/interface
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	Will probably remove an expression from the local list.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatFreeExpCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	      Tcl_Obj *const objv[])
{
    ExpList **elemPtrPtr, *elemPtr;
    int id;

    if (objc < 2
	|| TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &id)) {
	Tcl_AppendResult(interp, "Illegal usage: should be \"",
			 Tcl_GetString(objv[0]), " id\"", (char *) NULL);
	return TCL_ERROR;
    }
    for (elemPtrPtr = &expListPtr; *elemPtrPtr;
	 elemPtrPtr=&(*elemPtrPtr)->next){
	if ((*elemPtrPtr)->id == id) {
	    elemPtr = *elemPtrPtr;
	    FreeExp(elemPtr->expPtr);
	    *elemPtrPtr = elemPtr->next;
	    ckfree(elemPtr);
	    return TCL_OK;
	}
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * RatExpMatch --
 *
 *      Checks if a given expression matches to given message.
 *
 * Results:
 *	True if it did match.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatExpMatch(Tcl_Interp *interp, int expId, RatInfoProc *infoProc,
	ClientData clientData, int index)
{
    ExpList *elemPtr;

    for (elemPtr = expListPtr; elemPtr && elemPtr->id != expId;
	elemPtr = elemPtr->next);
    if (!elemPtr) {
	return 0;
    }
    return RatExpMatchDo(interp, elemPtr->expPtr, infoProc, clientData, index);
}


/*
 *----------------------------------------------------------------------
 *
 * RatExpMatchDo --
 *
 *      Checks if a given expression matches to given message.
 *	This routine actually does the checking and may call itself
 *	recursively.
 *
 * Results:
 *	True if it did match.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatExpMatchDo(Tcl_Interp *interp, Expression *expPtr, RatInfoProc *infoProc,
	ClientData clientData, int index)
{
    char *sLower, *cPtr;
    int opIndex, val;
    static Tcl_Obj *sPtr = NULL;
    Tcl_Obj *oPtr;

    for (opIndex=0; tokenList[opIndex].token != expPtr->op; opIndex++);

    if (TT_Boolean == tokenList[opIndex].type) {
	val = RatExpMatchDo(interp, expPtr->arg1.expPtr, infoProc, clientData,
		index);
	if (!((T_Or == tokenList[opIndex].token && val) ||
		(T_And == tokenList[opIndex].token && !val))) {
	    val = RatExpMatchDo(interp, expPtr->arg2.expPtr, infoProc,
		    clientData, index);
	}
	if (expPtr->negate) {
	    val = val ? 0 : 1;
	}
	return val;
    } else {
	oPtr = (*infoProc)(interp, clientData, expPtr->arg1.info, index);
	if (!oPtr) {
	    if (!sPtr) {
		sPtr = Tcl_NewObj();
		Tcl_IncrRefCount(sPtr);
	    }
	    oPtr = sPtr;
	}
	if (T_Has == tokenList[opIndex].token
		|| T_Is == tokenList[opIndex].token) {
	    sLower = cpystr(Tcl_GetString(oPtr));
	    for (cPtr = sLower; *cPtr; cPtr++) {
		if (isupper((unsigned char)*cPtr)) {
		    *cPtr = tolower((unsigned char)*cPtr);
		}
	    }
	    val = Tcl_RegExpMatch(interp, sLower, expPtr->arg2.string);
	    ckfree(sLower);
	    return val;
	} else if (expPtr->arg1.info == RAT_FOLDER_SIZE) {
	    Tcl_GetIntFromObj(interp, oPtr, &val);
	    if (T_Gt == tokenList[opIndex].token) {
		return val > atoi(expPtr->arg2.string);
	    } else {
		return val < atoi(expPtr->arg2.string);
	    }
	}
    }
    return 0;
}
