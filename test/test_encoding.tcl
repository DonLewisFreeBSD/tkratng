puts "$HEAD Test encoding"

namespace eval test_encoding {
}

proc test_encoding::test_encoding {} {
    test_encoding::test_qp_encoding
    test_encoding::test_header_encoding
}

proc test_encoding::test_qp_encoding {} {
    global LEAD errors verbose

    #
    # List of test cases
    #  {text_to_encode charset expected_encoded_text}
    #
    set tests {
	{foo "" foo}
	{f=oo "" f=3Doo}
	{f=3Doo "" f=3D3Doo}
	{Räkan iso8859-1 R=E4kan}
	{Räkan utf-8 R=C3=A4kan}
    }
    foreach te $tests {
	set in [lindex $te 0]
	set charset [lindex $te 1]
	set out [lindex $te 2]

	if {$verbose} {
	    puts "Encoding '$in' in '$charset'"
	}
	if {[catch {RatEncodeQP $charset $in} result]} {
	    puts "$LEAD RatEncodeQP failed: $result"
	    incr errors
	    continue
	}
	if {$result != $out} {
	    puts "$LEAD RatEncodeQP encoded '$in' in '$charset' to '$result' expected '$out'"
	    incr errors
	    continue
	}
	if {[catch {RatDecodeQP $charset $out} result]} {
	    puts "$LEAD RatDecodeQP failed: $result"
	    incr errors
	    continue
	}
	if {$result != $in} {
	    puts "$LEAD RatDecodeQP encoded '$out' in '$charset' to '$result' expected '$in'"
	    incr errors
	    continue
	}
    }
    exit 1
}

proc test_encoding::test_header_encoding {} {
    global LEAD errors verbose

    set encodings {us-ascii iso-8859-1 iso-2022-jp}

    #
    # Check suggested encodings
    #
    set tests {
	{"us-ascii" "Ok in us-ascii"}
	{"iso-8859-1" "Ok in iso-8859-1 \u00a9"}
	{"iso-2022-jp" "in iso-2022-jp \u306f"}
	{"" "Not ok in any \u01a9"}
    }
    lappend tests [list "us-ascii" "Long string [string repeat { } 16385]"]
    foreach te $tests {
	set data [lindex $te 1]
	set logdata [string range $data 0 20]
	if {[catch {RatCheckEncodings data $encodings} enc]} {
	    puts "$LEAD Failed to check encodings for {$logdata}: $enc"
	    incr errors
	    continue
	}
	if {[lindex $te 0] != $enc} {
	    puts [concat "$LEAD Suggested frong encoding for {$logdata}." \
		  "Expected [lindex $te 0] but got $enc"]
	    incr errors
	}
    }

    #
    # Check header encodings
    #
    set tests {
	{0 "" ""}
	{0 "Test" "Test"}
	{0 "Te?st" "Te?st"}
	{65 "Break line" "Break line"}
	{66 "Break line" "Break\n line"}
	{0 "R\ue4kan lever?" "=?iso-8859-1?Q?R=E4kan_lever=3F?="}
	{0 "R\ue4ksm\uf6rg\ue5s" "=?iso-8859-1?Q?R=E4ksm=F6rg=E5s?="}
	{0 "\ue5\ue4\uf6" "=?iso-8859-1?Q?=E5=E4=F6?="}
	{0 "\u306f" "=?iso-2022-jp?Q?=1B(B=1B$@$O?="}
	{43 "\u306f\u306f" "=?iso-2022-jp?Q?=1B(B=1B$@$O$O?="}
	{44 "\u306f\u306f" "=?iso-2022-jp?Q?=1B(B=1B$@$O?=\n =?iso-2022-jp?Q?=1B(B=1B$@$O?="}
	{9 "(linuxppc-jp:11528) Performa5410 \u3067\u306e\u30cd\u30c3\u30c8\u30ef\u30fc\u30af" "(linuxppc-jp:11528) Performa5410\n =?iso-2022-jp?Q?=1B(B=1B$@$G$N%M%C%H%o!<%/?="}
	{8 "Fwd: From our Home and Family to your Home and Family . . .                                                                                    0001a" "Fwd: From our Home and Family to your Home and Family . . .        \n                                                                           \n 0001a"}
    }
    foreach te $tests {
	# Encode header
	if {[catch {RatTest encode_header $encodings \
	    [lindex $te 0] [lindex $te 1]} e]} {
	    puts "$LEAD Failed to encode header line: $e"
	    incr errors
	    continue
	}
	# Check encoded version
	if {$e != [lindex $te 2]} {
	    puts "$LEAD Incorrectly encoded header-line"
	    if {$verbose} {
		puts [list "Got" $e]
		puts [list "Expected" [lindex $te 2]]
	    }
	}
	# Decode the encoded
	if {[catch {RatTest decode_header $e} d]} {
	    puts "$LEAD Failed to decode header line: $e"
	    incr errors
	    continue
	}
	# Check decoded version
	if {$d != [lindex $te 1]} {
	    puts "$LEAD Incorrectly decoded header-line"
	    incr errors
	    if {$verbose} {
		puts [list "Decoded" $e]
		puts [list "Got" $d]
		puts [list "Expected" [lindex $te 1]]
	    }
	}
    }
}

test_encoding::test_encoding
