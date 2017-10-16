/*
 * ratPrint.c --
 *
 *	This file contains the code for a simple prettyprinter.
 *	Unfortunately it is currently limited to iso8859-1 characters.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "ratFolder.h"
#include <tk.h>
#include <math.h>

typedef enum {
    FONT_SMALL,
    FONT_NORMAL,
    FONT_BOLD,
    FONT_BIG
} RatFont;

/*
 * Misc defines and variables describing the size of the page and
 * other related data
 */
#define PS_LEFT_MARGIN	 50
#define PS_RIGHT_MARGIN	 25
#define PS_TOP_MARGIN	 25
#define PS_BOTTOM_MARGIN 25
static int ps_xsize, ps_ysize;
static int portrait;
static int fontsize;
static int resolution;
static char *font, *boldfont;
static int *font_wx, *boldfont_wx;
static int yPos;
static int pagenum;
static int last_font = -1;
#define DATEWIDTH	fontsize*6
#define PAGENUMWIDTH	fontsize*4
#define SPACE		5
#define HINDENT		20

#define CHECK_NEWPAGE(x, y) \
        {if (yPos < SPACE) Newpage(x, y, NULL, NULL, NULL);}

/*
 * PostScript prolog, partly stolen from the tk 8.2b1 one
 */
static char *prolog = "\n\
%%BeginProlog\n\
% Define the array ISOLatin1Encoding (which specifies how characters are\n\
% encoded for ISO-8859-1 fonts), if it isn't already present (Postscript\n\
% level 2 is supposed to define it, but level 1 doesn't).\n\
\n\
systemdict /ISOLatin1Encoding known not {\n\
    /ISOLatin1Encoding [\n\
        /space /space /space /space /space /space /space /space\n\
        /space /space /space /space /space /space /space /space\n\
        /space /space /space /space /space /space /space /space\n\
        /space /space /space /space /space /space /space /space\n\
        /space /exclam /quotedbl /numbersign /dollar /percent /ampersand\n\
            /quoteright\n\
        /parenleft /parenright /asterisk /plus /comma /minus /period /slash\n\
        /zero /one /two /three /four /five /six /seven\n\
        /eight /nine /colon /semicolon /less /equal /greater /question\n\
        /at /A /B /C /D /E /F /G\n\
        /H /I /J /K /L /M /N /O\n\
        /P /Q /R /S /T /U /V /W\n\
        /X /Y /Z /bracketleft /backslash /bracketright /asciicircum \
		/underscore\n\
        /quoteleft /a /b /c /d /e /f /g\n\
        /h /i /j /k /l /m /n /o \n\
        /p /q /r /s /t /u /v /w\n\
        /x /y /z /braceleft /bar /braceright /asciitilde /space\n\
        /space /space /space /space /space /space /space /space\n\
        /space /space /space /space /space /space /space /space\n\
        /dotlessi /grave /acute /circumflex /tilde /macron /breve /dotaccent\n\
        /dieresis /space /ring /cedilla /space /hungarumlaut /ogonek /caron\n\
        /space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n\
        /dieresis /copyright /ordfeminine /guillemotleft /logicalnot /hyphen\n\
            /registered /macron\n\
        /degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph\n\
            /periodcentered\n\
        /cedillar /onesuperior /ordmasculine /guillemotright /onequarter\n\
            /onehalf /threequarters /questiondown\n\
        /Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n\
        /Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex\n\
            /Idieresis\n\
        /Eth /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis \
		/multiply\n\
        /Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Yacute /Thorn\n\
            /germandbls\n\
        /agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n\
        /egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex\n\
            /idieresis\n\
        /eth /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n\
        /oslash /ugrave /uacute /ucircumflex /udieresis /yacute /thorn\n\
            /ydieresis\n\
    ] def\n\
} if\n\
\n\
% font ISOEncode font \n\
% This procedure changes the encoding of a font from the default\n\
% Postscript encoding to ISOLatin1.  It's typically invoked just\n\
% before invoking \"setfont\".  The body of this procedure comes from\n\
% Section 5.6.1 of the Postscript book.\n\
\n\
/ISOEncode {\n\
    dup length dict begin\n\
        {1 index /FID ne {def} {pop pop} ifelse} forall\n\
        /Encoding ISOLatin1Encoding def\n\
        currentdict\n\
    end \n\
        \n\
    % I'm not sure why it's necessary to use \"definefont\" on this new\n\
    % font, but it seems to be important; just use the name \"Temporary\"\n\
    % for the font. \n\
        \n\
    /Temporary exch definefont \n\
} bind def \n\
";

/*
 * Width table for Times-Roman with the iso8859-1 encoding
 */
