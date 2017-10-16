# rat_edit.tcl --
#
# This file contains the code which implements tkrat's text edit widget
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_edit 1.1

namespace eval rat_edit {
    namespace export create setWrap state prepareSnapshot \
        maybeStoreSnapshot storeSnapshot setWrap
    global tk_version

    variable undoDepth 10

    if {[info exists tk_version]} {
	foreach e {<Tab> <Control-i> <Insert> <KeyPress>} {
	    bind RatEdit $e {rat_edit::insert_wrap %W %A}
	}
	bind RatEdit <<Paste>> {rat_edit::paste_wrap %W}
	bind RatEdit <<Cut>> {rat_edit::wrap %W insert}
	bind RatEdit <Return> {rat_edit::newline_wrap %W}

        # Mouse pasting should occur at the current insertion point instead of
        # where the mouse currently is
	bind RatEditPreText <<PasteSelection>> \
            {rat_edit::paste_selection %W; break}

        # Delete and BackSpace should only delete the selection if the insert
        # cursor is inside it.
        bind RatEditPreText <Delete> {rat_edit::delete %W insert; break}
        bind RatEditPreText <BackSpace> {rat_edit::delete %W insert-1c; break}

        # Add undo handling to Cut, CutAll, Paste and text insertion
        foreach e {<<Cut>> <<Paste>> <<PasteSelection>>} {
            bind RatEditPreText $e {+;rat_edit::storeSnapshot %W}
        }
        bind RatEditPreText <<CutAll>> {
            %W tag add sel 1.0 end
            rat_edit::delete %W {}
        }
        foreach e {<Tab> <Control-i> <Return> <Insert> <KeyPress>} {
            bind RatEditPreText $e {rat_edit::prepareSnapshot %W}
        }

        # The actual undo & redo functions
        bind RatEditPreText <<RatUndo>> {rat_edit::undo %W}
        bind RatEditPreText <<RatRedo>> {rat_edit::redo %W}

        # tk8.5 added undo/redo to the text widget, disable that
        bind Text <<Undo>> {}
        bind Text <<Redo>> {}

        # Wrap paragraph
        bind RatEditPreText <<Wrap>> {rat_edit::wrap_paragraph %W}
    }
}

# rat_edit::create --
#
# Adds the editor bindings to the given text widget
#
# Arguments:
# w	- name of text widget to convert to editor
# wrap	- True if we should do automatic line-wrapping

proc rat_edit::create {w {wrap 1}} {
    upvar \#0 rat_edit::${w}_state hd
    variable undoDepth

    # Add the text wrapping bindings (for text insertion)
    bindtags $w [list $w RatEditPreText Text RatEdit . all]

    # Clean up when the window is destroyed
    bind $w <Destroy> {
	catch {
	    unset rat_edit::%W_state
	}
    }

    # Initialize marks and variables
    $w mark set wrap 1.0
    $w mark gravity wrap left
    $w mark set undoStart 1.0
    $w mark set undoEnd 1.0
    $w mark gravity undoStart left
    $w mark gravity undoEnd right
    set hd(wrap) $wrap

    # hd(undo,INT)     is a circular buffer (size decided by undoDepth)
    #                  which is used to store snapshots.
    # hd(numSnapshots) Number of snapshots stored
    # hd(nextPos)      Current position in the snapshot stack, that is
    #                  where the next snapshot shoudl be stored.
    # hd(currentPos)   Current position in snapshot stack (counted from the
    #                  last stored snapshot). That is the last snapshot
    #                  which got restored.
    set hd(numSnapshots) 0
    set hd(nextPos) 0
    set hd(currentPos) 0
    set hd(inhibitStore) 0
}

# rat_edit::setWrap --
#
# Set the wrapping mode (on or off)
#
# Arguments:
# w     - Widget to modify
# mode  - New wrpping mode (1 = on, 0 = off)

proc rat_edit::setWrap {w mode} {
    upvar \#0 rat_edit::${w}_state hd

    set hd(wrap) $mode
}


# rat_edit::state --
#
# Fills in the current state in the given array
#
# Arguments:
# w	- The text widget
# an	- Name of array to store state in

