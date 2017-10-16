# merge.tcl
#
# Merges messages in one language from one file into another file
#
# Usage: tclsh merge.tcl file_to_merge_into lang file_to_merge_from
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Check tcl version
if {[info tclversion] < 8.1} {
    puts "This script requires tclsh8.1 or later"
    exit 1
}

source defs.tcl

foreach l $languages {
    lappend langs [lindex $l 0]
}

#
# Check arguments
#
if {3 != [llength $argv]} {
    puts "Usage: $argv0 file_to_merge_into lang file_to_merge_from"
    exit 1
}
set basefile [lindex $argv 0]
set sourcelang [lindex $argv 1]
set sourcefile [lindex $argv 2]
if {![file exists $basefile] || ![file writable $basefile]} {
    puts "$basefile does not exist or is not writable"
    exit 1
}
if {-1 == [lsearch -exact $langs $sourcelang]} {
    puts "Unkown language $sourcelang, perhaps you need to modify defs.tcl"
    exit 1
}
if {![file readable $sourcefile]} {
    puts "Can't open $sourcefile for reading"
    exit 1
}

# Define label and variable proc
proc label {label} {
    global current_label labels

    set current_label $label
    lappend labels $label
}
proc variable {v} {
    global varName

    set varName $v
}

#
# Read definitions from sourcefile
#
foreach l $langs {
    proc $l t {}
}
proc $sourcelang m \
	"global t current_label; set t($sourcelang,\$current_label) \$m"
source $sourcefile

#
# Read definitions from basefile
#
foreach l $langs {
    proc $l m "global t current_label; set t($l,\$current_label) \$m"
}
proc $sourcelang m {}
set labels {}
source $basefile

#
# Write output
#
if {[catch {open $basefile w} fh]} {
    puts "Failed to open $basefile for writing: $fh"
    exit 1
}
puts $fh $message
puts $fh ""
puts $fh "variable $varName"
puts $fh ""
foreach lab $labels {
    puts $fh "label $lab"
    foreach lang $langs {
	if {![info exists t($lang,$lab)]} {
	    puts "$lang: missing $lab"
	} else {
	    puts $fh [list $lang $t($lang,$lab)]
	}
    }
    puts $fh ""
}
close $fh