static int tir_wx[256] = {
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 333, 408, 500, 500, 833, 778, 333, 333, 333, 500, 564, 250,
    564, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 278, 278,
    564, 564, 564, 444, 921, 722, 667, 667, 722, 611, 556, 722, 722, 333, 389,
    722, 611, 889, 722, 722, 556, 722, 667, 556, 611, 722, 722, 944, 722, 722,
    611, 333, 278, 333, 469, 500, 333, 444, 500, 444, 500, 444, 333, 500, 500,
    278, 278, 500, 278, 778, 500, 500, 500, 500, 333, 389, 278, 500, 500, 722,
    500, 500, 444, 480, 200, 480, 541, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 278, 333, 333, 333, 333, 333,
    333, 333, 333, 250, 333, 333, 250, 333, 333, 333, 250, 333, 500, 500, 500,
    500, 200, 500, 333, 760, 276, 500, 564, 333, 760, 333, 400, 564, 300, 300,
    333, 500, 453, 250, 333, 300, 310, 500, 750, 750, 750, 444, 722, 722, 722,
    722, 722, 722, 889, 667, 611, 611, 611, 611, 333, 333, 333, 333, 722, 722,
    722, 722, 722, 722, 722, 564, 722, 722, 722, 722, 722, 722, 556, 500, 444,
    444, 444, 444, 444, 444, 667, 444, 444, 444, 444, 444, 278, 278, 278, 278,
    500, 500, 500, 500, 500, 500, 500, 564, 500, 500, 500, 500, 500, 500, 500,
    500
};

/*
 * Width table for Times-Bold with the iso8859-1 encoding
 */
static int tib_wx[256] = {
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 333, 555, 500, 500, 1000, 833, 333, 333, 333, 500, 570, 250,
    570, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 333, 333,
    570, 570, 570, 500, 930, 722, 667, 722, 722, 667, 611, 778, 778, 389, 500,
    778, 667, 944, 722, 778, 611, 778, 722, 556, 667, 722, 722, 1000, 722, 722,
    667, 333, 278, 333, 581, 500, 333, 500, 556, 444, 556, 444, 333, 500, 556,
    278, 333, 556, 278, 833, 556, 500, 556, 556, 444, 389, 333, 556, 500, 722,
    500, 500, 444, 394, 220, 394, 520, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 278, 333, 333, 333, 333, 333,
    333, 333, 333, 250, 333, 333, 250, 333, 333, 333, 250, 333, 500, 500, 500,
    500, 220, 500, 333, 747, 300, 500, 570, 333, 747, 333, 400, 570, 300, 300,
    333, 556, 540, 250, 333, 300, 330, 500, 750, 750, 750, 500, 722, 722, 722,
    722, 722, 722, 1000, 722, 667, 667, 667, 667, 389, 389, 389, 389, 722, 722,
    778, 778, 778, 778, 778, 570, 778, 722, 722, 722, 722, 722, 611, 556, 500,
    500, 500, 500, 500, 500, 722, 444, 444, 444, 444, 444, 278, 278, 278, 278,
    500, 556, 500, 500, 500, 500, 500, 570, 500, 556, 556, 556, 556, 500, 556,
    500
};

/*
 * Width table for Helvetica with the iso8859-1 encoding
 */
static int hv_wx[256] = {
    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 355, 556, 556, 889, 667, 222, 333, 333, 389, 584, 278,
    584, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278,
    584, 584, 584, 556, 1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500,
    667, 556, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667,
    611, 278, 278, 278, 469, 556, 222, 556, 556, 500, 556, 556, 278, 556, 556,
    222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500, 278, 556, 500, 722,
    500, 500, 500, 334, 260, 334, 584, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 333, 333, 333, 333, 333,
    333, 333, 333, 278, 333, 333, 278, 333, 333, 333, 278, 333, 556, 556, 556,
    556, 260, 556, 333, 737, 370, 556, 584, 333, 737, 333, 400, 584, 333, 333,
    333, 556, 537, 278, 333, 333, 365, 556, 834, 834, 834, 611, 667, 667, 667,
    667, 667, 667, 1000, 722, 667, 667, 667, 667, 278, 278, 278, 278, 722, 722,
    778, 778, 778, 778, 778, 584, 778, 722, 722, 722, 722, 667, 667, 611, 556,
    556, 556, 556, 556, 556, 889, 500, 556, 556, 556, 556, 278, 278, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 584, 611, 556, 556, 556, 556, 500, 556,
    500
};

/*
 * Width table for Helvetica-Bold with the iso8859-1 encoding
 */