# States
# 0 Initial
# 1 With 'n' deleted
proc rat_edit::state {w an} {
    upvar \#0 rat_edit::${w}_state hd
    upvar \#1 $an state
    variable undoDepth

    if {[llength [$w tag ranges sel]]} {
	set state(selection) normal
    } else {
	set state(selection) disabled
    }
    if {0 == [catch {selection get -displayof $w -selection CLIPBOARD} sel]
	    && [string length $sel]} {
	set state(paste) normal
    } else {
	set state(paste) disabled
    }
    if {$hd(currentPos) > 1} {
        set state(redo) normal
    } else {
        set state(redo) disabled
    }
    if {$hd(currentPos) < $hd(numSnapshots)} {
        set state(undo) normal
    } else { 
        set state(undo) disabled
    }
}

# rat_edit::compareSnapshots --
#
# Compares two snapshots. Returns zero if they are equal.
# This routine ignores any marks.
#
# Arguments:
# s1, s2 - Snapshots to compare

proc rat_edit::compareSnapshots {s1 s2} {
    if {[llength $s1] != [llength $s2]
        || [lindex $s1 0] != [lindex $s2 0]
        || [compareLists [lindex $s1 1] [lindex $s2 1]]} {
        return 1
    } else {
        return 0
    }
}

# rat_edit::compareListss --
#
# Compares two lists. Retuens zero if they are equal.
#
# Arguments:
# l1, l2 - Lists to compare

proc rat_edit::compareLists {l1 l2} {
    if {[llength $l1] != [llength $l2]} {
        return 1
    }
    for {set i 0} {$i < [llength $l1]} {incr i} {
        if {[lindex $l1 $i] != [lindex $l2 $i]} {
            return 1
        }
    }
    return 0
}

# rat_edit::makeSnapshot --
#
# Take a snapshot of the widget for undo purposes
#
# Arguments:
# w	- Widget to take snapshot of

proc rat_edit::makeSnapshot {w} {
    set tags {}
    foreach tag [$w tag names] {
        lappend tags [concat $tag [$w tag ranges $tag]]
    }

    set marks {}
    foreach m [$w mark names] {
        lappend marks [list $m [$w index $m]]
    }
    return [list [$w get 1.0 end] $tags $marks]
}

# rat_edit::prepareSnapshot --
#
# Prepare a snapshot for later
#
# Arguments:
# w	- Widget to take snapshot of

proc rat_edit::prepareSnapshot {w} {
    upvar \#0 rat_edit::${w}_state hd
    if {[winfo exists $w]} {
        set hd(candidate) [makeSnapshot $w]
        set hd(snapshotStored) 0
        set hd(inhibitStore) 0
    }
}

# rat_edit::doStoreSnapshot --
#
# Actually store a snapshot
#
# Arguments:
# w	- Widget to take snapshot of
# s     - Snapshot to store

proc rat_edit::doStoreSnapshot {w s} {
    upvar \#0 rat_edit::${w}_state hd
    variable undoDepth

    if {0 != $hd(currentPos)} {
        incr hd(nextPos) [expr -1 * $hd(currentPos)]
        incr hd(numSnapshots) [expr -1 * $hd(currentPos)]
        set hd(currentPos) 0
    }
    if {$hd(nextPos) < 0} {
        incr hd(nextPos) $undoDepth
    }
    set hd(undo,$hd(nextPos)) $s
    set hd(currentPos) 0
    incr hd(nextPos)
    if {$hd(nextPos) == $undoDepth} {
        set hd(nextPos) 0
    }
    if {$hd(numSnapshots) < $undoDepth} {
        incr hd(numSnapshots)
    }
    set hd(snapshotStored) 1
}

# rat_edit::maybeStoreSnapshot --
#
# Compare a previously stored snapshot with teh current state. If they
# differ then store the snapshot.
#
# Arguments:
# w	- Widget to modify

