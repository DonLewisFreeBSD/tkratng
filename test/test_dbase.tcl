puts "$HEAD Test dbase operations"

namespace eval test_dbase {
    variable subjects
}

proc test_dbase::verify_search {search_exp expected} {
    variable subjects

    set fh [RatOpenFolder [list Dbase dbase {} remove +1 $search_exp]]
    set real [$fh list %s]

    set exp {}
    foreach s $expected {
        lappend exp [lindex $subjects $s]
    }
    if {$exp != $real} {
        puts [format "%-20s  %-20s" "Expected" "Real"]
        for {set i 0} {$i < [llength $exp] || $i < [llength $real]} {incr i} {
            puts [format "%-20s  %-20s" [lindex $exp $i] [lindex $real $i]]
        }
        ReportError "Dbase search result mismatch"
    }
    $fh close
}

proc test_dbase::test_dbase {} {
    global option dir hdr
    variable subjects

    set option(dbase_dir) $dir/db


    StartTest "Creating empty database"
    verify_search {or} {}

    StartTest "Inserting messages"
    # Prepare test messages
    set fn $dir/folder.[pid]
    set def [list Test file {} $fn]
    set fh [open $fn w]
    puts $fh $hdr
    for {set i 1} {$i < 21} {incr i} {
        upvar \#0 msg$i m
	puts $fh $m
    }
    close $fh
    set f1 [RatOpenFolder $def]
    set file_subjects [$f1 list %s]
    set dates [$f1 list %D]

    set exp {}
    for {set i 0} {$i < 20} {incr i} {
        set msg [$f1 get $i]
        lappend subjects [lindex $file_subjects $i]
        set key [format "key%02d" $i]
        RatInsert $msg $key +1 none
        lappend exp $i
        verify_search {or} $exp
        verify_search {and} {}
    }
    $f1 close

    StartTest "Single keyword search"
    for {set i 0} {$i < 20} {incr i} {
        set key [format "key%02d" $i]
        verify_search [list or keywords $key] $i
        verify_search [list and keywords $key] $i
    }

    StartTest "Multi keyword search (or)"
    verify_search [list or keywords [list key01 key02]] {1 2}
    verify_search [list or keywords [list key03 key02]] {2 3}

    StartTest "Multi keyword search (and)"
    verify_search [list and keywords [list key03 key02]] {2 3}

    StartTest "Time interval search"
    # We have to add an hour here because the $dates is in GMT
    set d2 [expr [lindex $dates 2]+3600]
    set d3 [expr [lindex $dates 3]+3600]
    verify_search [list int $d2 $d2 or] {2}
    verify_search [list int $d2 $d3 or] {2 3}
    verify_search [list int $d2 $d2 and] {}
    verify_search [list int $d2 $d3 and] {}

    StartTest "Time interval and keyword search"
    verify_search [list int $d2 $d3 and keywords key02] {2}
    verify_search [list int $d2 $d3 or keywords [list key02 key01]] {2}
}

test_dbase::test_dbase
