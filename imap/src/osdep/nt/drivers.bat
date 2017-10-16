@ECHO OFF
REM Program:	Driver Linkage Generator for DOS
REM
REM Author:	Mark Crispin
REM		Networks and Distributed Computing
REM		Computing & Communications
REM		University of Washington
REM		Administration Building, AG-44
REM		Seattle, WA  98195
REM		Internet: MRC@CAC.Washington.EDU
REM
REM Date:	11 October 1989
REM Last Edited:8 February 2001
REM
REM The IMAP toolkit provided in this Distribution is
REM Copyright 2001 University of Washington.
REM
REM The full text of our legal notices is contained in the file called
REM CPYRIGHT, included with this Distribution.

REM Erase old driver linkage
IF EXIST LINKAGE.* DEL LINKAGE.*

REM Set the default driver
ECHO #define DEFAULTPROTO %1proto > LINKAGE.H

REM Now define the new list
FOR %%D IN (%2 %3 %4 %5 %6 %7 %8 %9) DO CALL DRIVRAUX %%D

EXIT 0