proc rat_edit::maybeStoreSnapshot {w} {
    upvar \#0 rat_edit::${w}_state hd
    variable undoDepth

    if {$hd(snapshotStored) || $hd(inhibitStore)} {
        return
    }
    set s [makeSnapshot $w]
    if {[compareSnapshots $s $hd(candidate)]} {
        doStoreSnapshot $w $hd(candidate)
        set hd(candidate) {}
    }
}

# rat_edit::storeSnapshot --
#
# Make and store a snapshot of the widget for undo purposes
#
# Arguments:
# w	- Widget to modify

proc rat_edit::storeSnapshot {w} {
    set s [makeSnapshot $w]
    doStoreSnapshot $w $s
}

# rat_edit::restoreSnapshot --
#
# Restores everything in the text widget
#
# Arguments:
# w	- Widget to modify
# depth - Depth to restore snapshot at (counted in levels from next).

proc rat_edit::restoreSnapshot {w depth} {
    upvar \#0 rat_edit::${w}_state hd
    variable undoDepth

    set i [expr $hd(nextPos)-$depth]
    if {$i < 0} {
        incr i $undoDepth
    }
    set s $hd(undo,$i)

    $w delete 1.0 end
    $w insert 1.0 [lindex $s 0]
    foreach t [lindex $s 1] {
        if {1 < [llength $t]} {
            eval "$w tag add $t"
        }
    }
    foreach m [lindex $s 2] {
        $w mark set [lindex $m 0] [lindex $m 1]
    }
}

# rat_edit::delete --
# Deletes text, the selection is deleted only if the insertion point is
# located inside it
#
# Arguments:
# w	- The text window
# loc	- Location of character to delete

proc rat_edit::delete {w loc} {
    storeSnapshot $w
    if {[$w tag nextrange sel 1.0 end] != ""
	    && [$w compare sel.first <= insert]
	    && [$w compare sel.last >= insert]} {
	catch {$w delete sel.first sel.last}
    } elseif {"" != $loc} {
	$w delete $loc
    }
    wrap $w insert
    $w see insert
}

# rat_edit::undo --
# Undo the last remembered operation
#
# Arguments:
# w	- The text window

proc rat_edit::undo {w} {
    upvar \#0 rat_edit::${w}_state hd
    variable undoDepth

    if {$hd(currentPos) >= $hd(numSnapshots)} {
        bell
        return
    }

    if {0 == $hd(currentPos)} {
        storeSnapshot $w
        incr hd(currentPos)
    }
    incr hd(currentPos)

    set hd(inhibitStore) 1
    restoreSnapshot $w $hd(currentPos)
}

# rat_edit::redo --
# Redo the last remembered operation
#
# Arguments:
# w	- The text window

proc rat_edit::redo {w} {
    upvar \#0 rat_edit::${w}_state hd
    variable undoDepth
    
    if {$hd(currentPos) <= 1} {
        bell
        return
    }

    set hd(inhibitStore) 1
    incr hd(currentPos) -1
    restoreSnapshot $w $hd(currentPos)
}


# rat_edit::insert_wrap --
#
# Handles events which might have inserted text
#
# Arguments:
# w	- The text window
# c	- Character to insert

proc rat_edit::insert_wrap {w c} {
    if {"" != $c && [string is print $c]} {
        maybeStoreSnapshot $w
	$w tag remove noWrap insert-1c
	$w tag remove no_spell insert-1c
	$w tag remove Cited insert-1c
	wrap $w insert
    }
}


# rat_edit::paste_wrap --
#
# Adds noWrap tag to pasted material
#
# Arguments:
# w	- The text window

proc rat_edit::paste_wrap {w} {
    if {[catch {selection get -displayof $w -selection CLIPBOARD} sel]} {
	return
    }
    if {[regexp "\n" $sel]} {
	$w tag add noWrap undoStart undoEnd
    }
    wrap $w insert
}

# rat_edit::paste_selection --
#
# Pastes the selection into the text wisget
#
# Arguments:
# w	- The text window

proc rat_edit::paste_selection {w} {
    if {[catch {selection get -displayof $w -selection PRIMARY} sel]} {
	return
    }
    if {"" != $sel} {
        storeSnapshot $w
    }
    $w insert insert $sel noWrap
    wrap $w insert
    event generate $w <<RatPasteSelection>>
}


