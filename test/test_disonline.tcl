puts "$HEAD Test disconnected folders in online mode"

namespace eval test_disonline {
    global start_uid

    variable uidmap {}
    variable uid_lastlocal 0
    variable uid_lastremote [expr $start_uid-1]
}

proc test_disonline::verify_map {mf map} {
    foreach e $map {
	set expected($e) 1
    }
    if {[catch {open $mf r} f]} {
	return 0
    }
    file copy -force $mf /tmp/map
    while {-1 != [gets $f line]} {
	if {[catch {unset expected($line)}]} {
	    close $f
	    return 0
	}
    }
    close $f
    if {0 != [array size expected]} {
	return 0
    }
    return 1
}

proc test_disonline::dis_verify {f map name {diff 0}} {
    variable uidmap

    set expected_map $uidmap
    set num [expr {[llength $uidmap]+$diff}]

    set i [lindex [$f info] 1]
    if {$num != $i} {
	ReportError "$name: Got $i expected $num"
    }
    if {![verify_map $map $expected_map]} {
	ReportError "$name: map verify failed"
    }
}

proc test_disonline::add_to_uidmap {} {
    variable uidmap
    variable uid_lastlocal
    variable uid_lastremote

    lappend uidmap [list [incr uid_lastremote] [incr uid_lastlocal]]
}

proc test_disonline::add_mixed_to_uidmap {} {
    variable uidmap
    variable uid_lastlocal
    variable uid_lastremote

    lappend uidmap [list [expr {$uid_lastremote+2}] [expr {$uid_lastlocal+1}]]
    lappend uidmap [list [expr {$uid_lastremote+1}] [expr {$uid_lastlocal+2}]]
    incr uid_lastlocal +2
    incr uid_lastremote +2
}

proc test_disonline::remove_from_uidmap {index} {
    variable uidmap

    set uidmap [lreplace $uidmap $index $index]
}

proc test_disonline::verify_flag {f index flag expected desc} {
    set real [$f getFlag $index $flag]
    if {$expected != $real} {
        ReportError "$desc: flag $flag was $real (expected $expected)"
    }
}

