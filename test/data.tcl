# This file contains test messages etc.

set hdr {From MAILER-DAEMON Fri Dec  1 07:34:39 2000
Date: 01 Dec 2000 07:34:39 +0100
From: Mail System Internal Data <MAILER-DAEMON@kilauea.firedoor.se>
Subject: DON'T DELETE THIS MESSAGE -- FOLDER INTERNAL DATA
Message-ID: <975652479@kilauea.firedoor.se>
X-IMAP: 0975652479 0000000010
Status: RO

This text is part of the internal format of your mail folder, and is not
a real message.  It is created automatically by the mail system software.
If deleted, important folder data will be lost, and it will be re-created
with the data reset to initial values.

}

set timestamp [clock format [clock seconds] -format "%Y%m%d%H%M%S"]

for {set i 1} {$i < 21} {incr i} {
    upvar #0 msg$i m
    set i2 [format "%02d" $i]
    set date "Sun, 26 Nov 2000 12:36:$i2 +0100 (MET)"
    set data [string repeat "123456789\n" $i]
    set m "From maf@math.chalmers.se Tue Sep  5 18:02:22 2000 +0100
Date: $date
Message-Id: <$timestamp-$i@kilauea.firedoor.se>
From: Martin Forssen <maf@math.chalmers.se>
Subject: test $i2
To: Martin Forssen $i <maf@math.chalmers.se>
MIME-Version: 1.0
Content-Type: TEXT/plain; charset=us-ascii

test $i
$data
"
}
