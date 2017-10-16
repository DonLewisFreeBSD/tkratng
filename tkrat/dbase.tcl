# dbase.tcl --
#
# This file contains code which handles dbase checks
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.


# Expire --
#
# Run the database expiration
#
# Arguments:

proc Expire {} {
    global option t inbox vFolderDef expAfter idCnt fixedNormFont vFolderInbox

    # Sanity check
    # If it has been over a year and there are more than 100 messages in
    # the database then we ask the user for confirmation.
    if {[RatDaysSinceExpire] > 365 && [lindex [RatDbaseInfo] 0] > 100} {
        set action [RbatDialog "" $t(really_expire_title) \
                        $t(really_expire) question 1 $t(expire) $t(cancel)]
        if {$action != 0} {
            return
        }
    }

    # Prepare for next expiration
    if {1 > $option(expire_interval)} {
	set int 1
    } else {
	set int $option(expire_interval)
    }
    set expAfter [after [expr {$int*24*60*60*1000}] Expire]

    if { 0 == [string length $inbox] } {
	set vfolder $vFolderDef($vFolderInbox)
	set inb [RatOpenFolder $vFolderDef($vFolderInbox)]
    } else {
	set inb $inbox
    }
    set id [RatLog 2 $t(db_expire) explicit]
    if {[catch {RatExpire $inb [RatTildeSubst $option(dbase_backup)]} \
	    result]} {
	RatClearLog $id
	Popup [format $t(dbase_error) $result]
	return
    }
    RatClearLog $id
    if { 0 == [string length $inbox] } {
	$inb close
    }
    set scanned [lindex $result 0]
    set deleted [lindex $result 1]
    set backup [lindex $result 2]
    set inbox [lindex $result 3]
    set custom [lindex $result 4]

    if { 0 != $deleted || 0 != $backup || 0 != $inbox} {
	set w .exp[incr idCnt]
	toplevel $w -class TkRat
	wm title $w $t(expire)
	label $w.lab -text $t(expire_result):
	grid $w.lab -columnspan 2
	label $w.lab_scan -text $t(scanned): -anchor e
	label $w.val_scan -text [format %5d $scanned] \
		-width 10 -font $fixedNormFont -anchor w
	grid $w.lab_scan -column 0 -row 1 -sticky e
	grid $w.val_scan -column 1 -row 1 -sticky w
	label $w.lab_delete -text $t(deleted): -anchor e
	label $w.val_delete -text [format %5d $deleted] -font $fixedNormFont
	grid $w.lab_delete -column 0 -row 2 -sticky e
	grid $w.val_delete -column 1 -row 2 -sticky w
	label $w.lab_backup -text $t(backedup): -anchor e
	label $w.val_backup -text [format %5d $backup] -font $fixedNormFont
	grid $w.lab_backup -column 0 -row 3 -sticky e
	grid $w.val_backup -column 1 -row 3 -sticky w
	label $w.lab_inbox -text $t(moved_to_inbox): -anchor e
	label $w.val_inbox -text [format %5d $inbox] -font $fixedNormFont
	grid $w.lab_inbox -column 0 -row 4 -sticky e
	grid $w.val_inbox -column 1 -row 4 -sticky w
	button $w.but -text $t(dismiss) -command "destroy $w"
	grid $w.but -column 0 -columnspan 2 -row 5
        bind $w <Escape> "$w.but invoke"
    }
}
# DbaseCheck
#
# Checks the database and show the result
#
# Arguments:
# fix   - True if we should try to fix problems as well

proc DbaseCheck {fix} {
    global idCnt t fixedNormFont

    # Create identifier
    set id dbaseWin[incr idCnt]
    set w .$id

    # Do checking
    set mid [RatLog 2 $t(checking_dbase)... explicit]
    set result [RatDbaseCheck $fix]
    RatClearLog $mid

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(dbase_check)

    # Top part of window
    frame $w.top
    frame $w.top.l
    label $w.top.l.totm -text $t(total_num_messages):
    label $w.top.l.totmv -text [lindex $result 0] -font $fixedNormFont -width 5
    grid $w.top.l.totm -row 0 -column 0 -sticky e
    grid $w.top.l.totmv -row 0 -column 1 -sticky w
    label $w.top.l.tots -text $t(total_size):
    label $w.top.l.totsv -text [RatMangleNumber [lindex $result 4]] \
	    -font $fixedNormFont -width 5
    grid $w.top.l.tots -row 1 -column 0 -sticky e
    grid $w.top.l.totsv -row 1 -column 1 -sticky w

    frame $w.top.r
    label $w.top.r.numm -text $t(num_malformed):
    label $w.top.r.nummv -text [lindex $result 1] -font $fixedNormFont -width 5
    grid $w.top.r.numm -row 0 -column 0 -sticky e
    grid $w.top.r.nummv -row 0 -column 1 -sticky w
    label $w.top.r.numn -text $t(num_nomessages):
    label $w.top.r.numnv -text [lindex $result 2] -font $fixedNormFont -width 5
    grid $w.top.r.numn -row 1 -column 0 -sticky e
    grid $w.top.r.numnv -row 1 -column 1 -sticky w
    label $w.top.r.numu -text $t(num_unlinked):
    label $w.top.r.numuv -text [lindex $result 3] -font $fixedNormFont -width 5
    grid $w.top.r.numu -row 2 -column 0 -sticky e
    grid $w.top.r.numuv -row 2 -column 1 -sticky w

    pack $w.top.l \
	 $w.top.r -side left -pady 5 -padx 10 -anchor n

    # Messages
    frame $w.mess
    scrollbar $w.mess.scroll \
	    -relief sunken \
	    -command "$w.mess.text yview"
    text $w.mess.text \
	    -yscroll "$w.mess.scroll set" \
	    -setgrid true
    pack $w.mess.scroll -side right -fill y
    pack $w.mess.text -expand 1 -fill both
    foreach m [lindex $result 5] {
	$w.mess.text insert end "$m\n"
    }

    # Button
    button $w.dismiss -text $t(dismiss) -command "destroy $w"
    bind $w <Escape> "$w.dismiss invoke"

    # Pack it
    pack $w.top -side top
    pack $w.mess -side top -expand 1 -fill both
    pack $w.dismiss -pady 5

    # handle geometry
    ::tkrat::winctl::SetGeometry dbCheckW $w $w.mess.text
    bind $w.mess.text <Destroy> "::tkrat::winctl::RecordGeometry dbCheckW $w $w.mess.text"
}

