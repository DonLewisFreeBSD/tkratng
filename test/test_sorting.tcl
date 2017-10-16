puts "$HEAD Test message sorting"

namespace eval test_sorting {
}

proc test_sorting::check_order {fh msglist} {
    global errors LEAD

}

proc test_sorting::test_sorting {} {
    global LEAD option dir hdr errors mailServer verbose \
	    msg1 msg2 msg3 msg4 msg5 msg6 msg7 msg8 msg9 msg10 \
	    msg11 msg12 msg13 msg14 msg15 msg16 msg17 msg18 msg19
    variable tsmsg

    # Prepare test folder
    set fn $dir/folder.[pid]
    set def [list Test file {} $fn]
    set fh [open $fn w]
    puts $fh $hdr
    foreach m [list $msg1 $msg2 $msg3 $msg4 $msg5 \
	    $msg6 $msg7 $msg8 $msg9 $msg10] {
	puts $fh $m
    }
    close $fh

    # Loop through basic tests
    set f1 [RatOpenFolder $def]
    foreach tc {
	        {folder {1 2 3 4 5 6 7 8 9 10}}
	        {reverseFolder {10 9 8 7 6 5 4 3 2 1}}
	        {date {1 2 3 4 5 6 7 8 9 10}}
	        {reverseDate {10 9 8 7 6 5 4 3 2 1}}
	        {size {1 2 3 4 5 6 7 8 9 10}}
	        {reverseSize {10 9 8 7 6 5 4 3 2 1}}
	        {subject {1 2 3 4 5 6 7 8 9 10}}
	        {subjectonly {1 2 3 4 5 6 7 8 9 10}}
               } {
        puts "Testing sort order '[lindex $tc 0]'..."
	$f1 setSortOrder [lindex $tc 0]
        $f1 update update
        set expected {}
        foreach m [lindex $tc 1] {
	    lappend expected [format "test %02d" $m]
	}
	set current [$f1 list %s]
	if {$expected != $current} {
	    puts "$LEAD: failed"
	    if {$verbose} {
		puts "$LEAD: Expected:"
		foreach m $expected {puts $m}
		puts "$LEAD: Got:"
		foreach m $current {puts $m}
	    }
	    incr errors
	}
    }
    $f1 close
    file delete $fn

    foreach func {get_simple_thread get_back_thread
	get_real_thread get_strange_msgid} {
	set gts [eval $func]
	puts "Testing sort order 'threaded' [lindex $gts 0] ..."

	set fn $dir/folder.[pid]
	set def [list Test file {} $fn]
	set fh [open $fn w+]
	puts $fh $hdr
	foreach msg [lindex $gts 1] {
	    puts $fh $msg
	}
	close $fh
	set f1 [RatOpenFolder $def]
	$f1 setSortOrder threaded
	$f1 update update
	set expected [lindex $gts 2]
	set real [$f1 list "%t%M"]
	if {[llength $expected] != [llength $real]} {
	    puts "Fail lengthdiff"
	    set fail 1
	} else {
	    set fail 0
	    for {set i 0} {$fail == 0 && $i < [llength $expected]} {incr i} {
		if {[lindex $expected $i] != [lindex $real $i]} {
		    puts "Fail <[lindex $expected $i]> != <[lindex $real $i]>"
		    set fail 1
		}
	    }
	}
	$f1 close
	file delete $fn
	if {$fail} {
	    incr errors
	    puts "$LEAD: threads sorting failed"
	    if {$verbose} {
		puts "Expected:"
		foreach l $expected {
		    puts "$l"
		}
		puts "Real:"
		foreach l $real {
		    puts "$l"
		}
	    }
	}
    }
}

proc test_sorting::get_simple_thread {} {
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m1@foo.bar>
Date: Thu, 06 Sep 2001 14:25:00
Subject: other

THIS: msg1
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m2@foo.bar>
Date: Thu, 06 Sep 2001 14:25:01
Subject: foo

THIS: msg2
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m3@foo.bar>
Date: Thu, 06 Sep 2001 14:25:02
Subject: foo
In-Reply-To: <m2@foo.bar>

THIS: msg3
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m4@foo.bar>
Date: Thu, 06 Sep 2001 14:25:03
Subject: foo
In-Reply-To: <m3@foo.bar>

THIS: msg4
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m5@foo.bar>
Date: Thu, 06 Sep 2001 14:25:04
Subject: foo
In-Reply-To: <m2@foo.bar>

THIS: msg5
}
    set expected {
	{m1@foo.bar}
	{m2@foo.bar}
	{+m3@foo.bar}
	{|+m4@foo.bar}
	{+m5@foo.bar}
    }
    return [list "simple" $ml $expected]
}

proc test_sorting::get_back_thread {} {
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m1@foo.bar>
Date: Thu, 06 Sep 2001 14:25:00
Subject: other

THIS: msg1
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m2@foo.bar>
Date: Thu, 06 Sep 2001 14:25:01
Subject: foo
In-Reply-To: <m4@foo.bar>

THIS: msg2
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m3@foo.bar>
Date: Thu, 06 Sep 2001 14:25:02
Subject: foo
In-Reply-To: <m4@foo.bar>

THIS: msg3
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m4@foo.bar>
Date: Thu, 06 Sep 2001 14:25:03
Subject: foo

THIS: msg4
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m5@foo.bar>
Date: Thu, 06 Sep 2001 14:25:04
Subject: bar

THIS: msg5
}
    set expected {
	{m1@foo.bar}
	{m4@foo.bar}
	{+m2@foo.bar}
	{+m3@foo.bar}
	{m5@foo.bar}
    }
    return [list "backref" $ml $expected]
}

