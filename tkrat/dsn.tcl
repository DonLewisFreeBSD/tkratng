# dsn.tcl --
#
# This file contains code which shows the DSN window etc.
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


# A list of all text commands (used for updates)
set dsnCmds {}

# ShowDSNList --
#
# Shows the DSN list
#
# Arguments:

proc ShowDSNList {} {
    global idCnt t b dsnCmds

    # Create identifier
    set id dsnWin[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(notifications)

    # Populate window
    lappend dsnCmds $w.text

    text $w.text \
	    -width 60 \
	    -height 20 \
	    -highlightthickness 0 \
	    -bd 1 \
	    -relief sunken \
	    -wrap none \
	    -yscroll "$w.scroll set" \
	    -setgrid true
    scrollbar $w.scroll \
	    -relief sunken \
	    -command "$w.text yview" \
	    -highlightthickness 0 \
	    -bd 1
    button $w.dismiss -text $t(dismiss) -bd 1 \
	    -command "RecordPos $w showDSNList; DestroyDSN $w.text"
    grid $w.text -row 0 -column 0 -sticky nswe
    grid $w.scroll -row 0 -column 1 -sticky ns
    grid $w.dismiss -row 1 -column 0 -columnspan 2
    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 0 -weight 1
    set b($w.text) notification_window
    wm protocol $w WM_DELETE_WINDOW "DestroyDSN $w.text"

    UpdateDSNText $w.text
    $w.text see end
    Place $w showDSNList
}


# DestroyDSN --
#
# Dismiss a DSN window
#
# Arguments:
# textCmd -	The command name for the text part of the DSN window to dismiss

proc DestroyDSN {textCmd} {
    global dsnCmds b

    unset b($textCmd)
    set i [lsearch $dsnCmds $textCmd]
    set dsnCmds [lreplace $dsnCmds $i $i]
    destroy [winfo toplevel $textCmd]
}


# UpdateDSNText --
#
# Update a DSN text windget
#
# Arguments:
# text -	The text widget to update

proc UpdateDSNText {text} {
    global t option fixedNormFont fixedBoldFont
    set top [lindex [$text yview] 0]
    set id 0

    $text configure -state normal
    $text delete 0.0 end
    $text tag delete [$text tag names]
    $text tag configure Normal -font $fixedNormFont
    $text tag configure Bold -font $fixedBoldFont
    $text tag configure Failed -font $fixedBoldFont
    if { 4 < [winfo cells $text]} {
	$text tag configure Failed -foreground red
	set mark "-background #43ce80 -relief raised -borderwidth 1"
	set umrk "-background {} -relief flat"
    } else {
	$text tag configure Failed -relief raised -borderwidth 1
	set mark "-foreground white -background black"
	set umrk "-foreground {} -background {}"
    }

    foreach d [RatDSNList] {
	set args [clock format [lindex $d 1] -format "%Y %m %e %H %M %S"]
	regsub -all { 0}  $args { } args
	$text insert end " [eval "RatFormatDate $args"]  " Normal
	$text insert end [lindex $d 2] Normal
	$text insert end "\n"
	foreach r [lindex $d 3] {
	    regsub -all {\([^)]+\)} [lindex $r 0] {} action
	    set action [string tolower [string trim $action]]
	    switch $action {
	    failed	{set stat $t(failed); set f Failed}
	    delayed	{set stat $t(delayed); set f Normal}
	    delivered	{set stat $t(delivered); set f Bold}
	    relayed	{set stat $t(relayed); set f Bold}
	    expanded	{set stat $t(expanded); set f Bold}
	    default	{set stat -; set f Normal}
	    }
	    if {[string compare - $stat]} {
		set tag t[incr id]
		$text tag bind $tag <ButtonRelease-1> \
			"DSNShow [lindex $r 2] [lindex $r 1]"
		$text tag bind $tag <Any-Enter> "$text tag configure $tag $mark"
		$text tag bind $tag <Any-Leave> "$text tag configure $tag $umrk"
	    } else {
		set tag {}
	    }
	    $text insert end "    " "Normal $tag" \
		    [format %-14s $stat] "$f $tag"\
		    "[lindex $r 1]\n" "Normal $tag"
	}
    }

    $text yview moveto $top
    $text configure -state disabled
}

