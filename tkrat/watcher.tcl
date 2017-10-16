# watcher.tcl --
#
# This file contains code which handles the watcher window.
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# WatcherInit --
#
# Initializes some watcher variables
#
# Arguments:
# handler -	The folder handler to which this watcher is connected

proc WatcherInit {handler} {
    global folderUnseen folderChanged
    upvar #0 $handler hd

    set hd(watcher_unseen) $folderUnseen($handler)
    set hd(watcher_folderChanged) $folderChanged($handler)
}


# WatcherCreate --
#
# Builds the watcher window
#
# Arguments:

proc WatcherCreate {} {
    global b idCnt option watcherWins vFolderName watcherFont

    # Create toplevel
    set id watcher[incr idCnt]
    upvar #0 $id whd
    set w .$id
    set whd(watcher_w) $w
    set whd(watcher_list) $w.list
    set whd(watcher_size) ""
    toplevel $w -class TkRat
    wm title $w $option(watcher_name)
    wm protocol $w WM_DELETE_WINDOW "WatcherSleep $id"
    
    # Populate window
    frame $w.info
    label $w.info.name -textvariable ${id}(name) \
	    -font $watcherFont -relief raised -bd 1 -anchor w
    label $w.info.size -textvariable ${id}(watcher_size) \
	    -font $watcherFont -width 11 -relief raised -bd 1
    pack $w.info.size -side right
    pack $w.info.name -fill x -expand 1
    pack $w.info -side top -fill x -expand 1
    scrollbar $w.scroll \
	      -relief raised \
	      -bd 1 \
	      -highlightthickness 0 \
	      -command "$w.list yview"
    listbox $whd(watcher_list) \
	    -yscroll "$w.scroll set" \
	    -relief raised \
	    -bd 1 \
	    -font $watcherFont \
	    -exportselection false \
	    -highlightthickness 0
    set b($whd(watcher_list)) watcher
    Size $whd(watcher_list) watcher
    pack $w.scroll -side right -fill y
    pack $w.list -side left -expand 1 -fill both
    Place $w watcher

    foreach but {<1> <B1-Motion> <ButtonRelease-1> <Shift-1> <Control-1>
	       <B1-Leave> <B1-Enter> <space> <Select> <Control-Shift-space>
	       <Shift-Select> <Control-slash> <Control-backslash>} {
	bind $w.list $but {break}
    }
    bind $w.list <ButtonRelease-1>	"WatcherWakeMaster $id"
    bind $w.info.name <ButtonRelease-1>	"WatcherWakeMaster $id"
    bind $w.info.size <ButtonRelease-1>	"WatcherWakeMaster $id"
    bind $w.list <ButtonRelease-3>	"WatcherSleep $id"
    bind $w.info.name <ButtonRelease-3>	"WatcherSleep $id"
    bind $w.info.size <ButtonRelease-3>	"WatcherSleep $id"
	  bind $w.info.name <Destroy>	"WatcherDestroy $id"
    wm withdraw $w
    return $id
}

# WatcherSleep --
#
# Unmaps the watcher window if it was mapped. This should be called
# whenever the folder window is unmapped.
#
# Arguments:
# whandler -	The handler describing the watcher window

proc WatcherSleep {whandler} {
    upvar #0 $whandler whd
    if {[info exists whd(watcher_w)] && [winfo ismapped $whd(watcher_w)]} {
	upvar #0 $whd(folder_handler) hd
	wm withdraw $whd(watcher_w)
	regsub {[0-9]+x[0-9]+} [wm geom $whd(watcher_w)] {} hd(watcher_geom)
    }

    # Put it on the free-list
    set freeWatchers($whandler) 1
}

# WatcherSleepFH --
#
# Finds the watcher handler from the give folder handler and calls WatcherSleep
#
# Arguments:
# handler -	The handler describing the folder

proc WatcherSleepFH {handler} {
    global watcherWins

    if {[info exists watcherWins($handler)]} {
	WatcherSleep $watcherWins($handler)
    }
}

# WatcherDestroy --
#
# Called when the user destroys the watcher window
#
# Arguments:
# whandler -	The handler describing the watcher window

proc WatcherDestroy {whandler} {
    global $whandler freeWatchers
    upvar #0 $whandler whd
    
    WatcherSleep $whandler
    unset $whandler
    unset freeWatchers($whandler)
}

# WatcherTrig --
#
# Called when the number of messages in the folder has changed
#
# Arguments:
# name1, name2, op - Trace arguments

