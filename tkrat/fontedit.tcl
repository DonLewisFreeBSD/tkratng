# fontedit.tcl --
#
# Contains code for the edit font window
#
#  TkRat software and its included text is Copyright 1996-2005 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

namespace eval ::tkrat::fontedit {
    namespace export edit
}

# ::tkrat::fontedit::edit --
#
# Show the edit font window
#
# Arguments:
# font	 - Font setting to edit
# l	 - Label to update afterwards
# parent - Parent window

proc ::tkrat::fontedit::edit {font l parent} {
    global t idCnt pref
    
    set id doEditFont[incr idCnt]
    upvar \#0 $id hd

    # Initialization
    set hd(done) 0
    set hd(new_spec) $pref(opt,$font)
    set hd(old_spec) ""
    set hd(font_name) ""

    if {"components" == [lindex $hd(new_spec) 0]} {
	set hd(family) [lindex $hd(new_spec) 1]
	set hd(size) [lindex $hd(new_spec) 2]
	set hd(weight) [lindex $hd(new_spec) 3]
	set hd(slant) [lindex $hd(new_spec) 4]
	set hd(underline) [lindex $hd(new_spec) 5]
	set hd(overstrike) [lindex $hd(new_spec) 6]
	set hd(method) components
    } else {
	set hd(name) [lindex $hd(new_spec) 1]
	set hd(method) name
	set hd(family) Helvetica
	set hd(size) 12
    }

    # Create toplevel
    set w .fontedit
    toplevel $w -class TkRat
    wm title $w $t(edit_font)
    wm transient $w $parent

    # Top label
    label $w.topl -text $t(use_one_method)

    # Specification method frame
    frame $w.s -bd 1 -relief raised
    radiobutton $w.s.select -variable ${id}(method) -value components \
        -command "::tkrat::fontedit::update_font_spec $id components"
    label $w.s.fl -text $t(family):
    set m $w.s.family.m
    menubutton $w.s.family -bd 1 -relief raised -indicatoron 1 -menu $m \
        -textvariable ${id}(family) -width 15
    menu $m -tearoff 0
    set families [lsort -dictionary [font families]]
    foreach f $families {
	$m add command -label $f -command \
            "set ${id}(family) [list $f]; \
                 ::tkrat::fontedit::update_font_spec $id components"
    }
    FixMenu $m
    label $w.s.sl -text "  $t(size):"
    set m $w.s.size.m
    menubutton $w.s.size -bd 1 -relief raised -indicatoron 1 -menu $m \
        -textvariable ${id}(size) -width 3
    menu $m -tearoff 0
    foreach s {4 5 6 7 8 9 10 11 12 13 14 15 16 18 20 22 24 26 30 36} {
	$m add command -label $s -command \
            "set ${id}(size) $s; \
                 ::tkrat::fontedit::update_font_spec $id components"
    }
    checkbutton $w.s.weight -text "$t(bold) " -onvalue bold -offvalue normal \
        -variable ${id}(weight) \
        -command "::tkrat::fontedit::update_font_spec $id components"
    checkbutton $w.s.italic -text "$t(italic) " -onvalue italic \
        -offvalue roman -variable ${id}(slant) \
        -command "::tkrat::fontedit::update_font_spec $id components"
    checkbutton $w.s.underline -text "$t(underline) " \
        -variable ${id}(underline) \
        -command "::tkrat::fontedit::update_font_spec $id components"
    checkbutton $w.s.overstrike -text $t(overstrike) \
        -variable ${id}(overstrike) \
        -command "::tkrat::fontedit::update_font_spec $id components"

    pack $w.s.select \
        $w.s.fl $w.s.family \
        $w.s.sl $w.s.size \
        $w.s.weight \
        $w.s.italic \
        $w.s.underline \
        $w.s.overstrike -side left -pady 2

    # Name method frame
    frame $w.n -bd 1 -relief raised 
    radiobutton $w.n.select -variable ${id}(method) -value name \
        -command "::tkrat::fontedit::update_font_spec $id name"
    label $w.n.l -text $t(name):
    entry $w.n.e -width 20 -textvariable ${id}(name)
    set hd(updateButton) $w.n.set
    button $w.n.set -text $t(update) \
        -command "::tkrat::fontedit::update_font_spec $id name" -bd 1
    pack $w.n.select \
        $w.n.l \
        $w.n.e \
        $w.n.set -side left -pady 2
    trace variable hd(name) w "::tkrat::fontedit::fix_update_btn $id"
    trace variable hd(method) w "::tkrat::fontedit::fix_update_btn $id"
    fix_update_btn $id

    # Sample text
    message $w.sample -text $t(ratatosk) -aspect 200 -justify left
    set hd(sample) $w.sample

    # Buttons
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"
    set hd(okbutton) $w.buttons.ok

    # Pack things
    pack $w.topl \
        $w.s \
        $w.n -side top -fill x -pady 2 -padx 2
    pack $w.buttons -side bottom -fill x -pady 2 -padx 2
    pack $w.sample -fill x -pady 2 -padx 2

    # Bindings
    bind $w.n.e <Tab> "::tkrat::fontedit::update_font_spec $id name"
    bind $w.n.e <Return> "::tkrat::fontedit::update_font_spec $id name; break"

    # Update sample font
    update_font $id

    # Show window and wait for completion
    ::tkrat::winctl::SetGeometry editFont $w
    ::tkrat::winctl::ModalGrab $w
    pack propagate $w 0
    tkwait variable ${id}(done)

    # Finalization
    ::tkrat::winctl::RecordGeometry editFont $w
    destroy $w
    set pref(opt,$font) $hd(old_spec)
    if {"" != $hd(font_name)} {
	font delete $hd(font_name)
    }
    unset hd
    $l configure -text [ConvertFontToText $pref(opt,$font)] \
        -font [RatCreateFont $pref(opt,$font)]
}