proc test_disonline::test_disonline {} {
    global dir errors mailServer dis_def imap_map imap_def \
	    msg1 msg2 msg3 msg4 msg5 msg6 msg7 msg8 msg9 msg10 \
	    msg11 msg12 msg13 msg14 msg15 msg16 msg17 msg18 msg19 msg20

    # Setup
    InitTestmsgs
    RatLibSetOnlineMode 1
    set tmpfn [pwd]/folder.[pid]-tmp
    set tmpdef [list Test file {} $tmpfn]

    init_imap_folder $imap_def
    insert_imap $imap_def $msg1
    add_to_uidmap

    StartTest "opening"
    set f [RatOpenFolder $dis_def]
    dis_verify $f $imap_map "Initial"

    StartTest "update after open"
    $f update update
    dis_verify $f $imap_map "After first update"

    StartTest "new mail arrival"
    insert_imap $imap_def $msg2
    add_to_uidmap
    $f update sync
    dis_verify $f $imap_map "After 1 new message"
    $f close

    StartTest "opening again"
    set f [RatOpenFolder $dis_def]
    dis_verify $f $imap_map "Second open"

    StartTest "new mail arrival again"
    insert_imap $imap_def $msg18
    add_to_uidmap
    $f update sync
    dis_verify $f $imap_map "After another new message"

    StartTest "multiple new messages"
    insert_imap $imap_def $msg3 $msg4
    add_to_uidmap
    add_to_uidmap
    after 300
    $f update update
    dis_verify $f $imap_map "After 2 new messages"

    StartTest "deleting message"
    $f setFlag 1 deleted 1
    $f update sync
    remove_from_uidmap 1
    dis_verify $f $imap_map "After deleting"

    StartTest "new message and one deleted"
    $f setFlag 1 deleted 1
    remove_from_uidmap 1
    insert_imap $imap_def $msg5
    $f update sync
    add_to_uidmap
    dis_verify $f $imap_map "After new & deleted"

    StartTest "inserting one message"
    set fh [open $tmpfn w]
    puts $fh $msg6
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    set m [$f2 get 0]
    $f insert $m
    $f2 close
    file delete $tmpfn
    after 300
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
    $f update sync
    dis_verify $f $imap_map "After inserting two"

    StartTest "inserting two messages sequentially"
    set fh [open $tmpfn w]
    puts $fh $msg9
    puts $fh $msg10
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0]
    $f insert [$f2 get 1]
    $f2 close
    add_to_uidmap
    add_to_uidmap
    file delete $tmpfn
    $f close
    set f [RatOpenFolder $dis_def]
    $f update sync
    dis_verify $f $imap_map "After inserting two sequentially"

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
    $f update sync
    add_to_uidmap
    remove_from_uidmap $n
    dis_verify $f $imap_map "After immediately deleted inserted"

    StartTest "insert, delete and new messages"
    set fh [open $tmpfn w]
    puts $fh $msg12
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0]
    add_to_uidmap
    $f2 close
    file delete $tmpfn
    $f setFlag 1 deleted 1
    remove_from_uidmap 1
    insert_imap $imap_def $msg13
    add_to_uidmap
    $f update sync
    dis_verify $f $imap_map "After insert, deleted and new"

    StartTest "resetting folder"
    for {set i 0} {$i <9} {incr i} {
	$f setFlag $i deleted 1
	remove_from_uidmap 0
    }
    insert_imap $imap_def $msg14
    add_to_uidmap
    $f update sync
    dis_verify $f $imap_map "After reset"

    StartTest "offline-->online transition"
    # Go offline
    RatLibSetOnlineMode 0
    $f update sync
    dis_verify $f $imap_map "When offline"
    RatLibSetOnlineMode 1
    dis_verify $f $imap_map "When online again"
    $f update sync
    dis_verify $f $imap_map "After sync"
 
    StartTest "offline-->online transition (with flag changes)"
    RatLibSetOnlineMode 0
    $f setFlag 1 seen 0
    $f setFlag 1 seen 1
    $f setFlag 1 flagged 1
    $f setFlag 1 flagged 0
    dis_verify $f $imap_map "When offline"
    verify_flag $f 1 seen 1 "When offline"
    verify_flag $f 1 flagged 0 "When offline"
    # Go online
    RatLibSetOnlineMode 1
    dis_verify $f $imap_map "When online"
    verify_flag $f 1 seen 1 "When online"
    verify_flag $f 1 flagged 0 "When online"
    $f update sync
    dis_verify $f $imap_map "After sync"
    verify_flag $f 1 seen 1 "After sync"
    verify_flag $f 1 flagged 0 "After sync"

    StartTest "offline-->online transition (with new remote messages)"
    RatLibSetOnlineMode 0
    # Insert two new in remote
    insert_imap $imap_def $msg15 $msg16
    $f update sync
    dis_verify $f $imap_map "When offline"
    # Go online
    add_to_uidmap
    add_to_uidmap
    RatLibSetOnlineMode 1
    dis_verify $f $imap_map "When online"

    StartTest "offline-->online transition (with new local messages)"
    RatLibSetOnlineMode 0
    # Insert one new in local
    set fh [open $tmpfn w]
    puts $fh $msg17
    puts $fh $msg18
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0] [$f2 get 1]
    $f2 close
    file delete $tmpfn
    dis_verify $f $imap_map "When offline" 2
    add_to_uidmap
    add_to_uidmap
    # Go online
    RatLibSetOnlineMode 1
    dis_verify $f $imap_map "When online"

    StartTest "offline-->online transition (with new messages in both)"
    RatLibSetOnlineMode 0
    # Insert one new in local
    set fh [open $tmpfn w]
    puts $fh $msg19
    close $fh
    set f2 [RatOpenFolder $tmpdef]
    $f2 list "%s"
    $f insert [$f2 get 0]
    $f2 close
    file delete $tmpfn
    # Insert one new in remote
    insert_imap $imap_def $msg20
    $f update sync
    dis_verify $f $imap_map "When offline" 1
    add_mixed_to_uidmap
    # Go online
    RatLibSetOnlineMode 1
    dis_verify $f $imap_map "When online"

    StartTest "offline-->online transition (with local delete)"
    RatLibSetOnlineMode 0
    # Set delete flag in lcoal
    # The seen flag manipulation here is to simulate what happens when one
    # reads a folder. And it did trigger a bug once.
    $f setFlag 1 seen 0
    $f setFlag 1 deleted 1
    $f setFlag 1 seen 1
    dis_verify $f $imap_map "When offline"
    # Go online
    RatLibSetOnlineMode 1
    dis_verify $f $imap_map "When online"
    $f update sync
    remove_from_uidmap 1
    dis_verify $f $imap_map "After sync"

    # Cleanup
    $f close
    cleanup_imap_folder $imap_def
    file delete -force $dir/disconnected

    # Restore environment
    RatLibSetOnlineMode 0
}

test_disonline::test_disonline