static int hvb_wx[256] = {
    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 333, 474, 556, 556, 889, 722, 278, 333, 333, 389, 584, 278,
    584, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333,
    584, 584, 584, 611, 975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556,
    722, 611, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667,
    611, 333, 278, 333, 584, 556, 278, 556, 611, 556, 611, 556, 333, 611, 611,
    278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556, 333, 611, 556, 778,
    556, 556, 500, 389, 280, 389, 584, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 333, 333, 333, 333, 333,
    333, 333, 333, 278, 333, 333, 278, 333, 333, 333, 278, 333, 556, 556, 556,
    556, 280, 556, 333, 737, 370, 556, 584, 333, 737, 333, 400, 584, 333, 333,
    333, 611, 556, 278, 333, 333, 365, 556, 834, 834, 834, 611, 722, 722, 722,
    722, 722, 722, 1000, 722, 667, 667, 667, 667, 278, 278, 278, 278, 722, 722,
    778, 778, 778, 778, 778, 584, 778, 722, 722, 722, 722, 667, 667, 611, 556,
    556, 556, 556, 556, 556, 889, 556, 556, 556, 556, 556, 278, 278, 278, 278,
    611, 611, 611, 611, 611, 611, 611, 584, 611, 611, 611, 611, 611, 556, 611,
    556
};

/*
 * Width table for Courier with the iso8859-1 encoding
 */
static int co_wx[256] = {
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600
};

/*
 * Width table for Courier-Bold with the iso8859-1 encoding
 */
static int cob_wx[256] = {
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
    600
};

/*
 * Local functions
 */
static void InitPrintData(Tcl_Interp *interp);
static float GetStringLength(RatFont font, const char *string,
			     int length);
static void Newpage(Tcl_Interp *interp, Tcl_Channel channel, Tcl_Encoding enc,
		    const char *subjectArg, MESSAGECACHE *elt);
static void Startpage(Tcl_Interp *interp, Tcl_Channel channel,
                      Tcl_Encoding enc, const char *subject,
                      MESSAGECACHE *elt, int pagenum);
static void Endpage(Tcl_Channel channel);
static void PsPrintString(Tcl_Interp *interp, Tcl_Channel channel,
			  RatFont font, Tcl_Encoding enc,
			  float lm, float hm, const char *string, int length);
static void PrintHeaders(Tcl_Interp *interp, Tcl_Channel channel,
			 Tcl_Encoding enc, char *hs,
			 MessageInfo *msgPtr);
static void PrintBody(Tcl_Interp *interp, Tcl_Channel channel,
		      Tcl_Encoding enc, BodyInfo *bodyInfoPtr);
static int PrintBodyText(Tcl_Interp *interp, Tcl_Channel channel,
			 Tcl_Encoding enc, BodyInfo *bodyInfoPtr);
static int PrintBodyImage(Tcl_Interp *interp, Tcl_Channel channel,
			  BodyInfo *bodyInfoPtr);

