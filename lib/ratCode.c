/*
 * ratCode.c --
 *
 *	This file contains basic support for decoding and encoding of
 *	strings coded in various MIME-encodings.
 *
 * TkRat software and its included text is Copyright 1996-2002 by
 * Martin Forss�n
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"

/*
 * List used when decoding QP
 */
char alphabetHEX[17] = "0123456789ABCDEF";

/*
 * List used when decoding base64
 * It consists of 64 chars plus '=' and null
 */
static char alphabet64[66] =
	   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/*
 * List used when decoding modified base64
 * It consists of 64 chars plus '=' and null
 */
static char modified64[66] =
	   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,=";

#define RFC2047_MAX_LINE_LENGTH 75
#define RFC2047_MAX_ENCODED_WORD_LENGTH 75

static int FindMimeHdr(Tcl_Interp *interp, unsigned char *hdr,
	unsigned char **sPtr, unsigned char **ePtr, Tcl_Encoding *encoding,
	int *code, unsigned char **data, int *length);
static int RatUtf8to16(const unsigned char *src, unsigned char *dst);
static int RatUtf16to8(const unsigned char *src, unsigned char *dst);
static int RatCheckEncoding(Tcl_Interp *interp, char *encoding_name,
			    const char *string, int length);
static int CreateEncWord(Tcl_Interp *interp, Tcl_Encoding enc,
			 const char *charset, unsigned char *raw, int length,
			 Tcl_DString *dest, int maxUse);


/*
 *----------------------------------------------------------------------
 *
 * FindMimeHdr --
 *
 *      Find a string encoded according to rfc2047
 *
 * Results:
 *	Returns data in most arguments.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
FindMimeHdr(Tcl_Interp *interp, unsigned char *hdr, unsigned char **sPtr,
	unsigned char **ePtr, Tcl_Encoding *encoding, int *code,
	unsigned char **data, int *length)
{
    unsigned char *sCharset, *eCharset, *cPtr, c;

    for (cPtr = hdr; *cPtr; cPtr++) {
	if ('=' == cPtr[0] && '?' == cPtr[1]) {
	    *sPtr = cPtr;
	    sCharset = cPtr+2;
	    for (cPtr+=2; '?' != *cPtr && *cPtr; cPtr++);
	    if ('?' != *cPtr) return 0;
	    if ('?' != cPtr[2]) continue;
	    switch (cPtr[1]) {
		case 'b':
		case 'B':
		    *code = ENCBASE64;
		    break;
		case 'q':
		case 'Q':
		    *code = ENCQUOTEDPRINTABLE;
		    break;
		default:
		    continue;
	    }
	    eCharset = cPtr;
	    *data = cPtr+3;
	    for (cPtr+=3, *length = 0;
		    *cPtr && ('?' != *cPtr || '=' != cPtr[1]);
		    cPtr++, (*length)++);
	    if ('?' != *cPtr) return 0;
	    *ePtr = cPtr+2;
	    c = *eCharset;
	    *eCharset = '\0';
	    *encoding = RatGetEncoding(interp, (char*)sCharset);
	    *eCharset = c;
	    return 1;
	}
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * RatDecodeHeader --
 *
 *      Decodes a header line encoded according to rfc2047.
 *
 * Results:
 *	Returns a pointer to a static storage area
 *
 * Side effects:
 *	None
 *
 * TODO, handle address entries correct
 *
 *----------------------------------------------------------------------
 */