proc WatcherTrig {name1 name2 op} {
    global option t folderUnseen vFolderWatch folderWindowList watcherWins \
	   folderChanged
    upvar #0 $name2 hd

    if {"folderUnseen" == $name1} {
	if {$folderUnseen($name2) < $hd(watcher_unseen)} {
	    set hd(watcher_unseen) $folderUnseen($name2)
	}
	return
    }

    if {"u" == $op} {
	foreach fhd [array names folderWindowList] {
	    if {"$name2" == $folderWindowList($fhd)} {
		FolderWindowClear $fhd
	    }
	}
	return
    }

    # Check for new messages
    if {$hd(watcher_folderChanged) < $folderChanged($name2)} {
	set popup 0
	if {$hd(watcher_unseen) < $folderUnseen($name2)
	    && $vFolderWatch($name2)} {
	    if {1 == [llength [info commands RatUP_Bell]]} {
		if {[catch {RatUP_Bell} text]} {
		    Popup "$t(bell_cmd_failed): $text"
		}
	    } else {
		for {set i 0} {$i < $option(watcher_bell)} {incr i} {
		    after 200
		    bell
		}
	    }
	    set popup $option(watcher_enable)
	}
	set toSync {}
	foreach fhd [array names folderWindowList] {
	    if {"$name2" == $folderWindowList($fhd)} {
		upvar #0 $fhd fh

		lappend toSync $fhd
		if {[winfo ismapped $fh(toplevel)]} {
		    set popup 0
		}
	    }
	}
	if {$popup} {
	    WatcherPopup $name2
	}
	RatBusy {
	    foreach tos $toSync {
		Sync $tos update
	    }
	}
	set hd(watcher_unseen) $folderUnseen($name2)
	set hd(watcher_folderChanged) $folderChanged($name2)
    }
}


# WatcherPopup --
#
# New mail has arrived so we need to populate and popup the watcher window.
#
# Arguments:
# handler -	The folder handler to which this watcher is connected

proc WatcherPopup {handler} {
    global option watcherWins freeWatchers vFolderName
    upvar #0 $handler hd

    # See if there is already existing watcher window to handle this folder
    if {[info exists watcherWins($handler)]} {
	set whandler $watcherWins($handler)
    } else {
	# See if there is a free watcher window
	if {0 < [array size freeWatchers]} {
	    set whandler [lindex [array names freeWatchers] 0]
	    unset freeWatchers($whandler)
	} else {
	    set whandler [WatcherCreate]
	}
	set watcherWins($handler) $whandler
    }
    upvar #0 $whandler whd
    set whd(folder_handler) $handler
    set whd(name) $vFolderName($handler)

    # Populate listbox
    $whd(watcher_list) delete 0 end
    switch $option(watcher_show) {
	new {
	    set fullList [$handler list $option(watcher_format)]
	    set i 0
	    foreach elem [$handler list %S] {
		if {[regexp N $elem]} {
		    $whd(watcher_list) insert end [lindex $fullList $i]
		}
		incr i
	    }
	}
	default {
	    eval "$whd(watcher_list) insert 0 \
		    [$handler list $option(watcher_format)]"
	}
    }
    set lines [$whd(watcher_list) size]
    set height $option(watcher_max_height)
    if {$lines > $height} {
	set lines $height
	if { -1 == [lsearch -exact \
		[pack slaves $whd(watcher_w)] $whd(watcher_w).scroll]} {
	    pack $whd(watcher_w).scroll -side right -fill y
	}
    } elseif { -1 != [lsearch -exact \
		[pack slaves $whd(watcher_w)] $whd(watcher_w).scroll]} {
	pack forget $whd(watcher_w).scroll
    }
    if {!$lines} {
	set lines 1
    }
    $whd(watcher_list) configure -height $lines
    $whd(watcher_list) see [expr {[$whd(watcher_list) size]-1}]
    set info [$handler info]
    set whd(watcher_size) "[lindex $info 1]/[RatMangleNumber [lindex $info 2]]"

    wm deiconify $whd(watcher_w)

    # Fix placement. This can get confused due to the addition/removal of the
    # scroll-bar. So it is best to reset it here to a sane value.
    if {[info exists hd(watcher_geom)]} {
	wm geometry $whd(watcher_w) $hd(watcher_geom)
    }
}


# WatcherWakeMaster --
#
# The user wants us to wake the master up, so do that.
#
# Arguments:
# whandler -	The handler describing the watcher window

proc WatcherWakeMaster {whandler} {
    global folderWindowList
    upvar #0 $whandler whd

    foreach fhd [array names folderWindowList] {
	if {"$whd(folder_handler)" == $folderWindowList($fhd)} {
	    upvar #0 $fhd fh
	    FolderSelectUnread $fhd
	    wm deiconify $fh(toplevel)
	    return
	}
    }
    WatcherSleep $whandler

    # This must be a monitored folder, so find it and create a new folder
    global vFolderMonitorID vFolderDef
    NewFolderWin $vFolderDef($vFolderMonitorID($whd(folder_handler)))
}
