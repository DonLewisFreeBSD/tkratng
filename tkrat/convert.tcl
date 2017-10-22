# convert.tcl --
#
# This file contains code which converts old version of the database and
# vfolders to the latest version.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# FixDbase4 --
#
# Convert the database from version 4 to 5.
#
# Arguments:

proc FixDbase4 {} {
    global fix_scale option t

    # Check with user
    if { 0 != [RatDialog "" $t(upgrade_dbase) $t(old_dbase) {} \
	    0 $t(continue) $t(abort)]} {
	exit 1
    }
    wm withdraw .
    set dir [RatTildeSubst $option(dbase_dir)]

    # Tell user what we are doing
    set w .upgdbase
    toplevel $w -class TkRat
    wm title $w "Upgrade dbase to version 5"
    scale $w.scale -length 6c -showvalue 0 -sliderlength 5 \
	    -variable fix_scale  -orient horiz
    pack $w.scale -side top -padx 5 -pady 5
    ::tkrat::winctl::SetGeometry fixDbase $w

    # Find how many entries we must fix
    set fh [open $dir/index.info r]
    gets $fh l
    close $fh
    set entries [lindex $l 1]

    # Fix index.changes-file
    if {[file readable $dir/index.changes]} {
	set newIndex [open $dir/index.changes.new w]
	set oldIndex [open $dir/index.changes r]
	while {0 < [gets $oldIndex l]} {
	    if {"a" == [string index $l 0]} {
		incr entries
	    } else {
		puts $newIndex $l
	    }
	}
	close $oldIndex
	close $newIndex
    }

    # Fix index
    set fix_scale 0
    $w.scale configure -to $entries
    set lock [open $dir/lock w]
    puts $lock "Updating"
    close $lock
    set newIndex [open $dir/index.new w]
    set oldIndex [open $dir/index r]
    fconfigure $newIndex -encoding utf-8
    for {set fix_scale 0} {$fix_scale < $entries} {incr fix_scale} {
	update idletasks

	# Read old index entry
	for {set i 0} {$i < 11} {incr i} {
	    gets $oldIndex line($i)
	}

	# Get Message-Id and references from stored message
	set msgid {}
	set ref {}
	set m [open $dir/dbase/$line(10) r]
	gets $m joined
	while {0 < [gets $m l]} {
	    if {[regexp "^( |\t)" $l]} {
		set joined "$joined$l]"
		continue
	    }
	    if {[regexp -nocase {^message-id:[^<]*(<[^>]+>)} $joined {} r]} {
		set msgid $r
	    }
	    if {[regexp -nocase {^in-reply-to:[^<]*(<[^>]+>)} $joined {} r]} {
		set ref $r
	    }
	    if {"" == $ref && [regexp -nocase \
		    {^references:[^<]*(<[^>]+>)} $joined {} r]} {
		set ref $r
	    }
	    set joined $l
	}
	close $m

	# Remember offset of this entry
	set offset($i) [tell $newIndex]

	# Write entry
	for {set i 0} {$i < 3} {incr i} {
	    puts $newIndex $line($i)
	}
	puts $newIndex $msgid
	puts $newIndex $ref
	for {set i 3} {$i < 11} {incr i} {
	    puts $newIndex $line($i)
	}
    }
    close $newIndex
    close $oldIndex

    # Generate new files
    if {[file readable $dir/index.changes.new]} {
	file rename $dir/index.changes $dir/index.changes.4
	file rename -force -- $dir/index.changes.new $dir/index.changes
    }
    file rename -force -- $dir/index $dir/index.4
    file rename -force -- $dir/index.new $dir/index
    file rename -force -- $dir/index.info $dir/index.info.4
    set f [open $dir/index.info w]
    puts $f "5 $entries"
    close $f

    file delete -force -- $dir/lock
    ::tkrat::winctl::RecordGeometry fixDbase $w
    destroy $w
}


# FixDbase3 --
#
# Convert the database from version 3 to 4.
#
# Arguments:

