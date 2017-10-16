puts "$HEAD Test pgp"

namespace eval test_pgp {
    variable tmpfn [pwd]/folder.[pid]-tmp
    variable tmpdef [list Test file {} $tmpfn]

    set fh [open $tmpfn w]
    puts $fh $hdr
    close $fh

    variable error_msg
}

proc test_pgp::send_msg {role msg} {
    global vFolderDef vFolderOutgoing folderExists tickle option
    variable error_msg

    proc RatSendFailed {msg errmsg} {
	global test_pgp::error_msg
	
	set test_pgp::error_msg $errmsg
    }

    smtp_server::start
    set error_msg NONE
    set option(cache_conn) 0
    set option($role,from) maf@test.domain
    set option($role,sendprot) smtp
    set option($role,smtp_hosts) "localhost:[smtp_server::get_port]"
    set option($role,smtp_user) ""
    set option($role,smtp_passwd) ""
    set fh [RatOpenFolder $vFolderDef($vFolderOutgoing)]
    $fh insert $msg
    RatNudgeSender

    # Wait for send to complete
    for {set i 0} {$i < 600} {incr i} {
	# Force event loop
	after 100 "set tickle 1"
	vwait tickle
	if {0 == $folderExists($fh)} {
	    break
	}
    }
    if {"NONE" != $error_msg} {
	puts "Err: $error_msg"
    }
    set sent [lindex [smtp_server::get_received] 1]
    smtp_server::stop
    return $sent
}

proc test_pgp::test_signing {} {
    global option hdr smsgs
    variable tmpfn
    variable tmpdef

    set role $option(default_role)

    # Loop over test messages.
    #foreach mt [list [lindex $smsgs 1]]     
    foreach mt $smsgs  {
	StartTest "Signing [lindex $mt 0]"
	# Create message
	set msg [RatCreateMessage $role [lindex $mt 1]]

	# Sign it
	if {[catch {$msg pgp true false $role test_key@tkrat.org {}} err]} {
	    ReportError "Failed to sign: $err"
	    continue
	}

	# Send it
	set sent_list [send_msg $role $msg]
	set sent_msg [join $sent_list "\n"]

	# Check signature with gpg
	set boundary [string range [lindex $sent_list end] 2 end-2]
	set mfile "msg.[pid]"
	set sfile "sig.[pid]"
	for {set i 0} {"[lindex $sent_list $i]" != "--$boundary"} {incr i} {
	}
	set fd [open $mfile w]
	fconfigure $fd -translation crlf
	set first 1
	for {incr i} {"[lindex $sent_list $i]" != "--$boundary"} {incr i} {
	    if {$first} {
		set first 0
	    } else {
		puts -nonewline $fd "\n"
	    }
	    puts -nonewline $fd [lindex $sent_list $i]
	}
	close $fd
	set fd [open $sfile w]
	for {incr i} {"[lindex $sent_list $i]" != "--$boundary--"} {incr i} {
	    puts $fd [lindex $sent_list $i]
	}
	close $fd
	if {[catch "exec gpg $option(pgp_args) --verify $sfile $mfile 2>errout" err]} {
	    set fh [open errout r]
	    while {-1 != [gets $fh line]} {
		puts $line
	    }
	    close $fh
	    puts "File: [pwd]/$mfile"
	    puts "Sig:  [pwd]/$sfile"
	    ReportError "External signature verification failed"
	    continue
	}

	# Check signature with tkrat (ok expected)
	set fh [open $tmpfn w]
	puts $fh $hdr
	puts $fh "From maf@tkrat.org Tue Sep  5 18:02:22 2000 +0100"
	puts $fh $sent_msg
	close $fh
	set fh [RatOpenFolder $tmpdef]
	set msg [$fh get 0]
	set body [$msg body]
	$body checksig
	if {"pgp_good" != [$body sigstatus]} {
	    ReportError "Signature check in TkRat failed [$body sigstatus]"
	    continue
	}
	$fh close 1

	# Check signature with tkrat (failure expected)
	set fh [open $tmpfn w]
	puts $fh $hdr
	puts $fh "From maf@tkrat.org Tue Sep  5 18:02:22 2000 +0100"
	# Add a trailing blank to each bodypart
	regsub -all -- "--$boundary" $sent_msg "\n--$boundary" broken
	puts $fh $broken
	close $fh
	set fh [RatOpenFolder $tmpdef]
	set msg [$fh get 0]
	set body [$msg body]
	$body checksig
	if {"pgp_bad" != [$body sigstatus]} {
	    ReportError "Signature check of bad message in TkRat failed [$body sigstatus]"
	    continue
	}
	$fh close 1
    }
}

