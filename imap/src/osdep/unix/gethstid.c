/*
 * Program:	Get host ID emulator
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	3 May 1995
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Emulator for BSD gethostid() call
 * Returns: unique identifier for this machine
 */

long gethostid (void)
{
  /* No gethostid() here, so just fake it and hope things turn out okay. */
  return 0xdeadface;
}