# RatDSNRecieve --
#
# This function gets called when new DSN(s) arrive.
#
# Arguments:
# subject   - The subject of the message the DSN refers to
# action    - What has been done to the message
# recipient - Whom the message was destined to
# id	    - The identification for this DSN

proc RatDSNRecieve {subject action recipient id} {
    global dsnCmds option t

    regsub -all {\([^)]+\)} $action {} action
    set action [string tolower [string trim $action]]
    foreach v $option(dsn_verbose) {
	if {![string compare $action [lindex $v 0]]} {
	    if {![string compare [lindex $v 1] status]} {
		RatLog 2 [format $t(dsn_text_$action) \
			[string range $recipient 0 15] \
			[string range $subject 0 15]]
	    } elseif {![string compare [lindex $v 1] notify]} {
		DSNShow $id $recipient
	    }
	}
    }

    foreach t $dsnCmds {
	UpdateDSNText $t
    }
}


# DSNShow --
#
# Shows a given DSN to the user
#
# Arguments:
# dsnID     - Identifier
# recipient - Recipient we are interestred in

proc DSNShow {dsnID recipient} {
    global idCnt t b option fixedNormFont fixedBoldFont

    # Get info
    set msg [RatDSNGet msg $dsnID]
    set report [RatDSNGet report $dsnID $recipient]

    # Create identifier
    set id dsnWin[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(notification)

    # The human readable part
    frame $w.h
    text $w.h.text \
	    -width 80 \
	    -height 15 \
	    -highlightthickness 0 \
	    -bd 1 \
	    -relief sunken \
	    -wrap none \
	    -yscroll "$w.h.scroll set" \
	    -setgrid true
    scrollbar $w.h.scroll \
	    -relief sunken \
	    -command "$w.h.text yview" \
	    -highlightthickness 0 \
	    -bd 1
    set b($w.h.text) dsn_readable
    pack $w.h.scroll -side right -fill y
    pack $w.h.text -expand 1 -fill both
    pack $w.h -side top -fill both -expand 1 -padx 5 -pady 5
    # The message data
    upvar #0 $w.h.text texth \
    	     msgInfo_$msg msgInfo
    set texth(show_header) $option(show_header)
    set texth(struct_menu) $w.m
    menu $texth(struct_menu)
    $texth(struct_menu) delete 0 end
    set body [$msg body]
    set children [$body children]
    set msgInfo(show,$msg) 1
    set msgInfo(show,$body) 1
    set msgInfo(show,[lindex $children 0]) 1
    foreach c [lrange $children 1 end] {
	set msgInfo(show,$c) 0
    }
    Show $w.h.text $msg 0
    $w.h.text mark set humanEnd end

    # The information part
    frame $w.i
    set row 0
    foreach r $report {
	set key [string tolower [lindex $r 0]]
	if {[info exists t($key)]} {
	    set key $t($key)
	} else {
	    set key [lindex $r 0]
	}
	label $w.i.lk$row -text $key: -font $fixedBoldFont
	label $w.i.lv$row -text [lindex $r 1] -font $fixedNormFont
	grid $w.i.lk$row -row $row -column 0 -sticky e
	grid $w.i.lv$row -row $row -column 1 -sticky w
	incr row
    }
    pack $w.i -side top -padx 5 -pady 5

    # Buttons
    frame $w.but
    checkbutton $w.but.more -text $t(see_more_of_message) -bd 1 \
	    -variable msgInfo_${msg}(show,[lindex $children 2]) \
	    -command "Show $w.h.text $msg 0" \
	    -bd 1 -relief raised -pady 4 -padx 4
    button $w.but.dismiss -text $t(dismiss) -bd 1 \
	    -command "RecordPos $w dSNShow; \
		      unset b($w.h.text); \
		      unset b($w.but.more); \
		      unset b($w.but.dismiss); \
		      destroy $w"
    pack $w.but.more \
	 $w.but.dismiss -side left -expand 1
    pack $w.but -fill x -pady 5 -padx 5
    set b($w.but.more) dsn_more_info
    set b($w.but.dismiss) dismiss
    wm protocol $w WM_DELETE_WINDOW "\
		      unset b($w.h.text); \
		      unset b($w.but.more); \
		      unset b($w.but.dismiss); \
		      destroy $w"

    Place $w dSNShow
}
