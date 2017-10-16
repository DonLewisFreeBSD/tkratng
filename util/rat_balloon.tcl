# balloon.tcl
#
#	This file implements the balloon help system
#	It is loosely based on code by Jeffrey Hobbs
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_balloon 1.0

namespace eval rat_balloon {
    namespace export Init

    variable last -1
    variable last_text -1
    variable afterID {}
    variable enabled 1
    variable toplevel .__balloonHelp
    variable toplevel_visible 0
    variable text ""
    variable winsVar ""
    variable textsVar ""
    variable ignore 0

    option add *TkRat*ballonBackground #fefeb4 widgetDefault
    option add *TkRat*ballonForeground black widgetDefault
}

# rat_balloon::Init --
#
#	Initialize the balloon help module
#
# Arguments:
#	wins		Name of text spcifier array
#	texts		Name of texts array
# Results:
#	Initializes Enter and Leave bindings for all widgets.

proc rat_balloon::Init {wins texts} {
    variable winsVar $wins
    variable textsVar $texts
    variable toplevel

    # Prepare bindings
    bind all <Any-Motion>           {+rat_balloon::Event %W %y}
    bind all <Any-Button>           {+rat_balloon::Event %W %y}
    bind all <Any-Key>              {+rat_balloon::Event %W %y}
    bind all <Any-MouseWheel>       {+rat_balloon::Event %W %y}
    bind all <Enter>                {+rat_balloon::Event %W %y}
    bind all <Leave>		    {+rat_balloon::Hide}
    bind Balloons <Any-Key>         {+rat_balloon::Hide}
    bind Balloons <Any-Button>      {+rat_balloon::HideW}

    # Create the actual balloon
    toplevel $toplevel -bd 1 -class TkRat
    set fg [option get $toplevel ballonForeground Color]
    set bg [option get $toplevel ballonBackground Color]
    wm overrideredirect $toplevel 1
    wm positionfrom $toplevel program
    wm withdraw $toplevel
    label $toplevel.l -highlightthickness 0 -bd 0 \
	    -background $bg -foreground $fg \
	    -textvariable rat_balloon::text \
	    -justify left -padx 2 -pady 2
    pack $toplevel.l
}


# rat_balloon::Event --
#
#       Handles incoming events
#
# Arguments:
#       w - window of event
#       y - position of event
#
# Results:
#       Resets the timers

proc rat_balloon::Event {w y} {
    variable winsVar
    variable afterID
    variable last
    variable ignore
    upvar \#0 $winsVar wins
    global option

    if {$ignore} {
	return
    }

    catch {
	if {$option(show_balhelp)} {
	    rat_balloon::Hide
	    if {"Menu" == [winfo class $w]} {
		set last -1
		set cur [$w index @$y]
		if {[info exists wins($w,$cur)]} {
		    set afterID [after $option(balhelp_delay) \
				     [list rat_balloon::Show $w $cur]]
		}
	    } elseif [info exists wins($w)] {
		set afterID [after $option(balhelp_delay) \
				 [list rat_balloon::Show $w]]
	    }
	}
    }
}

# rat_balloon::Show --
#
#	Show the help balloon
#
# Arguments:
#	w	Window to show help for
#	i	Index in window to show the help for
# Results:
#	Sets the helptext, shows the balloon and adds bindings to the window
#	if not already there

proc rat_balloon::Show {w {i {}}} {
    if {![winfo exists $w] || [string compare \
	    $w [eval winfo containing [winfo pointerxy $w]]]} return

    variable toplevel
    variable toplevel_visible
    variable text
    variable last
    variable last_text
    variable winsVar
    variable textsVar
    upvar \#0 $winsVar wins
    upvar \#0 $textsVar texts

    if {[string compare {} $i]} {
	set text $texts($wins($w,$i))
    } else {
	set text $texts($wins($w))
	if {$last == $w && $last_text == $text} {
	    return
	}
	set last $w
	set last_text $text
    }

    update idletasks
    set b $toplevel
    set x [expr {[winfo pointerx $w]+16}]
    set y [expr {[winfo pointery $w]+10}]
    if {$x<0} {
        set x 0
    } elseif {($x+[winfo reqwidth $b])>[winfo screenwidth $w]} {
        set x [expr {[winfo screenwidth $w]-[winfo reqwidth $b]}]
    }
    wm geometry $b +$x+$y
    wm deiconify $b
    raise $b

    if {-1 == [lsearch [bindtags $w] Balloon]} {
	bindtags $w [linsert [bindtags $w] end Balloon]
    }
    set f [focus]
    if {"" != $f && -1 == [lsearch [bindtags $f] Balloon]} {
	bindtags $f [linsert [bindtags $f] end Balloon]
    }
    set toplevel_visible 1
}


# rat_balloon::Hide --
#
#	Hide the help balloon
#
# Arguments:
#	None
# Results:
#	Withdraws the ballon window and cancels any pending shows.

proc rat_balloon::Hide {} {
    variable afterID
    variable toplevel
    variable toplevel_visible
    variable ignore

    if {$ignore} {
	return
    }
    if {"" != $afterID} {
	after cancel $afterID
	set afterID ""
    }
    if {0 != $toplevel_visible} {
	catch {wm withdraw $toplevel}
	set toplevel_visible 0
    }
}

# rat_balloon::SetIgnore --
#
#       Tells the ballon kit if it shoudl ignore events or not. This is useful
#       if one wants to do stuff to the window system which do not affec the
#       ballon kit.
#
# Arguments
#       value = The ignore value (true means ignore events)

proc rat_balloon::SetIgnore {value} {
    variable ignore

    if {$value} {
	set ignore 1
    } else {
	after idle set rat_balloon::ignore 0
    }
}
