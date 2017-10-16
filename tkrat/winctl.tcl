#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

namespace eval ::tkrat::winctl {
    namespace export Place Size GetPane SetGeometry RecordGeometry

    variable init 0

    # True if data has been modified
    variable needSave 0

    # Default window sizes
    variable defaultSize
    array set defaultSize {
        folderWindow    {580 710}
        addrBookDelete	{ 20  10}
        alias   	{ 80  20}
        groupMessages   { 80  40}
        vFolderDef	{600 540}
        compose	    	{ 80  24}
        composeChoose	{400 400}
        dbCheckW	{ 80  10}
        ratText	    	{ 80  20}
        sendBug	    	{ 80  20}
        keydef  	{470 500}
        showSource	{ 80  30}
        vFolderWizard   {440 400}
        testImportResult { 30  15}
        ispell          { 20   7}
        watcher	    	{ 60  10}
        seeLog	    	{ 80  20}
        expList	    	{ 30  10}
        giveCmd	    	{ 80   4}
        cmdList	    	{ 20  10}
        extView	    	{ 80  40}
        aliasChooser    { 30  15}
        pgpError	{ 60  12}
        pgpGet	    	{ 80  40}
        help	        { 80  40}
        showGH	    	{ 80  10}
        editorList	{ 30  10}
        prefTree	{120 200}
        preferences     {400 350}
        firstUseWizard  {400 350}
        firstUseAdv     {400 500}
        fbox            {500 200}
    }
    # Compatibility mapping (new_name old_name)
    variable savedSizeTrans
    array set savedSizeTrans {
        groupMessages gFolderL
        alias aliasList
        addrBookDelete bookList
        composeChoose msgList
        dbCheckW dbcheckList
        help heltext
        sendBug bugText
        keydef keyCanvas
        preferences prefPane
        showSource source
        vFolderWizard wizardBody
        testImportResult testImportList
    }

    # Default pane factors
    variable defaultPane
    array set defaultPane {
        folderPane 0.35
        vFolderDef 0.35
    }
    variable paneIdTrans
    # Compatibility mapping (new_name old_name)
    array set paneIdTrans {
        vFolderDef vFolderPane
        folderWindow folderPane
    }

    # For placement
    variable savedPos
    # Compatibility mapping (new_name old_name)
    variable savedPosTrans
    array set savedPosTrans {
        groupMessages groupMessageList
        folderWindow folder
        firstUseWizard vFolderWizard
        firstUseAdv vFolderWizardAdv
    }
}

# ::tkrat::winctl::Place --
#
# Place a window just as the user left it last time it was used.
#
# Arguments:
# id - Identifier
# w  - Window to place

proc ::tkrat::winctl::Place {id w} {
    variable savedPos
    variable savedPosTrans
    variable origPos
    global option

    if {[info exists savedPosTrans($id)]
        && [info exists savedPos($savedPosTrans($id))]} {
        set savedPos($id) $savedPos($savedPosTrans($id))
        unset savedPos($savedPosTrans($id))
    }
    if {[info exists savedPos($id)] && $option(keep_pos)} {
	wm geom $w $savedPos($id)
    } else {
	switch $id {
	folder	{ set origPos($id) $option(main_geometry) }
	compose	{ set origPos($id) $option(compose_geometry) }
	watcher	{ set origPos($id) $option(watcher_geometry) }
	default {
		wm withdraw $w
		update idletasks
		set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
			- [winfo vrootx [winfo parent $w]]}]
		set y [expr {[winfo screenheight $w]/2-[winfo reqheight $w]/2 \
			- [winfo vrooty [winfo parent $w]]}]
		set origPos($id) +$x+$y
	    }
	}
	wm geom $w $origPos($id)
	# The geometry may be expressed with minuses.
	regsub {[0-9]+x[0-9]+} [wm geom $w] {} origPos($id)
    }
}

# ::tkrat::winctl::RecordPos --
#
# Record a window's position
#
# Arguments:
# id - Identifier
# w  - Window to place

