puts "$HEAD Test sending"

namespace eval test_sending {
    # The latest message we received
    variable message {}

    # List of the subjects of all received messages
    variable received {}

    # Gets set to done when a new message has been received
    variable result {}

    # List of files to remove when we are done
    variable cleanup {}

    # Unique id
    variable id 0
}

proc test_sending::smtp_server {c host port} {
    variable message
    variable received
    variable result
    global debug

    if {$debug} {
	puts "SMTP connection from $host:$port"
    }
    fconfigure $c -buffering line
    puts $c "220 SMTP simulator"
    set mode initial
    set message ""
    while {![catch {gets $c line} r] && -1 != $r && "quit" != $mode} {
	regsub -all "\r" $line {} line
	set cmd [string toupper [lindex $line 0]];
	if {"initial" == $mode && "EHLO" == $cmd} {
	    set resp "250-Hello on yourself"
	    set resp "$resp\n250-EXPN"
	    set resp "$resp\n250-VERB"
	    set resp "$resp\n250-8BITMIME"
	    set resp "$resp\n250-SIZE"
	    set resp "$resp\n250-DSN"
	    set resp "$resp\n250-ONEX"
	    set resp "$resp\n250-ETRN"
	    set resp "$resp\n250-XUSR"
	    set resp "$resp\n250 HELP"
	    set mode command
	} elseif {"command" == $mode && "MAIL" == $cmd} {
	    set resp "250 sender ok"
	    set mode get_rcpt
	} elseif {"command" == $mode && "QUIT" == $cmd} {
	    set resp "221 closing connection"
	    set mode quit
	} elseif  {"get_rcpt" == $mode && "RCPT" == $cmd} {
	    set resp "250 rcpt ok"
	} elseif  {"get_rcpt" == $mode && "DATA" == $cmd} {
	    set resp "354 Enter mail"
	    set mode data
	} elseif {"data" == $mode && "." != $line} {
	    lappend message $line
	    continue
	} elseif {"data" == $mode && "." == $line} {
	    set resp "250 Message accepted"
	    set mode command
	} elseif {"RSET" == $line} {
	    set resp "250 reset"
	    set mode command
	} else {
	    set resp "500 Command unrecognized"
	}
	puts $c $resp
    }
    catch {close $c}

    foreach l $message {
	if {![string compare -nocase "subject:" [lindex $l 0]]} {
	    lappend received [lindex [split $l { }] 1]
	    break
	}
    }

    set result done
}

proc test_sending::generate {msgid} {
    variable cleanup
    variable id
    global option

    set handler test_sending::hd[incr id]
    upvar #0 $handler hd
    upvar #0 ${handler}_body bhd
    set tmp $option(send_cache)/rt_[pid].[incr id]
    lappend cleanup $tmp

    set hd(to) tester
    set hd(from) ratatosk_test
    set hd(subject) $msgid
    set hd(message_id) <${msgid}@kilauea.no.such.domain>
    set hd(charset) us-ascii
    set hd(body) ${handler}_body
    set bhd(type) text
    set bhd(subtype) plain
    set bhd(encoding) 7bit
    set bhd(filename) $tmp
    set bhd(copy) 0
    set bhd(removeFile) 1

    set f [open $tmp w]
    puts $f "Test message $msgid"
    close $f

    return $handler
}

proc test_sending::verify {hd id name} {
    variable result
    variable received
    global errors LEAD

    set aid [after 2000 {set test_sending::result timeout}]
    vwait test_sending::result
    if {"done" != $result || $id != $received} {
	puts "$LEAD $name: failed $result $received"
	incr errors
    }
    after cancel $aid
}

proc test_sending::verify_nosend {name} {
    variable received
    global errors LEAD

    after 5000
    if {0 != [llength $received]} {
	puts "$LEAD $name: failed send seems to have occurred $received"
	incr errors
    }
}


proc test_sending::test_sending {} {
    variable message
    variable result
    variable cleanup
    variable received
    global option LEAD

    # Setup server
    set port 42301
    set server [socket -server test_sending::smtp_server $port]
    file mkdir $option(send_cache)

    # Configure
    set option($option(default_role),sendprot) smtp
    set option($option(default_role),smtp_hosts) localhost:$port

    puts "Test simple direct sending case"
    set option(delivery_mode) direct
    RatLibSetOnlineMode 1
    set hd [generate 1]
    after 100 "RatSend send $hd"
    verify $hd 1 "direct sending"

    puts "Test deferred"
    set option(delivery_mode) deferred
    RatLibSetOnlineMode 1
    set received {}
    set hd [generate 2]
    after 100 RatSend send $hd
    verify_nosend "deferred sending"
    after 100 RatSend sendDeferred
    verify $hd 2 "deferred sending"

    puts "Test direct when offline->online"
    set option(delivery_mode) direct
    RatLibSetOnlineMode 0
    set hd [generate 3]
    set received {}
    after 100 RatSend send $hd
    verify_nosend "direct sending when offline"
    after 100 RatSend sendDeferred
    verify_nosend "direct sending when offline (triggered by sendDeferred)"
    after 100 {RatLibSetOnlineMode 1}
    verify $hd 3 "direct sending after we have gone online"

    puts "Test deferred when offline->online"
    set option(delivery_mode) deferred
    RatLibSetOnlineMode 0
    set hd [generate 4]
    set received {}
    after 100 RatSend send $hd
    verify_nosend "deferred sending when offline"
    after 100 {RatLibSetOnlineMode 1}
    verify_nosend "deferred sending when offline (triggered by online)"
    after 100 RatSend sendDeferred
    verify $hd 4 "deferred sending after we have triggered it"

    # Cleanup
    close $server
    foreach f $cleanup {
	file delete -force $f
    }
}

test_sending::test_sending
