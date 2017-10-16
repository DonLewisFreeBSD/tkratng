puts "$HEAD Test alias"

namespace eval test_alias {
}

proc test_alias::do_test {case} {
    global option

    set r [lindex $case 0]
    set a [lindex $case 1]
    StartTest "Expanding $a"

    set e [RatAlias expand display $a $r]
    set o [lindex $case 2]
    if {$e != $o} {
	ReportError "expand display '$a' gave '$e' expected '$o'"
    }
    set display $e

    set e [RatAlias expand sending $a $r]
    set o [lindex $case 3]
    if {$e != $o} {
	ReportError "expand sending '$a' gave '$e' expected '$o'"
    }

    set e [RatAlias expand sending $display $r]
    set o [lindex $case 3]
    if {$e != $o} {
	ReportError "expand sending (via display) '$a' gave '$e' expected '$o'"
    }

    set e [RatAlias expand pgp $a $r]
    set o [lindex $case 4]
    if {$e != $o} {
	ReportError "expand pgp '$a' gave '$e' expected '$o'"
    }

    set e [RatAlias expand pgp $display $r]
    set o [lindex $case 4]
    if {$e != $o} {
	ReportError "expand pgp (via display) '$a' gave '$e' expected '$o'"
    }

    set e [RatAlias expand pgpactions $a $r]
    set o [lindex $case 5]
    if {$e != $o} {
	ReportError "expand pgpactions '$a' gave '$e' expected '$o'"
    }

    set e [RatAlias expand pgpactions $display $r]
    set o [lindex $case 5]
    if {$e != $o} {
	ReportError "expand pgpactions (via display) '$a' gave '$e' expected '$o'"
    }
}