proc ::tkrat::winctl::RecordPos {id w} {
    variable savedPos
    variable origPos
    variable needSave
    global option

    # Should we really do this?
    if {$option(keep_pos)} {
    } else {
	return
    }

    # Get geometry and make sure it is within limits
    catch {
        regsub {[0-9]+x[0-9]+} [wm geom $w] {} geom
        regsub {[0-9]+x[0-9]+([-+]+)([0-9]+)([-+]+)([0-9]+)} \
	    [wm geom $w] {\1 \2 \3 \4} geom
        set x [lindex $geom 1]
        if {[regexp {[-+]-} [lindex $geom 0]]} {
            set width [winfo width $w]
        } else {
            set width [winfo screenwidth $w]
        }
    }
    if {![info exists width]} {
        return
    }
    while {$x > $width} {
	incr x -$width
    }
    set y [lindex $geom 3]
    if {[regexp {[-+]-} [lindex $geom 0]]} {
	set height [winfo height $w]
    } else {
	set height [winfo screenheight $w]
    }
    while {$y > $height} {
	incr y -$height
    }
    set geom "[lindex $geom 0]${x}[lindex $geom 2]${y}"
    if {[info exists origPos($id)]} {
	if {[string compare $origPos($id) $geom]} {
	    set savedPos($id) $geom
	    set needSave 1
	}
	unset origPos($id)
    } elseif {[info exists savedPos($id)]} {
	if {[string compare $savedPos($id) $geom]} {
	    set savedPos($id) $geom
	    set needSave 1
	}
    } else {
	set savedPos($id) $geom
	set needSave 1
    }
}

# ::tkrat::winctl::Size --
#
# Resize a listbox or text
#
# Arguments:
# id - The identifier
# w  - The window handler for this listbox/text

proc ::tkrat::winctl::Size {id w} {
    variable defaultSize
    variable savedSize
    variable savedSizeTrans
    variable sizeChar
    variable sizeBorder
    global option

    if {[info exists savedSizeTrans($id)]
        && [info exists savedSize($savedSizeTrans($id))]} {
        set savedSize($id) $savedSize($savedSizeTrans($id))
        unset savedSize($savedSizeTrans($id))
    }

    if {[info exists savedSize($id)] && $option(keep_pos)
        && 0 != [lindex $savedSize($id) 0]
        && 0 != [lindex $savedSize($id) 1]} {
	set width [lindex $savedSize($id) 0]
	set height [lindex $savedSize($id) 1]
    } else {
        if {[info exists defaultSize($id)]} {
            set s $defaultSize($id)
        } else {
            set s {200 200}
        }
        set width [lindex $s 0]
        set height [lindex $s 1]
    }
    $w configure -width $width -height $height
    # Remembering information of this window:
    #  sizeBorder -    the size of the total borders in one axis (in pixels)
    #  sizeChar   -    the size of each character
    set bd [expr {2*([$w cget -borderwidth] \
	    +[$w cget -highlightthickness])}]
    set sizeBorder($id) $bd
    set sizeChar($id) [list [expr {([winfo reqwidth $w]-$bd)/$width}] \
	    [expr {([winfo reqheight $w]-$bd)/$height}]]
    if {0 == [lindex $sizeChar($id) 0] || 0 == [lindex $sizeChar($id) 1]} {
	set sizeChar($id) [list 1 1]
    }
}

# R::tkrat::winctl::RecordSize --
#
# Remember the size of an listbox or text
#
# Arguments:
# id - The identifier
# w  - The window handler for this listbox/text

proc ::tkrat::winctl::RecordSize {id w} {
    variable needSave
    variable defaultSize
    variable savedSize
    variable sizeChar
    variable sizeBorder
    global option

    # Should we really do this?
    if {$option(keep_pos)} {
    } else {
	return
    }

    set val [list \
	    [expr {([winfo width $w]-$sizeBorder($id))/ \
	    [lindex $sizeChar($id) 0]}] \
    	    [expr {([winfo height $w]-$sizeBorder($id))/ \
	    [lindex $sizeChar($id) 1]}]]

    # Ignore unmapped windows
    if { 0 >= [lindex $val 0] || 0 >= [lindex $val 1]} {
	return
    }
    if {[info exists defaultSize($id)]} {
        if {[lindex $val 0] != [lindex $defaultSize($id) 0]
            || [lindex $val 1] != [lindex $defaultSize($id) 1]} {
	    set savedSize($id) $val
	    set needSave 1
	}
    } elseif {[info exists savedSize($id)]} {
        if {[lindex $val 0] != [lindex $savedSize($id) 0]
            || [lindex $val 1] != [lindex $savedSize($id) 1]} {
	    set savedSize($id) $val
	    set needSave 1
	}
    } else {
	set savedSize($id) $val
	set needSave 1
    }
}

# ::tkrat::winctl::GetPane --
#
# Get a pane value
#
# Arguments:
# id - The identifier

proc ::tkrat::winctl::GetPane {id} {
    variable defaultPane
    variable paneIdTrans
    variable savedPane

    if {[info exists savedPane($id)]} {
        set v $savedPane($id)
    } elseif {[info exists paneIdTrans($id)]
              && [info exists savedPane($paneIdTrans($id))]} {
        set v $savedPane($paneIdTrans($id))
        unset savedPane($paneIdTrans($id))
    }
    if {![info exists v]
	|| $v < 0.01 || $v > 0.99} {
        if {[info exists defaultPane($id)]} {
            set v $defaultPane($id)
        } elseif {[info exists defaultPane($paneIdTrans($id))]} {
            set v $defaultPane($paneIdTrans($id))
        } else {
            set v 0.35
        }
	set savedPane($id) $v
    } 
    return $v 
}

