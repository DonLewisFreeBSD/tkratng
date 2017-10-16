puts "$HEAD Parameter encoding"

namespace eval test_parmencode {
}

proc test_parmencode::compare_lists {exp got n} {
    global option

    set diff [expr [llength $exp]-[llength $got]]
    for {set i 0} {0 == $diff && $i < [llength $exp]} {incr i} {
	if {[lindex $exp $i] != [lindex $got $i]} {
	    set diff 1
	}
    }
    if {$diff} {
	puts "Expected:"
	foreach p $exp {
	    puts "  [list $p]"
	}
	puts "Got:"
	foreach p $got {
	    puts "  [list $p]"
	}
	ReportError "Encode parameter '$n' with '$option(parm_enc)' failed"
    }
}

proc test_parmencode::test_parmencode {} {
    global option

    set old_enc $option(parm_enc)

    # Each test consists of
    #  - Name of test case
    #  - Parameters to encode
    #  - Expected result with rfc2047
    #  - Expected result with rfc2231
    #  - Expected result with both
    set tests {
	{
	    "one simple parameter"
	    {{p1 simple.txt}}
	    {{P1 simple.txt}}
	    {{P1 simple.txt}}
	    {{P1 simple.txt}}
	}
	{
	    "two simple parameters"
	    {{p1 simple.txt} {p2 stupido}}
	    {{P1 simple.txt} {P2 stupido}}
	    {{P1 simple.txt} {P2 stupido}}
	    {{P1 simple.txt} {P2 stupido}}
	    {{P1 simple.txt} {P2 stupido}}
	}
	{
	    "one localized parameter"
	    {{p1 Räckmackan.txt}}
	    {{P1 =?iso-8859-1?Q?R=E4ckmackan=2Etxt?=}}
	    {{P1* iso-8859-1''R%E4ckmackan%2Etxt}}
	    {
		{P1 =?iso-8859-1?Q?R=E4ckmackan=2Etxt?=}
		{P1* iso-8859-1''R%E4ckmackan%2Etxt}
	    }
	}
	{
	    "one localized parameter and one simple"
	    {{p1 Räckmackan.txt} {p2 simple.txt}}
	    {{P1 =?iso-8859-1?Q?R=E4ckmackan=2Etxt?=} {P2 simple.txt}}
	    {{P1* iso-8859-1''R%E4ckmackan%2Etxt} {P2 simple.txt}}
	    {
		{P1 =?iso-8859-1?Q?R=E4ckmackan=2Etxt?=}
		{P1* iso-8859-1''R%E4ckmackan%2Etxt}
		{P2 simple.txt}
	    }
	}
	{
	    "A long parameter"
	    {{parameter1 {This parameter has a really long value, actually it is so long that it should wrap}}}
	    {{PARAMETER1 {This parameter has a really long value, actually it is so long that it should wrap}}}
	    {
		{PARAMETER1*0 {This parameter has a really long value, actually it is so long}}
		{PARAMETER1*1 { that it should wrap}}
	    }
	    {
		{PARAMETER1 {This parameter has a really long value, actually it is so long that it should wrap}}
		{PARAMETER1*0 {This parameter has a really long value, actually it is so long}}
		{PARAMETER1*1 { that it should wrap}}
	    }
	}
    }

    foreach te $tests {
	set n [lindex $te 0]
        StartTest "Encoding $n"

	set option(parm_enc) rfc2047
	set r [RatTest encode_parameters [lindex $te 1]]
	compare_lists [lindex $te 2] $r $n

	set option(parm_enc) rfc2231
	set r [RatTest encode_parameters [lindex $te 1]]
	compare_lists [lindex $te 3] $r $n

	set option(parm_enc) both
	set r [RatTest encode_parameters [lindex $te 1]]
	compare_lists [lindex $te 4] $r $n
    }

    set option(parm_enc) $old_enc
}

test_parmencode::test_parmencode