# rat_edit::wrap --
# Wrap the specified text line
#
# Arguments:
# w	- The text window
# loc	- Text index on line to wrap

proc rat_edit::wrap {w loc} {
    upvar \#0 rat_edit::${w}_state hd
    global option

    # Check if we should wrap at all
    if {!$hd(wrap)
	|| -1 != [lsearch \
		  [concat \
		   [$w tag names "$loc linestart"] \
		   [$w tag names "$loc lineend-1c"]] \
		  noWrap]} {
	return $loc
    }

    $w mark set wrap insert
    set line [expr {int([$w index $loc])}]

    set exp_norm "^(\[ \t\]*)\[^ \t*.-\]"
    set exp_list "^(\[\\d*.-\]*\\)?\[ \t\]+)\[^ \t*.-\]"

    # Start by joining with the previous line (if any)
    if {$line > 1 &&
        "" != [string trim [$w get $line.0-1l "$line.0-1l lineend"]]} {
	set lp [expr {$line-1}]
	set indent ""

	# Find the indention depth
	# Which expression to use depends on if there is a forced line-break
	# at the start of the line. If there is then the line may be the
	# first of a list (enumerated or not)
	if {$lp > 1 && -1 == [lsearch [$w tag names $lp.0-1c] noWrap]} {
	    set exp $exp_norm
	} else {
	    set exp $exp_list
	}
	regexp $exp [$w get $lp.0 $lp.$option(wrap_length)] {} indent]
	set ilen [RatLL $indent]
	set len [RatLL [string trimright [$w get $lp.0 $lp.end]]]
	set i [wrap_join $w $lp $ilen $len]
	if {$i != "$lp.0"} {
	    return $i
	}
    }

    # Find the indention depth
    # Which expression to use depends on if there is a forced line-break
    # at the start of the line. If there is then the line may be the
    # first of a list (enumerated or not)
    if {$line > 1 && -1 == [lsearch [$w tag names $line.0-1c] noWrap]} {
        set exp $exp_norm
    } else {
        set exp $exp_list
    }
    set indent ""
    regexp $exp [$w get $line.0 $line.$option(wrap_length)] {} indent
    set ilen [RatLL $indent]
    set len [RatLL [string trimright [$w get $line.0 $line.end]]]

    if {$option(wrap_length) < $len} {
	set i [wrap_wrap $w $line $ilen]
    } elseif {"" != [string trim [$w get $line.0-1l "$line.0-1l lineend"]]} {
	set i [wrap_join $w $line $ilen $len]
    } else {
	set i $line.0
    }
    $w mark set insert wrap
    $w see insert
    return $i
}

# rat_edit::wrap_wrap --
# Wraps the given line and following
#
# Arguments:
# w	- The text window
# line	- Line to wrap
# indent- Length of indention

proc rat_edit::wrap_wrap {w line indent} {
    global option

    while {1} {
	while {$option(wrap_length) <
	       [RatLL [string trimright [$w get $line.0 $line.end]]]} {
	    set p [$w search -backwards " " \
		       $line.$option(wrap_length)+1c $line.$indent+1c]
            if {"" == $p} {
                set p [$w search " " $line.$option(wrap_length) $line.end]
            }
	    if {"" == $p} {
		return $line.0
	    }
	    set start [$w search -regexp -backwards {[^ 	]} $p]
	    set end [$w search -regexp {[^ 	]} $p]
	    $w delete $start+1c $end
	    $w insert $start+1c "\n[RatGen $indent]" {}
	    incr line
	}
	set ln [expr {$line+1}]
	if {![regexp "^(\[ \t\]*)\[^ \t\]" [$w get $ln.0 $ln.$indent+1c] {} s]
		|| [RatLL $s] != $indent
		|| -1 != [lsearch [$w tag names $ln.0-1c] noWrap]
		|| -1 != [lsearch [$w tag names $ln.0] noWrap]
	        || "" == [string trim [$w get $ln.0 $ln.end]]} {
	    return $line.0
	}
	$w insert "$line.0 lineend" " "
	$w delete $ln.0-1c $ln.[string length $s]
	if {![$w compare $line.end > $line.$option(wrap_length)]} {
	    return $line.0
	}
    }
}