# DbaseInfo --
#
# Show information about dbase
#
# Arguments:

proc DbaseInfo {} {
    global idCnt t fixedNormFont

    # Create identifier
    set id dbaseWin[incr idCnt]
    upvar \#0 $id hd
    set w .$id

    # Collect data
    set dinfo [RatDbaseInfo]
    set keywords [RatDbaseKeywords]
    set hd(from) [lindex $dinfo 1]
    set hd(to) [lindex $dinfo 2]

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(dbase_check)

    # Top information
    label $w.num_lab -text $t(total_num_messages):
    label $w.num -text [lindex $dinfo 0] \
        -font $fixedNormFont -width 7 -anchor e
    grid $w.num_lab $w.num -sticky w

    label $w.total_lab -text $t(total_size):
    label $w.total -text [RatMangleNumber [lindex $dinfo 3]] \
        -font $fixedNormFont -width 7 -anchor e
    grid $w.total_lab $w.total -sticky w

    label $w.start_lab -text $t(earliest_date):
    label $w.start -text [clock format [lindex $dinfo 1]] -font $fixedNormFont
    grid $w.start_lab $w.start -sticky w

    label $w.end_lab -text $t(latest_date):
    label $w.end -text [clock format [lindex $dinfo 2]] -font $fixedNormFont
    grid $w.end_lab $w.end -sticky w

    # Table
    rat_table::create $w.table [list \
                                    [list $t(keyword) string] \
                                    [list $t(usage_count) int]] \
        $keywords -bd 1 -relief sunken
    set hd(table) $w.table
    bind $hd(table) <<ListboxSelect>> [list DbaseInfoSelect $id]
    grid $w.table -

    # Buttons
    frame $w.b
    button $w.b.show -text $t(show_messages) -state disabled\
        -command [list DbaseInfoShow $id]
    set hd(show) $w.b.show
    button $w.b.dismiss -text $t(dismiss) -command "destroy $w" -default active
    bind $w <Escape> "$w.b.dismiss invoke"
    bind $w <Return> "$w.b.dismiss invoke"
    pack $w.b.show $w.b.dismiss -side left -expand 1 -padx 5 -pady 5
    grid $w.b -

    bind $hd(table) <<Action>> [list $hd(show) invoke]

    # Handle geometry
    ::tkrat::winctl::SetGeometry dbInfoW $w
    bind $w.num <Destroy> "::tkrat::winctl::RecordGeometry dbInfoW $w"
}

# DbaseInfoSelect --
#
# Called when the selection in the table changes
#
# Arguments:
# id - identifies the info window

proc DbaseInfoSelect {id} {
    upvar \#0 $id hd

    set hd(selected) [lindex [rat_table::get_selection $hd(table)] 0]
    if {"" != $hd(selected)} {
        $hd(show) configure -state normal
    } else {
        $hd(show) configure -state disabled
    }
}

# DbaseInfoShow --
#
# Show messages with the selected keyword
#
# Arguments:
# id - identifies the info window

proc DbaseInfoShow {id} {
    upvar \#0 $id hd

    set exp [list "int" $hd(from) $hd(to) "and" "keywords" $hd(selected)]
    set vf [list def [list "Dbase search" dbase {} {} {} $exp]]
    NewFolderWin $vf
}

# MsgDbInfo --
#
# Show the message dbinfo dialog
#
# Arguments:
# src     - source of info ("folder" or "msg")
# info    - dbase info
# folder  - folder handler of the folder containing the message
# indexes - list of message indexes