proc FixDbase3 {} {
    global fix_scale option t

    # Check with user
    if { 0 != [RatDialog "" $t(upgrade_dbase) $t(old_dbase) {} \
	    0 $t(continue) $t(abort)]} {
	exit 1
    }
    wm withdraw .
    set dir [RatTildeSubst $option(dbase_dir)]
    FixOldDbase dir

    # Tell user what we are doing
    set w .upgdbase
    toplevel $w -class TkRat
    wm title $w "Upgrade dbase to version 4"
    scale $w.scale -length 6c -showvalue 0 -sliderlength 5 \
	    -variable fix_scale  -orient horiz
    pack $w.scale -side top -padx 5 -pady 5
    ::tkrat::winctl::SetGeometry fixDbase $w

    # Find how many entries we must fix
    set fh [open $dir/index.ver r]
    gets $fh version
    gets $fh entries
    close $fh
    set fix_scale 0
    $w.scale configure -to $entries

    # Do actual fixing
    set lock [open $dir/lock w]
    puts $lock "Updating"
    close $lock
    set newIndex [open $dir/index.new w]
    set oldIndex [open $dir/index r]
    for {set fix_scale 0} {$fix_scale < $entries} {incr fix_scale} {
	update idletasks
	for {set i 0} {$i < 14} {incr i} {
	    gets $oldIndex line($i)
	}
	# To
	set result $line(0)
	regsub {@.+$} $result {} name
	while {[regexp {[a-zA-Z][ 	]+[a-zA-Z]} $result match]} {
	    regsub {[ 	]+} $match {,} subst
	    regsub $match $result $subst result
	}
	puts -nonewline $newIndex $result
	regsub {(, )+} $line(1) {} result
	if {[string length $result]} {
	    puts -nonewline $newIndex " ($result)"
	}
	puts $newIndex ""
	# From
	set result $line(2)
	while {[regexp {[a-zA-Z][ 	]+[a-zA-Z]} $result match]} {
	    regsub {[ 	]+} $match {,} subst
	    regsub $match $result $subst result
	}
	puts -nonewline $newIndex $result
	regsub {(, )+} $line(3) {} result
	if {[string length $result]} {
	    puts -nonewline $newIndex " ($result)"
	}
	puts $newIndex ""
	# Cc
	puts $newIndex $line(4)
	# Subject
	puts $newIndex $line(5)
	# Date (UNIX time_t as a string)
	puts $newIndex $line(6)
	# Keywords (SPACE separated list)
	puts $newIndex $line(7)
	# Size
	puts $newIndex [file size $dir/dbase/$line(13)]
	# Status
	set status ""
	set msgFh [open $dir/dbase/$line(13) r]
	while {[string length [gets $msgFh hline]]} {
	    if { 0 == [string length $hline]} {
		break
	    }
	    if {![string compare status: [string tolower [lindex $hline 0]]]} {
		set status [lindex $hline 1]
		break
	    }
	}
	close $msgFh
	puts $newIndex $status
	# Expiration time (UNIX time_t as a string)
	if {[string length $line(11)]} {
	    puts $newIndex [RatTime +100]
	} else {
	    puts $newIndex ""
	}
	# Expiration event (none, remove, incoming, backup or custom *)
	puts $newIndex $line(12)
	# Filename
	regsub {[%,].+} $name {} fdir
	if {[file exists $dir/dbase/$fdir/.seq]} {
	    set seqFh [open $dir/dbase/$fdir/.seq r+]
	    set sequence [expr {1+[gets $seqFh]}]
	    seek $seqFh 0
	    puts $seqFh $sequence
	    close $seqFh
	} else {
	    set sequence 0
	    if {![file isdirectory $dir/dbase/$fdir]} {
		exec mkdir $dir/dbase/$fdir
	    }
	    set seqFh [open $dir/dbase/$fdir/.seq w]
	    puts $seqFh $sequence
	    close $seqFh
	}
	set modSequence ""
	for {set i [expr {[string length $sequence]-1}]} {$i>=0} {incr i -1} {
	    set modSequence $modSequence[string index $sequence $i]
	}
	set filename $fdir/$modSequence
	puts $newIndex $filename
	exec mv $dir/dbase/$line(13) $dir/dbase/$filename
    }
    close $newIndex
    close $oldIndex
    set infoFH [open  $dir/index.info w]
    puts $infoFH "4 $entries"
    close $infoFH
    file delete -force -- $dir/index.ver
    file delete -force -- $dir/index.changes
    file delete -force -- $dir/index.read
    exec mv $dir/index.new $dir/index
    file delete -force -- $dir/lock

    # Find unlinked entries
    pack forget $w.scale
    label $w.message -text "Looking for unlinked entries"
    pack $w.message
    update
    set unlinkedList [exec find $dir/dbase -name *@* -print]
    ::tkrat::winctl::RecordGeometry fixDbase $w
    if {[llength $unlinkedList]} {
	global vFolderDef

	foreach file $unlinkedList {
	    exec cat $file >>[RatTildeSubst ~/UnlinkedMessages]
	    file delete -force -- $file
	}
	destroy $w
	RatDialog "" $t(unlinked_messages) \
		"$t(unl_m1) [llength $unlinkedList] $t(unl_m2)" {} 0 \
		$t(continue)
	set id [expr {[lindex \
		[lsort -integer -decreasing [array names vFolderDef]] 0] + 1}]
	set vFolderDef($id) [list UnlinkedMessages file {} \
		[RatTildeSubst ~/UnlinkedMessages]]
	set vFolderDef(0) [replace $vFolderDef(0) 3 3 \
		[concat [lindex $vFolderDef(0) 3] $id]]
	VFolderWrite
    } else {
	destroy $w
    }
    wm withdraw .
}


# FixOldDbase --
#
# This repairs any inconstencies in the database that are created by
# a fault in the logic in the old version.
#
# Arguments:
# dir -		Directory in which to find dbase

