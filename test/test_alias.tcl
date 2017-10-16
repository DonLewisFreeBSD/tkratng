puts "$HEAD Test alias"

namespace eval test_alias {
}

proc test_alias::do_test {case} {
    global LEAD errors option

    set r [lindex $case 0]
    set a [lindex $case 1]
    for {set i 0} {$i < 3} {incr i} {
	set option(alias_expand) $i
	set e [RatAlias expand1 $a $r]
	set o [lindex $case [expr $i+2]]
	if {$e != $o} {
	    puts "$LEAD expand $i '$a' gave '$e' expected '$o'"
	    incr errors
	}
    }
}

proc test_alias::test_alias {} {
    global LEAD errors verbose option env

    # Setup role and aliases to test with
    set option(tr42,from) role@from.adr
    set option(tr43,from) "Full Name <$env(USER)@domain.org>"
    RatAlias add Personal key_simple {Key Name} al_simple@exp.org {} {}
    RatAlias add Personal key_nogecos {} al_nogecos@exp.org {} {}
    RatAlias add Personal key_req {Recurs} {al_req@exp.org, key_nogecos} {} {}

    # List of tests. Each test consists of:
    #   Role
    #   Input string
    #   Result of expand level 1
    #   Result of expand level 2
    #   Result of expand level 3
    set tests [list \
	    [list tr43 \
	          "foo (foo,,,)" \
	          "foo (foo,,,)" \
	          "foo (foo,,,)" \
		  "foo (foo,,,)"] \
	    [list tr43 \
	          "Full Name <$env(USER)@domain.org>" \
	          "Full Name <$env(USER)@domain.org>" \
	          "Full Name <$env(USER)@domain.org>" \
		  "Full Name <$env(USER)@domain.org>"] \
	    [list tr42 \
	          "should, not, change" \
	          "should, not, change" \
	          "should, not, change" \
		  "should, not, change"] \
	    [list tr42 \
		  "key_simple" \
	          "al_simple@exp.org" \
	          "key_simple (Key Name)" \
		  "al_simple@exp.org (Key Name)"] \
	    [list tr42 \
		  "$env(USER)" \
	          "$env(USER)" \
	          "$env(USER) ($env(GECOS))" \
		  "$env(USER)@from.adr ($env(GECOS))"] \
	    [list tr42 \
		  "foo, key_simple ,bar" \
	          "foo, al_simple@exp.org, bar" \
	          "foo, key_simple (Key Name), bar" \
		  "foo, al_simple@exp.org (Key Name), bar"] \
	    [list tr42 \
		  "(foo)(foo2) (foo3) key_simple (bar)(bar2) (bar3)" \
	          "al_simple@exp.org" \
	          "key_simple (Key Name)" \
		  "al_simple@exp.org (Key Name)"] \
	    [list tr42 \
		  "key_nogecos" \
	          "al_nogecos@exp.org" \
	          "key_nogecos" \
		  "al_nogecos@exp.org"] \
	    [list tr42 \
		  "key_req" \
	          "al_req@exp.org, al_nogecos@exp.org" \
	          "key_req (Recurs)" \
		  "al_req@exp.org, al_nogecos@exp.org"] \
	    [list tr42 \
	          "m" \
	          "m" \
	          "m" \
		  "m"] \
	    [list tr42 \
	          "a, b, c" \
	          "a, b, c" \
	          "a, b, c" \
		  "a, b, c"] \
    ]

    # Do tests
    foreach case $tests {
	do_test $case
    }

    # Test alias which collides with a local user
    RatAlias add Personal $env(USER) {Collision} {coll@coll.org} {} {}

    # List of tests. Each test consists of:
    #   Role
    #   Input string
    #   Result of expand level 1
    #   Result of expand level 2
    #   Result of expand level 3
    set tests [list \
	    [list tr42 \
		  "$env(USER)" \
	          "coll@coll.org" \
	          "$env(USER) (Collision)" \
		  "coll@coll.org (Collision)"]]

    # Do tests
    foreach case $tests {
	do_test $case
    }
}

# Test the RatSplitAdr function
proc test_alias::test_split {} {
    global LEAD errors verbose option env

    # List of tests
    #  Alist to split
    #  Expected output
    set tests {
	{ "foo" "foo"}
	{ "foo,bar" "foo bar"}
	{ " foo , bar " "foo bar"}
	{ "foo,,bar" "foo {} bar"}
	{ " foo , , bar " "foo {} bar"}
    }

    foreach case $tests {
	set i [lindex $case 0]
	set e [lindex $case 1]
	set r [RatSplitAdr $i]
	if {"$r" != "$e"} {
	    puts "$LEAD split $i gave '$r' expected '$e'"
	    incr errors
	}
    }
}

test_alias::test_alias
test_alias::test_split