/*
 *----------------------------------------------------------------------
 *
 * RatPrettyPrintMsgCmd --
 *
 *      Print a message prettily
 *
 * Results:
 *	A standard tcl result
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

int
RatPrettyPrintMsgCmd(ClientData dummy, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[])
{
    MESSAGECACHE *elt;
    Tcl_Channel channel;
    Tcl_CmdInfo cmdInfo;
    MessageInfo *msgPtr;
    char *subject, *hs, buf[1024];
    Tcl_Obj *oPtr, **bv;
    Tcl_Encoding enc;
    int bc, i;

    if (5 != objc) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tcl_GetString(objv[0]), " channel header_set msg bodys\"",
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Get data from options
     */
    channel = Tcl_GetChannel(interp, Tcl_GetString(objv[1]), NULL);
    hs = Tcl_GetString(objv[2]);
    if (0 == Tcl_GetCommandInfo(interp, Tcl_GetString(objv[3]), &cmdInfo)) {
        oPtr = Tcl_GetVar2Ex(interp, "t", "message_deleted", TCL_GLOBAL_ONLY);
        Tcl_SetObjResult(interp, oPtr);
	return TCL_ERROR;
    }
    msgPtr = (MessageInfo*)cmdInfo.objClientData;
    oPtr = RatMsgInfo(interp, msgPtr, RAT_FOLDER_SUBJECT);
    subject = Tcl_GetString(oPtr);
    elt = RatMessageInternalDate(interp, msgPtr);

    /*
     * Init print data
     */
    InitPrintData(interp);
    pagenum = 0;
    enc = Tcl_GetEncoding(interp, "iso8859-1");

    /* Print prelude & prolog */
    Tcl_WriteChars(channel, "%!PS-Adobe-3.0\n"
    			    "%%Createor: TkRat\n"
    			    "%%Pages: (atend)\n"
    			    "%%DocumentData: Clean7Bit\n", -1);
    snprintf(buf, sizeof(buf),
	    "%%%%Orientation: %s\n"
	    "%%%%DocumentNeededResources: font %s\n%%%%+ font %s\n",
	    (portrait ? "Portrait" : "Landscape"), font, boldfont);
    Tcl_WriteChars(channel, buf, -1);
    Tcl_WriteChars(channel, "%%EndComments\n", -1);
    Tcl_WriteChars(channel, prolog, -1);
    snprintf(buf, sizeof(buf),
	    "/smallfont /%s findfont %.2f scalefont ISOEncode def\n",
	    font, fontsize/2.0);
    Tcl_WriteChars(channel, buf, -1);
    snprintf(buf, sizeof(buf),
	    "/textfont /%s findfont %d scalefont ISOEncode def\n",
	    font, fontsize);
    Tcl_WriteChars(channel, buf, -1);
    snprintf(buf, sizeof(buf),
	    "/boldfont /%s findfont %d scalefont ISOEncode def\n",
	    boldfont, fontsize);
    Tcl_WriteChars(channel, buf, -1);
    snprintf(buf,sizeof(buf),
	    "/bigfont /%s findfont %d scalefont ISOEncode def\n",
	    boldfont, fontsize*2);
    Tcl_WriteChars(channel, buf, -1);
    Tcl_WriteChars(channel, "%%EndProlog\n", -1);

    /* Print page borders etc */
    Newpage(interp, channel, enc, subject, elt);

    /* Print headers */
    PrintHeaders(interp, channel, enc, hs, msgPtr);

    /* Print bodyparts */
    Tcl_ListObjGetElements(interp, objv[4], &bc, &bv);
    for (i=0; i<bc; i++) {
	yPos -= fontsize*1.1;
	CHECK_NEWPAGE(interp, channel);
	Tcl_GetCommandInfo(interp, Tcl_GetString(bv[i]), &cmdInfo);
	PrintBody(interp, channel, enc, (BodyInfo*)cmdInfo.objClientData);
    }

    /* Print postludium */
    Endpage(channel);
    snprintf(buf, sizeof(buf),"%%%%Trailer\n%%%%Pages: %d\n%%%%EOF\n",pagenum);
    Tcl_WriteChars(channel, buf, -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InitPrintData --
 *
 *      Initialize printing options
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Init global data
 *
 *----------------------------------------------------------------------
 */

static void
InitPrintData(Tcl_Interp *interp)
{
    Tcl_Obj *oPtr, *o2Ptr, *o3Ptr;
    CONST84 char *s, *f;
    int i;

    /*
     * Papersize
     */
    s = Tcl_GetVar2(interp, "option", "print_papersize", TCL_GLOBAL_ONLY);
    oPtr = Tcl_GetVar2Ex(interp, "option", "print_papersizes",TCL_GLOBAL_ONLY);
    Tcl_ListObjLength(interp, oPtr, &i);
    for (i--; i >= 0; i--) {
	Tcl_ListObjIndex(interp, oPtr, i, &o2Ptr);
	Tcl_ListObjIndex(interp, o2Ptr, 0, &o3Ptr);
	if (!strcmp(s, Tcl_GetString(o3Ptr))) {
	    break;
	}
    }
    Tcl_ListObjIndex(interp, o2Ptr, 1, &o3Ptr);
    Tcl_ListObjIndex(interp, o3Ptr, 0, &o2Ptr);
    Tcl_GetIntFromObj(interp, o2Ptr, &ps_xsize);
    Tcl_ListObjIndex(interp, o3Ptr, 1, &o2Ptr);
    Tcl_GetIntFromObj(interp, o2Ptr, &ps_ysize);
    ps_xsize -= PS_LEFT_MARGIN + PS_RIGHT_MARGIN;
    ps_ysize -= PS_TOP_MARGIN + PS_BOTTOM_MARGIN;

    /*
     * Orientation
     */
    s = Tcl_GetVar2(interp, "option", "print_orientation", TCL_GLOBAL_ONLY);
    if (!strcmp("portrait", s)) {
	portrait = 1;
    } else {
	portrait = 0;
	i = ps_xsize;
	ps_xsize = ps_ysize;
	ps_ysize = i;
    }

    /*
     * Fonts
     */
    f = Tcl_GetVar2(interp, "option", "print_fontfamily", TCL_GLOBAL_ONLY);
    if (!strcasecmp("helvetica", f)) {
	font = "Helvetica";
	boldfont = "Helvetica-Bold";
	font_wx = hv_wx;
	boldfont_wx = hvb_wx;
    } else if (!strcasecmp("courier", f)) {
	font = "Courier";
	boldfont = "Courier-Bold";
	font_wx = co_wx;
	boldfont_wx = cob_wx;
    } else {
	font = "Times-Roman";
	boldfont = "Times-Bold";
	font_wx = tir_wx;
	boldfont_wx = tib_wx;
    }

    /*
     * Rest of the variables
     */
    oPtr = Tcl_GetVar2Ex(interp, "option", "print_fontsize", TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &fontsize);
    oPtr = Tcl_GetVar2Ex(interp, "option", "print_resolution",TCL_GLOBAL_ONLY);
    Tcl_GetIntFromObj(interp, oPtr, &resolution);
}

/*
 *----------------------------------------------------------------------
 *
 * GetStringLength --
 *
 *      Get the length of a given string in poscript points
 *
 * Results:
 *	the length of the string
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static float
GetStringLength(RatFont font, const char *string, int length)
{
    int *wx = tir_wx, i;
    float l, size = fontsize;

    if (-1 == length) {
	length = strlen((char*)string);
    }
    switch(font) {
	case FONT_SMALL:  wx = font_wx;
			  size /= 2;
			  break;
	case FONT_NORMAL: wx = font_wx; break;
	case FONT_BOLD:   wx = boldfont_wx; break;
	case FONT_BIG:    wx = boldfont_wx;
			  size *= 2;
			  break;
    }
    for (i=l=0; i<length; i++) {
	l += wx[(unsigned char)string[i]];
    }
    return (l*size)/1000;
}

/*
 *----------------------------------------------------------------------
 *
 * Newpage --
 *
 *      Prepares a new page, finishes off the old one if needed
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static void
Newpage(Tcl_Interp *interp, Tcl_Channel channel, Tcl_Encoding encArg,
        const char *subjectArg, MESSAGECACHE *eltArg)
{
    static const char *subject;
    static Tcl_Encoding enc;
    static MESSAGECACHE *elt;

    if (subjectArg) {
	subject = subjectArg;
    }
    if (eltArg) {
	elt = eltArg;
    }
    if (encArg) {
	enc = encArg;
    }

    if (pagenum > 0) {
	Endpage(channel);
    }
    Startpage(interp, channel, enc, subject, elt, ++pagenum);
}

/*
 *----------------------------------------------------------------------
 *
 * Startpage --
 *
 *      Writes start of page data
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static void
Startpage(Tcl_Interp *interp, Tcl_Channel channel, Tcl_Encoding enc,
          const char *subject, MESSAGECACHE *elt, int pagenum)
{
    char buf[1024];
    const char *cPtr;
    CONST84 char *s;
    float x, y, w, h, l;
    int objc;
    Tcl_Obj **objv, *oPtr;
    Tcl_DString ds;

    /*
     * prepare
     */
    snprintf(buf, sizeof(buf), "%%%%Page: %d %d\n"
	     "save\n"
	     "%d %d translate\n",
	     pagenum, pagenum, PS_LEFT_MARGIN, PS_BOTTOM_MARGIN);
    Tcl_WriteChars(channel, buf, -1);

    /*
     * Background
     */
    x = SPACE/2;
    y = ps_ysize+SPACE/2;
    w = ps_xsize-SPACE;
    h=fontsize*2.6+SPACE;
    snprintf(buf, sizeof(buf), ".8 setgray %.2f %.2f %.2f %.2f rectfill\n"
		 "0 setgray %.2f %.2f %.2f %.2f rectstroke\n",
		 x, y-h, w, h, x, y-h, w, h);
    Tcl_WriteChars(channel, buf, -1);

    /*
     * Date
     */
    x = SPACE;
    y = ps_ysize;
    w = fontsize*2.4;
    h = fontsize*2.6;
    snprintf(buf, sizeof(buf), "%.2f %.2f moveto %.2f 0 rlineto 0 -%.2f "
		 "rlineto -%.2f 0 rlineto closepath stroke\n", x, y, w, h, w);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%d", elt->day);
    l = GetStringLength(FONT_BOLD, buf, -1);
    sprintf(buf, "%.2f %.2f moveto boldfont setfont (%d) show\n",
	    x+w/2-l/2, y-fontsize*1.1, elt->day);
    Tcl_WriteChars(channel, buf, -1);
    oPtr = Tcl_GetVar2Ex(interp, "t", "months", TCL_GLOBAL_ONLY),
    Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);
    s = Tcl_GetString(objv[elt->month-1]);
    l = GetStringLength(FONT_SMALL, s, -1);
    snprintf(buf, sizeof(buf),"%.2f %.2f moveto smallfont setfont (%s) show\n",
	    x+w/2-l/2, y-fontsize*1.7, s);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%d", elt->year+BASEYEAR);
    l = GetStringLength(FONT_SMALL, buf, -1);
    sprintf(buf, "%.2f %.2f moveto smallfont setfont (%d) show\n",
	    x+w/2-l/2, y-fontsize*2.3, elt->year+BASEYEAR);
    Tcl_WriteChars(channel, buf, -1);

    x += fontsize*3;
    y -= fontsize/2;
    s = Tcl_GetVar2(interp, "t", "received", TCL_GLOBAL_ONLY);
    snprintf(buf, sizeof(buf),"%.2f %.2f moveto smallfont setfont (%s) show\n",
	    x, y, s);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%.2f %.2f moveto textfont setfont (%02d:%02d) show\n",
	    x, y-fontsize*1.2, elt->hours, elt->minutes);
    Tcl_WriteChars(channel, buf, -1);

    /*
     * Subject
     */
    x = SPACE+DATEWIDTH;
    y = ps_ysize-1.7*fontsize;
    yPos = ps_ysize-fontsize*4;
    Tcl_WriteChars(channel, buf, -1);
    s = Tcl_GetVar2(interp, "t", "mail_regarding", TCL_GLOBAL_ONLY);
    snprintf(buf, sizeof(buf),"boldfont setfont %.2f %.2f moveto\n(%s) show\n",
	    x, y, s);
    Tcl_WriteChars(channel, buf, -1);
    x += GetStringLength(FONT_BOLD, s, -1);
    snprintf(buf, sizeof(buf),"textfont setfont %.2f %.2f moveto\n(", x+2, y);
    Tcl_WriteChars(channel, buf, -1);
    Tcl_UtfToExternalDString(enc, subject, -1, &ds);
    for (cPtr = Tcl_DStringValue(&ds); *cPtr; cPtr++) {
	if ('(' == *cPtr || ')' == *cPtr || '\\' == *cPtr) {
	    Tcl_WriteChars(channel, "\\", 1);
	}
	if (*cPtr >= 32 && *cPtr < 127) {
	    Tcl_WriteChars(channel, cPtr, 1);
	} else {
	    snprintf(buf, sizeof(buf), "\\%o", *cPtr);
	    Tcl_WriteChars(channel, buf, -1);
	}
    }
    Tcl_DStringFree(&ds);
    Tcl_WriteChars(channel, ") show\n", -1);

    /*
     * Page number
     */
    s = Tcl_GetVar2(interp, "t", "page", TCL_GLOBAL_ONLY);
    x = ps_xsize - PAGENUMWIDTH - GetStringLength(FONT_SMALL, s, -1);
    y = ps_ysize-fontsize;
    snprintf(buf, sizeof(buf),"%.2f %.2f moveto smallfont setfont (%s) show\n",
	    x, y, s);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%d", pagenum);
    x = ps_xsize-PAGENUMWIDTH/2 - GetStringLength(FONT_BIG, buf, -1)/2;
    y = ps_ysize-2*fontsize;
    sprintf(buf, "bigfont setfont %.2f %.2f moveto (%d) show\n", x, y,pagenum);
    Tcl_WriteChars(channel, buf, -1);

    /*
     * Can not set this to FONT_BIG because then tab_width will not be
     * calculated in PsPrintString().
     */
    last_font = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Endpage --
 *
 *      Writes end of page data
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static void
Endpage(Tcl_Channel channel)
{
    Tcl_WriteChars(channel, "restore\nshowpage\n", -1);
}