proc FixOldDbase {dir} {
    global option

    # Check for existance
    if { 0 == [file exists $dir/index]} {
	return
    }

    # The database is good so far
    set good 1

    # First check for locks
    if { 1 == [file exists $dir/index.read]} {
	if { 0 < [file size $dir/index.read]} {
	    set result [RatDialog "" "Dbase in use?" \
					  "I find a lock on the database.\
 Are you running another copy of tkrat somewhere?" {} 1 Yes No ]
	
	    if { $result == 0} {
		# Another copy is runing don't touch the database
		return
	    } else {
		# Possibly corrupt database
		set good 0
		catch "file delete -force -- $dir/index.read"
	    }
	}
    }

    # Now do a quick consistency check of the database
    if { 1 == [file exists $dir/index.lock]} {
	set good 0
	catch "file delete -force -- $dir/index.lock"
    }

    if { 1 == [file exists $dir/index.changes] } {
	set good 0
	catch "file delete -force -- $dir/index.changes"
    }

    if { 0 == [file exists $dir/index.ver] } {
	set good 0
    } else {
	set fh [open $dir/index.ver r]
	gets $fh version
	gets $fh orig_entries
	close $fh
    }

    if { 1 == $good } {
	scan [exec wc -l $dir/index] "%d" lines

	if { [expr {($lines/14)*14}] != $lines } {
	    # Not even divisible by 14
	    set good 0
	} else {
	    if { [expr {$lines/14}] != $orig_entries} {
		# Mismatch with info in index.ver
		set good 0
	    }
	}
    }

    if { 1 == $good } {
	# Dbase seems to be OK
	return
    }

    # Tell the user
    set w .dbc
    toplevel $w -class TkRat
    wm title $w Dbase
    wm iconname $w Dbase

    message $w.msg -text "Database corrupt. Fixing it..." -aspect 800
    pack $w.msg -padx 10 -pady 10

    ::tkrat::winctl::SetGeometry fixDbase2 $w
    update

    DoFixOldDbase $dir

    # Final cleanup
    ::tkrat::winctl::RecordGeometry fixDbase2 $w
    destroy $w
}


# DoFixOldDbase --
#
# This routine does the acutual fixing
#
# Arguments:
# dir -		Directory of the dbase

proc DoFixOldDbase {dir} {

    # Initialize
    set entries 0
    set in [open $dir/index r]
    set out [open $dir/nindex w]

    while { 0 < [gets $in line(0)] && 0 == [eof $in]} {
	# Read 13 lines
	for {set i 1} {$i < 14} {incr i} {
	    gets $in line($i)
	}

	# Check that the last line contains a /< sequence
	while { 0 == [regexp /< $line(13)] } {
	    # Nope, corrupt entry... fix it
	    for {set i 1} {$i < 14} {incr i} {
		if { 1 == [regexp {^ |^	} $line($i)] } {
		    set p [expr {$i-1}]
		    set line($p) "$line($p)$line($i)"
		    for {set j $i} {$j < 13} {incr j} {
			set line($j) $line([expr {$j+1}])
		    }
		    gets $in line(13)
		}
	    }

	    if { 1 == [eof $in]} {
		tk_Dialog Error "Can't fix database, giving up" {} 0 Ok
		exit
	    }
	}

	# Write this entry
	for {set i 0} {$i < 14} {incr i} {
	    puts $out $line($i)
	}
	incr entries

	# Consistency check
	if { 1 == [eof $in]} {
	    tk_Dialog Error "Can't fix database, giving up" {} 0 Ok
	    exit
	}
    }

    close $in
    close $out
    exec mv $dir/nindex $dir/index

    set fh [open $dir/index.ver w]
    puts $fh 2
    puts $fh $entries
    close $fh
}

# FixVFolderList --
#
# Upgrade the vfolderlist if needed.
#
# Arguments:

proc FixVFolderList {} {
    global vfolder_list vfolder_def vFolderStructIdent vFolderStruct \
	   vFolderDef vFolderDefIdent vFolderVersion option

    set vFolderStructIdent 0
    set vFolderStruct(0) {}
    if {![info exists vfolder_list]} {
	return
    }
    FixVFolderStruct $vfolder_list
    unset vfolder_list

    set vFolderDefIdent 1
    set vFolderDef(0) $option(default_folder)
    set vFolderStruct(0) [linsert $vFolderStruct(0) 0 {vfolder 0 INBOX}]
    foreach vf [array names vfolder_def] {
	if {![info exists vFolderDef($vf)]} {
	    continue
	}
	if {$vf > $vFolderDefIdent} {
	    set vFolderDefIdent $vf
	}
	set l $vfolder_def($vf)
	set n $vFolderDef($vf)
	if {![string compare [lindex $l 0] file]} {
	    set vFolderDef($vf) [list $n file {} [lindex $l 1]]
	} else {
	    set l2 [lindex $l 2]
	    set vFolderDef($vf) [list $n dbase {} \
		    [lindex $l2 0] [lindex $l2 1] \
		    [string trimleft [lindex $l2 3] +]]
	}
    }
    incr vFolderDefIdent
    set vFolderVersion 4
    VFolderWrite
}

# FixVFolderStruct --
#
# Fixes one menu in the vFolderStruct
#
# Arguments:
# content -	The menu to fix (in the old format)

proc FixVFolderStruct {content} {
    global vFolderStructIdent vFolderStruct vFolderDef

    set ident $vFolderStructIdent
    incr vFolderStructIdent
    foreach elem $content {
	if {![string compare [lindex $elem 1] dir]} {
	    lappend vFolderStruct($ident) [list struct \
		    [FixVFolderStruct [lindex $elem 2]] [lindex $elem 0]]
	} else {
	    set vFolderDef([lindex $elem 2]) [lindex $elem 0]
	    lappend vFolderStruct($ident) [list vfolder [lindex $elem 2] \
		    [lindex $elem 0]]
	}
    }
    return $ident
}

# UpgradeVFolderList4to5 --
#
# Upgrade the vfolderlist from version 4 to version 5
# This upgrade removes the pair of extra braces around the folder specs
#
# Arguments:

