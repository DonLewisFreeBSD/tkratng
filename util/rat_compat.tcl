# rat_compat.tcl --
#
# Contains code emulating newver tcl features for older versions.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_compat 1.0

namespace eval rat_compat {
    namespace export labelframe init8_3
}

# rat_compat::labelframe --
#
# Creates a labeled frame
#
# Arguments:
# w	- name of frame to create
# args  - extra arguments

proc rat_compat::labelframe {w args} {
    frame $w -bd 2 -relief ridge

    array set args_array $args
    foreach a [array names args_array] {
        if {"-text" == $a} {
            label $w.labelframe_label -text $args_array($a) -anchor w
            pack $w.labelframe_label -side top -fill x
        } else {
            $w configure $ $args_array($a)
        }
    }
    return $w
}

# rat_compat::init8_3 --
#
# Setup compatibility fro tcl/tk 8.3
#
# Arguments:

proc rat_compat::init8_3 {} {
    rename rat_compat::labelframe ::labelframe
}