# ::tkrat::fontedit::fix_update_btn --
#
# Set state of the update button
#
# Arguments:
# handler - Handler of font window
# args    - Possibly standard trace args

proc ::tkrat::fontedit::fix_update_btn {handler args} {
    upvar \#0 $handler hd

    if {"" != $hd(name) && "name" == $hd(method)} {
	set state normal
    } else {
	set state disabled
    }
    $hd(updateButton) configure -state $state
}

# ::tkrat::fontedit::update_font_spec --
#
# Update the shown font
#
# Arguments:
# handler - Handler of font window
# method  - which method to use

proc ::tkrat::fontedit::update_font_spec {handler method} {
    upvar \#0 $handler hd

    if {"components" == $method} {
	set hd(new_spec) [list components $hd(family) $hd(size) $hd(weight) \
                              $hd(slant) $hd(underline) $hd(overstrike)]
	set hd(method) components
    } else {
	set hd(new_spec) [list name $hd(name)]
	set hd(method) name
    }
    update_font $handler
}

# ::tkrat::fontedit::update_font --
#
# Update the sample text
#
# Arguments:
# handler - Handler of font window

proc ::tkrat::fontedit::update_font {handler} {
    upvar \#0 $handler hd
    global t

    if {"$hd(new_spec)" == "$hd(old_spec)"} {
	return
    }
    if {[lindex $hd(new_spec) 0] == "components"} {
	if {"" == $hd(font_name)} {
	    set op create
	    set hd(font_name) fontedit
	} else {
	    set op configure
	}
	font $op $hd(font_name) \
            -family [lindex $hd(new_spec) 1] \
            -size -[lindex $hd(new_spec) 2] \
            -weight [lindex $hd(new_spec) 3] \
            -slant [lindex $hd(new_spec) 4] \
            -underline [lindex $hd(new_spec) 5] \
            -overstrike [lindex $hd(new_spec) 6]
	set fn $hd(font_name)
    } else {
	set fn [lindex $hd(new_spec) 1]
    }

    set hd(old_spec) $hd(new_spec)

    if {[catch {$hd(sample) configure -font $fn} err]} {
	set okstatus disabled
	set msg $t(invalid_font)
	set aspect 1000
	$hd(sample) configure -font fixed
    } else {
	set okstatus normal
	set msg $t(ratatosk)
	set aspect 200
    }
    $hd(sample) configure -text $msg -aspect $aspect
    $hd(okbutton) configure -state $okstatus
}
