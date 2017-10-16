# rat_table.tcl
#
# Create a simple table
#

package provide rat_table 1.0

namespace eval rat_table {
    global tcl_version

    namespace export create

    if {$tcl_version >= 8.4} {
        variable arrow_up [image create photo -data {
            R0lGODlhCwAGAIABAAAAAP///yH5BAEKAAEALAAAAAALAAYAAAINjA2nCLnR
            4or00AvvLQA7}]
        variable arrow_down [image create photo -data {
            R0lGODlhCwAGAIABAAAAAP///yH5BAEKAAEALAAAAAALAAYAAAINhI8QieGs
            3GtSnoqjLAA7}]
    }
}

# rat_table::create --
#
# Creates the table
#
# Arguments:
# w        - the name of the window to create
# data     - List of tuples where each tuple consists of the
#            column heanding and column type (int or string)
# rows     - List of rows of data

proc rat_table::create {w data rows args} {
    variable arrow
    upvar \#0 rat_table::$w hd
    set id rat_table::$w

    eval [concat frame $w $args]

    set hd(buttons) {}
    set hd(lists) {}
    set num 0
    set hd(types) {}
    set hd(headings) {}
    foreach d $data {
        lappend hd(headings) [lindex $d 0]
        lappend hd(types) [lindex $d 1]
        button $w.h_$num -text [lindex $d 0] -highlightthickness 0 -bd 1 \
            -command [list rat_table::button_pressed $w $num]
        lappend hd(buttons) $w.h_$num

        set hd(list_$num) {}
        listbox $w.l_$num \
            -bd 0 -highlightthickness 0 \
            -yscrollcommand [list rat_table::yscroll $w $w.l_$num] \
            -selectmode single \
            -exportselection false
        bind $w.l_$num <<ListboxSelect>> [list rat_table::select $w $w.l_$num]
        bind $w.l_$num <Double-1> [list event generate $w <<Action>>]
        lappend hd(lists) $w.l_$num
        incr num
    }

    scrollbar $w.scroll \
        -relief sunken \
        -command "rat_table::yview $w"

    eval [concat grid $hd(buttons) -sticky ew]
    eval [concat grid $hd(lists) $w.scroll -sticky nsew]

    set hd(scrollbar) $w.scroll
    set hd(rows) $rows
    set hd(sort_by) -1
    set hd(sort_incr) 0

    sort $w 0 1

}

# rat_table::yview --
#
# Adjust the view of the listboxes with data from the scrollbar
#
# Arguments:
# w - name of rat_table
# see listbox yview command

proc rat_table::yview {w args} {
    upvar \#0 rat_table::$w hd

    foreach l $hd(lists) {
        eval [concat $l yview $args]
    }
}

# rat_table::yscroll --
#
# Adjust the view of the listboxes with data from a listbox
#
# Arguments:
# w          - name of rat_table
# originator - name of listbox which created this change
# s, e       - set scrollbar 'set' command

proc rat_table::yscroll {w originator s e} {
    upvar \#0 rat_table::$w hd

    $hd(scrollbar) set $s $e

    foreach l $hd(lists) {
        if {$l != $originator} {
            $l yview moveto $s
        }
    }
}

# rat_table::button_pressed --
#
# Called when a button was pressed
#
# Arguments:
# w     - name of rat_table
# num   - index of selected button

proc rat_table::button_pressed {w num} {
    upvar \#0 rat_table::$w hd

    if {$hd(sort_by) == $num} {
        set incr [expr !$hd(sort_incr)]
    } else {
        set incr 1
    }
    sort $w $num $incr
}

# rat_table::sort --
#
# Sort the data and populate listboxes
#
# Arguments:
# w     - name of rat_table
# index - index of column to sort on
# incr  - true if we shoudl sort in increasing order

proc rat_table::sort {w index incr} {
    upvar \#0 rat_table::$w hd

    # Remove old marking
    if {$index != $hd(sort_by) && $hd(sort_by) != -1} {
        remove_sort_mark $w
    }

    # Set new marking
    set hd(sort_by) $index
    set hd(sort_incr) $incr
    show_sort_mark $w

    # Do sorting
    if {"int" == [lindex $hd(types) $index]} {
        set sa -integer
    } else {
        set sa -dictionary
    }
    if {!$incr} {
        lappend sa -decreasing
    }
    set hd(rows) [eval [concat lsort -index $index $sa [list $hd(rows)]]]

    # Empty tables
    foreach l $hd(lists) {
        $l delete 0 end
    }

    # Populate table
    foreach r $hd(rows) {
        set n 0
        foreach d $r {
            if {$n < [llength $hd(lists)]} {
                [lindex $hd(lists) $n] insert end $d
                incr n
            }
        }
    }
}

# rat_table::show_sort_mark --
#
# Add the sort mark to a button
#
# Arguments:
# w     - name of rat_table

proc rat_table::show_sort_mark {w} {
    upvar \#0 rat_table::$w hd
    variable arrow_up
    variable arrow_down

    set but [lindex $hd(buttons) $hd(sort_by)]
    if {[info exists arrow_up]} { # Tk supports -compound
        if {$hd(sort_incr)} {
            set image $arrow_down
        } else {
            set image $arrow_up
        }
        $but configure -compound left -image $image
    } else {
        if {$hd(sort_incr)} {
            set c "v"
        } else {
            set c "^"
        }
        $but configure -text "$c      [lindex $hd(headings) $hd(sort_by)]"
    }
}

# rat_table::remove_sort_mark --
#
# Removes the sort mark from a button
#
# Arguments:
# w     - name of rat_table

proc rat_table::remove_sort_mark {w} {
    upvar \#0 rat_table::$w hd
    variable arrow_up

    set but [lindex $hd(buttons) $hd(sort_by)]
    if {[info exists arrow_up]} { # Tk supports -compound
        $but configure -image {}
    } else {
        $but configure -text [lindex $hd(headings) $hd(sort_by)]
    }
}

# rat_table::select --
#
# Called when the listbox selection changed
#
# Arguments:
# w     - name of rat_table
# orig  - listbox generating event

proc rat_table::select {w orig} {
    upvar \#0 rat_table::$w hd

    set selected [$orig curselection]
    foreach l $hd(lists) {
        if {$l != $orig} {
            $l selection clear 0 end
            $l selection set $selected
        }
    }
    event generate $w <<ListboxSelect>>
}

# rat_table::get_selection --
#
# Returns the currently selected row, or an empty list if no row is selected
#
# Arguments:
# w     - name of rat_table

proc rat_table::get_selection {w} {
    upvar \#0 rat_table::$w hd

    set selected [[lindex $hd(lists) 0] curselection]
    if {-1 == $selected} {
        return {}
    } else {
        return [lindex $hd(rows) $selected]
    }
}
