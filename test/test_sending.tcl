puts "$HEAD Test sending"

namespace eval test_sending {
    # Messages to send
    variable messages

    # Location of files
    variable tmpfn [pwd]/folder.[pid]-tmp

    # Error message when send failed
    variable error_msg
}

proc test_sending::generate_send_command {} {
    global tclsh_binary

    set cmd [pwd]/testsend.[pid]
    set fh [open $cmd w 0755]
    puts $fh "#!$tclsh_binary"
    puts $fh {
# Usage: sendcmd STATUS FD AMOUNT WHEN RECIPIENTS

set out [open "$argv0-out" w 0600]
puts $out [lrange $argv 4 end]
set mfd [lindex $argv 1]
if {"big" == [lindex $argv 2]} {
	set msg "Start of message\n"
	for {set i 0} {$i < [expr 9000/64]} {incr i} {
	    set msg "$msg[string repeat M 63]\n"
	}
    set msg "${msg}End of message"
} else {
    set msg "Message"
}
set when [lindex $argv 3]

while {-1 != [gets stdin line]} {
    if {"during" == $when} {
	puts $mfd $msg
    }
    puts $out $line
}
if {"after" == $when} {
    puts $mfd $msg
}
exit [lindex $argv 0]
}
    close $fh
    return $cmd
}


proc test_sending::generate_messages {num} {
    variable tmpfn
    variable messages
    global hdr

    set fh [open $tmpfn w 0600]
    puts $fh $hdr
    for {set i 0} {$i < $num} {incr i} {
	set i2 [format "%02d" $i]
	set id "<$i@no.such.domain>"
	set date "Sat,  5 Jul 2002 10:14:$i2 +0100 (MET)"
	set m "From maf@math.chalmers.se Tue Sep  5 18:02:22 2000 +0100
Date: $date
From: Martin Forssen <maf@tkrat.org>
Sender: Martin Forssen <maf@tkrat.org>
Reply-To: Martin Forssen <maf@tkrat.org>
Subject: test $i2
To: Martin Forssen $i <maf@tkrat.org>
cc: cc@tkrat.org
Bcc: bcc@tkrat.org
Message-ID: $id
MIME-Version: 1.0
Content-Type: TEXT/PLAIN; CHARSET=us-ascii
X-TkRat-Internal-role: r0

test $i
"
	puts $fh $m
	lappend messages $m
    }

    set m "From maf@math.chalmers.se Tue Sep  5 18:02:22 2000 +0100
Date: Sat,  5 Jul 2002 10:14:42 +0100 (MET)
From: Martin Forssen <maf@tkrat.org>
Sender: Martin Forssen <maf@tkrat.org>
Reply-To: Martin Forssen <maf@tkrat.org>
Subject: Test with a really long subject line which definitely should wrap Y into at least three lines. Specially since it invokes the magic formula Y Shrimp-sandwich
To: Martin Forssen <maf@tkrat.org>
cc: cc@tkrat.org
Bcc: bcc@tkrat.org
Message-ID: <1@no,such.domain>
MIME-Version: 1.0
Content-Type: TEXT/PLAIN; CHARSET=us-ascii
X-TkRat-Internal-role: r0

test with long header line
"
    puts $fh $m
    set r [regsub -all "Y" $m "Y\n" m]
    lappend messages $m

    close $fh
    set tmpdef [list Test file {} $tmpfn]
    return [RatOpenFolder $tmpdef]
}

proc test_sending::generate_complicated_message {num} {
    variable tmpfn
    variable messages
    global hdr

    set fh [open $tmpfn a 0600]
    puts $fh $hdr
    for {set i 0} {$i < $num} {incr i} {
	set i2 [format "%02d" $i]
	set id "<$i@no.such.domain>"
	set date "Sat,  5 Jul 2002 10:14:$i2 +0100 (MET)"
	set m "From maf@math.chalmers.se Tue Sep  5 18:02:22 2000 +0100
Date: Sat,  5 Jul 2002 10:14:42 +0100 (MET)
From: Martin Forssen <maf@tkrat.org>
Sender: Martin Forssen <maf@tkrat.org>
Reply-To: Martin Forssen <maf@tkrat.org>
Subject: Test with a really long subject line which definitely should wrap into at least three lines. Specially since it invokes the magic formula 'Räckmackan'
To: Martin Forssen <maf@tkrat.org>
cc: cc@tkrat.org
Bcc: bcc@tkrat.org
Message-ID: <1@no,such.domain>
MIME-Version: 1.0
Content-Type: TEXT/PLAIN; CHARSET=us-ascii
X-TkRat-Internal-role: r0

test $i
"
	puts $fh $m
	lappend messages $m
    }
    close $fh
    set tmpdef [list Test file {} $tmpfn]
    return [RatOpenFolder $tmpdef]
}

