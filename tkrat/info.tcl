# info.tcl --
#
# This file contains rotines which provied the user with some information
# about this program.
#
#
#  TkRat software and its included text is Copyright 1996-2005 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


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
    label $w.copyright -text "TkRat is copyright 1995-2005 by" -font norm
    label $w.author -text "Martin Forssén (maf@tkrat.org)" -font norm
    label $w.tcltk -text "$t(using) Tcl-$tcl_patchLevel/Tk-$tk_patchLevel"
    pack $w.tkrat \
	 $w.date \
	 $w.copyright \
	 $w.author \
	 $w.tcltk -side top -padx 5
    if {![catch {package present Tkhtml 2.0} version]} {
	label $w.tkhtml -text "$t(using) TkHtml-$version"
	pack $w.tkhtml -side top -padx 5
    }
    if {![catch {package present Img} version]} {
	label $w.img -text "$t(using) Img-$version"
	pack $w.img -side top -padx 5
    }
    message $w.more -text $t(send_bugs_etc) -aspect 700
    pack $w.more -side top -padx 5

    button $w.ok \
            -text $t(ok) \
            -command "destroy $w" \
            -width 20

    pack $w.tkrat \
	 $w.date \
	 $w.copyright \
	 $w.author \
	 $w.tcltk \
	 $w.more -side top -padx 5
    pack $w.ok -side top -padx 5 -pady 5

    bind $w <Escape> "$w.ok invoke"
    bind $w.ok <Destroy> "::tkrat::winctl::RecordGeometry about $w"
    ::tkrat::winctl::SetGeometry about $w
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
            -command "destroy $w" \
            -width 20

    message $w.message -aspect 400 -text $t(ratatosk)
    pack $w.message $w.ok -side top -padx 5 -pady 5

    bind $w <Escape> "$w.ok invoke"
    bind $w.ok <Destroy> "::tkrat::winctl::RecordGeometry ratatosk $w"
    ::tkrat::winctl::SetGeometry ratatosk $w
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

    bind .welcome <Escape> ".welcome.b.cont invoke"
    ::tkrat::winctl::SetGeometry welcome .welcome
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
    InitMessages $option(language) balText
    wm title .welcome $changes(welcome_title)
    .welcome.message.text configure -state normal -font $propNormFont
    .welcome.message.text delete 0.0 end
    .welcome.message.text insert 0.0 $changes(welcome)
    .welcome.message.text configure -state disabled
    WelcomeShowMenu
}

# InfoFeatures --
#
# Inform the user of new features since the last time.
#
# Arguments:
# featureIndexes - List of features

proc InfoFeatures {featureIndexes} {
    global option changesDone changesChanges \
	   tkrat_version features t b

    # Init messages
    InitMessages $option(language) features

    # Create window
    toplevel .changes -class TkRat
    wm title .changes $t(changes_title)

    # Message part
    frame .changes.message
    text .changes.message.text -relief sunken -bd 1 \
	-yscrollcommand ".changes.message.scroll set" -setgrid 1 \
	-height 30 -width 80 -highlightthickness 0
    .changes.message.text tag configure feature -wrap word \
	-rmargin 10 -lmargin1 10 -lmargin2 25 -spacing1 10 -tabs 25
    scrollbar .changes.message.scroll -relief sunken -bd 1 \
	    -command ".changes.message.text yview" -highlightthickness 0
    pack .changes.message.scroll -side right -fill y
    pack .changes.message.text -expand 1 -fill both

    # Buttons
    frame .changes.b
    checkbutton .changes.b.shutup -text $t(do_not_show_window_in_future) \
	-onvalue 0 -offvalue 1 -variable option(info_changes)

    button .changes.b.cont -textvariable t(continue) -bd 1 \
	    -command {destroy .changes}
    pack .changes.b.cont -side bottom -pady 10
    pack .changes.b.shutup -side left -padx 10
    set b(.changes.b.cont) welcome_cont
    
    pack .changes.message -side top -expand 1 -fill both -padx 5 -pady 5
    pack .changes.b -fill x

    bind .changes ".changes.b.cont invoke"
    ::tkrat::winctl::SetGeometry infoChanges .changes

    # Populate textwindow
    foreach featureIndex $featureIndexes {
	foreach f [split $features($featureIndex) "\n"] {
	    .changes.message.text insert end "-\t$f\n" feature
	}
    }
    .changes.message.text configure -state disabled

    tkwait window .changes

    ::tkrat::winctl::RecordGeometry infoChanges .changes
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
    button $w.button -text $t(close) -command "destroy $w"
    listbox $w.list -yscroll "$w.scroll set" -relief sunken -bd 1 \
	    -selectmode extended
    scrollbar $w.scroll -relief raised -bd 1 \
	    -command "$w.list yview"
    pack $w.button -side bottom -padx 5 -pady 5
    pack $w.scroll -side right -fill y
    pack $w.list -expand 1 -fill both

    foreach m [GetRatLog] {
	$w.list insert end $m
    }

    ::tkrat::winctl::SetGeometry seeLog $w $w.list

    bind $w.list <Destroy> "::tkrat::winctl::RecordGeometry seeLog $w $w.list"
    bind $w <Escape> "$w.button invoke"
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
    upvar \#0 $id hd
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
    grid $w.text - $w.scroll -sticky nsew
    set hd(text) $w.text

    frame $w.f
    button $w.f.cancel -text $t(cancel) -command "destroy $w"
    button $w.f.continue -text $t(continue) -command "DoSendBugReport $id"
    pack $w.f.continue $w.f.cancel -side left -padx 10
    grid $w.f - -

    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 2 -weight 1

    # Binding and focus
    rat_edit::create $w.text
    focus $w.sentry

    ::tkrat::winctl::SetGeometry sendBug $w $w.text

    bind $w.text <Destroy> "::tkrat::winctl::RecordGeometry sendBug $w $w.text; unset $id"
    bind $w <Escape> "$w.f.cancel invoke"
}


