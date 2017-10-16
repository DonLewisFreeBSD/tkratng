puts "$HEAD Message parsing"

namespace eval test_msgparse {
}

proc test_msgparse::compare_msg {msg data} {
    set err [CompareLists [lindex $data 0] [$msg headers]]
    if {"" != $err} {
	ReportError "Headers differed:\n$err"
	return false
    }

    return [compare_body [$msg body] [lindex $data 1]]
}

proc test_msgparse::compare_body {body data} {
    if {[$body type] != [lindex $data 0]} {
	ReportError \
	    "Body type differed, got [$body type] expected [lindex $data 0]"
	return false
    }
    set err [CompareLists [lindex $data 1] [$body params]]
    if {"" != $err} {
	ReportError "Body parameters differed:\n$err"
	return false
    }
    if {[$body disp_type] != [lindex $data 2]} {
	ReportError "Body disposition differed, got [$body disp_type] expected [lindex $data 2]"
	return false
    }
    set err [CompareLists [lindex $data 3] [$body disp_parm]]
    if {"" != $err} {
	ReportError "Body disposition parameters differed:\n$err"
	return false
    }
    if {"MESSAGE RFC822" == [$body type]} {
	return [compare_msg [$body message] [lindex $data 4]]
    } elseif {"MULTIPART" == [lindex [$body type] 0]} {
	set index 4
	foreach c [$body children] {
	    if {"true" != [compare_body $c [lindex $data $index]]} {
		return false
	    }
	    incr index
	}
    } else {
	return true
    }
}

proc test_msgparse::test_msgparse {} {
    global dir option smsgs

    # Test folders to store messages in
    set fn $dir/folder.[pid]
    set def [list Test1 file {} $fn]
    set role $option(default_role)
    set option($role,from) "test@test.domain"

    # Do tests
    foreach case $smsgs {
	StartTest "[lindex $case 0]"

	# Store message into folder
	file delete -force $fn
	set f [open $fn w]
	puts $f "From maf@kilauea Thu Dec 26 23:11:56 2002 +0100"
	foreach l [lindex $case 2] {
	    puts $f $l
	}
	close $f

	# Open folder and get message
	set fh [RatOpenFolder $def]
	$fh list "%s"
	set msg [$fh get 0]
	compare_msg $msg [lindex $case 3]
	$fh close
    }
}


test_msgparse::test_msgparse
