puts "$HEAD Test import"

namespace eval test_import {
    # List of directories to remove when we are done
    variable cleanup {}

    # Array contaning desired result
    variable result
}

proc test_import::check_result {path idr idv} {
    global vFolderDef
    variable result

    if {"" == $path} {
	set type "import"
	set contents 5
    } else {
	set type "struct"
	set contents 3
    }
    if {$type != [lindex $vFolderDef($idv) 1]} {
	ReportError [join [list "vFolderDef($idv) is not an $type entry" \
			       "[list $vFolderDef($idv)]"] "\n"]
	return 1
    }
    foreach id [lindex $result($idr) $contents] {
	set r([lindex $result($id) 1],[lindex $result($id) 0]) $result($id)
	set rid([lindex $result($id) 1],[lindex $result($id) 0]) $id
    }
    foreach id [lindex $vFolderDef($idv) $contents] {
	set v([lindex $vFolderDef($id) 1],[lindex $vFolderDef($id) 0]) \
		$vFolderDef($id)
	set vid([lindex $vFolderDef($id) 1],[lindex $vFolderDef($id) 0]) $id
    }

    if {[array size r] != [array size v]} {
	ReportError "Length of contents in struct differs ($path)"
	return 1
    }

    foreach n [array names r] {
	if {![info exists v($n)]} {
	    ReportError "Did not find element $path/$n in vFolderDef"
	    return 1
	}
	if {"[lindex $r($n) 2]" != "[lindex $v($n) 2]"} {
	    ReportError [joid [list "Flags of $path/$n differs" \
				   "[lindex $r($n) 2] != [lindex $v($n) 2]"] \
			     "\n"]
	    return 1
	}
	switch [lindex $r($n) 1] {
	    file {
		if {"[lindex $r($n) 3]" != "[lindex $v($n) 3]"} {
		    ReportError \
			[join [list "Filename of $path/$n differs" \
				   "[lindex $r($n) 3] != [lindex $v($n) 3]"] \
			     "\n"]
		    return 1
		}
	    }
	    struct {
		if {1 == [check_result $path/$n $rid($n) $vid($n)]} {
		    return 1
		}
	    }
	}
    }
    return 0
}