proc UpgradeVFolderList4to5  {} {
    global vFolderDef vFolderVersion

    foreach n [array names vFolderDef] {
	set p [lindex $vFolderDef($n) 1]
	if {"pop3" != $p && "imap" != $p} {
	    continue
	}
	set d $vFolderDef($n)
	set vFolderDef($n) [lreplace $d 3 3 [lindex [lindex $d 3] 0]]
    }
    set vFolderVersion 5
}

# UpgradeVFolderList5to6 --
#
# Upgrade the vfolderlist from version 5 to version 6
# This upgrade Adds monitor and watch to the inbox and fixes so that the
# features list always has an even number of elements
#
# Arguments:

proc UpgradeVFolderList5to6  {} {
    global vFolderDef vFolderVersion vFolderInbox

    foreach id [array names vFolderDef] {
	set f [lindex $vFolderDef($id) 2]
	if {-1 != [set i [lsearch -exact trace $f]]} {
	    set f [linsert $f [expr {$i+1}] 1]
	}
	if {-1 != [set i [lsearch -exact subscribed $f]]} {
	    set f [linsert $f [expr {$i+1}] 1]
	}
	if {$vFolderInbox == $id} {
	    set f [concat $f {monitor 1 watch 1}]
	}
	set vFolderDef($id) [lreplace $vFolderDef($id) 2 2 $f]
    }
    set vFolderVersion 6
}

# GetHid --
#
# Create a host definition, or reuse an old one if a match is found
#
# Arguments:
# def - Host definition {host port flags user}

proc GetHid {def} {
    global mailServer

    foreach m [array names mailServer] {
	if {![string compare $mailServer($m) $def]} {
	    return $m
	}
    }

    set i 0
    set hid [lindex $def 0]
    while {[info exists mailServer($hid)]} {
	set hid [lindex $def 0]-[incr i]
    }
    set mailServer($hid) $def
    return $hid
}

# UpgradeVFolderDef6to7 --
#
# Upgrade a folder definition from version 6 to version 7
#
# Arguments:
# d - Folder definition

proc UpgradeVFolderDef6to7 {d} {
    global option

    # Make a backup
    file copy -force $option(ratatosk_dir)/vfolderlist \
	    $option(ratatosk_dir)/vfolderlist.backup

    switch -regexp [lindex $d 1] {
	imap|dis {
	    regexp {\{([^:\}]+)(:([0-9]+))?\}(.*)} [lindex $d 3] \
		    unused host u2 port path
	    set md [list $host $port {} [lindex $d 4]]
	    set d [concat [lrange $d 0 2] [GetHid $md] [list $path]]
	}
	pop3 {
	    regexp {\{([^/:\}]+)(:([0-9]+))?/pop3\}} [lindex $d 3] \
		    unused host u2 port
	    if {"" == $port} {
		set port 110
	    }
	    set md [list $host $port {pop3} [lindex $d 4]]
	    set d [concat [lrange $d 0 2] [GetHid $md]]
	}
	dir {
	    set d [lreplace $d 1 1 file]
	}
	dbase {
	    set d [list \
		    [lindex $d 0] \
		    [lindex $d 1] \
		    [lindex $d 2] \
		    [lindex $d 4] \
		    [lindex $d 5] \
		    [list and keywords [lindex $d 3]]]
	}
	file|mh|dynamic {
	    # Nothing special needed
	}
    }
    return $d
}

# UpgradeVFolderList6to7 --
#
# Upgrade the vfolderlist from version 6 to version 7
#
# Arguments:

proc UpgradeVFolderList6to7  {} {
    global vFolderDef vFolderVersion vFolderInbox vFolderDefIdent \
	    vFolderStruct vFolderStructIdent vFolderSave

    unset vFolderDefIdent vFolderStructIdent

    # Create mapping to new ids
    set nextId 0
    foreach id [array names vFolderStruct] {
	set tmps($id) $vFolderStruct($id)
	if {0 == $id} {
	    set structmap($id) $id
	} else {
	    set structmap($id) [incr nextId]
	}
	unset vFolderStruct($id)
    }
    foreach id [array names vFolderDef] {
	set tmpd($id) $vFolderDef($id)
	set defmap($id) [incr nextId]
	unset vFolderDef($id)
    }

    # Find names of menus
    set structname(0) {}
    foreach id [array names tmps] {
	foreach e $tmps($id) {
	    if {"struct" == [lindex $e 0]} {
		set structname([lindex $e 1]) [lindex $e 2]
	    }
	}
    }

    # Convert vFolderStruct entries
    foreach id [array names structname] {
	set s {}
	foreach e $tmps($id) {
	    switch [lindex $e 0] {
		"struct" {
		    set n $structmap([lindex $e 1])
		}
		"vfolder" {
		    set n $defmap([lindex $e 1])
		}
		"import" {
		    set n $defmap([lindex $e 1])
		    set imported([lindex $e 1]) 1
		}
	    }
	    lappend s $n
	}
	set vFolderDef($structmap($id)) [list $structname($id) struct {} $s]
    }

    # Convert vFolderDef entries
    foreach id [array names tmpd] {
	if {[info exists imported($id)]} {
	    set pat [lindex $tmpd($id) 4]
	    set fl(trace) 0
	    array set fl [lindex $tmpd($id) 2]
	    if {0 != $fl(trace)} {
		set flags [list reimport session]
	    } else {
		set flags [list reimport manually]
	    }
	    if {"imap" == [lindex $tmpd($id) 1]
    	            || "dis" == [lindex $tmpd($id) 1]} {
		if {"" == [lindex $tmpd($id) 8]} {
		    set host \{[lindex $tmpd($id) 5]\}[lindex $tmpd($id) 7]
		} else {
		    set host \{[lindex $tmpd($id) 5]\:[lindex $tmpd($id) 8]\}[lindex $tmpd($id) 7]
		}
		set d [concat [lrange $tmpd($id) 0 1] \
			      [list [lindex $tmpd($id) 3]] \
			      [list $host] [lindex $tmpd($id) 6]]
	    } else {
		set d [concat [lrange $tmpd($id) 0 1] \
			      [list [lindex $tmpd($id) 3]] \
			      [lrange $tmpd($id) 5 end]]
	    }
	} else {
	    set d $tmpd($id)
	}
	set d [UpgradeVFolderDef6to7 $d]
	if {[info exists imported($id)]} {
	    set d [list [lindex $tmpd($id) 0] import $flags $d $pat {}]
	}
	set vFolderDef($defmap($id)) $d
    }

    set vFolderVersion 7
    if {"" != $vFolderInbox} {
	set vFolderInbox $defmap($vFolderInbox)
    }
    if {"" != $vFolderSave} {
	set vFolderSave $defmap($vFolderSave)
    }
}