proc MsgDbInfo {src info folder indexes} {
    global idCnt t b fixedNormFont

    # Create identifier
    set id dbaseWin[incr idCnt]
    upvar \#0 $id hd
    set w .$id

    set hd(toplevel) $w
    set hd(folder) $folder
    set hd(indexes) $indexes

    # Do we have valid data?
    if {"" == [lindex $info 0]} {
        set msg [$folder get [lindex $indexes 0]]
        set info [$msg dbinfo_get]
        set src first_msg
    }

    # Collect data
    set hd(keywords) [lindex $info 0]
    set hd(keywords_orig) $hd(keywords)
    set time [lindex $info 1]
    if {$time < 833839200} { # 19960604
        set hd(ex_date) "+$time"
    } else {
        set hd(ex_date) [clock format $time -format "%Y-%m-%d %T"]
    }
    set hd(ex_date_orig) $hd(ex_date)
    set hd(ex_type) [lindex $info 2]
    set hd(ex_type_orig) $hd(ex_type)

    # Create toplevel
    toplevel $w -class TkRat -bd 5
    wm title $w $t(dbinfo)

    # Top label
    label $w.info -text $t(dbinfo_info_$src)
    grid $w.info -

    # Information
    label $w.keywords_lab -text $t(keywords): -pady 5
    entry $w.keywords -textvariable ${id}(keywords) -width 35
    grid $w.keywords_lab $w.keywords -sticky w
    set b($w.keywords) keywords

    label $w.exdate_lab -text $t(exdate): -pady 5
    frame $w.exdate -pady 5
    entry $w.exdate.entry -textvariable ${id}(ex_date) -width 35
    label $w.exdate.expl -text $t(exdate_expl)
    pack $w.exdate.entry $w.exdate.expl -side top
    grid $w.exdate_lab $w.exdate -sticky wn
    set b($w.exdate) exp_date

    label $w.extype_lab -text $t(extype):
    radiobutton $w.extype_none -text $t(none) \
        -variable ${id}(ex_type) -value none
        set b($w.extype_none) exp_none
    grid $w.extype_lab $w.extype_none -sticky w
    foreach e {remove incoming backup} {
        radiobutton $w.extype_$e -text $t($e) \
            -variable ${id}(ex_type) -value $e
        grid x $w.extype_$e -sticky w
        set b($w.extype_$e) exp_$e
    }

    # Buttons
    frame $w.b
    button $w.b.apply -text $t(apply) -command [list MsgDbInfoApply $id]
    set b($w.b.apply) dbinfo_apply_$src
    set hd(apply) $w.b.apply
    button $w.b.reset -text $t(reset) -command [list MsgDbInfoReset $id]
    set b($w.b.reset) reset
    set hd(reset) $w.b.reset
    button $w.b.dismiss -text $t(dismiss) -command "destroy $w"
    set b($w.b.dismiss) dismiss
    set hd(dismiss) $w.b.dismiss
    bind $w <Escape> "$w.b.dismiss invoke"
    pack $w.b.apply $w.b.reset $w.b.dismiss \
        -side left -expand 1 -padx 5 -pady 5
    grid $w.b -

    # Handle geometry
    ::tkrat::winctl::SetGeometry msgDbInfoW $w
    ::tkrat::winctl::ModalGrab $w $w.keywords
    bind $w.keywords <Destroy> "::tkrat::winctl::RecordGeometry msgDbInfoW $w"

    if {"msg" == $src} {
        trace variable hd w [list MsgDbInfoTrace $id]
        # Trigger the trace
        set hd(keywords) $hd(keywords)
    }
}

# MsgDbInfoTrace --
#
# Trace function for MsgDbInfo, enables/disables the apply button
#
# Arguments:
# id   - identifies the window
# args - standard trace arguments

proc MsgDbInfoTrace {id name1 name2 op} {
    upvar \#0 $id hd

    set state disabled

    if {$hd(keywords) != $hd(keywords_orig)
        || $hd(ex_type) != $hd(ex_type_orig)
        || $hd(ex_date) != $hd(ex_date_orig)} {
        set state normal
    }
    $hd(apply) configure -state $state
    $hd(reset) configure -state $state
}

# MsgDbInfoReset --
#
# Reset to original values
#
# Arguments:
# id   - identifies the window

proc MsgDbInfoReset {id} {
    upvar \#0 $id hd

    foreach v {keywords ex_type ex_date} {
        set hd($v) $hd(${v}_orig)
    }
}

# MsgDbInfoApply --
#
# Apply new values
#
# Arguments:
# id   - identifies the window

proc MsgDbInfoApply {id} {
    upvar \#0 $id hd
    global t

    if {"+" == [string index $hd(ex_date) 0]} {
        set add [expr [string range $hd(ex_date) 1 end]*24*60*60]
        set ex_date [expr [clock seconds] + $add]
    } else {
        if {[catch {clock scan $hd(ex_date)} ex_date]} {
            Popup $t(date_parsing_failed) $hd(toplevel)
            return
        }
    }
    $hd(folder) dbinfo_set $hd(indexes) $hd(keywords) $ex_date $hd(ex_type)
    $hd(dismiss) invoke
}