proc test_import::test_import {} {
    global option vFolderDef mailServer env imap_serv
    variable result

    # Copy old values
    set vfd_backup [array get vFolderDef]

    # Create hierarchy
    #   a
    #   b
    #   c+
    #    a
    #    b+
    #     d
    #   e+
    #    f
    #   g
    set base [pwd]/import
    file mkdir $base
    exec touch $base/a
    file mkdir $base/c
    exec touch $base/c/a
    file mkdir $base/c/b
    exec touch $base/c/b/d
    file mkdir $base/e
    exec touch $base/e/f
    exec touch $base/g
    exec touch $base/b

    # Setup result original
    set result(0) {{} struct {} {1}}
    set result(1) [list test import {subscribed 0} \
	    [list NAME1 file {} $base/import] * {2 3 4 5 6}]
    set result(2) [list a file {} $base/a]
    set result(3) [list b file {} $base/b]
    set result(4) [list c struct {} {7 8}]
    set result(5) [list e struct {} {9}]
    set result(6) [list g file {} $base/g]
    set result(7) [list a file {} $base/c/a]
    set result(8) [list b struct {} {10}]
    set result(9) [list f file {} $base/e/f]
    set result(10) [list d file {} $base/c/b/d]

    StartTest "Simple file import"
    catch {unset vFolderDef}
    set vFolderDef(0) {{} struct {} {1}}
    set vFolderDef(1) [list testroot import {} \
	    [list NAME1 file {} $base] * {}]
    RatImport 1
    check_result "" 1 1

    StartTest "Re-import"
    RatImport 1
    check_result "" 1 1

    StartTest "Re-import with addition at top level"
    exec touch $base/h
    set result(1) [list test import {subscribed 0} \
	    [list NAME1 file {} $base/import] * {2 3 4 5 6 11}]
    set result(11) [list h file {} $base/h]
    RatImport 1
    check_result "" 1 1

    StartTest "Re-import with deletion at top level"
    file delete $base/h
    set result(1) [list test import {subscribed 0} \
	    [list NAME1 file {} $base/import] * {2 3 4 5 6}]
    unset result(11)
    RatImport 1
    check_result "" 1 1

    StartTest "Re-import with addition down below"
    exec touch $base/c/b/h
    set result(8) [list b struct {} {10 11}]
    set result(11) [list h file {} $base/c/b/h]
    RatImport 1
    check_result "" 1 1

    StartTest "Re-import with deletion down below"
    file delete $base/c/b/h
    set result(8) [list b struct {} {10}]
    unset result(11)
    RatImport 1
    check_result "" 1 1

    StartTest "With a flag-change"
    set result(3) [list b file {flag 1} $base/b]
    foreach id [array names vFolderDef] {
	if {"b" == [lindex $vFolderDef($id) 0]} {
	    set vFolderDef($id) [lreplace $vFolderDef($id) 2 2 {flag 1}]
	    break
	}
    }
    RatImport 1
    check_result "" 1 1

    StartTest "import via IMAP"
    if {$imap_serv == "cyrus"} {
	set result(1) [lreplace $result(1) 3 3 \
		[list NAME! imap {} localhost user.$env(USER).import]]
	# Setup result original
	set ib user.$env(USER).import
	set result(0) {{} struct {} {1}}
	set result(1) [list test import {subscribed 0} \
		[list NAME1 imap {} localhost ${ib}] * \
		{2 102 3 103 4 5 6 106}]
	set result(2) [list a imap {} localhost ${ib}.a]
	set result(3) [list b imap {} localhost ${ib}.b]
	set result(4) [list c struct {} {7 107 8}]
	set result(5) [list e struct {} {9 109}]
	set result(6) [list g imap {} localhost ${ib}.g]
	set result(7) [list a imap {} localhost ${ib}.c.a]
	set result(8) [list b struct {} {10 110}]
	set result(9) [list f imap {} localhost ${ib}.e.f]
	set result(10) [list d imap {} localhost ${ib}.c.b.d]
	set result(102) [list a struct {} {}]
	set result(103) [list b struct {} {}]
	set result(106) [list g struct {} {}]
	set result(107) [list a struct {} {}]
	set result(109) [list f struct {} {}]
	set result(110) [list d struct {} {}]

	foreach id [array names result] {
	    if {"imap" == [lindex $result($id) 1]} {
		init_imap_folder $result($id)
	    }
	}
	unset vFolderDef
	set vFolderDef(0) {{} struct {} {1}}
	set vFolderDef(1) [list testroot import {} \
		[list NAME1 imap {} localhost $ib] * {}]
	RatImport 1
	check_result "" 1 1

	# Cleanup
	foreach id [array names result] {
	    cleanup_imap_folder $result($id)
	}
	
    } else {
	set result(1) [lreplace $result(1) 3 3 \
		[list NAME! imap {} localhost $base/import]]
	foreach id [array names result] {
	    if {"file" == [lindex $result($id) 1]} {
		set result($id) [lreplace $result($id) 1 2 imap {} localhost]
	    }
	}
	unset vFolderDef
	set vFolderDef(0) {{} struct {} {1}}
	set vFolderDef(1) [list testroot import {} \
		[list NAME1 imap {} localhost $base] * {}]
	RatImport 1
	check_result "" 1 1
    }
    
    StartTest "Import after clear"
    unset result
    unset vFolderDef
    set result(0) {{} struct {} {1}}
    set result(1) [list test import {subscribed 0} \
	    [list NAME1 file {} $base.noexist] * {}]
    set vFolderDef(0) {{} struct {} {1}}
    set vFolderDef(1) [list testroot import {} \
	    [list NAME1 file {} $base.noexist] * {2}]
    set vFolderDef(2) [list g file {} $base/NOEXIST]
    RatImport 1
    check_result "" 1 1

    # Cleanup
    unset vFolderDef
    array set vFolderDef $vfd_backup
    file delete -force $base
}

test_import::test_import
