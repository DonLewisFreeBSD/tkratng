# info.tcl --
#
# This file contains rotines which provied the user with some information
# about this program.
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# The version history
set ratHistory {0.50 0.51 0.52 0.53 0.54 0.55 0.56 0.57 0.58 0.59
		0.60 0.61 0.62 0.63 0.64 0.65 0.66 0.67 0.68 0.69
		0.70 0.71 0.72 0.73 0.74 0.75 1.0 1.0.1 1.0.2 1.0.3
		1.0.4 1.0.5 1.1a 1.2 2.0}

# Version --
#
# Opens a window and prints the version information in it.
#
# Arguments:

proc Version {} {
    global t tkrat_version tkrat_version_date tcl_patchLevel tk_patchLevel

    set w .about

    if {[winfo exists $w]} {
	wm deiconify $w
	raise $w
	return
    }
    toplevel $w -relief raised -class TkRat
    wm title $w $t(about)
    wm minsize $w 1 1

    catch {font create big -family Helvetica -size -24 -weight bold}
    catch {font create norm -family Helvetica -size -12 -weight bold}
    label $w.tkrat -text "TkRat v$tkrat_version" -font big
    label $w.date -text "$t(date): $tkrat_version_date" -font norm
    label $w.tcltk -text "Tcl-$tcl_patchLevel/Tk-$tk_patchLevel" -font norm
    label $w.copyright -text "TkRat is copyright 1995-2000 by" -font norm
    label $w.author -text "Martin Forssén (maf@tkrat.org)" -font norm
    message $w.more -text $t(send_bugs_etc) -aspect 700
    text $w.text -relief flat -width 40 -height 8 -font norm
    $w.text insert 1.0 $t(send_postcards)
    $w.text configure -state disabled

    button $w.ok \
            -text $t(ok) \
            -command "RecordPos $w about; destroy $w" \
            -width 20

    pack $w.tkrat \
	 $w.date \
	 $w.tcltk \
	 $w.copyright \
	 $w.author \
	 $w.more \
	 $w.text \
	 $w.ok -side top -padx 5 -pady 5
    Place $w about
}

# Ratatosk --
#
# Opens a window an displays a short text about ratatosk in it
#
# Arguments:

proc Ratatosk {} {
    global t

    set w .ratatosk

    if {[winfo exists $w]} {
	wm deiconify $w
	raise $w
	return
    }
    toplevel $w -relief raised -class TkRat
    wm title $w Ratatosk
    wm minsize $w 1 1

    button $w.ok \
            -text $t(ok) \
            -command "RecordPos $w ratatosk; destroy $w" \
            -width 20

    message $w.message -aspect 400 -text $t(ratatosk)
    pack $w.message $w.ok -side top -padx 5 -pady 5
    Place $w ratatosk
}

# InfoWelcome --
#
# This is called when the user invokes tkrat for the first time and
# shows a welcome message to the user. In this window the user may chose
# user interface language and whether changes info should be displayed.
#
# Arguments:

proc InfoWelcome {} {
    global b option changes welcomeLanguage

    # Init messages
    InitMessages $option(language) changes

    # Create window
    toplevel .welcome -class TkRat
    wm title .welcome $changes(welcome_title)

    # Populate window
    frame .welcome.message
    text .welcome.message.text -relief sunken -bd 1 \
	    -yscrollcommand ".welcome.message.scroll set" -setgrid 1 \
	    -height 30 -width 80 -highlightthickness 0
    scrollbar .welcome.message.scroll -relief sunken -bd 1 \
	    -command ".welcome.message.text yview" -highlightthickness 0
    pack .welcome.message.scroll -side right -fill y
    pack .welcome.message.text -expand 1 -fill both
    .welcome.message.text insert 0.0 $changes(welcome)
    .welcome.message.text configure -state disabled

    frame .welcome.b
    frame .welcome.b.lang
    label .welcome.b.lang.label -textvariable changes(language)
    menubutton .welcome.b.lang.menu -textvariable welcomeLanguage \
	    -indicatoron 1 -menu .welcome.b.lang.menu.m -relief raised -bd 1 \
	    -width 20 -anchor c
    menu .welcome.b.lang.menu.m -tearoff 0
    pack .welcome.b.lang.menu \
	 .welcome.b.lang.label -side right
    set b(.welcome.b.lang.menu) welcome_lang

    frame .welcome.b.shutup
    label .welcome.b.shutup.label -textvariable changes(show_changes)
    menubutton .welcome.b.shutup.menu -textvariable welcomeChanges \
	    -indicatoron 1 -menu .welcome.b.shutup.menu.m -relief raised \
	    -bd 1 -width 20 -anchor c
    menu .welcome.b.shutup.menu.m -tearoff 0
    pack .welcome.b.shutup.menu \
	 .welcome.b.shutup.label -side right
    set b(.welcome.b.shutup.menu) welcome_shutup

    button .welcome.b.cont -textvariable changes(continue) -bd 1 \
	    -command {destroy .welcome}
    pack .welcome.b.lang \
	 .welcome.b.shutup -side top -fill x -pady 4
    pack .welcome.b.cont -side top -pady 10
    set b(.welcome.b.cont) welcome_cont
    
    pack .welcome.message -side top -expand 1 -fill both -padx 5 -pady 5
    pack .welcome.b

    set i 0
    foreach l [GetLanguages] {
	.welcome.b.lang.menu.m add command -label [lindex $l 1] \
		-command [list WelcomeLanguage $l]
	.welcome.b.lang.menu.m entryconfigure $i
	incr i
	if {![string compare [lindex $l 0] $option(language)]} {
	    WelcomeLanguage $l
	}
    }

    Place .welcome welcome
    tkwait window .welcome

    foreach bn [array names b .welcome.*] {unset b($bn)}
}


# WelcomeShowMenu --
#
# Build the menu which asks the user if they want to see changes messages.
#
# Arguments:
# font - which font to use (if not the default one)

proc WelcomeShowMenu {{font {}}} {
    global option changes welcomeChanges

    set m .welcome.b.shutup.menu.m
    $m delete 0 end
    if {$option(info_changes)} {
	set welcomeChanges $changes(show)
    } else {
	set welcomeChanges $changes(dont_show)
    }

    $m add command -label $changes(show) \
	    -command "set welcomeChanges [list $changes(show)] ; \
		      set option(info_changes) 1"
    $m add command -label $changes(dont_show) \
	    -command "set welcomeChanges [list $changes(dont_show)] ; \
		      set option(info_changes) 0"
    if {[string length $font]} {
	$m entryconfigure 0 -font $font
	$m entryconfigure 1 -font $font
    }
}


# WelcomeLanguage --
#
# Is called when the user changes the language in the welcome window.
#
# Arguments:
# lang -	The language information

proc WelcomeLanguage {lang} {
    global welcomeLanguage option changes propNormFont

    set welcomeLanguage [lindex $lang 1]
    set option(language) [lindex $lang 0]
    InitMessages $option(language) changes
    wm title .welcome $changes(welcome_title)
    .welcome.message.text configure -state normal -font $propNormFont
    .welcome.message.text delete 0.0 end
    .welcome.message.text insert 0.0 $changes(welcome)
    .welcome.message.text configure -state disabled
    WelcomeShowMenu
}

# InfoChanges --
#
# Inform the user of the changes to the program since the last time it
# was run.
#
# Arguments:

