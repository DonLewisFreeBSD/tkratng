# Implements a smtp-server

namespace eval smtp_server {
    # Port to use for SMTP
    variable port 42301

    # The latest message we received
    variable message {}

    # smtp state
    variable state disconnected

    # Numer of connections to SMTP-server
    variable opens 0

    # Channel between server and client. Used to forcibly close the conn
    variable channel

    # List of recipients of the last message
    variable recipients {}

    # "ok" if the server should accept the message
    variable action ok

    # Server socket
    variable server
}

# Start the smtp-server
proc smtp_server::start {} {
    variable port
    variable server

    while {[catch "socket -server smtp_server::open $port" server]} {
	incr port
    }
}

# Get port number
proc smtp_server::get_port {} {
    variable port

    return $port
}

# Prepare server for new incoming message
proc smtp_server::prepare_incoming {{new_action {ok}}} {
    variable message {}
    variable action $new_action
}

# Get current state
proc smtp_server::get_state {} {
    variable state

    return $state
}

# Get open count
proc smtp_server::get_opens {} {
    variable opens

    return $opens
}

# Get received message
proc smtp_server::get_received {} {
    variable message
    variable recipients

    return [list $recipients $message]
}

# Close the current session
proc smtp_server::close_session {} {
    variable channel

    close $channel
}

# Close the server
proc smtp_server::stop {} {
    variable server

    close $server
}

proc smtp_server::open {c host port} {
    variable message
    variable state
    variable channel $c
    variable opens
    variable recipients
    global debug

    if {$debug} {
	puts "SMTP connection from $host:$port"
    }
    incr opens
    fconfigure $c -buffering line
    puts $c "220 SMTP simulator"
    set state initial
    set message {}
    set recipients {}
    fileevent $c readable "smtp_server::handle_data $c"
}

proc smtp_server::handle_data {c} {
    variable message
    variable state
    variable recipients
    variable action
    global debug

    if {-1 == [gets $c line]} {
	# Sender closed the connection
	if {$debug} {
	    puts "SMTP connection closed by client"
	}
	set state disconnected
	close $c
	return
    }
    if {$debug} {
	puts "IN:  $line"
    }
    regsub -all "\r" $line {} line
    set cmd [string toupper [lindex $line 0]];
    if {"initial" == $state && "EHLO" == $cmd} {
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
	set state command
    } elseif {"command" == $state && "MAIL" == $cmd} {
	if {"ok" != $action} {
	    set resp "550 I have been instructed to deny you"
	    set state command
	} else {
	    set resp "250 sender ok"
	    set state get_rcpt
	    set message {}
	    set recipients {}
	}
    } elseif {"command" == $state && "QUIT" == $cmd} {
	set resp "221 closing connection"
	set state quit
    } elseif  {"get_rcpt" == $state && "RCPT" == $cmd} {
	lappend recipients [lindex [split $line "<>"] 1]
	set resp "250 rcpt ok"
    } elseif  {"get_rcpt" == $state && "DATA" == $cmd} {
	set resp "354 Enter mail"
	set state data
    } elseif {"data" == $state && "." != $line} {
	lappend message $line
	return
    } elseif {"data" == $state && "." == $line} {
	set resp "250 Message accepted"
	set state command
    } elseif {"RSET" == $line} {
	set resp "250 reset"
	set state command
    } else {
	set resp "500 Command unrecognized"
    }
    if {$debug} {
	foreach o [split $resp "\n"] {
	    puts "OUT: $o"
	}
    }
    puts $c $resp
}