# DoSendBugReport --
#
# Construct a skeleton bug report
#
# Arguments:
# handler - name of global array holding data for the report

proc DoSendBugReport {handler} {
    upvar \#0 $handler hd
    global idCnt option t option tkrat_version tkrat_version_date \
	   tcl_version tk_version tcl_patchLevel tk_patchLevel rat_lib \
	   rat_tmp

    set mhandler composeM[incr idCnt]
    upvar \#0 $mhandler mh

    set mh(to) dl-tkrat@catspoiler.org
    set mh(subject) $hd(subject)
    set mh(description) "Bug report"
    set mh(data) [$hd(text) get 1.0 end-1c]
    set mh(role) $option(default_role)
    set mh(data_tags) {}

    foreach a $hd(attachments) {
	set ahandler composeB[incr idCnt]
	upvar \#0 $ahandler ah

	lappend mh(attachmentList) $ahandler
	set ah(type) text
	set ah(subtype) plain
	set ah(description) [lindex $a 0]
	set ah(filename) $rat_tmp/rat.[RatGenId]
	set ah(removeFile) 1
	set fh [open $ah(filename) w]
        set ah(parameter) ""
        set ah(disp_parm) ""
	puts $fh [lindex $a 1]
	close $fh
    }

    set ahandler composeB[incr idCnt]
    upvar \#0 $ahandler ah
    lappend mh(attachmentList) $ahandler
    set ah(type) text
    set ah(subtype) plain
    set ah(content_description) $t(configuration_information)
    set ah(filename) $rat_tmp/rat.[RatGenId]
    set ah(removeFile) 1
    set ah(parameter) ""
    set ah(disp_parm) ""
    set fh [open $ah(filename) w]
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
        if {![regexp {,smtp_passwd} $n]} {
            puts $fh "option($n): '$option($n)'"
        }
    }
    close $fh

    catch {focus $hd(oldfocus)}
    destroy $hd(w)

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
    button $w.dismiss -text $t(dismiss) -command "destroy $w"
    pack $w.msg -side top
    pack $w.but -side top -anchor w -pady 5 -padx 5
    pack $w.dismiss -pady 5

    ::tkrat::winctl::SetGeometry warning $w
    bind $w <Escape> "$w.dismiss invoke"

    tkwait window $w

    ::tkrat::winctl::RecordGeometry warning $w
}


# StartupInfo --
#
# Give information when starting a new version for for the first time
#
# Arguments:

proc StartupInfo {} {
    global option tkrat_version tkrat_version_date currentLanguage_t features

    # Check which version the user last used
    if {0 == $option(last_version_date)} {
	InfoWelcome

	# Reinitialize language (if needed)
	if {[string compare $option(language) $currentLanguage_t]} {
	    InitMessages $option(language) t
            InitMessages $option(language) balText
	}
	set option(last_version_date) $tkrat_version_date

	InitMessages $option(language) features
	set fs [lsort -integer [array names features]]
	set option(last_seen_feature) [lindex $fs end]

        return 1
    } elseif {$option(last_version_date) < $tkrat_version_date} {
	InitMessages $option(language) features

	set fs [lsort -integer [array names features]]
	if {$option(last_seen_feature) != [lindex $fs end]} {
	    set start [expr [lsearch -exact $fs $option(last_seen_feature)]+1]
	    InfoFeatures [lrange $fs $start end]
	}
	set option(last_seen_feature) [lindex $fs end]
	set option(last_version_date) $tkrat_version_date
        SaveOptions
        return 0
    } else {
        return 0
    }
}


# dumpenv --
#
# Dump environment (commands and globals) to file
set dumpno 0
proc dumpenv {} {
    global dumpno

    set name [format "dump_%03d" [incr dumpno]]
    set f [open $name w]

    # Dump commands
    foreach c [lsort [info commands]] {
        puts $f "cmd:$c"
    }

    # Dump globals
    foreach g [lsort [info globals]] {
        puts $f "global:$g"
        upvar \#0 $g v
        if {[array exists v]} {
            foreach n [lsort [array names v]] {
                puts $f "  $n"
            }
        }
    }

    close $f
}