# rat_edit::wrap_join --
# Joins the given line and following
#
# Arguments:
# w	- The text window
# line	- Line to join
# indent- Length of indention
# len	- Length of line

proc rat_edit::wrap_join {w line indent len} {
    global option

    while {1} {
	set ln [expr {$line+1}]
	set r [expr {$option(wrap_length)-$len-1}]
	if {[regexp "^(\[ \t\]*)(\[\[:alpha:\]\\(\])" \
		[$w get $ln.0 $ln.$indent+1c] {} s]
	&& [RatLL $s] == $indent
	&& "" != [$w get $ln.0 $ln.end]
	&& -1 == [lsearch [$w tag names $line.end] noWrap]
	&& -1 == [lsearch [$w tag names $ln.0] noWrap]
	&& (([$w compare $ln.[expr {$r+$indent}]+1c > $ln.end]
	&& "" != [set p $ln.end])
	|| "" != [set p [$w search -backwards " " \
		$ln.[expr {$r+$indent}]+1c $ln.$indent]])} {
	    if {"$ln.end" != $p} {
		set start [$w search -regexp -backwards {[^ 	]} $p]
		set end [$w search -regexp {[^ 	]} $p]
		$w delete $start+1c $end
		$w insert $start+1c "\n" {} $s
	    }
	    $w insert $line.end " "
	    $w delete $line.end $ln.[string length $s]
	} else {
	    return $line.0
	}
	incr line
	set len [RatLL [$w get $line.0 $line.end]]
    }
    return $line.0
}

# rat_edit::wrap_paragraph --
#
# Wraps the selected text
#
# Arguments:
# w	- The text window

proc rat_edit::wrap_paragraph {w} {
    # The algorithm here is:
    # 1. If the insertion cursor is within the selection, then
    #    the start point is the start of the selection otherwise
    #    it is the insertion point. The same goes for the end point.
    set start insert
    set end insert
    catch {
	if {[$w compare sel.first <= insert]
		&& [$w compare sel.last >= insert]} {
	    set start sel.first
	    set end sel.last
	}
    }

    # 2. Move the start point to the start of the paragraph it exists in.
    set line [expr {int([$w index $start])-1}]
    while {$line > 1 &&
           "" != [string trim [$w get $line.0 $line.end]]} {
	incr line -1
    }
    set s $line

    # 3. Move the end point to the end of the paragraph it exists in.
    set line [expr {int([$w index $end])+1}]
    while {"" != [string trim [$w get $line.0 $line.end]]} {
	incr line +1
    }
    set e [expr $line-1]

    # 4. Remove the NoWrap tag between the start and end points
    $w tag remove noWrap $s.0 $e.end

    # 5. Loop over all lines and wrap them. But keep paragraphs separate
    while {$s <= $e} {
	if {"" != [string trim [$w get $s.0 $s.end]]} {
	    set loc [wrap $w $s.0]
	} else {   
	    set loc $s.0
	}
	set s [expr {int([$w index $loc])+1}]
    }
}

# rat_edit::newline_wrap --
#
# Called after return has been inserted. This adds the noWrap tags and
# adds any indention. We should also do a wrap on teh new line.
#
# Arguments:
# w	- The text window

proc rat_edit::newline_wrap {w} {
    upvar \#0 rat_edit::${w}_state hd

    $w tag add noWrap insert-1c
    if {$hd(wrap)} {
	set pl [$w get "insert -1 lines linestart" "insert -1 lines lineend"]
	if {[regexp "^(\[ \t\]+)\[^ \t\]" $pl unused indent]} {
	    $w insert insert $indent
	} else {
	    if {"" == [string trim $pl]} {
		$w delete "insert -1 lines linestart" "insert -1 lines lineend"
	    }
	}
    }
    $w tag remove Cited insert-1c insert
    wrap $w insert
}
