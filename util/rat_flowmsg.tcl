# rat_flowmsg.tcl
#
# Create a message-widget which fills the available horizontal space

package provide rat_flowmsg 1.0

namespace eval rat_flowmsg {
}

# rat_flowmsg::create
#
# Creates the flowing message
#
# Arguments:
# The same as the message widget

proc rat_flowmsg::create {w args} {
    eval [concat message $w -highlightthickness 0 -bd 0 -padx 0 $args]
    set p [winfo parent $w]
    bind $w <Configure> "$w configure -width \[expr \[winfo width $p\] - 2*(\[$w cget -bd\]+\[$w cget -padx\]+\[$w cget -highlightthickness\])\]"
}
