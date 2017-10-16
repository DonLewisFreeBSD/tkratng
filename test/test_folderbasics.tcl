puts "$HEAD Test basic folder operations"

namespace eval test_folderbasics {
}

proc test_folderbasics::check_folder {type fn fh msglist} {
    global hdr imap_serv cyrus_dir

    set num [llength $msglist]
    foreach m $msglist {
	foreach l [split $m "\n"] {
	    if {[regexp -nocase {subject:[ ]*(.+)$} $l unused s]} {
		set s1($s) ""
		set s2($s) ""
		break
	    }
	}
    }

    # Apply checks to folder handle
    if {$num != [lindex [$fh info] 1]} {
	ReportError "Number of messages in tkrat folder is wrong [lindex [$fh info] 1] != $num"
    }
    foreach s [$fh list %s] {
	if {![info exists s1($s)]} {
	    ReportError "Subject '$s' not found in tkrat folder"
	} else {
	    unset s1($s)
	}
    }
    if {[array size s1]} {
	ReportError "Subjects [array names s1] not found in tkrat folder"
    }

    # Apply checks to underlying file
    if {"imap" == $type && "cyrus" == $imap_serv} {
	set f [open "|cyrcat $cyrus_dir $fn"]
    } else {
	set f [open $fn r]
	seek $f [string length $hdr]
    }
    set found 0
    while {-1 != [gets $f line]} {
	if {[regexp -nocase {subject:[ ]*(.+)$} $line unused s]} {
	    incr found
	    if {![info exists s2($s)]} {
		ReportError "Subject '$s' not found in underlying file"
	    } else {
		unset s2($s)
	    }
	}
    }
    close $f
    if {$num != $found} {
	ReportError "Number of messages in underlying folder is wrong $found != $num"
    }
    if {[array size s2]} {
	ReportError "Subjects [array names s2] not found in underlying file"
    }
}

proc test_folderbasics::type_tests {type fn1 def1 fn2 def2} {
    global option dir hdr imap_serv \
	    msg1 msg2 msg3 msg4 msg5 msg6 msg7 msg8 msg9 msg10 \
	    msg11 msg12 msg13 msg14 msg15 msg16 msg17 msg18 msg19

    InitTestmsgs

    StartTest "$type: Opening folder with two messages"
    if {"imap" == [lindex $def1 1]} {
	init_imap_folder $def1
	insert_imap $def1 $msg1 $msg2
    } else {
	set fh [open $fn1 w]
	puts $fh $hdr
	puts $fh $msg1
	puts $fh $msg2
	close $fh
    }
    set f1 [RatOpenFolder $def1]
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg1 $msg2]

    StartTest "$type: New message arrival"
    if {"imap" == [lindex $def1 1]} {
	insert_imap $def1 $msg3
    } else {
	set fh [open $fn1 a]
	puts $fh $msg3
	close $fh
    }
    $f1 update sync
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg1 $msg2 $msg3]

    StartTest "$type: Message deletion"
    $f1 setFlag 1 deleted 1
    $f1 update sync
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg1 $msg3]

    StartTest "$type: Deleting multiple messages"
    $f1 setFlag 0 deleted 1
    $f1 setFlag 1 deleted 1
    $f1 update sync
    check_folder [lindex $def1 1] $fn1 $f1 {}

    StartTest "$type: Multiple new message arrival"
    if {"imap" == [lindex $def1 1]} {
	insert_imap $def1 $msg4 $msg5
    } else {
	set fh [open $fn1 a]
	puts $fh $msg4
	puts $fh $msg5
	close $fh
    }
    $f1 update sync
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg4 $msg5]

    StartTest "$type: New message deletion and simultaneously new message"
    if {"imap" == [lindex $def1 1]} {
	insert_imap $def1 $msg6
    } else {
	set fh [open $fn1 a]
	puts $fh $msg6
	close $fh
    }
    $f1 setFlag 1 deleted 1
    $f1 update sync
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg4 $msg6]

    StartTest "$type: Inserting message"
    if {"imap" == [lindex $def2 1]} {
	init_imap_folder $def2
	insert_imap $def2 $msg7
    } else {
	set fh [open $fn2 w]
	puts $fh $hdr
	puts $fh $msg7
	close $fh
    }
    set f2 [RatOpenFolder $def2]
    $f2 list "%s"
    set m [$f2 get 0]
    $f1 insert $m
    $f2 close
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg4 $msg6 $msg7]

    StartTest "$type: Inserting multiple messages"
    if {"imap" == [lindex $def2 1]} {
	insert_imap $def2 $msg8
	insert_imap $def2 $msg9
    } else {
	set fh [open $fn2 a]
	puts $fh $msg8
	puts $fh $msg9
	close $fh
    }
    set f2 [RatOpenFolder $def2]
    $f2 list "%s"
    set m1 [$f2 get 1]
    set m2 [$f2 get 2]
    $f1 insert $m1 $m2
    $f2 close
    check_folder [lindex $def1 1] $fn1 $f1 [list $msg4 $msg6 $msg7 $msg8 $msg9]

    StartTest "$type: Copying to another (not open) folder"
    $f1 list "%s"
    set m [$f1 get 0]
    $m copy $def2
    set f2 [RatOpenFolder $def2]
    check_folder [lindex $def2 1] $fn2 $f2 [list $msg7 $msg8 $msg9 $msg4]

    StartTest "$type: Copying to another open folder"
    set m [$f1 get 1]
    $m copy $def2
    $f2 update update
    check_folder [lindex $def2 1] $fn2 $f2 [list $msg7 $msg8 $msg9 $msg4 $msg6]

    # Cleanup
    set old_cache $option(cache_conn)
    set option(cache_conn) 0
    $f1 close
    $f2 close
    set option(cache_conn) $old_cache
    if {"imap" == [lindex $def1 1]} {
	cleanup_imap_folder $def1
	cleanup_imap_folder $def2
    }
}

