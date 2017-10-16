# rat_ed.tcl --
#
# This file contains the code which implements the enabledisable command
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_ed 1.0

namespace eval rat_ed {
    namespace export enabledisable enable disable
    variable disabledFg #a3a3a3
    variable enabledFg Black
}

# rat_ed::enabledisable --
#
# Enables or disables all widgets under the given one
#
# Arguments:
# ed	- true if we should enable
# w	- name of the parent widget

proc rat_ed::enabledisable {ed w} {
    variable disabledFg
    variable enabledFg

    if {$ed} {
	set state normal
	set fg $enabledFg
    } else {
	set state disabled
	set fg $disabledFg
    }
    foreach c [winfo children $w] {
	if {[llength [winfo children $c]]} {
	    rat_ed::enabledisable $ed $c
	}
	if {![catch {$c cget -state}]} {
	    $c configure -state $state
	}
	$c configure -foreground $fg
    }
}


# rat_ed::enable --
#
# Enables a widget
#
# Arguments:
# w	- name of the widget

proc rat_ed::enable {w} {
    variable enabledFg

    if {![catch {$w cget -state}]} {
	$w configure -state normal
    }
    if {"" != [option get . *foreground Color]} {
	set enabledFg [option get . *foreground Color]
    }
    $w configure -foreground $enabledFg
}


# rat_ed::disable --
#
# Disables a widget
#
# Arguments:
# w	- name of the widget

proc rat_ed::disable {w} {
    variable disabledFg

    if {![catch {$w cget -state}]} {
	$w configure -state disabled
    }
    if {"" != [option get . *disabledForeground Color]} {
	set disabledFg [option get . *disablednabledForeground Color]
    }
    $w configure -foreground $disabledFg
}