proc test_sending::count {needle haystack} {
    set n 0
    set pos 0
    while {-1 != [set i [string first $needle $haystack $pos]]} {
	incr n
	set pos [expr $i+1]
    }
    return $n
}

proc RatSendFailed {msg errmsg} {
    global vFolderDef vFolderOutgoing
    global test_sending::error_msg

    set test_sending::error_msg $errmsg
}

proc test_sending::do_send {message_index mf} {
    global vFolderDef vFolderOutgoing folderExists tickle
    variable error_msg

    set error_msg "NONE"
    set fh [RatOpenFolder $vFolderDef($vFolderOutgoing)]
    set msg [$mf get $message_index]
    $fh insert $msg
    $fh close
    RatNudgeSender

    # Wait for send to complete
    for {set i 0} {$i < 600} {incr i} {
	# Force event loop
	after 100 "set tickle 1"
	vwait tickle

	if {"NONE" != $error_msg} {
	    set result fail
	    break
	}
	if {0 == $folderExists($fh)} {
	    set result ok
	    break
	}
    }
    if {"NONE" != $error_msg} {
	set result fail
    }
    return $result
}

proc test_sending::test_prog {message_index mf cmd status fd amount when}  {
    global option verbose
    variable error_msg
    variable messages

    if {0 != $status} {
	set expected fail
    } else {
	set expected ok
    }
    StartTest "Prog $message_index $expected $fd $amount $when"

    # Setup for sending
    set option(r0,sendprot) prog
    set option(r0,sendprog) "$cmd $status $fd $amount $when"
    set option(smtp_verbose) 0
    file delete -force $cmd-out

    set result [do_send $message_index $mf]

    # Prepare for testing
    set m [lrange [split [lindex $messages $message_index] "\n"] 1 end]

    # Handle expected failures
    if {0 != $status} {
	if {"fail" != $result} {
	    ReportError "Succeeded when a failure was expected"
	    return
	}
	# Check error_msg
	if {"stderr" != $fd} {
	    if {"" != $error_msg} {
		ReportError "Got error message when no stderr output"
		if {$verbose} {
		    puts $error_msg
		}
		return
	    }
	    return
	}
	if {"silent" == $when} {
	    if {"" != $error_msg} {
		ReportError "Got error message in silent mode"
		if {$verbose} {
		    puts $error_msg
		}
		return
	    }
	    return
	}
	set cm [count "Message" $error_msg]
	set csm [count "Start of message" $error_msg]
	set cem [count "End of message" $error_msg]
	set lines [llength $m]
	if {"during" == $when} {
	    # The '2' is for the bcc and X-TkRat lines
	    set expected [expr $lines-2]
	} else {
	    set expected 1
	}
	if {"big" == $amount} {
	    if {$cm != 0 || $csm != $expected || $cem != $csm} {
		ReportError "Unexpected message"
		puts "$cm != 0 || $csm != $expected || $cem != $csm"
		return
	    }
	} else {
	    if {$cm != $expected || $csm != 0 || $cem != $csm} {
		ReportError "Unexpected message"
		puts "$cm != $expected || $csm != 0 || $cem != $csm"
		return
	    }
	}
	return
    }

    # We only get here if the send was expected to work
    if {"ok" != $result} {
	ReportError "Send failed unexpectedly\n$error_msg"
	return
    }

    # Check generated message
    set fh [open "[lindex $cmd 0]-out" r]
    gets $fh line
    if {"maf@tkrat.org cc@tkrat.org bcc@tkrat.org" != $line} {
	ReportError "Unexpected recipients\n$line"
	return
    }
    set i 0
    while {-1 != [gets $fh line]} {
	while {[string match -nocase "bcc:*" [lindex $m $i]]
	    || [string match -nocase "x-tkrat-internal*" [lindex $m $i]]} {
	    incr i
	}
	if {$i > [llength $m]} {
	    ReportError "Sent message too long\n[list $line]"
	    close $fh
	    return
	}
	if {$line != [lindex $m $i]} {
	    ReportError [join [list "Sent message differs from expected" \
				   "Got: <$line>" \
				   "Exp: <[lindex $m $i]>"] "\n"]
	    close $fh
	    return
	}
	incr i
    }
    close $fh
    if {$i < [llength $m]} {
	ReportError "Sent message too short\nGot $i lines expected [llength $m]"
	return
    }
    return
}

