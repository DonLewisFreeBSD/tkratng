# help.tcl --
#
# This file contains code which handles help windows
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Order of helptext entries
set helporder { intro
                roles
		folders
		folderdef
		dbase
		deleting
		grouping
		userproc
		bugreport}

# Help --
#
# Creates a new help window and shows the requested help-entry (or an
# introduction if none is specified).
#
# Arguments:
# section  - The section to show (may be empty)

proc Help {{subject intro}} {
    global idCnt t b help option helporder

    # Initialize help texts (if needed)
    if {![info exists help(intro)]} {
	InitMessages $option(language) help
    }

    # Create identifier
    set id helpWin[incr idCnt]
    upvar \#0 $id hd
    set w .$id
    set hd(w) $w

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(help_window)

    # Populate window
    label $w.subjects -text $t(subjects)
    scrollbar $w.subjscroll \
	-relief sunken \
	-command "$w.subjlist yview" \
	-highlightthickness 0
    listbox $w.subjlist \
	-yscroll "$w.subjscroll set" \
	-relief sunken \
	-bd 1 \
	-exportselection false \
	-highlightthickness 0 \
	-selectmode single \
        -width 20 \
        -height 9
    set hd(list) $w.subjlist
    set b($hd(list)) help_subjlist
    button $w.dismiss -text $t(dismiss) -command "destroy $w"
    set b($w.dismiss) dismiss
    scrollbar $w.textscroll \
	-relief sunken \
	-command "$w.texttext yview" \
	-highlightthickness 0
    text $w.texttext \
	-yscroll "$w.textscroll set" \
	-setgrid true \
	-wrap word \
	-relief sunken \
	-bd 1 \
	-highlightthickness 0
    set hd(text) $w.texttext
    set b($hd(text)) help_text

    grid $w.subjects
    grid $w.subjlist $w.subjscroll -sticky nsew -pady 5
    grid $w.dismiss - -column 2 -row 1 -padx 10
    grid $w.texttext - - $w.textscroll -sticky nsew
    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 1 -weight 1
    grid rowconfigure $w 2 -weight 10

    # Bindings
    bind $w <Key-space> "$hd(text) yview scroll 1 pages"
    bind $w <Key-BackSpace> "$hd(text) yview scroll -1 pages"
    bind $hd(list) <ButtonRelease-1> "SelectHelp $id"
    bind $hd(text) <Destroy> "DismissHelp $id"

    # Populate list
    foreach topic $helporder {
	$hd(list) insert end $help(title,$topic)
    }

    ::tkrat::winctl::SetGeometry help $w $hd(text)

    ShowHelp $id $subject
}

# SelectHelp --
#
# Figure which subject was selected and show that
#
# Arguments:
# id	- The help-window identifier

proc SelectHelp {id} {
    global helporder
    upvar \#0 $id hd

    set topic [lindex $helporder [$hd(list) curselection]]
    ShowHelp $id $topic
}


# ShowHelp --
#
# Populates the help window
#
# Arguments:
# id	- The help-window identifier
# topic	- The topic to show

proc ShowHelp {id topic} {
    global help helporder
    upvar \#0 $id hd

    # The subject list
    set i [lsearch -exact $helporder $topic]
    if {$i != [$hd(list) curselection]} {
	$hd(list) selection clear 0 end
	$hd(list) selection set $i
    }

    # The text window
    $hd(text) configure -state normal
    $hd(text) delete 0.0 end
    $hd(text) insert end $help($topic)
    $hd(text) configure -state disabled
}

# DismissHelp --
#
# DImisses the help window
#
# Arguments:
# id	- The help-window identifier

proc DismissHelp {id} {
    upvar \#0 $id hd

    ::tkrat::winctl::RecordGeometry help $hd(w) $hd(text)
    unset hd
}