/*
 *----------------------------------------------------------------------
 *
 * PsPrintString --
 *
 *      Prints the given string with the given margins. May break the
 *	line if it does not fit, in that case yPos is updated to point at
 *	the last line.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel and may modify yPos and
 *	initialize a new page.
 *
 *----------------------------------------------------------------------
 */

static void
PsPrintString(Tcl_Interp *interp, Tcl_Channel channel, RatFont font,
	      Tcl_Encoding enc, float lm, float hm,
	      const char *string, int length)
{
    static float tab_width;
    char buf[1024], *fn = "";
    const unsigned char *cPtr;
    float ll, l;
    Tcl_DString ds;

    if (font != last_font) {
        switch (font) {
	case FONT_SMALL:  fn = "smallfont"; break;
	case FONT_NORMAL: fn = "textfont"; break;
	case FONT_BOLD:	  fn = "boldfont"; break;
	case FONT_BIG:	  fn = "bigfont"; break;
        }
        snprintf(buf, sizeof(buf), "%s setfont ", fn);
        Tcl_WriteChars(channel, buf, -1);
        last_font = font;
        tab_width = GetStringLength(font, "XXXXXXXX", 8);
    }

    Tcl_UtfToExternalDString(enc, string, length, &ds);

    snprintf(buf, sizeof(buf), "%.2f %d moveto\n(", lm, yPos);
    Tcl_WriteChars(channel, buf, -1);
    for (cPtr = (unsigned char*)Tcl_DStringValue(&ds), ll=lm; *cPtr; cPtr++) {
        if ('\t' == *cPtr) {
            ll = floor((ll-lm+tab_width)/tab_width)*tab_width+lm;
            snprintf(buf, sizeof(buf), ") show\n%.2f %d moveto (", ll, yPos);
            Tcl_WriteChars(channel, buf, -1);
        } else {
            ll += (l = GetStringLength(font, (char*)cPtr, 1));
            if (ll > ps_xsize-hm) {
                Tcl_WriteChars(channel, ") show\n", -1);
                yPos -= fontsize*1.1;
                CHECK_NEWPAGE(interp, channel);
                snprintf(buf, sizeof(buf), "%.2f %d moveto\n(", lm, yPos);
                Tcl_WriteChars(channel, buf, -1);
                ll = lm;
            }
            
            if ('(' == *cPtr || ')' == *cPtr || '\\' == *cPtr) {
                Tcl_WriteChars(channel, "\\", 1);
            }
            if (*cPtr >= 32 && *cPtr < 127) {
                Tcl_WriteChars(channel, (char*)cPtr, 1);
            } else {
                snprintf(buf, sizeof(buf), "\\%o", *cPtr);
                Tcl_WriteChars(channel, buf, -1);
            }
        }
    }
    Tcl_WriteChars(channel, ") show\n", -1);
    Tcl_DStringFree(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * PrintHeaders --
 *
 *      Prints the selected message headers
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static void
PrintHeaders(Tcl_Interp *interp, Tcl_Channel channel,
	     Tcl_Encoding enc, char *hs, MessageInfo *msgPtr)
{
    Tcl_Obj **fhv, **phv, *oPtr, *tPtr, *o2Ptr, **thv;
    char buf[1024], *s;
    int i, j, l, fhc, phc;
    float maxX, *lengths;

    if (!strcmp("none", hs)) {
	return;
    }

    /*
     * Get headers to print
     */
    snprintf(buf, sizeof(buf), "%s headers", msgPtr->name);
    Tcl_Eval(interp, buf);
    Tcl_ListObjGetElements(interp, Tcl_GetObjResult(interp), &fhc, &fhv);
    if (!strcmp("selected", hs)) {
	oPtr = Tcl_GetVar2Ex(interp, "option", "show_header_selection",
		TCL_GLOBAL_ONLY);
	Tcl_ListObjLength(interp, oPtr, &l);
	phv = (Tcl_Obj**)ckalloc(sizeof(Tcl_Obj*)*l);
	for (i=phc=0; i<l; i++) {
	    Tcl_ListObjIndex(interp, oPtr, i, &o2Ptr);
	    s = Tcl_GetString(o2Ptr);
	    for (j=0; j<fhc; j++) {
		Tcl_ListObjIndex(interp, fhv[j], 0, &o2Ptr);
		if (!strcasecmp(s, Tcl_GetString(o2Ptr))) {
		    phv[phc++] = fhv[j];
		    break;
		}
	    }
	}
    } else {
	phv = fhv;
	phc = fhc;
    }
    thv = (Tcl_Obj**)ckalloc(phc * sizeof(char*));

    /*
     * Translate header names
     */
    for (i=0; i<phc; i++) {
	Tcl_ListObjIndex(interp, phv[i], 0, &oPtr);
        strlcpy(buf, Tcl_GetString(oPtr), sizeof(buf));
        for (s=buf; *s; s++) {
            if ('-' == *s) {
                *s = '_';
            } else if (isupper((unsigned char)*s)) {
                *s = tolower((unsigned char)*s);
            }
        }
        tPtr = Tcl_GetVar2Ex(interp, "t", buf, TCL_GLOBAL_ONLY);
        if (!tPtr) {
            tPtr = oPtr;
        }
        Tcl_IncrRefCount(tPtr);
        thv[i] = tPtr;
    }
        
    /*
     * Find maximum length
     */
    lengths = (float*)ckalloc(sizeof(float)*phc);
    for (i=maxX=0; i<phc; i++) {
	lengths[i] = GetStringLength(FONT_BOLD, Tcl_GetString(thv[i]), -1);
	if (lengths[i] > maxX) {
	    maxX = lengths[i];
	}
    }
    maxX += GetStringLength(FONT_BOLD, ": ", 2);

    /*
     * Print headers
     */
    for (i=0; i<phc; i++) {
	CHECK_NEWPAGE(interp, channel);
	snprintf(buf, sizeof(buf), "%s:", Tcl_GetString(thv[i]));
	PsPrintString(interp, channel, FONT_BOLD, enc,
		      HINDENT + maxX - lengths[i], 0, buf, -1);
	Tcl_ListObjIndex(interp, phv[i], 1, &oPtr);
	PsPrintString(interp, channel, FONT_NORMAL, enc, HINDENT + maxX + 10,
		      0, Tcl_GetString(oPtr), -1);
	yPos -= fontsize*1.1;
    }

    /*
     * Cleanup
     */
    if (!strcmp("selected", hs)) {
	ckfree(phv);
    }
    for (i=0; i<phc; i++) {
        Tcl_DecrRefCount(thv[i]);
    }
    ckfree(thv);
    ckfree(lengths);
}

/*
 *----------------------------------------------------------------------
 *
 * PrintBody --
 *
 *      Prints the given bodypart
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static void
PrintBody(Tcl_Interp *interp, Tcl_Channel channel, Tcl_Encoding enc,
	  BodyInfo *bodyInfoPtr)
{
    Tcl_Obj *oPtr = RatBodyType(bodyInfoPtr), **objv;
    int objc;
    char buf[1024], buf2[42];
    CONST84 char *m;

    Tcl_ListObjGetElements(interp, oPtr, &objc, &objv);

    /*
     * Try to print it
     */
    if (!strcasecmp("TEXT", Tcl_GetString(objv[0]))) {
	if (TCL_OK == PrintBodyText(interp, channel, enc, bodyInfoPtr)) {
	    return;
	}
    } else if (!strcasecmp("IMAGE", Tcl_GetString(objv[0]))) {
	if (TCL_OK == PrintBodyImage(interp, channel, bodyInfoPtr)) {
	    return;
	}
    }

    /*
     * Print failure notice
     */
    m = Tcl_GetVar2(interp, "t", "unprintable", TCL_GLOBAL_ONLY);
    snprintf(buf2, sizeof(buf2), "%s/%s", Tcl_GetString(objv[0]),
	    Tcl_GetString(objv[1]));
    snprintf(buf, sizeof(buf), m, buf2);
    CHECK_NEWPAGE(interp, channel);
    PsPrintString(interp, channel, FONT_BOLD, enc, SPACE, SPACE, buf, -1);
    yPos -= fontsize*1.1;

}

/*
 *----------------------------------------------------------------------
 *
 * PrintBodyText --
 *
 *      Prints the given text/ bodypart
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static int
PrintBodyText(Tcl_Interp *interp, Tcl_Channel channel, Tcl_Encoding enc,
	      BodyInfo *bodyInfoPtr)
{
    Tcl_Obj *oPtr;
    char *cPtr, *nPtr;

    oPtr = RatBodyData(interp, bodyInfoPtr, 0, NULL);
    Tcl_IncrRefCount(oPtr);

    cPtr = Tcl_GetString(oPtr);
    while (*cPtr) {
	if (NULL == (nPtr = strchr(cPtr, '\n'))) {
	    nPtr = cPtr + strlen(cPtr)-1;
	}
	CHECK_NEWPAGE(interp, channel);
	PsPrintString(interp, channel, FONT_NORMAL, enc, SPACE, SPACE, cPtr,
		nPtr-cPtr);
	yPos -= fontsize*1.1;
	cPtr = nPtr+1;
    }

    Tcl_DecrRefCount(oPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PrintBodyImage --
 *
 *      Prints the given text/ bodypart
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Adds data to the passed channel
 *
 *----------------------------------------------------------------------
 */

static int
PrintBodyImage(Tcl_Interp *interp, Tcl_Channel channel, BodyInfo *bodyInfoPtr)
{
    Tk_PhotoHandle ph;
    Tk_PhotoImageBlock block;
    Tcl_Obj *objv[5], *namePtr;
    int i, r, l, psw, psh, x, y;
    char buf[1024];
    unsigned char *p, *bp;

    /*
     * Get image data
     */
    objv[i=0] = Tcl_NewStringObj("image", -1);
    objv[++i] = Tcl_NewStringObj("create", -1);
    objv[++i] = Tcl_NewStringObj("photo", -1);
    objv[++i] = Tcl_NewStringObj("-data", -1);
    objv[++i] = RatCode64(RatBodyData(interp, bodyInfoPtr, 0, NULL));
    r = Tcl_EvalObjv(interp, i, objv, 0);
    for (; i>=0; i--) {
	Tcl_DecrRefCount(objv[i]);
    }
    if (TCL_OK != r) {
	return TCL_ERROR;
    }
    namePtr = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(namePtr);
    ph = Tk_FindPhoto(interp, Tcl_GetString(namePtr));
    Tk_PhotoGetImage(ph, &block);

    /*
     * Print it
     */
    psw = (block.width*72)/resolution;
    psh = (block.height*72)/resolution;
    if (yPos < psh+SPACE) {
	Newpage(interp, channel, NULL, NULL, NULL);
    }
    yPos -= psh;
    sprintf(buf, "gsave\n/picstr %d string def\n", block.width*3);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%d %d translate\n", ps_xsize/2-psw/2, yPos);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%d %d scale\n", psw, psh);
    Tcl_WriteChars(channel, buf, -1);
    sprintf(buf, "%d %d 8 [%d 0 0 -%d 0 %d]\n", block.width, block.height,
	    block.width, block.height, block.height);
    Tcl_WriteChars(channel, buf, -1);
    Tcl_WriteChars(channel,
	    "{currentfile picstr readhexstring pop} false 3 colorimage\n",
	    -1);
    for (l=y=0, p = block.pixelPtr; y < block.height;
	    y++, p = bp+block.pitch) {
	bp = p;
	for (x=0; x<block.width; x++, p += block.pixelSize) {
	    sprintf(buf, "%02x%02x%02x", p[block.offset[0]],
		    p[block.offset[1]], p[block.offset[2]]);
	    Tcl_WriteChars(channel, buf, -1);
	    if (++l == 13) {
		Tcl_WriteChars(channel, "\n", -1);
		l = 0;
	    }
	}
    }
    Tcl_WriteChars(channel, "\n", -1);
    Tcl_WriteChars(channel, "grestore\n", -1);

    /*
     * Cleanup
     */
    objv[i=0] = Tcl_NewStringObj("image", -1);
    objv[++i] = Tcl_NewStringObj("delete", -1);
    objv[++i] = namePtr;
    r = Tcl_EvalObjv(interp, i+1, objv, 0);
    Tcl_DecrRefCount(namePtr);
    for (; i>=0; i--) {
	Tcl_DecrRefCount(objv[i]);
    }

    return TCL_OK;
}
