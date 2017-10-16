# rat_edit.tcl --
#
# This file contains the code which implements tkrat's text edit widget
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_edit 1.0

namespace eval rat_edit {
    namespace export create initUndo setWrap
    variable undoBuffer
    variable undoTags
    variable doWrap
    global tk_version

    if {[info exists tk_version]} {
	foreach e {<Tab> <Control-i> <Insert> <KeyPress>} {
	    bind RatEdit $e {rat_edit::insert %W %A}
	}
	bind RatEdit <<Paste>> {rat_edit::paste %W}
	bind RatEdit <<PasteSelection>> {rat_edit::paste %W}
	bind RatEdit <<Cut>> {rat_edit::wrap %W insert}
	bind RatEdit <Return> {rat_edit::newline %W}
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
    variable undoBuffer
    variable undoTags
    variable doWrap

    # Add the text wrapping bindings (for text insertion)
    bindtags $w [list $w Text RatEdit . all]

    # Clean up when the window is destroyed
    bind $w <Destroy> {
	catch {
	    unset undoBuffer(%W)
	    unset undoTags(%W)
	}
    }

    # Initialize marks and variables
    $w mark set wrap 1.0
    $w mark gravity wrap left
    $w mark set undoStart 1.0
    $w mark set undoEnd 1.0
    $w mark gravity undoStart left
    $w mark gravity undoEnd right
    set undoBuffer($w) {}
    set undoTags($w) {}
    set doWrap($w) $wrap

    # Mouse pasting should occur at the current insertion point instead of
    # where the mouse currently is
    bind $w <ButtonRelease-2> {
	rat_edit::initUndo %W insert insert
	catch {
	    set text [selection get -displayof %W]
	    if {[regexp "\n" $text]} {
		%W insert insert $text noWrap
	    } else {
		%W insert insert $text
		rat_edit::wrap %W insert
	    }
	    %W see insert
	}
	if {[%W cget -state] == "normal"} {focus %W}
	break
    }

    # Delete and BackSpace should only delete the selection if the insert
    # cursor is inside it.
    bind $w <Delete> {rat_edit::delete %W insert; break}
    bind $w <BackSpace> {rat_edit::delete %W insert-1c; break}

    # Add undo handling to Cut, CutAll, Paste and text insertion
    foreach e {<<Cut>> <<Paste>> <<PasteSelection>>} {
	bind $w $e {rat_edit::initUndo %W sel.first sel.last}
    }
    bind $w <<CutAll>> {
	%W tag add sel 1.0 end
	rat_edit::delete %W {}
    }
    foreach e {<Tab> <Control-i> <Return> <Insert> <KeyPress>} {
	bind $w $e {
	    catch {
		if {[%W compare sel.first <= insert]
			&& [%W compare sel.last >= insert]} {
		    rat_edit::initUndo %W sel.first sel.last
		}
	    }
	}
    }

    # The actual undo function
    bind $w <<Undo>> {rat_edit::undo %W}

    # Wrap paragraph
    bind $w <<Wrap>> {rat_edit::wrap_paragraph %W}
}

# rat_edit::setWrap --
#
# Set the wrapping mode (on or off)
#
# Arguments:
# w	- Widget to modify
# mode	- New wrpping mode (1 = on, 0 = off)

proc rat_edit::setWrap {w mode} {
    variable doWrap

    set doWrap($w) $mode
}


# rat_edit::state --
#
# Fills in the current state in the given array
#
# Arguments:
# w	- The text widget
# an	- Name of array to store state in

proc rat_edit::state {w an} {
    upvar #1 $an state

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
}

# rat_edit::delete --
# Deletes text, the selection is deleted only if the insertion point is
# located inside it
#
# Arguments:
# w	- The text window
# loc	- Location of character to delete

proc rat_edit::delete {w loc} {
    if {[$w tag nextrange sel 1.0 end] != ""
	    && [$w compare sel.first <= insert]
	    && [$w compare sel.last >= insert]} {
	rat_edit::initUndo $w sel.first sel.last
	catch {$w delete sel.first sel.last}
    } elseif {"" != $loc} {
	$w delete $loc
    }
    rat_edit::wrap $w insert
    $w see insert
}

# rat_edit::initUndo --
# Initialize the undo buffering
#
# Arguments:
# w	- The text window
# s, e	- Start and end of are to store in buffer

proc rat_edit::initUndo {w s e} {
    variable undoBuffer
    variable undoTags

    if {[catch {$w mark set undoStart $s; $w mark set undoEnd $e}]} {
	return
    }
    set undoBuffer($w) [$w get undoStart undoEnd]
    set undoTags($w) [$w tag ranges noWrap]
}

# rat_edit::undo --
# Undo the last remembered operation
#
# Arguments:
# w	- The text window

proc rat_edit::undo {w} {
    variable undoBuffer
    variable undoTags

    set redoBuffer [$w get undoStart undoEnd]
    set redoTags [$w tag ranges noWrap]
    $w delete undoStart undoEnd
    $w insert undoStart $undoBuffer($w)
    if {[info exists undoTags($w)]} {
	for {set i 0} {$i < [llength $undoTags($w)]} {incr i 2} {
	    $w tag add noWrap [lindex $undoTags($w) $i] \
		    [lindex $undoTags($w) [expr {$i+1}]]
	}
    }
    set undoBuffer($w) $redoBuffer
    set undoTags($w) $redoTags
}


# rat_edit::insert --
#
# Handles events which might have inserted text
#
# Arguments:
# w	- The text window
# c	- Character to insert

proc rat_edit::insert {w c} {
    if {"" != $c && [string is print $c]} {
	$w tag remove noWrap insert-1c
	$w tag remove no_spell insert-1c
	$w tag remove Cited insert-1c
	rat_edit::wrap $w insert
    }
}


# rat_edit::paste --
#
# Adds noWrap tag to pasted material
#
# Arguments:
# w	- The text window

proc rat_edit::paste {w} {
    if {[catch {selection get -displayof $w -selection CLIPBOARD} sel]} {
	return
    }
    if {[regexp "\n" $sel]} {
	$w tag add noWrap undoStart undoEnd
    }
    rat_edit::wrap $w insert
}


# rat_edit::wrap --
# Wrap the specified text line
#
# Arguments:
# w	- The text window
# loc	- Text index on line to wrap

proc rat_edit::wrap {w loc} {
    variable doWrap
    global option

    # Check if we should wrap at all
    if {!$doWrap($w)
	|| -1 != [lsearch \
		  [concat \
		   [$w tag names "insert linestart"] \
		   [$w tag names "insert lineend-1c"]] \
		  noWrap]} {
	return $loc
    }

    $w mark set wrap insert
    set line [expr {int([$w index $loc])}]

    # Start by joining with the previous line (if any)
    if {$line > 1 &&
        "" != [string trim [$w get $line.0-1l "$line.0-1l lineend"]]} {
	set lp [expr {$line-1}]
	set indent ""

	# Find the indention depth
	# Which expression to use depends on if there is a forced line-break
	# at the start of the line. If there is then the line may be the
	# first of a list (enumerated or not)
	if {-1 == [lsearch [$w tag names $lp.0-1c] noWrap]} {
	    set exp "^(\[ \t\]*)\[^ \t*.-\]"
	} else {
	    set exp "^(\[ \t\\d*.-\]*)\[^ \t*.-\]"
	}
	regexp $exp [$w get $lp.0 $lp.$option(wrap_length)] {} indent]
	set ilen [RatLL $indent]
	set len [RatLL [string trimright [$w get $lp.0 $lp.end]]]
	set i [rat_edit::wrap_join $w $lp $ilen $len]
	if {$i != "$lp.0"} {
	    return $i
	}
    }

