# balloon.tcl
#
#	This file implements the balloon help system
#	It is loosely based on code by Jeffrey Hobbs
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
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
    variable text ""
    variable winsVar ""
    variable textsVar ""

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
    bind all <Any-Motion> {+
    if {$option(show_balhelp)} {
	    rat_balloon::Hide
	    if {"Menu" == [winfo class %W]} {
		set rat_balloon::last -1
		set cur [%W index active]
		if {[info exists ${rat_balloon::winsVar}(%W,$cur)]} {
		    set rat_balloon::afterID [after $option(balhelp_delay) \
			    [list rat_balloon::Show %W $cur]]
		}
	    } elseif [info exists ${rat_balloon::winsVar}(%W)] {
		set rat_balloon::afterID [after $option(balhelp_delay) \
					      [list rat_balloon::Show %W]]
	    }
	}
    }
    bind all <Leave>		    {+rat_balloon::Hide }
    bind Balloons <Any-KeyPress>    {+rat_balloon::Hide }
    bind Balloons <Any-Button>      {+rat_balloon::Hide }

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
    variable text
    variable last
    variable last_text
    variable winsVar
    variable textsVar
    upvar #0 $winsVar wins
    upvar #0 $textsVar texts

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

    after cancel $afterID
    catch {wm withdraw $toplevel}
}
