# rat_find.tcl --
#
# Incremental search module. GUI should be provided by caller.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_find 1.0

namespace eval rat_find {
    namespace export init uninit
}

# rat_find::init --
#
# Initialize a find context and return a handler to it
#
# Arguments:
# find_in	- Text widget to search in
# find_text    	- Text entry containing text to look for
# match_case	- Checkbutton controlling match case
# button_next	- "Find next" button
# button_prev	- "Find previous" button

proc rat_find::init {find_in find_text match_case button_next button_prev} {
    set id rat_find::state$button_next
    upvar \#0 $id hd

    set hd(find_in) $find_in
    set hd(find_text) $find_text
    set hd(find_text_var) [$find_text cget -textvariable]
    set hd(match_case_var) [$match_case cget -variable]
    set hd(button_next) $button_next
    set hd(button_prev) $button_prev
    set hd(start_at) 1.0
    set hd(last_text) ""

    bind $hd(find_text) <Return> "rat_find::next $id 1 ; break"
    upvar \#0 $hd(find_text_var) find_text_val
    trace variable find_text_val w [list rat_find::text_changed $id]

    $hd(button_next) configure -command "rat_find::next $id 1"
    $hd(button_prev) configure -command "rat_find::prev $id"

    return $id
}

# rat_find::uninit --
#
# Destroy a find context
#
# Arguments:
# handler	- Handler to the find context

proc rat_find::uninit {handler} {
    upvar \#0 $handler hd

    bind $hd(find_text) <Return> {}
    $hd(button_next) configure -command ""
    $hd(button_prev) configure -command ""

    upvar \#0 $hd(find_text_var) find_text
    trace vdelete find_text w [list rat_find::text_changed $handler]

    unset hd
}

# rat_find::text_changed --
#
# Called whenever the text to search for has changed.
#
# Arguments:
# handler	- Handler to the find context
# trace args    - Normal variable trace callback arguments 

proc rat_find::text_changed {handler args} {
    upvar \#0 $handler hd
    upvar \#0 $hd(find_text_var) find_text

    set slf [string length $find_text]
    if {0 < $slf} {
        set slp [string length $hd(last_text)]
        for {set m $slp} {$m > 0} {incr m -1} {
            if {0 == [string compare -length $m $find_text $hd(last_text)]} {
                break
            }
        }
        if {$slf > $slp && $slp == $m} {
            next $handler 0
        } elseif {$slf < $slp && $slf==$m && [info exists hd(last_pos,$slf)]} {
            $hd(find_in) tag remove Found 1.0 end
            set p1 [lindex $hd(last_pos,$slf) 0]
            set p2 [lindex $hd(last_pos,$slf) 1]
            $hd(find_in) tag add Found $p1 $p2
            $hd(find_in) see $p1
            set hd(start_at) $p1
        } else {
            set hd(start_at) 1.0
            next $handler 0
        }
        $hd(button_next) configure -state normal
        $hd(button_prev) configure -state normal
    } else {
        $hd(button_next) configure -state disabled
        $hd(button_prev) configure -state disabled
        $hd(find_in) tag remove Found 1.0 end
    }

    set hd(last_text) $find_text
}

# rat_find::next --
#
# Find next instance
#
# Arguments:
# handler	- Handler to the find context

proc rat_find::next {handler adv} {
    upvar \#0 $handler hd
    upvar \#0 $hd(find_text_var) find_text
    upvar \#0 $hd(match_case_var) match_case

    $hd(find_in) tag remove Found 1.0 end
    if {$adv} {
        set start "$hd(start_at) +1c"
    } else {
        set start $hd(start_at)
    }
    if {$match_case} {
        set pos [$hd(find_in) search -count num -- $find_text $start]
    } else {
        set pos [$hd(find_in) search -nocase -count num -- $find_text $start]
    }
    if {"" != $pos} {
        set end [list $pos +${num}c]
        $hd(find_in) tag add Found $pos $end
        $hd(find_in) see $pos
        set hd(last_pos,[string length $find_text]) [list $pos $end]
        set hd(start_at) $pos
    } else {
        bell
        $hd(button_next) configure -state disabled
    }
}


# rat_find::prev --
#
# Find previous instance
#
# Arguments:
# handler	- Handler to the find context

proc rat_find::prev {handler} {
    upvar \#0 $handler hd
    upvar \#0 $hd(find_text_var) find_text
    upvar \#0 $hd(match_case_var) match_case

    $hd(find_in) tag remove Found 1.0 end
    if {$match_case} {
        set pos [$hd(find_in) search -backwards -count num -- \
                     $find_text "$hd(start_at) -1c"]
    } else {
        set pos [$hd(find_in) search -backwards -nocase -count num -- \
                     $find_text "$hd(start_at) -1c"]
    }
    if {"" != $pos} {
        set end [list $pos +${num}c]
        $hd(find_in) tag add Found $pos $end
        $hd(find_in) see $pos
        set hd(start_at) $pos
    } else {
        bell
        $hd(button_next) configure -state disabled
    }
}
