puts "$HEAD Test disconnected folders in offline mode"

namespace eval test_disoffline {
    global start_uid

    variable uidmap {}
    variable uid_lastlocal 0
    variable uid_lastremote [expr $start_uid-1]
}

proc test_disoffline::verify_map {mf map} {
    foreach e $map {
	set expected($e) 1
    }
    if {![catch "open $mf r" f]} {
	set f [open $mf r]
	file copy -force $mf /tmp/map
	while {-1 != [gets $f line]} {
	    if {[catch {unset expected($line)}]} {
		close $f
		return "Did not expect [list $line]"
	    }
	}
	close $f
    }
    if {0 != [array size expected]} {
	return "Did not find [list [array names expected]]"
    }
    return ""
}

proc test_disoffline::dis_verify {f map name {diff 0}} {
    variable uidmap

    set expected_map $uidmap
    set num [expr {[llength $uidmap]+$diff}]

    set i [lindex [$f info] 1]
    if {$num != $i} {
	ReportError "$name: found $i messages expected $num"
	return
    }
    set err [verify_map $map $expected_map]
    if {"" != $err} {
	ReportError "$name: map verify failed\n$uidmap\n$err"
    }
}

proc test_disoffline::add_to_uidmap {} {
    variable uidmap
    variable uid_lastlocal
    variable uid_lastremote

    lappend uidmap [list [incr uid_lastremote] [incr uid_lastlocal]]
}

proc test_disoffline::add_mixed_to_uidmap {} {
    variable uidmap
    variable uid_lastlocal
    variable uid_lastremote

    lappend uidmap [list [expr {$uid_lastremote+2}] [expr {$uid_lastlocal+1}]]
    lappend uidmap [list [expr {$uid_lastremote+1}] [expr {$uid_lastlocal+2}]]
    incr uid_lastlocal +2
    incr uid_lastremote +2
}

proc test_disoffline::remove_from_uidmap {index} {
    variable uidmap

    set uidmap [lreplace $uidmap $index $index]
}

proc test_disoffline::test_disoffline {} {
    global option dir hdr mailServer imap_def dis_def imap_map \
	    msg1 msg2 msg3 msg4 msg5 msg6 msg7 msg8 msg9 msg10 \
	    msg11 msg12 msg13 msg14 msg15 msg16 msg17 msg18 msg19

    # Setup
    InitTestmsgs
    RatLibSetOnlineMode 0
    set tmpfn [pwd]/folder.[pid]-tmp
    set tmpdef [list Test file {} $tmpfn]

    init_imap_folder $imap_def
    insert_imap $imap_def $msg1

    StartTest "opening"
    set f [RatOpenFolder $dis_def]
    dis_verify $f $imap_map "Initial"
    StartTest "update after open"
    $f update update
    dis_verify $f $imap_map "After first update"

    StartTest "update after netsync"
    $f netsync
    $f update update
    add_to_uidmap
    dis_verify $f $imap_map "After netsync"

    StartTest "new mail arrival"
    insert_imap $imap_def $msg2
    $f update update
    dis_verify $f $imap_map "Before netsync"
    add_to_uidmap
    $f netsync
    $f update sync
    dis_verify $f $imap_map "After 1 new message"

    StartTest "multiple new messages"
    insert_imap $imap_def $msg3 $msg4
    add_to_uidmap
    add_to_uidmap
    $f netsync
    $f update update
    dis_verify $f $imap_map "After 2 new messages"

    StartTest "deleting message"
    $f setFlag 1 deleted 1
    $f update sync
    $f netsync
    remove_from_uidmap 1
    dis_verify $f $imap_map "After deleting"

    StartTest "new message and one deleted"
    $f setFlag 1 deleted 1
    remove_from_uidmap 1
    insert_imap $imap_def $msg5
    $f netsync
    $f update sync
    add_to_uidmap
    dis_verify $f $imap_map "After new & deleted"

    StartTest "inserting one message"
    set fh [open $tmpfn w]
    puts $fh $hdr
    puts $fh $msg6
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    set m [$f2 get 0]
    $f insert $m
    $f2 close
    file delete $tmpfn
    $f netsync
    $f update sync
    add_to_uidmap
    dis_verify $f $imap_map "After inserting"

    StartTest "inserting one message two times (different)"
    set fh [open $tmpfn w]
    puts $fh $msg7
    puts $fh $msg8
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0]
    $f insert [$f2 get 1]
    $f2 close
    add_to_uidmap
    add_to_uidmap
    file delete $tmpfn
    $f netsync
    $f update sync
    dis_verify $f $imap_map "After inserting one two times"

    StartTest "inserting two messages"
    set fh [open $tmpfn w]
    puts $fh $msg9
    puts $fh $msg10
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0] [$f2 get 1]
    $f2 close
    add_to_uidmap
    add_to_uidmap
    file delete $tmpfn
    $f netsync
    $f update sync
    dis_verify $f $imap_map "After inserting two"

    StartTest "deleting inserted directly"
    set n [lindex [$f info] 1]
    set fh [open $tmpfn w]
    puts $fh $msg11
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0]
    $f2 close
    file delete $tmpfn
    $f setFlag $n deleted 1
    $f netsync
    $f update sync
    add_to_uidmap
    remove_from_uidmap $n
    dis_verify $f $imap_map "After immediately deleted inserted"

    StartTest "flagging"
    set option(cache_conn) 0
    $f netsync
    set f2 [RatOpenFolder $imap_def]
    $f2 setFlag 0 flagged 1
    if {0 != [$f getFlag 0 flagged]} {
	$f2 close
        ReportError "Flag set before it was expected to be"
    } else {
	$f2 close
	$f netsync
	dis_verify $f $imap_map "After setting flag"
	if {1 != [$f getFlag 0 flagged]} {
	    ReportError "Flag not set after sync"
	}
    }
    set option(cache_conn) 1

    StartTest "resetting folder"
    set num [lindex [$f info] 1]
    for {set i 0} {$i < $num} {incr i} {
	$f setFlag $i deleted 1
	remove_from_uidmap 0
    }
    insert_imap $imap_def $msg12
    add_to_uidmap
    $f netsync
    $f update sync
    dis_verify $f $imap_map "After reset"

    # Cleanup
    $f close
    cleanup_imap_folder $imap_def
    file delete -force $dir/disconnected

    # Restore environment
    RatLibSetOnlineMode 0
}

test_disoffline::test_disoffline
