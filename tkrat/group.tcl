# group.tcl --
#
# This file contains code which handles group operations
#
#
#  TkRat software and its included text is Copyright 1996-2006 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# GroupMessageList --
#
# Pops a message list that lets the user select messages for a group
#
# Arguments:
# handler -	The handler which identifies the folder window

proc GroupMessageList {handler} {
    global b idCnt t option
    upvar \#0 $handler fh

    # Create identifier
    set id f[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(edit_group)
    frame $w.f
    scrollbar $w.f.scroll \
        -relief sunken \
        -command "$w.f.list yview" \
	-highlightthickness 0
    listbox $w.f.list \
        -yscroll "$w.f.scroll set" \
        -exportselection false \
	-highlightthickness 0 \
	-selectmode multiple \
	-setgrid true
    set b($w.f.list) group_list_editor
    pack $w.f.scroll -side right -fill y
    pack $w.f.list -side left -expand 1 -fill both
    frame $w.buttons
    button $w.buttons.ok -text $t(ok) \
	    -command "GroupMessageListDone $w $handler 1"
    set b($w.buttons.ok) group_window_ok
    button $w.buttons.sel -text $t(select_all) \
	    -command "$w.f.list selection set 0 end"
    set b($w.buttons.sel) group_window_selall
    button $w.buttons.unsel -text $t(deselect_all) \
	    -command "$w.f.list selection clear 0 end"
    set b($w.buttons.unsel) group_window_unselall
    button $w.buttons.cancel -text $t(cancel) \
	    -command "GroupMessageListDone $w $handler 0"
    set b($w.buttons.cancel) cancel
    pack $w.buttons.ok \
	 $w.buttons.sel \
	 $w.buttons.unsel \
	 $w.buttons.cancel -side left -expand 1
    pack $w.buttons -side bottom -fill x -pady 5
    pack $w.f -expand 1 -fill both

    set fi 0
    set li 0
    foreach e [$fh(folder_handler) list "%u $option(list_format)"] {
	regexp {^([^ ]*) (.*)} $e unused uid list_entry
        if {"" == $fh(filter)
            || [string match -nocase "*$fh(filter)*" $list_entry]} {
            lappend fh($w.uids) $uid
            $w.f.list insert end $list_entry
            set rmapping($fi) $li
            incr li
        }
        incr fi
    }
    foreach i [$fh(folder_handler) flagged flagged 1] {
        if {[info exists rmapping($i)]} {
            $w.f.list selection set $rmapping($i)
        }
    }
    lappend fh(groupMessageLists) $w
    bind $w.f.list <Destroy> "GroupMessageListDone $w $handler 0"

    bind $w <Escape> "$w.buttons.cancel invoke"
    ::tkrat::winctl::SetGeometry groupMessages $w $w.f.list

    set fh(grouplist) $w.f.list
    bind $w.f.list <1> "GroupListAnchor $handler \[%W index @%x,%y\]"
    bind $w.f.list <B1-Motion> \
        "GroupListMotion $handler %W \[%W index @%x,%y\]"
}


# GroupListAnchor --
#
# Called when user possibly starts a drag
#
# Arguments:
# handler - The handler which identifies the folder window
# index -   The element under the pointer (must be a number).

proc GroupListAnchor {handler index} {
    upvar \#0 $handler fh

    set fh(grouplist_last) $index
    set fh(grouplist_selection) [$fh(grouplist) curselection]
    if {[$fh(grouplist) selection includes $index]} {
        set fh(grouplist_mode) clear
    } else {
        set fh(grouplist_mode) set
    }
    $fh(grouplist) selection anchor $index
}

# GroupListMotion --
#
# Called when user drags in listbox
#
# Arguments:
# handler - The handler which identifies the folder window
# w -       The listbox widget.
# index -   The element under the pointer (must be a number).

proc GroupListMotion {handler w index} {
    upvar \#0 $handler fh

    if {$fh(grouplist_last) == $index} {
        return
    }
    set anchor [$w index anchor]
    if {$index > $anchor || $fh(grouplist_last) > $anchor} {
        if {$index > $fh(grouplist_last)} {
            set d 1
            set apply 1
        } else {
            set d -1
            set apply 0
        }
    } else {
        if {$index < $fh(grouplist_last)} {
            set d -1
            set apply 1
        } else {
            set d 1
            set apply 0
        }
    }
    if {$apply} {
        $fh(grouplist) selection $fh(grouplist_mode) $fh(grouplist_last) $index
    } else {
        set i [expr $fh(grouplist_last)]
        while {$i != $index} {
            if {-1 == [lsearch $fh(grouplist_selection) $i]} {
                $fh(grouplist) selection clear $i
            } else {
                $fh(grouplist) selection set $i
            }
            incr i $d
        }
    }
    set fh(grouplist_last) $index
}

# 
# GroupMessageListUpdate --
#
# Update the message list since the underlying folder was updated
#
# Arguments:
# w	  -	The group selection window
# handler -	The handler which identifies the folder window

proc GroupMessageListUpdate {w handler} {
    upvar \#0 $handler fh
    global option

    foreach c [$w.f.list curselection] {
	set selected([lindex $fh($w.uids) $c]) 1
    }
    set top [lindex [$w.f.list yview] 0]
    $w.f.list delete 0 end
    set fh($w.uids) {}
    foreach e [$fh(folder_handler) list "%u $option(list_format)"] {
	regexp {^([^ ]*) (.*)} $e unused uid list_entry
        if {"" != $fh(filter)
            && ![string match -nocase "*$fh(filter)*" $list_entry]} {
            continue
        }
	lappend fh($w.uids) $uid
	$w.f.list insert end $list_entry
	if {[info exists selected($uid)]} {
	    $w.f.list selection set end
	}
    }
    $w.f.list yview moveto $top
}

# GroupMessageListDone --
#
# Calls when the grouping is done
#
# Arguments:
# w	  -	The group selection window
# handler -	The handler which identifies the folder window
# done    -	The users selection (1=ok, 0=cancel)

proc GroupMessageListDone {w handler done} {
    upvar \#0 $handler fh
    global b option

    bind $w.f.list <Destroy> {}
    if {$done} {
	set candidates [$w.f.list curselection]
	set isset [$fh(folder_handler) flagged flagged 1]
	set toset {}
	set toclear {}
        set torefresh {}
	for {set i 0} {$i < [$w.f.list size]} {incr i} {
	    set nv [expr {-1 != [lsearch $candidates $i]}]
            set ov [expr {-1 != [lsearch $isset $i]}]
            if {$nv != $ov} {
                if {$nv} {
                    lappend toset $fh(mapping,$i)
                } else {
                    lappend toclear $fh(mapping,$i)
                }
                lappend torefresh $i
            }
	}
	$fh(folder_handler) setFlag $toset flagged 1
	$fh(folder_handler) setFlag $toclear flagged 0
	foreach i $torefresh {
	    FolderListRefreshEntry $handler $i
	}
    }
    ::tkrat::winctl::RecordGeometry groupMessages $w $w.f.list
    set index [lsearch $w $fh(groupMessageLists)]
    set fh(groupMessageLists) [lreplace $fh(groupMessageLists) $index $index]
    destroy $w
    unset fh($w.uids)
    foreach a [array names b $w*] {
	unset b($a)
    }
}

# GroupClear --
#
# Removes the flag from every message
#
# Arguments:
# handler -	The handler which identifies the folder window

proc GroupClear {handler} {
    upvar \#0 $handler fh
    global option

    foreach i [$fh(folder_handler) flagged flagged 1] {
	$fh(folder_handler) setFlag $i flagged 0
	FolderListRefreshEntry $handler $fh(rmapping,$i)
    }
}

# SetupGroupMenu --
#
# Setup the entries in the group menu
#
# Arguments:
# m	  -	The menu command name
# handler -	The handler which identifies the folder window

proc SetupGroupMenu {m handler} {
    upvar \#0 $handler fh
    global t

    # Create groups
    if {$fh(num_messages) > 0} {
        set s normal
    } else {
        set s disabled
    }
    foreach i {1 2 3} {
        $m entryconfigure $i -state $s
    }

    # Group operations
    set num 0
    if {![info exists fh(folder_handler)]} {
	set s disabled
    } elseif {[set num [llength [$fh(folder_handler) flagged flagged 1]]]} {
	set s normal
    } else {
	set s disabled
    }
    foreach i {4 6 7 8 9 11 12 13 15 16 17 18 19 20 22} {
        $m entryconfigure $i -state $s
    }
    # Number of grouped messages
    $m entryconfigure 4 -label "$t(clear_group) ($num)"

    # Disable some ops in drafts folder
    if {$s == "normal"} {
        if {"drafts" == $fh(special_folder)} {
            set s disabled
        } else {
            set s normal
        }
        foreach i {17 18 19 20} {
            $m entryconfigure $i -state $s
        }
    }

    # Disable dbinfo entry if first message is not dbase
    if {![info exists fh(folder_handler)]
        || "dbase" != [$fh(folder_handler) type]} {
        $m entryconfigure 22 -state disabled
    }
}

# GroupSameSubject --
#
# Mark all messages in teh current folder which have the same subject
# as the current one.
#
# Arguments:
# handler -	The handler which identifies the folder window
# msg     -     The current message

proc GroupSameSubject {handler msg} {
    upvar \#0 $handler fh
    global option

    regsub -all -nocase "^$option(re_regexp)" [$msg list "%s"] "" match
    set match [string trim $match]
    set subjects [$fh(folder_handler) list "%s"]
    set flag {}
    for {set i 0} {$i < [llength $subjects]} {incr i} {
        set subject [lindex $subjects $i]
        regsub -all -nocase "^$option(re_regexp)" $subject "" subject
        if {$match == [string trim $subject]} {
            lappend flag $i
        }
    }
    if {[string length $flag]} {
        SetFlag $handler flagged 1 $flag
    }
}