    # Find the indention depth
    # Which expression to use depends on if there is a forced line-break
    # at the start of the line. If there is then the line may be the
    # first of a list (enumerated or not)
    if {-1 == [lsearch [$w tag names $line.0-1c] noWrap]} {
	set exp "^(\[ \t\]*)\[^ \t*.-\]"
    } else {
	set exp "^(\[ \t\\d*.-\]*)\[^ \t*.-\]"
    }
    set indent ""
    regexp $exp [$w get $line.0 $line.$option(wrap_length)] {} indent
    set ilen [RatLL $indent]
    set len [RatLL [string trimright [$w get $line.0 $line.end]]]

    if {$option(wrap_length) < $len} {
	set i [rat_edit::wrap_wrap $w $line $ilen]
    } elseif {"" != [string trim [$w get $line.0-1l "$line.0-1l lineend"]]} {
	set i [rat_edit::wrap_join $w $line $ilen $len]
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
		return $line.0
	    }
	    set start [$w search -regexp -backwards {[^ 	]} $p]
	    set end [$w search -regexp {[^ 	]} $p]
	    $w delete $start+1c $end
	    $w insert $start+1c "\n[RatGen $indent]"
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
		$w insert $start+1c "\n$s"
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
	    set loc [rat_edit::wrap $w $s.0]
	} else {   
	    set loc $s.0
	}
	set s [expr {int([$w index $loc])+1}]
    }
}

# rat_edit::newline --
#
# Called after return has been inserted. This adds the noWrap tags and
# adds any indention. We should also do a wrap on teh new line.
#
# Arguments:
# w	- The text window

proc rat_edit::newline {w} {
    variable doWrap

    $w tag add noWrap insert-1c
    if {$doWrap($w)} {
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
    rat_edit::wrap $w insert
}
