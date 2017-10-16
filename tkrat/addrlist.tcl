# addrlist.tcl --
#
# This file contains the code which handles the list of possible
# address auto completions.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Global state variables
set addrlist_loaded 0

# The address list is stored as a simple list like this:
#  email1
#  name1
#  email2
#  name2
#  ...
# Unknown names are expressed as empty strings.
set addrlist {}

# AddrListAdd --
#
# Add addresses to the list of remembered addresses (if enabled)
#
# Arguments:
# addrs - One or more addresses to add

proc AddrListAdd {addrs} {
    global option addrlist_loaded addrlist

    if {0 == $option(num_autocomplete_addr)} {
        return
    }

    set alist [RatSplitAdr $addrs]
    if {0 == [llength $alist]} {
        return
    }

    if {0 == $addrlist_loaded} {
        LoadAddrList
    }
    set additions {}
    foreach al $alist {
        set a [RatCreateAddress -nodomain $al]
        set name [$a get name]
        set i [lsearch -exact $addrlist [$a get mail]]
        if {$i != -1} {
            if {"" == $name} {
                set name [lindex $addrlist [expr $i+1]]
            }
            set addrlist [lreplace $addrlist $i [expr $i+1]]
        }
        lappend additions [$a get mail]
        lappend additions $name
    }
    set addrlist [concat $additions [lrange $addrlist 0 [expr ($option(num_autocomplete_addr)-[llength $alist])*2-1]]]
}

# LoadAddrList --
#
# Load the address list from disk
#
# Arguments:

proc LoadAddrList {} {
    global addrlist option addrlist_loaded

    if {[file readable $option(ratatosk_dir)/addrlist]} {
        set fh [open $option(ratatosk_dir)/addrlist r]
        fconfigure $fh -encoding utf-8
        set data [read $fh]
        close $fh
        eval $data
    }
    set addrlist_loaded 1
}

# SaveAddrList --
#
# Save the address list on disk
#
# Arguments:

proc SaveAddrList {} {
    global addrlist option

    set fh [open $option(ratatosk_dir)/addrlist w]
    fconfigure $fh -encoding utf-8
    puts $fh "set addrlist [list $addrlist]"
    close $fh
}

# GetMatchingAddrs --
#
# Get any addresses whose first characters of the email or name portion
# matches the given argument (non case sensitive match)
#
# Arguments:
# match - String to match
# max   - Maximum number of entries to return

proc GetMatchingAddrs {match max} {
    global addrlist option addrlist_loaded

    if {0 == $addrlist_loaded} {
        LoadAddrList
    }

    return [RatGetMatchingAddrsImpl $addrlist $match $max]
}

############################################################################
# Routines for handling the popup of an autocomplete list

# AddrListInit --
#
# Initialize the popup list stuff
#
# Arguments:
# w - text widget to use

proc AddrListInit {w} {
    upvar \#0 _addrList$w hd

    set hd(start_pos) {}
    set hd(showing) 0
    bind $w <KeyRelease> {AddrListHandleKeyRelease %W}
    bind $w <Destroy> {+;AddrListHandleDestroy %W}
    bind $w <Return> {break}
    foreach k {Up Down Return space} {
        bind $w <KeyRelease-$k> {if [AddrListHandleListKey %W %K] break}
    }
}

# AddrListHandleDestroy --
#
# Clean up when the parent is destroyed
#
# Arguments:
# w	  - The text widget

proc AddrListHandleDestroy {w} {
    upvar \#0 _addrList$w hd

    if {[info exists hd(w)]} {
        destroy $hd(w)
    }
    unset hd
}

# AddrListAlias --
#
# Get matching aliases
#
# Arguments:
# match - String to match
# max   - Maximum number of entries to return

proc AddrListAlias {match max} {
    RatAlias list aliases nocase
    set result {}
    foreach n [lrange [array names aliases "$match*"] 0 [expr $max - 1]] {
        set content [lindex $aliases($n) 2]
        if {-1 != [string first "," $content]} {
            lappend result $content
        } else {
            lappend result "[lindex $aliases($n) 1] <$content>"
        }
    }
    return $result
}

# AddrListHandleKeyRelease --
#
# Handle KeyRelease events
#
# Arguments:
# w	  - The text widget

