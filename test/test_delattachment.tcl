puts "$HEAD Test delete attachment"

namespace eval test_delattachment {
}

# Create messages
proc test_delattachment::init {} {
    global t dir

    variable base_msg "Date: Sat, 16 Jul 2005 19:14:00 +0100 (MET)
From: Martin Forssen <maf@tkrat.org>
Subject: test with attachments
To: Martin Forssen <maf@tkrat.org>
Message-ID: <42@tkrat.org>
MIME-Version: 1.0
Content-Type: MULTIPART/MIXED; BOUNDARY=BD

--BD
@ATTACHMENT_0@
--BD
@ATTACHMENT_1@
--BD
@ATTACHMENT_2@
--BD--
"

    set deleted "Content-Type: TEXT/PLAIN; CHARSET=us-ascii

$t(deleted_attachment)"
    set a0 "Content-Type: TEXT/PLAIN; CHARSET=us-ascii

Attachment 0, to complicate matters BD"
    set a1_full "Content-Type: MULTIPART/MIXED; BOUNDARY=2BD

--2BD
Content-Type: TEXT/PLAIN; CHARSET=us-ascii

Attachment 1_0
--2BD
Content-Type: TEXT/PLAIN; CHARSET=us-ascii

Attachment 1_1
--2BD--
"
    set a1_top "Content-Type: MULTIPART/MIXED; BOUNDARY=2BD

--2BD
Content-Type: TEXT/PLAIN; CHARSET=us-ascii

Attachment 1_0
--2BD
$deleted
--2BD--
"
    set a1_bottom "Content-Type: MULTIPART/MIXED; BOUNDARY=2BD

--2BD
$deleted
--2BD
Content-Type: TEXT/PLAIN; CHARSET=us-ascii

Attachment 1_1
--2BD--
"
    set a2 "Content-Type: TEXT/PLAIN; CHARSET=us-ascii

Attachment 2"

    variable orig_msg $base_msg
    regsub @ATTACHMENT_0@ $orig_msg $a0 orig_msg
    regsub @ATTACHMENT_1@ $orig_msg $a1_full orig_msg
    regsub @ATTACHMENT_2@ $orig_msg $a2 orig_msg

    variable tests [list \
        [list "No deletions" {} $a0 $a1_full $a2] \
        [list "Last simple" {2} $a0 $a1_full $deleted] \
        [list "two simple" {0 2} $deleted $a1_full $deleted] \
        [list "two simple2" {2 0} $deleted $a1_full $deleted] \
        [list "Entire multipart" {1} $a0 $deleted $a2] \
        [list "First subpart" {{1 0}} $a0 $a1_bottom $a2] \
        [list "Second subpart" {{1 1}} $a0 $a1_top $a2] \
        [list "Second sub and last" {{1 1} 2} $a0 $a1_top $deleted] \
        [list "Second sub and last2" {2 {1 1}} $a0 $a1_top $deleted]]

    variable fn1 $dir/folder.[pid]-1
    variable def1 [list Test1 file {} $fn1]
}

# Run a test
proc test_delattachment::run_test {msg test} {
    global verbose hdr
    variable base_msg

    StartTest [lindex $test 0]

    # Initialize expected message
    regsub @ATTACHMENT_0@ $base_msg [lindex $test 2] expected
    regsub @ATTACHMENT_1@ $expected [lindex $test 3] expected
    regsub @ATTACHMENT_2@ $expected [lindex $test 4] expected
    regsub -all "\n" $expected "\r\n" expected

    # Run test
    if {[catch {$msg delete_attachments [lindex $test 1]} nmsg]} {
        ReportError "Command failed: $nmsg"
        return
    }

    # Verify result
    set result [$nmsg rawText]
    if {[string compare $expected $result]} {
        if {$verbose} {
            puts "Expected:"
            puts $expected
            puts "Result:"
            puts $result
        }
        ReportError "New messages differs from expected"
    }
}

# Run all tests
proc test_delattachment::test_delattachment {} {
    global hdr
    variable tests
    variable orig_msg
    variable fn1
    variable def1

    init

    # Create base message
    set fh [open $fn1 w]
    puts $fh $hdr
    puts $fh "From maf@tkrat.org Tue Sep  5 18:02:22 2000 +0100"
    puts $fh $orig_msg
    close $fh
    set f [RatOpenFolder $def1]
    set msg [$f get 0]

    foreach test $tests {
        run_test $msg $test
    }
    $f close
    file delete $fn1
}

test_delattachment::test_delattachment