# UpgradeVFolderList7to8 --
#
# Upgrade the vfolderlist from version 7 to version 8
#
# Arguments:

proc UpgradeVFolderList7to8  {} {
    global vFolderDef vFolderVersion

    foreach id [array names vFolderDef] {
	switch -regexp [lindex $vFolderDef($id) 1] {
	    imap|dis {
		set enc [RatEncodeQP system [lindex $vFolderDef($id) 4]]
		set vFolderDef($id) [lreplace $vFolderDef($id) 4 4 $enc]
	    }
	    file|mh|dynamic {
		set enc [RatEncodeQP system [lindex $vFolderDef($id) 3]]
		set vFolderDef($id) [lreplace $vFolderDef($id) 3 3 $enc]
	    }
	}
    }

    set vFolderVersion 8
}

# FixOldOptions --
#
# Read old options files and try to adapt to modern options
#
# Arguments:

proc FixOldOptions {} {
    upvar \#0 option newOption

    source $newOption(ratatosk_dir)/ratatoskrc.gen
    set changed 0

    if {[info exists option(show_header)]} {
	set newOption(show_header_selection) $option(show_header)
	set changed 1
    }
    if {[info exists option(reply_lead)]} {
	set newOption(reply_lead) $option(reply_lead)
	set changed 1
    }
    if {[info exists option(signature)]} {
	set newOption(signature) $option(signature)
	set changed 1
    }
    if {[info exists option(xeditor)]} {
	set newOption(editor) $option(xeditor)
	set changed 1
    }
    if {[info exists option(watcher_geom)]} {
	set newOption(watcher_geometry) $option(watcher_geom)
	set changed 1
    }
    if {[info exists option(printcmd)]} {
	set newOption(print_command) $option(printcmd)
	set changed 1
    }
    if {$changed} {
	SaveOptions
    }
    file delete -force [RatTildeSubst $newOption(ratatosk_dir)/ratatoskrc.gen]
}


# ScanAliases --
#
# See if the user has any old alias files, and if then scan them.
#
# Arguments:

proc ScanAliases {} {
    global option t

    set n 0
    if {[file readable ~/.mailrc]} {
	incr n [ReadMailAliases ~/.mailrc $option(default_book)]
    }
    if {[file readable ~/.elm/aliases.text]} {
	incr n [ReadElmAliases ~/.elm/aliases.text $option(default_book)]
    }
    if {[file readable ~/.addressbook]} {
	incr n [ReadPineAliases ~/.addressbook $option(default_book)]
    }
    if {$n} {
	foreach book $option(addrbooks) {
	    if {$option(default_book) == [lindex $book 0]} {
		set file [lindex $book 2]
		break
	    }
	}
	RatAlias save $option(default_book) $file
    }

    set option(scan_aliases) 3
    SaveOptions
    AliasesPopulate
}


# AddImapPorts --
#
# Add port spexification to all imap folders (except those that already
# have it.
#
# Arguments:

proc AddImapPorts {} {
    global option vFolderDef

    VFolderRead
    foreach id [array names vFolderDef] {
	if {[string compare imap [lindex $vFolderDef($id) 1]]} {
	    continue
	}
	set spec [lindex $vFolderDef($id) 2]
	regsub {(\{[^\{\}:]*)\}} $spec "\\1:$option(imap_port)\}" spec
	set vFolderDef($id) [lreplace $vFolderDef($id) 2 2 $spec]
    }
    VFolderWrite
}

# ConvertHold --
#
# Convert the old-style message hold to a new folder
#
# Arguments:
# dir - Hold to convert
# var - Name of vfolderdef which contains the new folder def

