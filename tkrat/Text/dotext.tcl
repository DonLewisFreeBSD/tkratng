# dotext.tcl
#
# See README for information about what this program does.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Parse arguments
set warn 1
foreach a $argv {
    switch -exact -- $a {
	-nowarn {
	    set warn 0
	}
	-warn {
	    set warn 1
	}
	default {
	    puts "Usage: $argv0 \[-nowarn\]"
	    exit 1
	}
    }
}

# Check tcl version
if {[info tclversion] < 8.1} {
    puts "This script requires tclsh8.1 or later"
    exit 1
}

# Directory where we should store the output files:
set outdir ../.messages

source defs.tcl

# First we should create the language procedures
foreach l $languages {
    set lang [lindex $l 0]
    proc $lang m "addmsg $lang \$m"
    set lang_name($lang) [lindex $l 1]
}

# variable --
#
# Sets the variable name that the messages from this file will end up in.
#
# Arguments:
# name -	The variable name

proc variable {name} {
    global varName
    set varName $name
}

# label --
#
# Sets the label to be used for all subsequent language commands, that
# is until the next call to this function.
#
# Arguments:
# l -	The label

proc label {l} {
    global lab labels
    if { -1 != [lsearch -exact $labels $l]} {
	puts "*** Label '$l' is already defined"
	exit
    }
    lappend labels $l
    set lab $l
}

# addmsg --
#
# Add a text string
#
# Arguments:
# lang - The language
# m    - The message

proc addmsg {lang m} {
    global lab text lang_name
    if {[info exists text($lang,$lab)]} {
        puts "Multiple definitions of $lab in $lang_name($lang)"
    }
    set text($lang,$lab) $m
}

# Now we should build the languages.tcl file, we start by definig the
# procedures.
set getLanguages "proc GetLanguages {} {return \"$languages\"}"
set initMessages {
# InitMessages --
#
# Initializes a set of messages.
#
# Arguments:
# lang -	The language if the messages
# var  -	The variable the messages are in

proc InitMessages {lang var} {
    upvar \#0 currentLanguage_$var currentLanguage
    global ratCurrent

    set currentLanguage $lang
    set oldCharset $ratCurrent(charset)
    set ratCurrent(charset) utf-8
    eval init_${var}_${lang}
    set ratCurrent(charset) $oldCharset
}

}

set fh [open $outdir/languages.tcl w]
puts $fh $message
puts $fh ""
puts $fh $getLanguages
puts $fh ""
puts $fh $initMessages
close $fh


# Source the text input files and write the apropriate output file after
# each input file is read.
foreach f [glob *.text] {
    set labels {}

    # Read data
    set fh [open $f r]
    fconfigure $fh -encoding binary
    eval [read $fh]
    close $fh

    # Write datafiles
    foreach lang $languages {
	set l [lindex $lang 0]
	set fh [open $outdir/text_${varName}_${l}.tcl w]
	fconfigure $fh -encoding binary
	puts $fh $message
	puts $fh ""
	puts $fh "# The following is the function which does the actual work"
	puts $fh "proc init_${varName}_${l} {} {"
	puts $fh "global $varName"
	foreach n $labels {
	    if {[info exists text($l,$n)]} {
		set rl $l
	    } else {
		if {$warn} {
		    puts "Text $n not found in [lindex $lang 1] substituting English"
		}
		set rl en
	    }
	    puts $fh "set ${varName}($n) [list $text($rl,$n)]"
	}
	puts $fh "}"
	puts $fh $trailer
	close $fh

	# Convert datafile from local encding to utf-8
	set fh [open $outdir/text_${varName}_${l}.tcl r]
	fconfigure $fh -encoding [lindex $lang 2]
	set data [read $fh]
	close $fh
	set fh [open $outdir/text_${varName}_${l}.tcl w]
	fconfigure $fh -encoding utf-8
	puts $fh $data
	close $fh
    }
    catch {unset labels}
    catch {unset text}
}