char*
RatDecodeHeader(Tcl_Interp *interp, const char *data, int adr)
{
    static Tcl_DString ds, tmp;
    static int initialized = 0;
    unsigned char *sPtr, *ePtr, *decoded, *text, *cPtr,
	    *point = (unsigned char*)data;
    int length, code, first = 1;
    unsigned long dlen;
    unsigned int i;
    Tcl_Encoding encoding;
    Tcl_DString *myPtr = NULL;

    if (!data || !*data) {
	return "";
    }

    if (!initialized) {
	Tcl_DStringInit(&ds);
	initialized = 1;
    } else {
	Tcl_DStringSetLength(&ds, 0);
    }

    /*
     * Check for headers from buggy programs (with raw eight-bit data
     * in them)
     */
    for (cPtr = (unsigned char*)data; *cPtr; cPtr++) {
	if (*cPtr & 0x80) {
	    myPtr = (Tcl_DString*)ckalloc(sizeof(Tcl_DString));
	    Tcl_DStringInit(myPtr);
	    Tcl_ExternalToUtfDString(NULL, data, -1, myPtr);
	    data = Tcl_DStringValue(myPtr);
	    point = (unsigned char*)data;
	    break;
	}
    }

    while (FindMimeHdr(interp, point, &sPtr, &ePtr, &encoding, &code, &text,
	    &length)) {
	if (sPtr != point) {
	    if (!first) {
		for (cPtr = point; cPtr < sPtr && isspace(*cPtr); cPtr++);
		if (cPtr < sPtr) {
		    Tcl_DStringAppend(&ds, (char*)point, sPtr-point);
		}
	    } else {
		for (i=0; i<sPtr-point; i++) {
		    if ('\n' != point[i]) {
			Tcl_DStringAppend(&ds, &point[i], 1);
		    }
		}
	    }
	}
	first = 0;
	point = ePtr;
	if (NULL == encoding) {
	    Tcl_DStringAppend(&ds, (char*)sPtr, ePtr-sPtr);
	    continue;
	}
	if (ENCBASE64 == code) {
	    decoded = rfc822_base64(text, length, &dlen);
	} else {
	    decoded = (char*)ckalloc(length+1);
	    for (dlen=0, cPtr=text; cPtr-text < length; cPtr++) {
		if ('_' == *cPtr) {
		    decoded[dlen++] = ' ';
		} else if ('=' == *cPtr) {
		    decoded[dlen++] =
			    ((strchr(alphabetHEX, cPtr[1])-alphabetHEX)<<4) +
			     (strchr(alphabetHEX, cPtr[2])-alphabetHEX);
		    cPtr += 2;
		} else {
		    decoded[dlen++] = *cPtr;
		}
	    }
	    decoded[dlen] = '\0';
	}
	Tcl_ExternalToUtfDString(encoding, decoded, dlen, &tmp);
	ckfree(decoded);
	Tcl_DStringAppend(&ds,
			  Tcl_DStringValue(&tmp), Tcl_DStringLength(&tmp));
	Tcl_DStringFree(&tmp);
    }
    if (*point) {
	for (sPtr = point; *sPtr; sPtr++) {
	    if ('\n' != *sPtr) {
		Tcl_DStringAppend(&ds, (char*)sPtr, 1);
	    }
	}
    }
    if (myPtr) {
	Tcl_DStringFree(myPtr);
	ckfree(myPtr);
    }
    return Tcl_DStringValue(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * RatDecode --
 *
 *	General decoding interface. It takes as arguments a chunk of data,
 *	the encoding the data is in. And returns a new ckalloced block of
 *	decoded data. The decoded data will not have any \r or \0 in it
 *	\0 will be changed to the string \0, unless the toCharset parameter
 *	is NULL. If that is the case the data is assumed to be wanted
 *	in raw binary form.
 *	It is also possible to get this routine to do some character set
 *	transformation, but this is not yet implemented.
 *
 * Results:
 *	A block of decoded data. It is the callers responsibility to free
 *	this data.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_DString*
RatDecode(Tcl_Interp *interp, int cte, const char *data, int length,
	  const char *charset)
{
    char *dst, buf[64], lbuf[4];
    const char *src;
    int allocated, dataIndex = 0, index, srcLength, len;
    Tcl_Encoding enc = NULL;
    Tcl_DString *dsPtr = (Tcl_DString*)ckalloc(sizeof(Tcl_DString)),
		tmpDs;

    Tcl_DStringInit(dsPtr);
    if (charset) {
        enc = RatGetEncoding(interp, charset);
    }
    while (dataIndex < length) {
	if (cte == ENCBASE64) {
	    src = buf;
	    for (srcLength = 0; dataIndex < length
		    && srcLength < sizeof(buf)-2;) {
		for (index=0; dataIndex<length && index<4; dataIndex++) {
		    if (strchr(alphabet64, data[dataIndex])) {
			lbuf[index++] = strchr(alphabet64, data[dataIndex])
				- alphabet64;
		    }
		}
		if (0 == index) {
		    continue;
		}
		buf[srcLength++] = lbuf[0] << 2 | ((lbuf[1]>>4)&0x3);
		if (strchr(alphabet64, '=')-alphabet64 != lbuf[2]) {
		    buf[srcLength++] = lbuf[1] << 4 | ((lbuf[2]>>2)&0xf);
		    if (strchr(alphabet64, '=')-alphabet64 != lbuf[3]) {
			buf[srcLength++] = lbuf[2] << 6 | (lbuf[3]&0x3f);
		    }
		}
	    }
	} else if (cte == ENCQUOTEDPRINTABLE) {
	    src = buf;
	    for (srcLength = 0; dataIndex < length &&
		    srcLength < sizeof(buf); ) {
		if ('=' == data[dataIndex]) {
		    if ('\r' == data[dataIndex+1]) {
			dataIndex += 3;
		    } else if ('\n' == data[dataIndex+1]) {
			dataIndex += 2;
		    } else {
			buf[srcLength++] = 16*(strchr(alphabetHEX,
				data[dataIndex+1])-alphabetHEX)
				+ strchr(alphabetHEX,
				data[dataIndex+2])-alphabetHEX;
			dataIndex += 3;
		    }
		} else {
		    buf[srcLength++] = data[dataIndex++];
		}
	    }
	} else {
	    src = data;
	    srcLength = length;
	    dataIndex = length;
	    allocated = 0;
	}
	if (charset) {
	    Tcl_ExternalToUtfDString(enc, src, srcLength, &tmpDs);
	    Tcl_DStringAppend(dsPtr,
		    Tcl_DStringValue(&tmpDs), Tcl_DStringLength(&tmpDs));
	    Tcl_DStringFree(&tmpDs);
	} else {
	    Tcl_DStringAppend(dsPtr, src, srcLength);
	}
    }
    if (charset) {
	len = Tcl_DStringLength(dsPtr);
	for (src = dst = Tcl_DStringValue(dsPtr); *src; src++) {
	    if (*src != '\r') {
		*dst++ = *src;
	    } else {
		len--;
	    }
	}
	Tcl_DStringSetLength(dsPtr, len);
    }
    return dsPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateEncWord --
 *
 *      Tres to create an encoded word (if needed) by the given string.
 *      It uses at most length bytes from raw and stores the result in
 *      dest. The result will be no more than maxUse characters.
 *
 * Results:
 *      Returns non-zero if the encoding was successful.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static int
CreateEncWord(Tcl_Interp *interp, Tcl_Encoding enc, const char *charset,
	      unsigned char *raw, int length, Tcl_DString *dest, int maxUse)
{
    unsigned char buf[RFC2047_MAX_ENCODED_WORD_LENGTH+1],
	buf2[RFC2047_MAX_ENCODED_WORD_LENGTH+1];
    Tcl_EncodingState state;
    int i, consumed, wrote, d;
    
    /*
     * Check if we must encode this
     */
    for (i=0; i<length && raw[i] < 0x80; i++);
    if (i == length) {
	Tcl_DStringAppend(dest, raw, length);
	return 1;
    }

    /*
     * Nope, we must encode this. Adjust the max output size
     */
    if (maxUse > sizeof(buf)-1) {
	maxUse = sizeof(buf)-1;
    }

    /*
     * Try to convert to external encoding
     */
    if (TCL_OK != Tcl_UtfToExternal(interp, enc, raw, length,
				    TCL_ENCODING_START|TCL_ENCODING_END,
				    &state, buf2, sizeof(buf2),
				    &consumed, &wrote, NULL)
	|| consumed != length) {
	return 0;
    }

    /*
     * Convert into quoted-printable, check that we have room all the time
     */
    snprintf(buf, sizeof(buf), "=?%s?Q?", charset);
    for (i=0, d=strlen(buf); i<wrote && d < maxUse-2; i++) {
	if (' ' == buf2[i]) {
	    buf[d++] = '_';
	} else if (buf2[i] & 0x80
	    || !isprint(buf2[i])
	    || '=' == buf2[i]
	    || '_' == buf2[i]
	    || '?' == buf2[i]) {
	    if (d+3 >= maxUse-2) {
		return 0;
	    }
	    buf[d++] = '=';
	    buf[d++] = alphabetHEX[buf2[i]>>4];
	    buf[d++] = alphabetHEX[buf2[i]&0xf];
	} else {
	    buf[d++] = buf2[i];
	}
    }
    if (i < wrote) {
	return 0;
    }
    buf[d++] = '?';
    buf[d++] = '=';
    Tcl_DStringAppend(dest, buf, d);
    return 1;

}

/*
 *----------------------------------------------------------------------
 *
 * RatEncodeHeaderLine --
 *
 *	Encodes one header line according to MIME (rfc2047).
 *	The nameLength argument should tell how long the header name is in
 *	characters. This is so that the line folding can do its job properly.
 *
 * Results:
 *	A block of encoded header line. THis block of data will be valid
 *      until the next call to thius function.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatEncodeHeaderLine (Tcl_Interp *interp, Tcl_Obj *line, int nameLength)
{
    static Tcl_DString ds;
    static int initialized = 0;
    Tcl_Obj **objv;
    int i, objc, l, l1, pre = nameLength, maxUse;
    char *s;
    const char *charset;
    Tcl_Encoding enc;

    if (NULL == line) {
	return NULL;
    }

    if (!initialized) {
	Tcl_DStringInit(&ds);
	initialized = 1;
    } else {
	Tcl_DStringSetLength(&ds, 0);
    }

    /*
     * Find suitable encoding
     */
    Tcl_ListObjGetElements(interp,
			   Tcl_GetVar2Ex(interp, "option",
					 "charset_candidates",
					 TCL_GLOBAL_ONLY),
			   &objc, &objv);
    s = Tcl_GetStringFromObj(line, &l);
    for (i=0; i<objc; i++) {
	if (RatCheckEncoding(interp, Tcl_GetString(objv[i]), s, l)) {
	    break;
	}
    }
    if (i<objc) {
	charset = Tcl_GetString(objv[i]);
    } else {
	charset = Tcl_GetVar2(interp, "option", "charset", TCL_GLOBAL_ONLY);
    }
    enc = RatGetEncoding(interp, charset);

    /*
     * Do while we have characters left to consume
     *  - Find candidate for line-break
     *  - Loop while it can NOT be encoded into a word
     *    - Search backwards for new canidate
     *    - If no new canididate is found switch to test every character
     */
    while (*s) {
	if (strlen(s)+pre <= RFC2047_MAX_LINE_LENGTH) {
	    l = strlen(s);
	} else {
	    for (l = RFC2047_MAX_LINE_LENGTH-pre; l>0 && !isspace(s[l]); l--);
	    if (0 == l) {
		l = RFC2047_MAX_LINE_LENGTH-pre;
	    }
	}
	maxUse = RFC2047_MAX_LINE_LENGTH-pre;
	while (!CreateEncWord(interp, enc, charset, s, l, &ds, maxUse)) {
	    for (l1 = l-1; l1 > 0 && !isspace(s[l1]); l1--);
	    if (0 < l1) {
		l = l1;
	    } else {
		maxUse = 1024;
		l--;
	    }
	}
	s += l;
	if (*s) {
	    Tcl_DStringAppend(&ds, "\r\n", 2);
	    for (pre=0; isspace(*s) && pre<RFC2047_MAX_LINE_LENGTH; s++,pre++){
		Tcl_DStringAppend(&ds, s, 1);
	    }
	    if (0 == pre) {
		Tcl_DStringAppend(&ds, " ", 1);
		pre = 1;
	    }
	}
    }
    
    Tcl_FreeEncoding(enc);
    return Tcl_DStringValue(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * RatEncodeAddresses --
 *
 *	Encodes the fullname portions of a bunch of addreses.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fullnames of the addresses may change.
 *
 *
 *----------------------------------------------------------------------
 */

void
RatEncodeAddresses(Tcl_Interp *interp, ADDRESS *adrPtr)
{
    Tcl_Obj *oPtr;
    char *cPtr;

    while (adrPtr) {
	if (adrPtr->personal) {
	    for (cPtr = adrPtr->personal; *cPtr; cPtr++) {
		if (*cPtr & 0x80) {
		    oPtr = Tcl_NewStringObj(adrPtr->personal, -1);
		    cPtr = RatEncodeHeaderLine(interp, oPtr, 0);
		    Tcl_DecrRefCount(oPtr);
		    ckfree(adrPtr->personal);
		    adrPtr->personal = cpystr(cPtr);
		}
	    }
	}
	adrPtr = adrPtr->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RatGetEncoding --
 *
 *	Return the tcl-encoding attached to the given name. This name
 *      may be mapped from a MIME-name into a tcl-name.
 *
 * Results:
 *      A tcl Tcl_Endoding blob. The given encoding must be freed by the
 *      caller by calling Tcl_FreeEncoding().
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Encoding
RatGetEncoding(Tcl_Interp *interp, const char *name)
{
    Tcl_Encoding enc;
    const char *tclName;
    char lname[256];

    if (NULL == name) {
	return NULL;
    }

    strlcpy(lname, name, sizeof(lname));
    lcase(lname);
    tclName = Tcl_GetVar2(interp, "charsetMapping", lname, TCL_GLOBAL_ONLY);
    if (NULL == tclName) {
	tclName = lname;
    }

    enc = Tcl_GetEncoding(interp, tclName);
    if (NULL == enc) {
	return NULL;
    }
    return enc;
}


/*
 *----------------------------------------------------------------------
 *
 * RatCheckEncoding --
 *
 *	Check if the given encoding can encode the given string
 *
 * Results:
 *      Non-zero if all characters in the give string can be encoded
 *      successfully
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */
static int
RatCheckEncoding(Tcl_Interp *interp, char *encoding_name,
		 const char *string, int length)
{
    Tcl_EncodingState state;
    Tcl_Encoding enc;
    char buf[1024];
    int ret, in;

    if (NULL == (enc = RatGetEncoding(interp, encoding_name))) {
	return 0;
    }
    ret = 0;
    while (length && TCL_CONVERT_UNKNOWN != ret) {
	ret = Tcl_UtfToExternal(interp, enc, string, length,
				TCL_ENCODING_STOPONERROR|TCL_ENCODING_START,
				&state, buf, sizeof(buf),
				&in, NULL, NULL);
	string += in;
	length -= in;
    }
    Tcl_FreeEncoding(enc);
    return TCL_CONVERT_UNKNOWN != ret;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCheckEncodingsCmd --
 *
 *	See ../doc/interface for a descriptions of arguments and result.
 *
 * Results:
 *      See above
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

int
RatCheckEncodingsCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	Tcl_Obj *const objv[])
{
    int i, listLength, srcLen;
    Tcl_Obj *oPtr, *vPtr;
    char *src;
    
    if (3 != objc) {
	Tcl_AppendResult(interp, "Usage: ", Tcl_GetString(objv[0]), \
		" variable charsets", (char*) NULL);
	return TCL_ERROR;
    }
    vPtr = Tcl_GetVar2Ex(interp, Tcl_GetString(objv[1]), NULL, 0);
    Tcl_ListObjLength(interp, objv[2], &listLength);
    src = Tcl_GetStringFromObj(vPtr, &srcLen);
    for (i=0; i<listLength; i++) {
	Tcl_ListObjIndex(interp, objv[2], i, &oPtr);
	if (RatCheckEncoding(interp, Tcl_GetString(oPtr),  src, srcLen)) {
	    Tcl_SetObjResult(interp, oPtr);
	    return TCL_OK;
	}
    }
    Tcl_SetResult(interp, "", TCL_STATIC);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatCode64 --
 *
 *	Encode the given object in base64
 *
 * Results:
 *      A new Tcl_Obj
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
RatCode64(Tcl_Obj *sPtr)
{
    Tcl_Obj *dPtr = Tcl_NewObj();
    unsigned char *cPtr, buf[4];
    int l, ll;

    cPtr = (unsigned char*)Tcl_GetStringFromObj(sPtr, &l);

    for (ll = 0; l > 0; l -= 3, cPtr += 3) {
	buf[0] = alphabet64[cPtr[0] >> 2];
	buf[1] = alphabet64[((cPtr[0] << 4) + (l>1 ? (cPtr[1]>>4) : 0))&0x3f];
	buf[2] = l > 1 ?
	    alphabet64[((cPtr[1]<<2) + (l>2 ? (cPtr[2]>>6) : 0)) & 0x3f] : '=';
	buf[3] = l > 2 ? alphabet64[cPtr[2] & 0x3f] : '=';
	Tcl_AppendToObj(dPtr, (char*)buf, 4);
	if (18 == ++ll || l < 4) {
	    Tcl_AppendToObj(dPtr, "\n", 1);
	    ll = 0;
	}
    }
    return dPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RatUtf8to16 --
 *
 *	Convert the given utf-8 character to UCS-2
 *
 * Results:
 *      Returns the number of characters consumed from src
 *	On failure a negative number is returned.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatUtf8to16(const unsigned char *src, unsigned char *dst)
{
    if (0 == (*src & 0x80)) {
	dst[0] = 0;
	dst[1] = *src;
	return 1;
    } else if (0xc0 == (*src & 0xe0)) {       
        if (!(src[1] & 0x80)) {
            return 1;
        }
        dst[0] = (src[0] & 0x1f) >> 2;
        dst[1] = ((src[0] & 0x03) << 6) + (src[1] & 0x3f);
        return 2;
    } else if (0xe0 == (*src & 0xf0)) {
        if (!(src[1] & 0x80) && !(src[2] & 0x80)) {
            return 1;
        }
        dst[0] = ((src[0] & 0x0f) << 4) + ((src[1] & 0x3f) >> 2);
        dst[1] = ((src[1] & 0x03) << 6) + (src[2] & 0x3f);
        return 3;
    } else {
	dst[0] = 0;
	dst[1] = *src;
	return 1;
    }
}   

/*
 *----------------------------------------------------------------------
 *
 * RatUtf16to8 --
 *
 *	Convert the given UCS-2 character to utf-8
 *
 * Results:
 *      Returns the length of the generated string on success.
 *	On failure a negative number is returned.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

static int
RatUtf16to8(const unsigned char *src, unsigned char *dst)
{
    if (src[0] >= 0x08) {
	dst[0] = 0xe0 | (src[0] >> 4);
	dst[1] = 0x80 | ((src[0] & 0x0f) << 2) | (src[1] >> 6);
	dst[2] = 0x80 | (src[1] & 0x3f);
	return 3;
    } else if (src[0] || src[1] > 0x7f) {
	dst[0] = 0xc0 | (src[0] << 2) | (src[1] >> 6);
	dst[1] = 0x80 | (src[1] & 0x3f);
	return 2;
    } else {
	dst[0] = src[1];
	return 1;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * RatUtf8toMutf7 --
 *
 *	Convert the given utf-8 encoded text to modified utf-7
 *
 * Results:
 *      Returns a pointer to a static buffer containing the new text
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatUtf8toMutf7(const char *signed_src)
{
    static unsigned char *dst = NULL;
    static int dstlen = 0;
    unsigned char buf[3], *src = (unsigned char*)signed_src;
    int len = 0, overflow = 0;

    if (dstlen < strlen((char*)src)*3+1) {
	dstlen = strlen((char*)src)*3;
	dst = (unsigned char *)ckrealloc(dst, dstlen);
    }
    while (*src) {
	if ('&' == *src) {
	    if (dstlen <= len+2) {
		dstlen += 128;
		dst = (unsigned char *)ckrealloc(dst, dstlen);
	    }
	    dst[len++] = '&';
	    dst[len++] = '-';
	    src++;
	} else if (*src & 0x80) {
	    if (dstlen <= len+6) {
		dstlen += 128;
		dst = (unsigned char *)ckrealloc(dst, dstlen);
	    }
	    dst[len++] = '&';
	    do {
		if (dstlen <= len+5) {
		    dstlen += 128;
		    dst = (unsigned char *)ckrealloc(dst, dstlen);
		}
		if (overflow) {
		    buf[0] = buf[3];
		    if (*src & 0x80) {
			src += RatUtf8to16(src, buf+1);
		    } else {
			buf[1] = buf[2] = 0;
		    }
		    overflow = 0;
		} else {
		    src += RatUtf8to16(src, buf);
		    if (*src & 0x80) {
			src += RatUtf8to16(src, buf+2);
			overflow = 1;
		    } else {
			buf[2] = buf[3] = 0;
		    }
		}
		dst[len++] = modified64[buf[0] >> 2];
		dst[len++] = modified64[((buf[0] << 4) + (buf[1]>>4)) & 0x3f];
		if (buf[1] || buf[2]) {
		    dst[len++] =
			modified64[((buf[1]<<2) + (buf[2]>>6)) & 0x3f];
		    if (buf[2]) {
			dst[len++] = modified64[buf[2] & 0x3f];
		    }
		}
	    } while (*src & 0x80 || overflow);
	    if (strchr(modified64, *src) || '\0' == *src) {
		dst[len++] = '-';
	    }
	} else {
	    if (dstlen <= len+1) {
		dstlen += 128;
		dst = (unsigned char *)ckrealloc(dst, dstlen);
	    }
	    dst[len++] = *src++;
	}
    }
    dst[len] = '\0';
    return (char*)dst;
}

/*
 *----------------------------------------------------------------------
 *
 * RatMutf7toUtf8 --
 *
 *	Convert the given modified utf-7 encoded text to utf-8
 *
 * Results:
 *      Returns the length of the generated string on success.
 *	On failure a negative number is returned.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

char*
RatMutf7toUtf8(const char *signed_src)
{
    static unsigned char *dst = NULL;
    static int dstlen = 0;
    unsigned char utf16[2], lbuf[4], *src = (unsigned char*)signed_src;
    int i, l, len=0, odd;

    if (dstlen < strlen((char*)src)*3) {
	dstlen = strlen((char*)src)*3;
	dst = (unsigned char *)ckrealloc(dst, dstlen);
    }
    while (*src) {
	if (len >= dstlen) {
	    dstlen += 128;
	    dst = (unsigned char *)ckrealloc(dst, dstlen);
	}
	if ('&' == *src && '-' == src[1]) {
	    dst[len++] = '&';
	    src += 2;
	} else if ('&' == *src) {
	    src++;
	    odd = 0;
	    do {
		for (i=0; i<4; i++) {
		    if (strchr(modified64, *src)) {
			lbuf[i] = strchr(modified64, *src++) - modified64;
		    } else {
			lbuf[i] = 0;
		    }
		}
		if (odd) {
		    odd = 0;
		    if (len >= dstlen+6) {
			dstlen += 128;
			dst = (unsigned char *)ckrealloc(dst, dstlen);
		    }
		    utf16[1] = (lbuf[0] << 2) | (lbuf[1] >> 4);
		    len += RatUtf16to8(utf16, dst+len);
		    utf16[0] = (lbuf[1] << 4) | (lbuf[2] >> 2);
		    utf16[1] = (lbuf[2] << 6) | lbuf[3];
		    if (utf16[0] != 0 || utf16[1] != 0) {
			l = RatUtf16to8(utf16, dst+len);
			len += l;
		    }
		} else {
		    if (len >= dstlen+3) {
			dstlen += 128;
			dst = (unsigned char *)ckrealloc(dst, dstlen);
		    }
		    utf16[0] = (lbuf[0] << 2) | (lbuf[1] >> 4);
		    utf16[1] = (lbuf[1] << 4) | (lbuf[2] >> 2);
		    len += RatUtf16to8(utf16, dst+len);
		    utf16[0] = (lbuf[2] << 6) | lbuf[3];
		    odd = 1;
		}
	    } while (strchr(modified64, *src));
	    if ('-' == *src) {
		src++;
	    }
	} else {
	    dst[len++] = *src++;
	}
    }
    dst[len] = '\0';
    return (char*)dst;
}

/*
 *----------------------------------------------------------------------
 *
 * RatEncodeQP -
 *
 *	Encode the given text to QP
 *
 * Results:
 *      Returns an intialized Tcl_DString pointer. It is up to the caller to
 *      free this when not needing it anymore.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */
Tcl_DString*
RatEncodeQP(const unsigned char *line)
{
    Tcl_DString *ds = (Tcl_DString*)ckalloc(sizeof(*ds));
    const unsigned char *c;
    unsigned char buf[4];

    Tcl_DStringInit(ds);
    for (c=line; *c; c++) {
	if ('=' == *c || 0x80 <= *c) {
	    snprintf(buf, sizeof(buf), "=%02X", *c);
	    Tcl_DStringAppend(ds, buf, 3);
	} else {
	    Tcl_DStringAppend(ds, c, 1);
	}
    }
    return ds;
}

/*
 *----------------------------------------------------------------------
 *
 * RatEncodeQPCmd --
 *
 *	See ../doc/interface
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */

int
RatEncodeQPCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    Tcl_Encoding enc;
    Tcl_DString ext, *encoded;
    
    if (objc != 3) {
 	Tcl_AppendResult(interp, "Bad usage", TCL_STATIC);
	return TCL_ERROR;
    }

    enc = Tcl_GetEncoding(interp, Tcl_GetString(objv[1]));
    Tcl_UtfToExternalDString(enc, Tcl_GetString(objv[2]), -1, &ext);
    encoded = RatEncodeQP(Tcl_DStringValue(&ext));
    Tcl_DStringFree(&ext);
    Tcl_DStringResult(interp, encoded);
    Tcl_FreeEncoding(enc);
    ckfree(encoded);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDecodeQP -
 *
 *	Dencode the given text from QP
 *
 * Results:
 *      Returns a pointer to a string. This string has been allocated with
 *      ckalloc and it is up to the caller to free it when not needing it.
 *
 * Side effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */

unsigned char*
RatDecodeQP(unsigned char *line)
{
    unsigned char *s, *d;

    d = s = line;
    while (*s) {
	if ('=' == *s && isxdigit(s[1]) && isxdigit(s[2])) {
	    *d++ = ((strchr(alphabetHEX, s[1])-alphabetHEX)<<4) +
		(strchr(alphabetHEX, s[2])-alphabetHEX);
	    s += 3;
	} else {
	    *d++ = *s++;
	}
    }
    *d = '\0';
    return line;
}

/*
 *----------------------------------------------------------------------
 *
 * RatDecodeQPCmd --
 *
 *	See ../doc/interface
 *
 * Results:
 *      A standard tcl result
 *
 * Side effects:
 *      None
 *
 *
 *----------------------------------------------------------------------
 */

int
RatDecodeQPCmd(ClientData dummy, Tcl_Interp *interp, int objc,
	   Tcl_Obj *const objv[])
{
    Tcl_Encoding enc;
    Tcl_DString utf;
    char *text;

    if (objc != 3) {
 	Tcl_AppendResult(interp, "Bad usage", TCL_STATIC);
	return TCL_ERROR;
    }

    enc = Tcl_GetEncoding(interp, Tcl_GetString(objv[1]));
    text = cpystr(Tcl_GetString(objv[2]));
    RatDecodeQP(text);
    Tcl_ExternalToUtfDString(enc, text, -1, &utf);
    ckfree(text);
    Tcl_DStringResult(interp, &utf);
    Tcl_FreeEncoding(enc);
    return TCL_OK;
}

/*
 * Test code for Mutf7 <-> utf8 functions
static void
Test(unsigned char *in)
{
    unsigned char stage1[1024], stage2[1024];

    printf("In:     %s\n", in); fflush(stdin);
    RatUtf8toMutf7(in, stage1, sizeof(stage1));
    printf("Stage1: %s\n", stage1); fflush(stdin);
    RatMutf7toUtf8(stage1, stage2, sizeof(stage2));
    printf("Stage2: %s\n", stage2); fflush(stdin);
    if (strcmp(stage2, in)) {
	printf("ERROR\n");
    }
    printf("\n");
}

int main()
{
    Test("får");
    Test("Räksmörgås");
    Test("å");
    Test("åä");
    Test("åäö");
    Test("åäöå");
    Test("åäöåä");
    Test("åäöåäö");

    return 0;
} */
