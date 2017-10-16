puts "$HEAD Message creation"

namespace eval test_msgcreat {
    variable ignore_headers {X-IMAPbase Status X-Status X-Keywords
        X-UID X-Original-Status}

}

proc test_msgcreat::compare {filename data} {
    variable ignore_headers

    set line 0
    set f [open $filename r]
    # Read and ignore from-line
    gets $f rl
    foreach el $data {
	incr line
	if {-1 == [gets $f rl]} {
	    ReportError "Generated file too short"
	    close $f
	    return 1
	}
	while {-1 != [lsearch $ignore_headers \
			  [string trimright [lindex $rl 0] ":"]]} {
	    gets $f rl
	}
	if {$rl != $el} {
	    ReportError [join [list "Generated message differs in line $line" \
				   "Got: $rl" \
				   "Exp: $el"] "\n"]
	    close $f
	    return 1
	}
    }
    if {-1 != [gets $f rl]} {
	ReportError "Generated file too long\nGot: $rl"
	close $f
	return 1
    }
    return 0
}

proc test_msgcreat::test_msgcreat {} {
    global dir option smsgs

    # Test folders to store messages in
    set fn $dir/folder.[pid]
    set def [list Test1 file {} $fn]
    set role $option(default_role)
    set option($role,from) "test@test.domain"

    # Do tests
    foreach case $smsgs {
	StartTest "[lindex $case 0]"

	# Create message #1
	if [catch {RatCreateMessage $role [lindex $case 1]} msg] {
	    ReportError "Failed to create message(1): $msg"
	    continue
	}
	# Insert into folder
	file delete -force $fn
	set fh [RatOpenFolder $def]
	if [catch {$fh insert $msg} error] {
	    ReportError "Failed to insert: $error"
	    continue
	}
	$fh close
	if {0 != [compare $fn [lindex $case 2]]} {
	    continue
	}
	rename $msg ""

	# Create message #2
	if [catch {RatCreateMessage $role [lindex $case 1]} msg] {
	    ReportError "Failed to create message(2): $msg"
	    continue
	}
	# Copy to folder
	file delete -force $fn
	if [catch {$msg copy $def} error] {
	    ReportError "Failed to copy: $error"
	    continue
	}
	if {0 != [compare $fn [lindex $case 2]]} {
	    ReportError "Copy differed"
	    continue
	}
	rename $msg ""
    }
}


test_msgcreat::test_msgcreat
