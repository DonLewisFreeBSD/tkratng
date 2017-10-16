# rat_scrollframe.tcl
#
# Create a frame which adds scrollbars if needed

package provide rat_scrollframe 1.0

namespace eval rat_scrollframe {
}

# rat_scrollframe::create
#
# Creates the scrollframe
#
# Arguments:
# The same as the frame widget

proc rat_scrollframe::create {w args} {
    eval [concat frame $w $args]
    global rat_scrollframe::$w

    scrollbar $w.scrolly -relief sunken -command "$w.c yview" \
	-highlightthickness 0 -bd 1 -takefocus 0
    scrollbar $w.scrollx -relief sunken -command "$w.c xview" \
	-highlightthickness 0 -orient horizontal -bd 1 -takefocus 0
    canvas $w.c -yscrollcommand "$w.scrolly set" \
	-xscrollcommand "$w.scrollx set" \
	-highlightthickness 0 -selectborderwidth 0 -takefocus 0
    frame $w.c.f
    set rat_scrollframe::$w [$w.c create window 0 0 -anchor nw -window $w.c.f]

    grid $w.c -sticky nsew
    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 0 -weight 1

    bind $w.c <Configure> "rat_scrollframe::configure_event $w"
    return $w.c.f
}

# rat_scrollframe::recalc
#
# Recalculates the bounding box
#
# Arguments:
# The name of the window


proc rat_scrollframe::recalc {w} {
    upvar #0 rat_scrollframe::$w id

    update
    $w.c itemconfigure $id -height [winfo reqheight $w.c.f] \
	-width [winfo reqwidth $w.c.f]
    configure_event $w
}

# rat_scrollframe::config_event
#
# Handles a configuration event
#
# Arguments:
# The name of the window

proc rat_scrollframe::configure_event {w} {
    upvar #0 rat_scrollframe::$w id

    $w.c configure -scrollregion \
	[list 0 0 [winfo reqwidth $w.c.f] [winfo reqheight $w.c.f]]
    # Check if we need vertical scrollbar
    if {[winfo reqheight $w.c.f] <= [winfo height $w.c]} {
	grid forget $w.scrolly
	$w.c itemconfigure $id -height [winfo height $w.c]
    } else {
	grid $w.scrolly -column 1 -row 0 -sticky ns
    }
    # Check if we need horizontal scrollbar
    if {[winfo reqwidth $w.c.f] <= [winfo width $w.c]} {
	grid forget $w.scrollx
	$w.c itemconfigure $id -width [winfo width $w.c]
    } else {
	grid $w.scrollx -column 0 -row 1 -sticky ew
    }
}
