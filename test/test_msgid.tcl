puts "$HEAD Test Messsage-ID parsing"

namespace eval test_msgid {
}

proc test_msgid::test_msgid {} {
    global option dir hdr

    # List of message-ids to test
    # Each element in teh list is a tuple
    #   Header - Header to look in
    #   MsgId  - Expected message id
    set tests {
	{"<msg@id>" "msg@id"}
	{"<msg1@id> <msg2@id>" "msg2@id"}
	{"<msg@id> (foo <foo@bar.com>'s message at)" "msg@id"}
	{"<\\>\">\"[>]msg@id>" "\{>>[>]msg@id\}"}
    }

    # Folder to use for testing
    set fn $dir/folder.[pid]
    set def [list Test file {} $fn]

    foreach te $tests {
	StartTest "Parsing '[lindex $te 0]'"

	# Generate folder
	set fh [open $fn w 0644]
	puts $fh $hdr
	puts $fh "From maf@kilauea Thu Sep  6 14:25:09 2001 -0400"
	puts $fh "Date: Thu, 06 Sep 2001 14:25:00"
	puts $fh "Message-ID: [lindex $te 0]"
	puts $fh ""
	puts $fh "Body"
	close $fh

	# Read message and get msgid
	set f [RatOpenFolder $def]
	set actual [$f list %M]
	$f close

	# Verify
	if {[string compare $actual [lindex $te 1]]} {
	    ReportError [join [list "Failed to extract correct msgid" \
				   "  Header: [lindex $te 0]" \
				   "Expected: [lindex $te 1]" \
				   "  Actual: $actual"] "\n"]
	}
    }
    file delete -force $fn
}

test_msgid::test_msgid
