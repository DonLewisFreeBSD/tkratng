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

    # Pack it
    pack $w.top -side top
    pack $w.mess -side top -expand 1 -fill both
    pack $w.dismiss -pady 5

    # handle geometry
    ::tkrat::winctl::SetGeometry dbCheckW $w $w.mess.text
    bind $w.mess.text <Destroy> "::tkrat::winctl::RecordGeometry dbCheckW $w $w.mess.text"
}
