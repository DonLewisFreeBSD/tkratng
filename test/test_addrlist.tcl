puts "$HEAD Test addrlist"

namespace eval test_addrlist {
}

# Test one explicit mapping request
proc test_addrlist::test_match {match max expected} {
    global verbose
    variable addresses

    StartTest "Matching '$match'"
    set ret [GetMatchingAddrs $match $max]
    set mismatch 0
    if {[llength $ret] != [llength $expected]} {
        set mismatch 1
    }
    for {set i 0} {0 == $mismatch && $i < [llength $ret]} {incr i} {
        if {[lindex $ret $i] != [lindex $addresses [lindex $expected $i]]} {
            set mismatch 1
        }
    }
    if {$mismatch} {
        ReportError "Bad result"
        if {$verbose} {
            puts "Got [llength $ret] element(s)"
            foreach e $ret {
                puts "  $e"
            }
            puts "Expected [llength $expected] element(s)"
            foreach ei $expected {
                puts "  [lindex $addresses $ei]"
            }
        }
    }
}

# Test the address list
proc test_addrlist::test_addrlist {} {
    global option
    variable addresses

    AddrListAdd "Apple Core <applet@tkrat.org>"
    AddrListAdd "apa@tkrat.org, adam@tkrat.org (Adam Somebody)"
    AddrListAdd "Martin Forssen <maf@tkrat.org>"
    
    set addresses [list \
                       "Martin Forssen <maf@tkrat.org>" \
                       "apa@tkrat.org" \
                       "Adam Somebody <adam@tkrat.org>" \
                       "Apple Core <applet@tkrat.org>"]

    test_match m 10 {0}
    test_match ma 10 {0}
    test_match a 10 {1 2 3}
    test_match ap 10 {1 3}
    test_match a 1 {1}

    AddrListAdd "Apple Core <applet@tkrat.org>"
    test_match a 10 {3 1 2}
}

# Test parsing of addresses
proc test_addrlist::test_addrparse {} {
    global option

    # Setup roles
    set option(tr0,from) test@foo.com
    set option(tr1,from) test@bar.com

    # Setup tests
    set tests {
        {"rcpt" "rcpt@foo.com" "rcpt@bar.com" "rcpt"}
        {"rcpt@foo.com" "rcpt@foo.com" "rcpt@foo.com" "rcpt@foo.com"}
        {"rcpt@bar.com" "rcpt@bar.com" "rcpt@bar.com" "rcpt@bar.com"}
        {"rcpt@apa.com" "rcpt@apa.com" "rcpt@apa.com" "rcpt@apa.com"}
    }

    foreach t $tests {
        StartTest "Parsing [lindex $t 0]"
        set a [RatCreateAddress [lindex $t 0] tr0]
        if {[$a get mail] != [lindex $t 1]} {
            ReportError "tr0: got '[$a get mail]' expected '[lindex $t 1]'"
        }
        set a [RatCreateAddress [lindex $t 0] tr1]
        if {[$a get mail] != [lindex $t 2]} {
            ReportError "tr1: got '[$a get mail]' expected '[lindex $t 2]'"
        }
        set a [RatCreateAddress -nodomain [lindex $t 0]]
        if {[$a get mail] != [lindex $t 3]} {
            ReportError "nodomain: got '[$a get mail]' expected '[lindex $t 3]'"
        }
    }
}

test_addrlist::test_addrlist
test_addrlist::test_addrparse