proc test_sorting::get_real_thread {} {
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m2@foo.bar>
Date: Thu, 06 Sep 2001 14:25:09 -0400
Subject: foo
In-Reply-To: <m10@foo.bar>
References: <m5@foo.bar>
 <m5@foo.bar>

THIS: msg1
}
    lappend ml {
From maf@kilauea Thu Sep  6 16:00:23 2001 -0400
Message-Id: <m12@foo.bar>
In-Reply-To: <m2@foo.bar>
References: <m10@foo.bar>
 <m5@foo.bar>
 <m5@foo.bar>
Date: Thu, 6 Sep 2001 16:00:23 -0400
Subject: foo

THIS: msg2
}
    lappend ml {
From maf@kilauea Thu Sep  6 18:03:18 2001 -0400
Message-Id: <m4@foo.bar>
Date: Thu, 06 Sep 2001 18:03:18 -0400
Subject: foo
In-Reply-To: <m12@foo.bar>
References: <m2@foo.bar>
 <m10@foo.bar>
 <m5@foo.bar>
 <m5@foo.bar>

THIS: msg3
}
    lappend ml {
From maf@kilauea Thu Sep  6 23:01:30 2001 +0200
Date: Thu, 6 Sep 2001 23:01:30 +0200 (CEST)
Subject: foo
In-Reply-To: <m2@foo.bar>
Message-ID: <m8@foo.bar>

THIS: msg4
}
    lappend ml {
From maf@kilauea Thu Sep  6 23:12:57 2001 +0200
Date: Thu, 6 Sep 2001 23:12:57 +0200 (MEST)
Subject: foo
In-Reply-To: <m8@foo.bar>
Message-ID: <m6@foo.bar>

THIS: msg5
}
    lappend ml {
From maf@kilauea Fri Sep  7 00:01:11 2001 +0200
Date: Fri, 7 Sep 2001 00:01:11 +0200 (CEST)
Subject: foo
In-Reply-To: <m6@foo.bar>
Message-ID: <m9@foo.bar>

THIS: msg6
}
    lappend ml {
From maf@kilauea Fri Sep  7 00:06:46 2001 +0200
Date: Fri, 7 Sep 2001 00:06:46 +0200 (MEST)
Subject: foo
In-Reply-To: <m9@foo.bar>
Message-ID: <m7@foo.bar>

THIS: msg7
}
    lappend ml {
From maf@kilauea Thu Sep  6 21:15:51 2001 -0400
Subject: foo
References: <m2@foo.bar>
 <m3@foo.bar>
Date: 06 Sep 2001 21:15:51 -0400
In-Reply-To: Bar Foo's message of "Thu, 06 Sep 2001 17:44:02 -0400"
Message-ID: <m11@foo.bar>

THIS: msg8
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:35:50 2001 -0700
Date: Thu, 06 Sep 2001 14:25:09 -0400
Message-Id: <m1@foo.bar>
Subject: foo
In-Reply-To: <m8@foo.bar> from "Foo Bar" at Sep 06, 2001 11:01:30 PM

THIS: msg9
}
    lappend ml {
From maf@kilauea Thu Sep  6 17:44:02 2001 -0400
Message-Id: <m3@foo.bar>
Date: Thu, 06 Sep 2001 17:44:02 -0400
Subject: foo
In-Reply-To: <m8@foo.
 bar>
References: <m2@foo.bar>

THIS: msg10
}   
    set expected {
	{m2@foo.bar}
	{+m12@foo.bar}
	{|+m4@foo.bar}
	{+m8@foo.bar}
	{ +m1@foo.bar}
	{ +m6@foo.bar}
	{ |+m9@foo.bar}
	{ | +m7@foo.bar}
	{ +m3@foo.bar}
	{  +m11@foo.bar}
    }
    return [list "Real-sample" $ml $expected]
}

proc test_sorting::get_strange_msgid {} {
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <Joe_Doe@[127.0.0.1]>
Date: Thu, 06 Sep 2001 14:28:00
Subject: other

THIS: msg1
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m2@foo.bar>
Date: Thu, 06 Sep 2001 14:27:00
In-Reply-To: <"Joe_Doe"@[127.0.0.1]>
Subject: foo

THIS: msg2
}
    lappend ml {
From maf@kilauea Thu Sep  6 14:25:09 2001 -0400
Message-Id: <m3@foo.bar>
Date: Thu, 06 Sep 2001 14:26:00
In-Reply-To: <"Joe\_Doe"@[127\.0\.0\.1]>
Subject: bar

THIS: msg3
}
    set expected {
	{Joe_Doe@[127.0.0.1]}
	{+m3@foo.bar}
	{+m2@foo.bar}
    }
    return [list "Contorted msgid" $ml $expected]
}

test_sorting::test_sorting
