puts "$HEAD Test sequence"

namespace eval test_sequence {
}

# Test sequence creation
proc test_sequence::test_notempty {} {
    set s [RatCreateSequence]
    if {[$s notempty]} {
        ReportError "Reported not empty when in fact empty"
    }
    $s add 1
    if {![$s notempty]} {
        ReportError "Reported !notempty when one number added"
    }
    rename $s ""
}

proc test_sequence::run_test {test} {
    global verbose

    set s [RatCreateSequence]
    foreach e [lindex $test 0] {
        $s add $e
    }
    set expected [lindex $test 1]
    set result [$s get]
    if {$result != $expected} {
        if {$verbose} {
            puts "     Got: $result"
            puts "Expected: $expected"
        }
        ReportError "Bad result for $test"
    }
}

proc test_sequence::test_sequence {} {
    StartTest "Test sequences"
    test_notempty

    foreach t {
        {{1} "1"}
        {{1 2} "1,2"}
        {{1 2 3} "1:3"}
        {{1 2 3 5} "1:3,5"}
        {{5 1 2 3} "1:3,5"}
        {{1 3 5} "1,3,5"}
        {{1 5 3} "1,3,5"}
        {{5 3 1} "1,3,5"}
        {{1 3 5 4} "1,3:5"}
        {{1 5 4 3} "1,3:5"}
        {{5 4 3 1} "1,3:5"}
        {{4 5 3 1} "1,3:5"}
        {{5 5 6} "5,6"}
        {{1 4 2 3 7 9 6 8} "1:4,6:9"}
        {{2 123456 3 1234567 4} "2:4,123456,1234567"}
        {{123456789} "123456789"}
        {{123456789 123456787} "123456787,123456789"}
    } {
        run_test $t
    }
}

test_sequence::test_sequence