proc test_folderbasics::test_folderbasics {} {
    global option dir hdr mailServer imap_def1 imap_def2 \
	    imap_fn1 imap_fn2 \
	    msg1 msg2 msg3 msg4 msg5 msg6 msg7 msg8 msg9 msg10 \
	    msg11 msg12 msg13 msg14 msg15 msg16 msg17 msg18 msg19

    set fn1 $dir/folder.[pid]-1
    set fn2 $dir/folder.[pid]-2

    # Test file-folders
    set def1 [list Test1 file {} $fn1]
    set def2 [list Test2 file {} $fn2]
    type_tests File $fn1 $def1 $fn2 $def2

    # Test imap-folders with connection caching
    set old_cache $option(cache_conn)
    set otion(cache_conn) 1
    type_tests IMAP-cache $imap_fn1 $imap_def1 $imap_fn2 $imap_def2

    # Test imap-folders without connection caching
    set otion(cache_conn) 0
    type_tests IMAP-nocache $imap_fn1 $imap_def1 $imap_fn2 $imap_def2
    set option(cache_conn) $old_cache

    # Prepare coming test
    set def1 [list Test1 file {} $fn1]
    set fh [open $fn1 w]
    puts $fh $hdr
    puts $fh $msg1
    puts $fh $msg2
    close $fh
    init_imap_folder $imap_def2
    insert_imap $imap_def2 $msg3

    StartTest "Copying between a file and an (not open) imap folder"
    set f1 [RatOpenFolder $def1]
    set m [$f1 get 0]
    $m copy $imap_def2
    set f2 [RatOpenFolder $imap_def2]
    check_folder imap $imap_fn2 $f2 [list $msg3 $msg1]

    StartTest "Copying between a file and an open imap folder"
    set m [$f1 get 1]
    $m copy $imap_def2
    check_folder imap $imap_fn2 $f2 [list $msg3 $msg1 $msg2]

    # Cleanup
    set old_cache $option(cache_conn)
    set option(cache_conn) 0
    $f1 close
    $f2 close
    set option(cache_conn) $old_cache
    file delete $fn1 $fn2
    cleanup_imap_folder $imap_def2
}

test_folderbasics::test_folderbasics
