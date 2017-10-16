puts "$HEAD Test current"

namespace eval test_current {
}

proc test_current::test_current {} {
    global LEAD errors verbose option env

    # Setup option(domain) and find out data
    set old_domain $option(domain)
    set option(domain) default.domain
    set defaultHost [info hostname]
    set defaultMailbox $env(USER)
    set defaultPersonal $env(GECOS)
    if {1 == [llength [split $defaultHost .]]} {
	set expHost $defaultHost.$option(domain)
    } else {
	set expHost $defaultHost
    }

    # Test definition
    # role_name from uqa_domain smtp_helo
    #           host mailbox personal ehlo
    set tests [list \
	[list tr0 "from@my.addr (fr om)" "" "" \
	          my.addr from "fr om" my.addr] \
	[list tr1 "" "" ""\
	          $expHost $defaultMailbox $defaultPersonal $expHost] \
	[list tr2 "from@my.addr" "" "" \
	          my.addr from $defaultPersonal my.addr] \
	[list tr3 "from@my.addr (Räkan)" "" ""\
	          my.addr from "=?iso-8859-1?Q?R=E4kan?=" my.addr] \
	[list tr4 "from@my.addr (fr om)" "uqa.domain" "helo.host" \
	          uqa.domain from "fr om" helo.host] \
    ]

    # Init roles
    foreach case $tests {
	set r [lindex $case 0]
	set option($r,from) [lindex $case 1]
	set option($r,uqa_domain) [lindex $case 2]
	set option($r,smtp_helo) [lindex $case 3]
    }

    # Do tests
    foreach case $tests {
	set r [lindex $case 0]
	set v [RatGetCurrent host $r]
	if {"[lindex $case 4]" != $v} {
	    puts "$LEAD case '$r' host was '$v' expected '[lindex $case 4]'"
	    incr errors
	}
	set v [RatGetCurrent mailbox $r]
	if {"[lindex $case 5]" != $v} {
	    puts "$LEAD case '$r' mailbox was '$v' expected '[lindex $case 5]'"
	    incr errors
	}
	set v [RatGetCurrent personal $r]
	if {"[lindex $case 6]" != $v} {
	    puts "$LEAD case '$r' personal was '$v' expected '[lindex $case 6]'"
	    incr errors
	}
	set v [RatGetCurrent smtp_helo $r]
	if {"[lindex $case 7]" != $v} {
	    puts "$LEAD case '$r' smtp_helo was '$v' expected '[lindex $case 7]'"
	    incr errors
	}
    }

    set option(domain) $old_domain
}

test_current::test_current