# ::tkrat::winctl::RecordPane --
#
# Record the pane value
#
# Arguments:
# id   - The identifier
# pane - Pane value

proc ::tkrat::winctl::RecordPane {id pane} {
    variable needSave
    variable savedPane
    global option

    # Should we really do this?
    if {$option(keep_pos)} {
    } else {
	return
    }
    if {$savedPane($id) != $pane} {
	set savedPane($id) $pane
	set needSave 1
    }
}

# ::tkrat::winctl::SavePos --
#
# Save positions
#
# Arguments:

proc ::tkrat::winctl::SavePos {} {
    variable savedPos
    variable savedSize
    variable savedPane
    variable needSave
    global option

    if { 0 != $needSave} {
	# Just return on errors
	if {[catch {open $option(placement) w} f]} {
	    return
	}
	foreach p [array names savedPos] {
	    puts $f "set savedPos($p) [list $savedPos($p)]"
	}
	foreach p [array names savedSize] {
	    puts $f "set savedSize($p) [list $savedSize($p)]"
	}
	foreach p [array names savedPane] {
	    puts $f "set savedPane($p) [list $savedPane($p)]"
	}
	close $f
	set needSave 0
    }
}

# ::tkrat::winctl::ReadPos --
#
# Read saved window positions
#
# Arguments:

proc ::tkrat::winctl::ReadPos {} {
    variable savedPos
    variable savedSize
    variable savedPane
    variable needSave
    global option

    if {[file readable $option(placement)]} {
	source $option(placement)
    }
    foreach m {{ratPlace savedPos} {ratSize savedSize}} {
        set o [lindex $m 0]
        set n [lindex $m 1]
        if {[array exists $o]} {
            foreach e [array names $o] {
                set ${n}($e) [set ${o}($e)]
            }
            unset $o
        }
    }
    set needSave 0
}

# RecordGeometry --
#
# Record all stuff about a windows geometry. Use this function instead of
# RecordPos, RecordSIze, RecordPane and SavePos
#
# Arguments:
# id    - Identity defining the window
# wpos  - Window to get position from
# wsize - Window to get size from (empty if no size should be measured)
# wpane - Window to record pane factor of (empty if no pane should be measured)

proc ::tkrat::winctl::RecordGeometry {id wpos {wsize {}} {wpane {}}} {
    RecordPos $id $wpos
    if {$wsize != ""} {
        RecordSize $id $wsize
    }
    if {$wpane != ""} {
        RecordPane $id $wpane
    }
    SavePos
}

# SetGeometry --
#
# Restores the geometry from saved values.
#
# Arguments:
# id    - Identity defining the window
# wpos  - Window to set position of
# wsize - Window to set size of (empty if no size should be set)

proc ::tkrat::winctl::SetGeometry {id wpos {wsize {}}} { 
    if {$wsize != ""} {
        Size $id $wsize
    }
    Place $id $wpos
    wm deiconify $wpos
}

# ::tkrat::winctl::ModalGrab --
#
# This is the second step in making a toplevel window modal.
#
# Arguments:
# w - window to make modal
# parent - Parent window (optional)

proc ::tkrat::winctl::ModalGrab {w {newFocus {}}} {
    set oldFocus [focus]
    if {"" == $oldFocus} {
	set unfocus ""
    } else {
	set unfocus "catch {focus -force $oldFocus}"
    }


    set ungrab ""
    catch {
	set oldGrab [grab current $w]
	if {$oldGrab != ""} {
	    set grabStatus [grab status $oldGrab]
	}
	tkwait visibility $w
	if {"" != $newFocus} {
	    focus -force $newFocus
	}
	grab $w
    }

    if {$oldGrab != ""} {
	if {$grabStatus == "global"} {
	    set ungrab "grab -global $oldGrab"
	} else {
	    set ungrab "grab $oldGrab"
	}
    }

    bind $w <Destroy> \
	    "+; if {\"%W\" == \"$w\"} {grab release $w; $unfocus; $ungrab}"
}

# ::tkrat::winctl::InstallMnemonic --
#
# Mark and bind a mnemonic
#
# Arguments:
# l - Label to show mnemonic in
# i - Index of char to use
# w - Widget to focus when mnemonic is triggered

proc ::tkrat::winctl::InstallMnemonic {l i w} {
    $l configure -underline $i
    set c [string index [$l cget -text] $i]
    bind [winfo toplevel $w] <Alt-$c> [list focus $w]
}