proc test_sending::test_smtp {message_index mf status desc} {
    global option
    variable messages
    variable smtp_recipients

    StartTest "SMTP $desc"

    # Setup for sending
    set option(r0,from) "maf@tkrat.org"
    set option(r0,sendprot) smtp
    set option(r0,smtp_hosts) "localhost:[smtp_server::get_port]"
    set option(r0,smtp_user) ""
    set option(r0,smtp_passwd) ""
    set option(smtp_verbose) 4

    smtp_server::prepare_incoming $status
    set result [do_send $message_index $mf]
    if {"$result" != "$status"} {
	ReportError "Sending got state '$result' when '$status' was expected"
    }

    # Get result
    set r [smtp_server::get_received]
    
    # Check that nothing got delivered in the failure case
    if {"fail" == $status} {
	if {"" != [lindex $r 1]} {
	    ReportError "Got message even though sending failed"
	}
	return
    }

    # Check recipients
    if {"[lindex $r 0]" != "maf@tkrat.org cc@tkrat.org bcc@tkrat.org"} {
	ReportError "Wrong recipients of SMTP message\nGot: '[lindex $r 0]'"
    }

    # Check the message which got delivered
    set exp [lrange [split [lindex $messages $message_index] "\n"] 1 end]
    set i 0
    foreach line [lindex $r 1] {
	while {[string match -nocase "bcc:*" [lindex $exp $i]]
	    || [string match -nocase "x-tkrat-internal*" [lindex $exp $i]]} {
	    incr i
	}
	if {$i > [llength $exp]} {
	    ReportError "Sent message too long\n[list $line]"
	    return
	}
	if {$line != [lindex $exp $i]} {
	    ReportError [join [list "Sent message differs from expected" \
				   "Got: <$line>" \
				   "Exp: <[lindex $exp $i]>"] "\n"]
	    return
	}
	incr i
    }
    if {$i < [llength $exp]} {
	ReportError "Sent message too short\nMissing [expr [llength $exp]-$i] lines"
	return
    }
}

proc test_sending::test_sending {} {
    global option tickle

    # Initialize stuff
    set cmd [generate_send_command]
    set mf [generate_messages 11]
    RatLibSetOnlineMode 1

    # Test program sending
    test_prog  0 $mf $cmd 0 stderr silent after
    test_prog  1 $mf $cmd 1 stderr silent after
    test_prog  2 $mf $cmd 0 stderr normal after
    test_prog  3 $mf $cmd 1 stderr normal after
    test_prog  4 $mf $cmd 0 stderr big after
    test_prog  5 $mf $cmd 1 stderr big after
    test_prog  6 $mf $cmd 0 stderr normal during
    test_prog  7 $mf $cmd 1 stderr normal during
    test_prog  8 $mf $cmd 0 stderr big during
    test_prog  9 $mf $cmd 1 stderr big during
    test_prog 10 $mf $cmd 0 stdout big during
    test_prog 11 $mf $cmd 0 stderr normal after

    # Test SMTP sending
    set option(cache_conn) 0
    smtp_server::start

    test_smtp 0 $mf ok "Simple message ok expected"
    test_smtp 1 $mf fail "Simple message fail expected"

    # Test connectio caching
    set option(cache_conn) 0
    set option(cache_conn_timeout) 10
    test_smtp 0 $mf ok "Simple message should not keep conn open"
    if {[smtp_server::get_state] != "disconnected"} {
	ReportError "Cached connection when option(cache_conn) = 0"
    }
    set option(cache_conn) 1
    set option(cache_conn_timeout) 1
    test_smtp 1 $mf ok "Simple message should keep conn open (cache check)"
    set o [smtp_server::get_opens]
    if {[smtp_server::get_state] != "command"} {
	ReportError "SMTP stream not in command mode when expected"
    }
    test_smtp 2 $mf ok "Simple message should reuse conn"
    if {[smtp_server::get_opens] != $o} {
	ReportError "SMTP stream not reused as expected"
    }
    after 2000 "set tickle 1"
    vwait tickle
    if {[smtp_server::get_state] != "disconnected"} {
	ReportError "Connection still open after expected timeout"
    }
    set option(cache_conn_timeout) 3
    test_smtp 3 $mf ok "Simple message should keep conn open (extend check)"
    after 2000 "set tickle 1"
    vwait tickle
    if {[smtp_server::get_state] != "command"} {
	ReportError "SMTP stream not in command mode when expected"
    }
    test_smtp 4 $mf ok "Simple message which should extend the timer"
    after 2000 "set tickle 1"
    vwait tickle
    if {[smtp_server::get_state] != "command"} {
	ReportError "SMTP stream not in command mode when expected"
    }
    after 2000 "set tickle 1"
    vwait tickle
    if {[smtp_server::get_state] != "disconnected"} {
	ReportError "Connection still open after expected timeout"
    }
    set option(cache_conn_timeout) 10
    test_smtp 5 $mf ok "Simple message should keep conn open (server closes)"
    smtp_server::close_session
    test_smtp 6 $mf ok "Simple message which should use closed cached conn"


    test_smtp 11 $mf ok "Complicated message ok expected"

    $mf close
    smtp_server::stop
}

test_sending::test_sending