proc InfoChanges {} {
    global option changes changesDone changesChanges \
	   tkrat_version ratHistory b

    # Init messages
    InitMessages $option(language) changes

    # Create window
    toplevel .changes -class TkRat
    wm title .changes $changes(changes_title)

    # Message part
    frame .changes.message
    text .changes.message.text -relief sunken -bd 1 \
	    -yscrollcommand ".changes.message.scroll set" -setgrid 1 \
	    -height 30 -width 80 -highlightthickness 0
    scrollbar .changes.message.scroll -relief sunken -bd 1 \
	    -command ".changes.message.text yview" -highlightthickness 0
    pack .changes.message.scroll -side right -fill y
    pack .changes.message.text -expand 1 -fill both

    # Populate textwindow
    set starts "********************"
    set i [expr {[lsearch -exact $ratHistory $option(last_version)]+1}]
    foreach ver [lrange $ratHistory $i end] {
	.changes.message.text insert end \
		"$starts $changes(changes_in) $ver $starts\n"
	.changes.message.text insert end $changes($ver)
	.changes.message.text insert end "\n\n\n"
    }
    .changes.message.text configure -state disabled

    # Buttons
    frame .changes.b
    frame .changes.b.shutup
    label .changes.b.shutup.label -textvariable changes(show_changes)
    menubutton .changes.b.shutup.menu -textvariable changesChanges \
	    -indicatoron 1 -menu .changes.b.shutup.menu.m -relief raised \
	    -bd 1 -width 20 -anchor c
    menu .changes.b.shutup.menu.m -tearoff 0
    pack .changes.b.shutup.menu \
	 .changes.b.shutup.label -side right
    if {$option(info_changes)} {
	set changesChanges $changes(show)
    } else {
	set changesChanges $changes(dont_show)
    }
    .changes.b.shutup.menu.m add command -label $changes(show) \
	    -command "set changesChanges [list $changes(show)] ; \
		      set option(info_changes) 1"
    .changes.b.shutup.menu.m add command -label $changes(dont_show) \
	    -command "set changesChanges [list $changes(dont_show)] ; \
		      set option(info_changes) 0"
    set b(.changes.b.shutup.menu) welcome_shutup

    button .changes.b.cont -textvariable changes(continue) -bd 1 \
	    -command {RecordPos .changes infoChanges; destroy .changes}
    pack .changes.b.shutup -side top -pady 4
    pack .changes.b.cont -side top -pady 10
    set b(.changes.b.cont) welcome_cont
    
    pack .changes.message -side top -expand 1 -fill both -padx 5 -pady 5
    pack .changes.b

    Place .changes infoChanges
    tkwait window .changes

    foreach bn [array names b .welcome.*] {unset b($bn)}
}

# SeeLog --
#
# Shows the remebered old log messages
#
# Arguments:

proc SeeLog {} {
    global idCnt t

    # Create identifier
    set id iw[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(seelog_title)

    # Message part
    button $w.button -text $t(close) -command "RecordPos $w seeLog; \
	    RecordSize $w.list seeLog; destroy $w"
    listbox $w.list -yscroll "$w.scroll set" -relief sunken -bd 1 \
	    -selectmode extended
    Size $w.list seeLog
    scrollbar $w.scroll -relief raised -bd 1 \
	    -command "$w.list yview"
    pack $w.button -side bottom -padx 5 -pady 5
    pack $w.scroll -side right -fill y
    pack $w.list -expand 1 -fill both

    foreach m [GetRatLog] {
	$w.list insert end $m
    }

    Place $w seeLog
}

# SendBugReport --
#
# Construct a skeleton bug report
#
# Arguments:
# attachments - List of extra attachments. The argument is a list of lists
#		which looks like {name data}

proc SendBugReport {{attachments {}}} {
    global idCnt t

    # Create identifier
    set id sb[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(oldfocus) [focus]

    # Create the toplevel
    toplevel $w -bd 5 -class TkRat
    wm title $w $t(send_bug)
    set hd(w) $w
    set hd(attachments) $attachments

    # The contents
    label $w.slabel -text $t(bug_shortdesc): -anchor e
    entry $w.sentry -textvariable ${id}(subject)
    grid $w.slabel $w.sentry - -sticky we

    label $w.dlabel -text $t(bug_description) -justify left 
    grid $w.dlabel - - -sticky w -pady 5

    text $w.text -relief sunken -bd 1 -setgrid true \
	    -yscrollcommand "$w.scroll set" -wrap none
    scrollbar $w.scroll -relief sunken -bd 1 -takefocus 0 \
	    -command "$w.text yview" -highlightthickness 0
    Size $w.text bugText
    grid $w.text - $w.scroll -sticky nsew
    set hd(text) $w.text

    button $w.continue -text $t(continue) -command "DoSendBugReport $id"
    grid $w.continue - -

    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 1 -weight 1

    # Binding and focus
    rat_edit::create $w.text
    focus $w.sentry

    # Place window
    Place $w sendBug
    wm protocol $w WM_DELETE_WINDOW "unset $id; destroy $w"
}


# DoSendBugReport --
#
# Construct a skeleton bug report
#
# Arguments:
# handler - name of global array holding data for the report

proc DoSendBugReport {handler} {
    upvar #0 $handler hd
    global idCnt option t option tkrat_version tkrat_version_date \
	   tcl_version tk_version tcl_patchLevel tk_patchLevel rat_lib

    set mhandler composeM[incr idCnt]
    upvar #0 $mhandler mh

    set mh(to) maf@tkrat.org
    set mh(subject) $hd(subject)
    set mh(description) "Bug report"
    set mh(data) [$hd(text) get 1.0 end-1c]
    set mh(role) $option(default_role)
    set mh(data_tags) {}

    foreach a $hd(attachments) {
	set ahandler composeB[incr idCnt]
	upvar #0 $ahandler ah

	lappend mh(attachmentList) $ahandler
	set ah(type) text
	set ah(subtype) plain
	set ah(description) [lindex $a 0]
	set ah(filename) [RatTildeSubst $option(send_cache)/rat.[RatGenId]]
	set ah(removeFile) 1
	set fh [open $ah(filename) w 0600]
	puts $fh [lindex $a 1]
	close $fh
    }

    set ahandler composeB[incr idCnt]
    upvar #0 $ahandler ah
    lappend mh(attachmentList) $ahandler
    set ah(type) text
    set ah(subtype) plain
    set ah(description) $t(configuration_information)
    set ah(filename) [RatTildeSubst $option(send_cache)/rat.[RatGenId]]
    set ah(removeFile) 1
    set fh [open $ah(filename) w 0600]
    catch {exec uname -a} uname
    puts $fh "uname -a: '$uname'"
    puts $fh "Version: $tkrat_version ($tkrat_version_date)"
    if {[info exists rat_lib(version)]} {
	puts $fh "Libversion: $rat_lib(version) ($rat_lib(date))"
    } else {
	puts $fh "Libversion: older"
    }
    puts $fh \
	    "Tcl/Tk: $tcl_version/$tk_version ($tcl_patchLevel/$tk_patchLevel)"
    foreach n [lsort [array names tcl_platform]] {
	puts $fh "tcl_platform($n): '$tcl_platform($n)'"
    }
    foreach n [lsort [array names option]] {
	puts $fh "option($n): '$option($n)'"
    }
    close $fh

    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    unset hd

    DoCompose $mhandler $mh(role) 0 1
}

# Warn --
#
# Warn user about something
#
# Arguments:
# tag - Tag describing the warning

proc Warn {tag} {
    global idCnt t option

    # Create identifier
    set id iw[incr idCnt]
    set w .$id
    set option($tag) 0

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(warning)

    # Message part
    message $w.msg -justify left -text $t($tag) -aspect 600 \
	    -relief raised -bd 1 -padx 5 -pady 5
    checkbutton $w.but -text $t(do_not_show_again) -variable option($tag) \
	    -onvalue 0 -offvalue 1
    button $w.dismiss -text $t(dismiss) -command \
	    "RecordPos $w warning; destroy $w"
    pack $w.msg -side top
    pack $w.but -side top -anchor w -pady 5 -padx 5
    pack $w.dismiss -pady 5

    Place $w warning

    tkwait window $w
}


# StartupInfo --
#
# Give information when starting a new version for for the first time
#
# Arguments:

proc StartupInfo {} {
    global option tkrat_version tkrat_version_date currentLanguage_t ratHistory

    # Check which version the user last used
    if {![string length $option(last_version)]} {
	InfoWelcome

	# Reinitialize language (if needed)
	if {[string compare $option(language) $currentLanguage_t]} {
	    InitMessages $option(language) t
	}
	set option(last_version) $tkrat_version
	set option(last_version_date) $tkrat_version_date
	SaveOptions
    } else {
	set io [lsearch -exact $ratHistory $option(last_version)]
	set ic [lsearch -exact $ratHistory $tkrat_version]
	if {$io < $ic && -1 != $io && $option(info_changes)} {
	    InfoChanges
	}
	if {$option(last_version_date) < $tkrat_version_date} {
	    set option(last_version_date) $tkrat_version_date
	    set option(last_version) $tkrat_version
	    SaveOptions
	}
    }
}