proc test_alias::test_alias {} {
    global option env

    # Setup role and aliases to test with
    set option(tr42,from) role@from.adr
    set option(tr43,from) "Full Name <$env(USER)@domain.org>"
    RatAlias add Personal key_simple {Key Name} al_simple@exp.org {} {}
    RatAlias add Personal key_nogecos {} al_nogecos@exp.org {} {}
    RatAlias add Personal key_nofull \
        {Full Name} {al_nofull@exp.org} {} {} {nofullname}
    RatAlias add Personal key_list {List} {e1, e2} {} {}
    RatAlias add Personal key_req {Recurs} {al_req@exp.org, key_nogecos} {} {}
    RatAlias add Personal key_pgp1 \
        {PGP1} al_pgp1@exp.org {} {p1 pgp1} {pgp_sign}
    RatAlias add Personal key_pgp2 \
        {PGP2} {al_pgp1@exp.org, al_pgp2} {} {p2 pg2} {pgp_encrypt}
    RatAlias add Personal key_loop {Key Loop} key_loop@from.adr {} {}
    RatAlias add Personal primary {First Level} second@from.adr {} {}
    RatAlias add Personal second {Second Level} third@from.adr {} {}

    # List of tests. Each test consists of:
    #   Role
    #   Input string
    #   Result of expand level 1
    #   Result of expand level 2
    #   Result of pgp expansion
    #   Expected pgp actions
    set tests \
        [list \
             [list tr43 \
                  "" \
                  "" \
                  "" \
                  "" \
                  "0 0" ] \
             [list tr43 \
                  "foo (foo,,,)" \
	          "\"foo,,,\" <foo>" \
	          "\"foo,,,\" <foo@domain.org>" \
		  "foo@domain.org" \
                  "0 0" ] \
             [list tr43 \
	          "Full Name <$env(USER)@domain.org>" \
	          "Full Name <$env(USER)@domain.org>" \
		  "Full Name <$env(USER)@domain.org>" \
		  "$env(USER)@domain.org" \
                  "0 0" ] \
             [list tr42 \
	          "should, not, change" \
	          "should, not, change" \
	          "should@from.adr, not@from.adr, change@from.adr" \
		  "should@from.adr not@from.adr change@from.adr" \
                  "0 0" ] \
             [list tr42 \
		  "key_simple" \
	          "Key Name <key_simple>" \
	          "Key Name <al_simple@exp.org>" \
		  "al_simple@exp.org" \
                  "0 0" ] \
             [list tr42 \
		  "key_loop" \
	          "Key Loop <key_loop>" \
	          "Key Loop <key_loop@from.adr>" \
		  "key_loop@from.adr" \
                  "0 0" ] \
             [list tr42 \
		  "$env(USER)" \
	          "$env(GECOS) <$env(USER)>" \
		  "$env(GECOS) <$env(USER)@from.adr>" \
		  "$env(USER)@from.adr" \
                  "0 0" ] \
             [list tr42 \
		  "foo, key_simple ,bar" \
	          "foo, Key Name <key_simple>, bar" \
	          "foo@from.adr, Key Name <al_simple@exp.org>, bar@from.adr" \
		  "foo@from.adr al_simple@exp.org bar@from.adr" \
                  "0 0" ] \
             [list tr42 \
		  "(foo)(foo2) (foo3) key_simple (bar)(bar2) (bar3)" \
	          "Key Name <key_simple>" \
	          "Key Name <al_simple@exp.org>" \
		  "al_simple@exp.org" \
                  "0 0" ] \
             [list tr42 \
		  "key_nogecos" \
		  "key_nogecos" \
		  "al_nogecos@exp.org" \
		  "al_nogecos@exp.org" \
                  "0 0" ] \
             [list tr42 \
		  "key_nofull" \
		  "Full Name <key_nofull>" \
		  "al_nofull@exp.org" \
                  "al_nofull@exp.org" \
                  "0 0" ] \
             [list tr42 \
		  "key_list" \
	          "List <key_list>" \
		  "e1, e2" \
		  "e1@from.adr e2@from.adr" \
                  "0 0" ] \
             [list tr42 \
		  "key_req" \
	          "Recurs <key_req>" \
		  "al_req@exp.org, al_nogecos@exp.org" \
		  "al_req@exp.org al_nogecos@exp.org" \
                  "0 0" ] \
             [list tr42 \
	          "m" \
	          "m" \
	          "m@from.adr" \
		  "m@from.adr" \
                  "0 0" ] \
             [list tr42 \
	          "a, b, c" \
	          "a, b, c" \
	          "a@from.adr, b@from.adr, c@from.adr" \
		  "a@from.adr b@from.adr c@from.adr" \
                  "0 0" ] \
             [list  tr42 \
		  "key_pgp1" \
		  "PGP1 <key_pgp1>" \
		  "PGP1 <al_pgp1@exp.org>" \
                  "{p1 pgp1}" \
                  "1 0" ] \
             [list  tr42 \
		  "al_pgp1@exp.org" \
		  "al_pgp1@exp.org" \
		  "al_pgp1@exp.org" \
                  "{p1 pgp1}" \
                  "1 0" ] \
             [list  tr42 \
		  "key_pgp2" \
		  "PGP2 <key_pgp2>" \
		  "al_pgp1@exp.org, al_pgp2" \
                  "{p2 pg2}" \
                  "1 1" ] \
             [list  tr42 \
		  "al_pgp2@exp.org" \
		  "al_pgp2@exp.org" \
		  "al_pgp2@exp.org" \
		  "al_pgp2@exp.org" \
                  "0 0" ] \
             [list tr42 \
		  "primary" \
	          "First Level <primary>" \
	          "First Level <second@from.adr>" \
	          "second@from.adr" \
                  "0 0" ] \
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
                        "Collision <$env(USER)>" \
                        "Collision <coll@coll.org>" \
                        "coll@coll.org" \
                        "0 0" ]]

    # Do tests
    foreach case $tests {
	do_test $case
    }
}

# Test the RatSplitAdr function
proc test_alias::test_split {} {
    global verbose option env

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
	StartTest "Splitting '$i'"
	set r [RatSplitAdr $i]
	if {"$r" != "$e"} {
	    ReportError "Gave '$r' expected '$e'"
	}
    }
}

test_alias::test_alias
test_alias::test_split
