@ECHO OFF
REM Program:	Portable C client makefile -- MS-DOS B&W link
REM
REM Author:	Mark Crispin
REM		Networks and Distributed Computing
REM		Computing & Communications
REM		University of Washington
REM		Administration Building, AG-44
REM		Seattle, WA  98195
REM		Internet: MRC@CAC.Washington.EDU
REM
REM Date:	26 June 1994
REM Last Edited:24 October 2000
REM
REM The IMAP toolkit provided in this Distribution is
REM Copyright 2000 University of Washington.
REM
REM The full text of our legal notices is contained in the file called
REM CPYRIGHT, included with this Distribution.

link /NOI /stack:32767 mtest.obj,mtest.exe,,cclient.lib llbwtcp.lib llibce.lib
