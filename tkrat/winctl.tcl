#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

# Place --
#
# Place a window just as the user left it last time it was used.
#
# Arguments:
# w  - Window to place
# id - Identifier

proc Place {w id} {
    global ratPlace ratPlaceO option

    if { [info exists ratPlace($id)] && $option(keep_pos)} {
	wm geom $w $ratPlace($id)
    } else {
	switch $id {
	folder	{ set ratPlaceO($id) $option(main_geometry) }
	compose	{ set ratPlaceO($id) $option(compose_geometry) }
	watcher	{ set ratPlaceO($id) $option(watcher_geometry) }
	default {
		wm withdraw $w
		update idletasks
		set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
			- [winfo vrootx [winfo parent $w]]}]
		set y [expr {[winfo screenheight $w]/2-[winfo reqheight $w]/2 \
			- [winfo vrooty [winfo parent $w]]}]
		set ratPlaceO($id) +$x+$y
	    }
	}
	wm geom $w $ratPlaceO($id)
	wm deiconify $w
	# The geometry may be expressed with minuses.
	regsub {[0-9]+x[0-9]+} [wm geom $w] {} ratPlaceO($id)
    }
}

# RecordPos --
#
# Record a window's position
#
# Arguments:
# w  - Window to place
# id - Identifier

proc RecordPos {w id} {
    global ratPlace ratPlaceModified ratPlaceO option

    # Should we really do this?
    if {$option(keep_pos)} {
    } else {
	return
    }
    if {![winfo exists $w]} {
	return
    }

    # Get geometry and make sure it is within limits
    regsub {[0-9]+x[0-9]+} [wm geom $w] {} geom
    regsub {[0-9]+x[0-9]+([-+]+)([0-9]+)([-+]+)([0-9]+)} \
	    [wm geom $w] {\1 \2 \3 \4} geom
    set x [lindex $geom 1]
    if {[regexp {[-+]-} [lindex $geom 0]]} {
	set width [winfo width $w]
    } else {
	set width [winfo screenwidth $w]
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
    if {[info exists ratPlaceO($id)]} {
	if {[string compare $ratPlaceO($id) $geom]} {
	    set ratPlace($id) $geom
	    set ratPlaceModified 1
	}
	unset ratPlaceO($id)
    } elseif {[info exists ratPlace($id)]} {
	if {[string compare $ratPlace($id) $geom]} {
	    set ratPlace($id) $geom
	    set ratPlaceModified 1
	}
    } else {
	set ratPlace($id) $geom
	set ratPlaceModified 1
    }
}

# Size --
#
# Resize a listbox or text
#
# Arguments:
# w  - The window handler for this listbox/text
# id - The identifier

proc Size {w id} {
    global ratSize ratSizeO ratSizeP ratSizeB option

    if {[info exists ratSize($id)] && $option(keep_pos)
    && 0 != [lindex $ratSize($id) 0] && 0 != [lindex $ratSize($id) 1]} {
	set width [lindex $ratSize($id) 0]
	set height [lindex $ratSize($id) 1]
    } else {
	switch $id {
	folderWindow	{set width 580; set height 710}
	watcher		{set width  60; set height  10}
	compose		{set width  80; set height  24}
	source		{set width  80; set height  30}
	seeLog		{set width  80; set height  20}
	vFolderDef	{set width 600; set height 540}
	gFolderL 	{set width  80; set height  10}
	expList		{set width  30; set height  10}
	giveCmd		{set width  80; set height   4}
	cmdList		{set width  20; set height  10}
	dbcheckList	{set width  80; set height  10}
	extView		{set width  80; set height  40}
	aliasChooser	{set width  30; set height  15}
	msgList		{set width 400; set height 400}
	keyCanvas	{set width 470; set height 500}
	pgpError	{set width  60; set height  12}
	ratText		{set width  80; set height  20}
	pgpGet		{set width  80; set height  40}
	aliasList	{set width  80; set height  20}
	aliasText	{set width  30; set height   5}
	bookList	{set width  20; set height  10}
	subjlist	{set width  20; set height   9}
	helptext	{set width  80; set height  40}
	showGH		{set width  80; set height  10}
	editorList	{set width  30; set height  10}
	bugText		{set width  80; set height  20}
	prefTree	{set width 120; set height 200}
	prefPane        {set width 400; set height 250}
	}
	set ratSizeO($id) [list $width $height]
    }
    $w configure -width $width -height $height
    # Remembering information of this window:
    #  ratSizeB	-    the size of the total borders in one axis (in pixels)
    #  ratSizeP -    the size of each character
    set bd [expr {2*([$w cget -borderwidth] \
	    +[$w cget -highlightthickness])}]
    set ratSizeB($id) $bd
    set ratSizeP($id) [list [expr {([winfo reqwidth $w]-$bd)/$width}] \
	    [expr {([winfo reqheight $w]-$bd)/$height}]]
    if {0 == [lindex $ratSizeP($id) 0] || 0 == [lindex $ratSizeP($id) 1]} {
	set ratSizeP($id) [list 1 1]
    }
}

# RecordSize --
#
# Remember the size of an listbox or text
#
# Arguments:
# w  - The window handler for this listbox/text
# id - The identifier

proc RecordSize {w id} {
    global ratSize ratSizeO ratPlaceModified ratSizeP ratSizeB option

    # Should we really do this?
    if {$option(keep_pos)} {
    } else {
	return
    }

    if {![winfo exists $w]} {
	return
    }

    set val [list \
	    [expr {([winfo width $w]-$ratSizeB($id))/ \
	    [lindex $ratSizeP($id) 0]}] \
    	    [expr {([winfo height $w]-$ratSizeB($id))/ \
	    [lindex $ratSizeP($id) 1]}]]

    # Ignore unmapped windows
    if { 0 >= [lindex $val 0] || 0 >= [lindex $val 1]} {
	return
    }
    if {[info exists ratSizeO($id)]} {
	if {[string compare $ratSizeO($id) $val]} {
	    set ratSize($id) $val
	    set ratPlaceModified 1
	}
	unset ratSizeO($id)
    } elseif {[info exists ratSize($id)]} {
	if {[string compare $ratSize($id) $val]} {
	    set ratSize($id) $val
	    set ratPlaceModified 1
	}
    } else {
	set ratSize($id) $val
	set ratPlaceModified 1
    }
}

# GetPane
#
# Get a pane value
#
# Arguments:
# id - The identifier

proc GetPane {id} {
    global ratPane

    if {![info exists ratPane($id)]} {
	switch $id {
	    folderPane {set p 0.35}
	    vFolderPane {set p 0.35}
	}
	set ratPane($id) $p
    } 
    return $ratPane($id)   
}

# RecordPane --
#
# Record the pane value
#
# Arguments:
# pane - Pane value
# id   - The identifier

proc RecordPane {pane id} {
    global ratPane ratPlaceModified option

    # Should we really do this?
    if {$option(keep_pos)} {
    } else {
	return
    }
    if {$ratPane($id) != $pane} {
	set ratPane($id) $pane
	set ratPlaceModified 1
    }
}

# SavePos
#
# Save positions
#
# Arguments:

proc SavePos {} {
    global ratPlace ratPlaceModified option ratSize ratPane

    if { 0 != $ratPlaceModified} {
	# Just return on errors
	if {[catch {open $option(placement) w} f]} {
	    return
	}
	foreach p [array names ratPlace] {
	    puts $f "set ratPlace($p) [list $ratPlace($p)]"
	}
	foreach p [array names ratSize] {
	    puts $f "set ratSize($p) [list $ratSize($p)]"
	}
	foreach p [array names ratPane] {
	    puts $f "set ratPane($p) [list $ratPane($p)]"
	}
	close $f
	set ratPlaceModified 0
    }
}

# ReadPos --
#
# Read saved window positions
#
# Arguments:

proc ReadPos {} {
    global option ratPlace ratSize ratPane ratPlaceModified

    if {[file readable $option(placement)]} {
	source $option(placement)
    }
    set ratPlaceModified 0
}

# ModalGrab --
#
# This is the second step in making a toplevel window modal.
#
# Arguments:
# w - window to make modal
# parent - Parent window (optional)

proc ModalGrab {w {newFocus {}}} {
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
