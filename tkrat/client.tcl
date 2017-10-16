#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

# TkRatClientUsage --
#
# Shows the usage message
#
# Arguments:

proc TkRatClientUsage {} {
    global argv0

    puts "Usage: $argv0 \[-confdir dir\] \[-appname name\] \[-open ?name?\] \\"
    puts "\t\[-opennew ?name?\] \[-compose ?args?\] \[-netsync ?set?\] \\"
    puts "\t\[-blank\]"
    exit 0
}


# TkRatClientStart --
#
# Parses command line arguments and sees if there is an existing tkrat
# invocation to use
#
# Arguments:

proc TkRatClientStart {} {
    global argv option

    catch {wm withdraw .}
    set started 0

    # Parse arguments
    set appname tkrat
    for {set i 0} {$i < [llength $argv]} {incr i} {
	set in [expr {$i+1}]
	switch -regexp -- [lindex $argv $i] {
	    -confdir {
		    if {$in == [llength $argv]} {
			TkRatClientUsage
		    }
		    set option(ratatosk_dir) [lindex $argv $in]
		    incr i
		}
	    -appname {
		    if {$in == [llength $argv]} {
			TkRatClientUsage
		    }
		    set appname [lindex $argv $in]
		    incr i
		}
	    -(open|opennew|compose|netsync|blank) {
		    regexp -- -(open|opennew|compose|netsync|blank) \
			    [lindex $argv $i] unused c
		    if {$in == [llength $argv]
			    || [regexp ^- [lindex $argv $in]]} {
			lappend cmds $c
		    } else {
			lappend cmds [list $c [lindex $argv $in]]
			incr i
		    }
		}
	    default {
		TkRatClientUsage
	    }
	}
    }
    if {![info exists cmds]} {
	set cmds open
    }

    # Check if we have a tkrat running, start it if not
    set appname $appname-[info host]
    if {[catch {send -- $appname RatPing}]} {
	set started 1
	tk appname $appname
	TkRatStart

	RatExec $cmds
    } else {
	# Send commands
	if {[catch {send -- $appname [list RatExec $cmds]} result]} {
	    # TODO
	    global errorInfo
	    puts $errorInfo
	    exit

	    puts $result
	    exit
	}
    }

    if {!$started} {
	destroy .
    }
}