proc test_pgp::test_encrypting {} {
    global option hdr smsgs
    variable tmpfn
    variable tmpdef

    set role $option(default_role)

    # Loop over test messages.
    foreach mt $smsgs  {
	StartTest "Encrypting [lindex $mt 0]"
	# Create message
	set msg [RatCreateMessage $role [lindex $mt 1]]

	# Encrypt & sign it
	if {[catch {$msg pgp true true $role test_key@tkrat.org test_key@tkrat.org} err]} {
	    ReportError "Failed to encrypt: $err"
	    continue
	}

	# Send it
	set sent_list [send_msg $role $msg]
	set sent_msg [join $sent_list "\n"]

	# Check encryption & signature with gpg
	set mfile "msg.[pid]"
	set fd [open $mfile w]
	fconfigure $fd -translation crlf
	regexp -- {-----BEGIN PGP MESSAGE.*END PGP MESSAGE-----} $sent_msg enc
	puts $fd $enc
	close $fd
	set ea "--status-fd 2 --decrypt $mfile 2>status.[pid]"
	if {[catch "exec gpg $option(pgp_args) $ea" output]} {
	    set err 1
	} else {
	    set err 0
	}
	catch {unset gpgout}
	set status ""
	set fh [open "status.[pid]" r]
	while {-1 != [gets $fh line]} {
	    set status "$status$line\n"
	    if {{[GNUPG:]} == "[lindex $line 0]"} {
		set gpgout([lindex $line 1]) [lrange $line 2 end]
	    }
	}
	if {1 == $err || ![info exists gpgout(GOODSIG)] \
		|| ![info exists gpgout(DECRYPTION_OKAY)]} {
	    ReportError "External verification failed\n$status"
	    continue
	}
	set expected_list {}
	set expected_body_list {}
	set in_header 1
	set in_content 0
	foreach l [lindex $mt 2] {
	    if {$in_header} {
		if {[regexp "^Content-" $l]} {
		    lappend expected_list $l
		    set in_content 1
		} elseif {$in_content && (" " == [string index $l 0]
					 || "\t" == [string index $l 0])} {
		    lappend expected_list $l
		} elseif {"" == $l} {
		    lappend expected_list $l
		    set in_header 0
		    set in_content 0
		} else {
		    set in_content 0
		}
	    } else {
		lappend expected_list $l
		lappend expected_body_list $l
	    }
	}
	set expected [join [lrange $expected_list 0 end-1] "\n"]
	if {"$output" != "$expected"} {
	    puts "******** Expected"
	    puts $expected
	    puts "******** Output"
	    puts $output
	    ReportError "Externally decrypted text does not match expected text"
	    continue
	}

	# Check decryption with tkrat
	set fh [open $tmpfn w]
	puts $fh $hdr
	puts $fh "From maf@tkrat.org Tue Sep  5 18:02:22 2000 +0100"
	puts $fh $sent_msg
	close $fh
	set fh [RatOpenFolder $tmpdef]
	set msg [$fh get 0]
	set body [$msg body]
	$body checksig
	if {"pgp_good" != [$body sigstatus]} {
	    ReportError "Decryption check in TkRat failed signature part"
	    continue
	}
	set expected_body [join [lrange $expected_body_list 0 end-1] "\n"]
	if {"[$body data true]" != "$expected_body"} {
	    ReportError "Internally decrypted text does not match expected text"
	    puts "******** Expected"
	    puts $expected_body_list
	    puts "******** Output"
	    puts [$body data true]
	    continue
	}
	$fh close 1
    }
}

proc test_pgp::compare_key {e o} {
    foreach i {0 2 4 5} {
	if {[lindex $e $i] != [lindex $o $i]} {
	    return "fail"
	}
    }
    foreach i {1 3} {
	set ei [lindex $e $i]
	set oi [lindex $o $i]
	if {[llength $ei] != [llength $oi]} {
	    return "fail"
	}
	for {set j 0} {$j < [llength $ei]} {incr j} {
	    if {[lindex $ei $j] != [lindex $oi $j]} {
		return "fail"
	    }
	}
    }
    return "ok"
}

proc test_pgp::compare_keylist {expected output} {
    if {[lindex $expected 0] != [lindex $output 0]} {
	return "fail"
    }
    set e [lindex $expected 1]
    set o [lindex $output 1]
    if {[llength $e] != [llength $o]} {
	return "fail"
    }
    for {set i 0} {$i < [llength $e]} {incr i} {
	if {"fail" == [compare_key [lindex $e $i] [lindex $o $i]]} {
	    return "fail"
	}
    }
    return "ok"
}

proc test_pgp::test_keylist {} {
    StartTest "Listing keys"

    set publist {
	{Public keyring}
	{
	    {ED6087318702C78A test_key@tkrat.org 
		{pub 1024 DSA (sign only)}
		{{TkRat Test Key (Do not trust this key!!!) <test_key@tkrat.org>}}
		1 0
	    }
	    {36D3FDF09AC2D77E test_key@tkrat.org
		{sub 768 ElGamal (encrypt only)}
		{{TkRat Test Key (Do not trust this key!!!) <test_key@tkrat.org>}}
		0 1}}
    }
    set result [RatPGP listkeys PubRing]
    if {"fail" == [compare_keylist $publist $result]} {
	puts "******** Expected"
	puts $publist
	puts "******** Output"
	puts $result
	ReportError "Public keylist differes from expected"
    }

    set seclist {
	{Secret keyring}
	{
	    {ED6087318702C78A test_key@tkrat.org
		{sec 1024 DSA (sign only)}
		{{TkRat Test Key (Do not trust this key!!!) <test_key@tkrat.org>}}
		1 0}
	    {36D3FDF09AC2D77E test_key@tkrat.org
		{ssb 768 ElGamal (encrypt only)}
		{{TkRat Test Key (Do not trust this key!!!) <test_key@tkrat.org>}}
		0 1}
	}
    }
    set result [RatPGP listkeys SecRing]
    if {"fail" == [compare_keylist $seclist $result]} {
	puts "******** Expected"
	puts $seclist
	puts "******** Output"
	puts $result
	ReportError "Secret keylist differes from expected"
    }

}

proc test_pgp::test_pgp {} {
    global option

    RatLibSetOnlineMode 1
    set option(pgp_version) gpg-1
    set option(pgp_args) "--no-default-keyring --keyring [pwd]/../pgp_pub --secret-keyring [pwd]/../pgp_sec --always-trust --no-options --homedir /tmp"
    #test_signing
    #test_encrypting
    test_keylist
}

proc RatGetPGPPassPhrase {} {
    return [list "ok" ""]
}

test_pgp::test_pgp