proc AddrListHandleKeyRelease {w} {
    upvar \#0 _addrList$w hd
    global option

    if {!$option(show_autocomplete)} {
        return
    }

    # Find start of the current address
    set start [$w search -backwards "," insert "insert linestart"]
    if {"" != $start} {
        set start [$w index "$start +1c"]
    } else {
        set start [$w index "insert linestart"]
    }
    while {[$w compare $start < end+1c]} {
        set c [$w get $start]
        if {" " == $c || "\t" == $c} {
            set start [$w index $start+1c]
        } else {
            break
        }
    }
    set hd(end_pos) [$w search "," $start "$start lineend"]
    if {"" == $hd(end_pos)} {
        set hd(end_pos) [$w index "$start lineend"]
    }
    set addr [string trim [$w get $start $hd(end_pos)]]

    if {"" == $addr} {
        if {[info exists hd(w)]} {
            wm withdraw $hd(w)
            set hd(showing) 0
            set hd(start_pos) {}
        }
        return
    }
    set matches [GetMatchingAddrs $addr \
                     $option(automplete_addr_num_suggestions)]
    set rem [expr $option(automplete_addr_num_suggestions) - [llength $matches]]
    if {$rem > 0} {
        foreach a [AddrListAlias $addr $rem] {
            lappend matches $a
        }
    }

    if {0 == [llength $matches]} {
        if {[info exists hd(w)]} {
            wm withdraw $hd(w)
            set hd(showing) 0
            set hd(start_pos) {}
        }
        return
    }

    if {$start != $hd(start_pos)} {
        if {![info exists hd(w)]} {
            AddrListCreatePopup $w
        }
        set bbox [$w bbox $start]
        set x [expr [lindex $bbox 0] + [winfo rootx $w]]
        set y [expr [lindex $bbox 1] + [lindex $bbox 3] + [winfo rooty $w]]
        wm geometry $hd(w) +$x+$y
        wm deiconify $hd(w)
        set hd(showing) 1
    }
    set hd(start_pos) $start
    $hd(list) delete 0 end
    foreach m $matches {
        $hd(list) insert end $m
    }
    if {[$hd(list) size] > 10} {
        $hd(list) configure -height 10
        if {![winfo ismapped $hd(scroll)]} {
            pack $hd(scroll) -side right -fill y
        }
    } else {
        $hd(list) configure -height [llength $matches]
        if {[winfo ismapped $hd(scroll)]} {
            pack forget $hd(scroll)
        }
    }
}

# AddrListCreatePopup --
#
# Create the popup window
#
# Arguments:
# w	  - The text widget

proc AddrListCreatePopup {w} {
    upvar \#0 _addrList$w hd
    global idCnt

    set hd(w) .addrlist[incr idCnt]
    toplevel $hd(w) -bd 1 -class TkRat
    wm overrideredirect $hd(w) 1
    wm transient $hd(w) $w
    wm positionfrom $hd(w) program
    wm withdraw $hd(w)

    set hd(list) $hd(w).l
    set hd(scroll) $hd(w).s
    scrollbar $hd(scroll) \
        -relief raised \
        -bd 1 \
        -highlightthickness 0 \
        -command "$hd(list) yview"
    listbox $hd(list) \
        -width 40 \
        -relief raised \
        -bd 1 \
        -exportselection false \
        -selectmode single \
        -highlightthickness 0 \
        -selectmode single \
        -yscroll "$hd(scroll) set"
    pack $hd(list) -expand 1 -fill both -side left

    bind $hd(w) <FocusIn> [list focus $w]
    bind $hd(list) <<ListboxSelect>> [list AddrListSelect $w]
}

# AddrListSelect --
#
# Handle list selection events
#
# Arguments:
# w	  - The text widget

proc AddrListSelect {w} {
    upvar \#0 _addrList$w hd

    set sel [$hd(list) curselection]
    if {1 != [llength $sel]} {
        return
    }
    $w mark set insert $hd(end_pos)
    $w delete $hd(start_pos) $hd(end_pos)
    $w insert $hd(start_pos) [$hd(list) get $sel]
    $w see insert
    wm withdraw $hd(w)
    set hd(showing) 0
    set hd(start_pos) {}
    focus $w
}

# AddrListClose --
#
# Close the popup window. Returns '1' if the close event can be ignored
#
# Arguments:
# w	  - The text widget
# force   - Force closure of address list window

proc AddrListClose {w force} {
    upvar \#0 _addrList$w hd
    set f [focus]

    if {!$force
        && [info exists hd(w)]
        && ($f == $hd(list) || $f == $hd(list) || $f == $hd(list))} {
        return 1
    }
    if {$force
        || ([info exists hd(w)]
            && $f != ""
            && $f != $hd(list)
            && $f != $hd(w)
            && $f != $w)} {
        if {[info exists hd(w)]} {
            wm withdraw $hd(w)
            set hd(showing) 0
        }
        set hd(start_pos) {}
    }
    return 0
}

# AddrListHandleListUp --
#
# Handle cursor keys in text widget. If listbox is active move selection.
# Return 1 if the event was handled here
#
# Arguments:
# w	  - The text widget
# key     - Key pressed (Up, Down, Return or space)

proc AddrListHandleListKey {w key} {
    upvar \#0 _addrList$w hd

    # Do not do anything if popup is not shown
    if {!$hd(showing)} {
        return 0
    }

    set sel [$hd(list) curselection]
    if {"space" == $key || "Return" == $key} {
        AddrListSelect $w
    } elseif {"Down" == $key} {
        if {"" == $sel} {
            set sel 0
        } elseif {$sel < [expr [$hd(list) size]-1]} {
            $hd(list) selection clear $sel
            incr sel
        }
        $hd(list) selection set $sel
    } elseif {"" != $sel} {
        $hd(list) selection clear $sel
        if {0 != $sel} {
            incr sel -1
            $hd(list) selection set $sel
        }
    }
    return 1
}
