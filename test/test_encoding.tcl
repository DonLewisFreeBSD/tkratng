puts "$HEAD Test encoding"

namespace eval test_encoding {
}

proc test_encoding::test_encoding {} {
    test_encoding::test_qp_encoding
    test_encoding::test_header_encoding
}

proc test_encoding::test_qp_encoding {} {

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

        StartTest "Encoding '$in' in '$charset'"
	if {[catch {RatEncodeQP $charset $in} result]} {
	    ReportError "RatEncodeQP failed: $result"
	    continue
	}
	if {$result != $out} {
	    ReportError "RatEncodeQP encoded '$in' in '$charset' to '$result' expected '$out'"
	    continue
	}
	if {[catch {RatDecodeQP $charset $out} result]} {
	    ReportError "RatDecodeQP failed: $result"
	    continue
	}
	if {$result != $in} {
	    ReportError "RatDecodeQP encoded '$out' in '$charset' to '$result' expected '$in'"
	    continue
	}
    }
}

proc test_encoding::test_header_encoding {} {
    global tcl_version
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
    set index 0
    foreach te $tests {
	set data [lindex $te 1]
	StartTest "Encoding body\[[incr index]\] in [lindex $te 0]"
	set logdata [string range $data 0 20]
	if {[catch {RatCheckEncodings data $encodings} enc]} {
	    ReportError "Failed to check encodings for {$logdata}: $enc"
	    continue
	}
	if {[lindex $te 0] != $enc} {
	    ReportError [concat "Suggested frong encoding for {$logdata}." \
		  "Expected [lindex $te 0] but got $enc"]
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
	{8 "Fwd: From our Home and Family to your Home and Family . . .                                                                                    0001a" "Fwd: From our Home and Family to your Home and Family . . .        \n                                                                           \n 0001a"}
    }
    # The way iso2022-jp is encoded changes between versions
    if {8.4 > $tcl_version} {
	set tests [concat $tests {
	    {0 "\u306f" "=?iso-2022-jp?Q?=1B=28B=1B=24=40=24O?="}
	    {33 "\u306f\u306f" "=?iso-2022-jp?Q?=1B=28B=1B=24=40=24O=24O?="}
	    {34 "\u306f\u306f" "=?iso-2022-jp?Q?=1B=28B=1B=24=40=24O?=\n =?iso-2022-jp?Q?=1B=28B=1B=24=40=24O?="}
	    {9 "(linuxppc-jp:11528) Performa5410 \u3067\u306e\u30cd\u30c3\u30c8\u30ef\u30fc\u30af" "(linuxppc-jp:11528) Performa5410\n =?iso-2022-jp?Q?=1B=28B=1B=24=40=24G=24N=25M=25C=25H=25o!=3C=25/?="}
	}]
    } elseif {8.4 == $tcl_version} {
	set tests [concat $tests {
	    {0 "\u306f" "=?iso-2022-jp?Q?=1B$B$O=1B(B?="}
	    {43 "\u306f\u306f" "=?iso-2022-jp?Q?=1B$B$O$O=1B(B?="}
	    {44 "\u306f\u306f" "=?iso-2022-jp?Q?=1B$B$O=1B(B?=\n =?iso-2022-jp?Q?=1B$B$O=1B(B?="}
	    {9 "(linuxppc-jp:11528) Performa5410 \u3067\u306e\u30cd\u30c3\u30c8\u30ef\u30fc\u30af" "(linuxppc-jp:11528) Performa5410\n =?iso-2022-jp?Q?=1B$B$G$N%M%C%H%o!<%/=1B(B?="}
	}]
    } else {
	set tests [concat $tests {
	    {0 "\u306f" "=?iso-2022-jp?Q?=1B=24B=24O=1B=28B?="}
	    {35 "\u306f\u306f" "=?iso-2022-jp?Q?=1B=24B=24O=24O=1B=28B?="}
	    {36 "\u306f\u306f" "=?iso-2022-jp?Q?=1B=24B=24O=1B=28B?=\n =?iso-2022-jp?Q?=1B=24B=24O=1B=28B?="}
	    {9 "(linuxppc-jp:11528) Performa5410 \u3067\u306e\u30cd\u30c3\u30c8\u30ef\u30fc\u30af" "(linuxppc-jp:11528) Performa5410\n =?iso-2022-jp?Q?=1B=24B=24G=24N=25M=25C=25H=25o!=3C=25/=1B=28B?="}
	}]
    }

    set index 0
    foreach te $tests {
	StartTest "Encoding header\[[incr index]\] [lindex $te 1]"
	# Encode header
	if {[catch {RatTest encode_header $encodings \
	    [lindex $te 0] [lindex $te 1]} e]} {
	    ReportError "Failed to encode header line: $e"
	    continue
	}
	# Check encoded version
	if {$e != [lindex $te 2]} {
	    ReportError [join [list "Incorrectly encoded header-line" \
				   "     Got [list $e]" \
				   "Expected [list [lindex $te 2]]"] "\n"]
	}
	# Decode the encoded
	if {[catch {RatTest decode_header $e} d]} {
	    ReportError "Failed to decode header line: $e"
	    continue
	}
	# Check decoded version
	if {$d != [lindex $te 1]} {
	    ReportError [join [list "Incorrectly decoded header-line" \
				   " Decoded [list $e]" \
				   "     Got [list $d]" \
				   "Expected [list [lindex $te 1]]"] "\n"]
	}
    }
}

test_encoding::test_encoding