proc ConvertHold {dir var} {
    global vFolderDef
    upvar \#0 $var v

    # Load old hold functions
    package require ratatosk_old 2.3

    # Get def of new folder
    VFolderRead
    set fh [RatOpenFolder $vFolderDef($v)]

    ComposeLoad

    # Loop over messages in hold
    while {0 < [llength [RatHold $dir list]]} {
	set hd [RatHold $dir extract 0]
	set msg [ConvertHoldMsg $hd]
	$fh insert $msg
	rename $msg ""
    }

    # Mark all messages in hold as read
    foreach i [$fh flagged seen 0] {
	$fh setFlag $i seen 1
    }
    $fh close
}

# ConvertHoldMsg --
#
# Prepares a message extracted from the hold to be made into a real message
#
# Arguments:
# mgh - handler of the extracted message

proc ConvertHoldMsg {mgh} {
    global charsetMapping option t composeHeaderList rat_tmp
    upvar \#0 $mgh mh

    if {[info exists mh(body)]} {
	upvar \#0 $mh(body) bh
	if {![string compare "$bh(type)/$bh(subtype)" text/plain]} {
	    set edit $mh(body)
	    set children {}
	} elseif {![string compare "$bh(type)" multipart]} {
	    set children $bh(children)
	    upvar \#0 [lindex $children 0] ch1
	    if {![string compare "$ch1(type)/$ch1(subtype)" text/plain]} {
		set edit [lindex $children 0]
		set children [lreplace $children 0 0]
	    } else {
		set edit {}
	    }
	} else {
	    set edit {}
	    set children $mh(body)
	}
	if {[info exists bh(pgp_sign)]} {
	    set mh(pgp_sign) $bh(pgp_sign)
	    set mh(pgp_encrypt) $bh(pgp_encrypt)
	}
	if {[string length $edit]} {
	    upvar \#0 $edit bp
	    set fh [open $bp(filename) r]
	    if {[info exists bp(parameter)]} {
		foreach lp $bp(parameter) {
		    set p([lindex $lp 0]) [lindex $lp 1]
		}
	    }
	    if {[info exists p(charset)]} {
		set charset $p(charset)
	    } else {
		if {[info exists mh(charset)]} {
		    set charset $mh(charset)
		} else {
		    set charset auto
		}
	    }
	    if {"auto" == $charset} {
		set charset utf-8
	    } 
	    set mh(charset) $charset
	    fconfigure $fh -encoding $charsetMapping($charset)
	    set mh(data) [read $fh]
	    set mh(data_tags) {}
	    close $fh
	    if {$bp(removeFile)} {
		catch "file delete -force -- $bp(filename)"
	    }
	}
	set mh(attachmentList) $children
    }

    #######################################################################
    # Create message

    # Envelope
    set envelope {}
    foreach h $composeHeaderList {
	if {[string length $mh($h)]} {
	    lappend envelope [list $h $mh($h)]
	}
    }
    lappend envelope [list X-TkRat-Internal-Role $mh(role)]

    # Determine suitable charset
    catch {unset p}
    set p(charset) $mh(charset)
    if {[info exists bh(parameter)]} {
	foreach lp $bp(parameter) {
	    set p([lindex $lp 0]) [lindex $lp 1]
	}
    }
    if {"auto" == $p(charset)} {
	set fallback $option(charset)
	set p(charset) [RatCheckEncodings mh(data) \
			 $option(charset_candidates)]
    } else {
	set fallback $p(charset)
	set p(charset) [RatCheckEncodings mh(data) $mh(charset)]
    }
    if {"" == $p(charset)} {
	if {0 != [RatDialog $mh(toplevel) $t(warning) $t(bad_charset) {} \
		      0 $t(continue) $t(abort)]} {
	    return {}
	}
	set p(charset) $fallback
    }

    # Prepare parameters array
    set params {}
    foreach name [array names p] {
	lappend params [list $name $p($name)]
    }

    # Find encoding
    set fn [RatTildeSubst $rat_tmp/rat.[RatGenId]]
    set fh [open $fn w]
    if {[info exists charsetMapping($p(charset))]} {
	fconfigure $fh -encoding $charsetMapping($p(charset))
    } else {
	fconfigure $fh -encoding $p(charset)
    }
    puts -nonewline $fh $mh(data)
    close $fh
    set encoding [RatGetCTE $fn]
    file delete $fn

    # Collect into body entity
    set body [list text plain $params $encoding inline {} {} \
		  [list utfblob $mh(data)]]
    # Handle attachments
    if { 0 < [llength $mh(attachmentList)]} {
	set attachments [list $body]
	foreach a $mh(attachmentList) {
	    upvar \#0 $a bh
	    set body_header {}
	    foreach h {description id} {
		if {[info exists bh($h)] && [string length $bh($h)]} {
		    lappend body_header [list $h $bh($h)]
		}
	    }
	    foreach v {parameter disp_parm} {
		if {![info exists bh($v)]} {
		    set bh($v) {}
		}
	    }
	    if {![info exists bh(encoding)]} {
		set bh(encoding) 7bit
	    }
	    set bodypart [list $bh(type) $bh(subtype) $bh(parameter) \
			      $bh(encoding) attachment $bh(disp_parm) \
			      $body_header \
			[list file $bh(filename)]]
	    lappend attachments $bodypart
	}
	set body [list multipart mixed {} 7bit {} {} {} $attachments]
    }

    set msg [RatCreateMessage $mh(role) [list $envelope $body]]
    
    # pgp stuff
    set mh(pgp_signer) [RatExtractAddresses $mh(role) $mh(from)]
    set mh(pgp_rcpts) [RatExtractAddresses $mh(role) $mh(to) $mh(cc)]
    if {$mh(pgp_sign) || $mh(pgp_encrypt)} {
	if {[catch {$msg pgp $mh(pgp_sign) $mh(pgp_encrypt) $mh(role) \
			$mh(pgp_signer) $mh(pgp_rcpts)}]} {
	    return
	}
    }

    return $msg
}

# NewVersionUpdate --
#
# Does updates that needs to be done when a new version is started for the
# first time.
#
# Arguments:

proc NewVersionUpdate {} {
    global option globalOption env t

    if {$option(last_version_date) < 19960908 && $option(smtp_verbose) == 2} {
	set option(smtp_verbose) 3
    }
    if {$option(last_version_date) < 19970112} {
	global ratPlace ratSize ratPlaceModified
	::tkrat::winctl::ReadPos
	catch {unset ratPlace(aliasList)}
	catch {unset ratPlace(aliasEdit)}
	catch {unset ratPlace(aliasCreate)}
	catch {unset ratSize(aliasList)}
	set ratPlaceModified 1
	::tkrat::winctl::SavePos
    }

    # Add port number to imap folders
    if {$option(last_version_date) < 19970209} {
	AddImapPorts
    }

    # Convert log timeout to seconds
    if {$option(last_version_date) < 19970601} {
	if {$option(log_timeout) > 100} {
	    set option(log_timeout) [expr {$option(log_timeout)/1000}]
	}
    }

    # Convert to new address book specification
    if {$option(last_version_date) < 19970731
	    && [info exists option(aliases_file)]} {
	set option(addrbooks) \
		[list [list Personal tkrat $option(aliases_file)]]
	unset option(aliases_file)
    }

    # Convert to new cache options
    if {$option(last_version_date) < 19970827} {
	if {[info exists option(pgp_pwkeep)]} {
	    if {0 != $option(pgp_pwkeep)} {
		set option(cache_pgp) 1
	    } else {
		set option(cache_pgp) 0
	    }
	    set option(cache_pgp_timeout) $option(pgp_pwkeep)
	}
	if {[info exists option(keep_conn)]} {
	    if {0 != $option(keep_conn)} {
		set option(cache_conn) 1
	    } else {
		set option(cache_conn) 0
	    }
	    set option(cache_conn_timeout) $option(keep_conn)
	}
    }

    # Check dbase
    if {[file readable $option(dbase_dir)/index.ver]} {
	# Upgrade to version 4
	FixDbase3
    }
    if {[file readable $option(dbase_dir)/index.info]} {
	set f [open $option(dbase_dir)/index.info r]
	gets $f line
	close $f
	if {3 == [lindex $line 0]} {
	    FixDbase4
	}
	if {4 == [lindex $line 0]} {
	    FixDbase4
	}
    }

    # Convert old options
    if {[file readable $option(ratatosk_dir)/ratatoskrc.gen]} {
	FixOldOptions
    }

    # Convert alias files to utf-8
    if {$option(last_version_date) < 19980214} {
	set as $option(addrbooks)
	lappend as $option(system_aliases)
	foreach a $as {
	    if {"tkrat" == [lindex $a 1] && [file writable [lindex $a 2]]} {
		set f [lindex $a 2]
		set fh [open $f r]
		while { 0 < [gets $fh l] && 0 == [eof $fh]} {
		    lappend lines $l
		}
		close $fh
		set fh [open $f w]
		fconfigure $fh -encoding utf-8
		foreach l $lines {
		    puts $fh "$l {}"
		}
		close $fh
	    }
	}
    }

    # Convert expression file
    if {[file readable $option(ratatosk_dir)/expressions]} {
	source $option(ratatosk_dir)/expressions
    }
    if {[info exists expArrayId]} {
	set f [open $option(ratatosk_dir)/expressions w]
	set newExpList {}
	foreach e $expList {
	    lappend newExpList $expName($e)
	    puts $f [list set expExp($expName($e)) $expExp($e)]
	}
	puts $f "set expList [list $newExpList]"
	close $f
    }

    # Convert fontsize option
    if {$option(last_version_date) < 19991219
	    && [info exists option(fontsize)]} {

	if {$option(fontsize) != 12} {
	    foreach o {prop_norm fixed_norm} {
		set option($o) [lreplace $option($o) 2 2 $option(fontsize)]
	    }
	}
	unset option(fontsize)
	unset globalOption(fontsize)
    }

    if {$option(last_version_date) < 19991219
	    && 1 == [llength $option(watcher_font)]} {
	set option(watcher_font) [list name $option(watcher_font)]
    }

    if {1 != [llength $option(watcher_time)]} {
	set new 30
	# Get value from std folders
	foreach v $option(watcher_time) {
	    if {"std" == [lindex $v 0] && 2 == [llength $v]} {
		set new [lindex $v 1]
		break
	    }
	}
	set option(watcher_time) $new
    }

    if {$option(last_version_date) < 20010809} {
	foreach v {bcc from masquerade_as name reply_to
	           sendprog sendprog_8bit sendprot signature smtp_hosts
                   save_outgoing} {
	    if {[info exists option($v)]} {
		set option(r0,$v) $option($v)
		unset option($v)
		catch {unset globalOption($v)}
	    }
	}
    }
    if {$option(last_version_date) < 20011206} {
	foreach r $option(roles) {
	    if {![info exists option($r,masquerade_as)]} {
		continue
	    }
	    if {"" != $option($r,masquerade_as)} {
		set from [string trim $option($r,from)]
		if {"" == $option($r,from)} {
		    set option($r,from) "$env(USER)@$option($r,masquerade_as)"
		} elseif {-1 == [string first "@" $option($r,from)]} {
		    set option($r,from) \
			    "$option($r,from)@$option($r,masquerade_as)"
		} elseif {![regexp "@$option($r,masquerade_as)(>|$)" \
			$option($r,from)]} {
		    Popup [format $t(masq_from_conflict) \
			    $option($r,name) $option($r,masquerade_as) \
			    $option($r,from)]
		}
	    }
	    unset option($r,masquerade_as)
	    if {[info exists globalOption($r,masquerade_as)]} {
		unset globalOption($r,masquerade_as)
	    }
	}
    }

    # Check if we have held messages in old format
    if {$option(last_version_date) < 20020808} {
	if {[info exists option(hold_dir)]} {
	    set hold_dir $option(hold_dir)
	} else {
	    set hold_dir $option(ratatosk_dir)/hold
	}
	if {[file isdirectory $hold_dir]
	    && 0 < [llength [glob -nocomplain -directory $hold_dir *.desc]]} {
	    ConvertHold $hold_dir vFolderHold
	}

	if {[info exists option(send_cache)]} {
	    set send_dir $option(send_cache)
	} else {
	    set send_dir $option(ratatosk_dir)/send
	}
	if {[file isdirectory $send_dir]
	    && 0 < [llength [glob -nocomplain -directory $send_dir *.desc]]} {
	    ConvertHold $send_dir vFolderOutgoing
	}
	file delete -force $send_dir
	file delete -force $hold_dir
    }

    # Add new fields to roles
    if {$option(last_version_date) < 20020423} {
        foreach r $option(roles) {
            if {![info exists option($r,uqa_domain)]} {
                set option($r,uqa_domain) ""
            }
            if {![info exists option($r,smtp_helo)]} {
                set option($r,smtp_helo) ""
            }
        }
    }

    # Add new fields to roles
    if {$option(last_version_date) < 20020830 ||
	$option(last_version_date) == 20050602} {
	foreach r $option(roles) {
	    foreach vt {{validate_cert 0}  {same_sending_prefs 0} \
			{smtp_user {}} {smtp_passwd {}}} {
		if {![info exists option($r,[lindex $vt 0])]} {
		    set option($r,[lindex $vt 0]) [lindex $vt 1]
		}
	    }
	}
    }

    # Adjust size of norm italic font
    if {($option(last_version_date) < 20030213 ||
        $option(last_version_date) == 20050602) 
        && [info exists option(fixed_italic)]} {
	set option(fixed_italic) \
	    [lreplace $option(fixed_italic) 2 2 [lindex $option(fixed_norm) 2]]
    }

    # Add pgp fields to roles
    if {$option(last_version_date) < 20031123 ||
	$option(last_version_date) == 20050602} {
	foreach r $option(roles) {
	    if {[info exists option(pgp_sign)]} {
		set option($r,sign_outgoing) $option(pgp_sign)
	    } else {
		set option($r,sign_outgoing) 0
	    }
	    set option($r,sign_as) {}
	}
    }

    # Remove old color settings (they are incompatible)
    if {$option(last_version_date) < 20040710 ||
	$option(last_version_date) == 20050602} {
        if {4 != [llength $option(color_set)]} {
            set option(color_set) {\#dde3eb black white black}
        }
    }

    # Convert saved address history encoding to utf-8
    if {$option(last_version_date) < 20050701
        && [file readable $option(ratatosk_dir)/addrlist]} {
        set fh [open $option(ratatosk_dir)/addrlist r]
        set data [read $fh]
        close $fh

        set fh [open $option(ratatosk_dir)/addrlist w]
        fconfigure $fh -encoding utf-8
        puts -nonewline $fh $data
        close $fh
    }

    # Convert font specifications
    if {$option(last_version_date) < 20050706} {
        if {[info exists option(prop_norm)]
            && "components" == [lindex $option(prop_norm) 0]} {
            set option(font_family_prop) \
                [string tolower [lindex $option(prop_norm) 1]]
            set option(font_size) [lindex $option(prop_norm) 2]
        }
        if {[info exists option(fixed_norm)]
            && "components" == [lindex $option(fixed_norm) 0]} {
            set option(font_family_fixed) \
                [string tolower [lindex $option(fixed_norm) 1]]
            if {$option(font_size) != [lindex $option(fixed_norm) 2]} {
                set option(font_size) [lindex $option(fixed_norm) 2]
            }
        }
    }

    if {$option(last_version_date) < 20050707
        && [info exists option($option(url_viewer))]} {
        set option(browser_cmd) $option($option(url_viewer))
        if {"firefox" == $option(browser_cmd)} {
            set option(url_viewer) firefox
        }
    }

    if {$option(last_version_date) < 20050707
        &&  [file exists "$option(ratatosk_dir)/log"]} {
        file delete "$option(ratatosk_dir)/log"
    }
}
