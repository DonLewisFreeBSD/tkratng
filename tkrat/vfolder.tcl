# vfolder.tcl --
#
# This file contains commands which interacts with the user about performing
# different types of folder operations
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


# The ident of the next defined vfolder
set vFolderDefIdent 0

# The ident of the next defined vFolderStruct
set vFolderStructIdent 0

set vFolderLastUsedList {}

proc AddToLastUsedList {id} {
    global vFolderLastUsedList
    set res [list $id]
    set l [llength $vFolderLastUsedList]
    set i 0
    set n 1
    while {$i < $l} {
	if { $id != [lindex $vFolderLastUsedList $i] } {
	    lappend res [lindex $vFolderLastUsedList $i]
	    incr n 1
	    if { $n >= 8 } break
	}
	incr i 1
    }
    set vFolderLastUsedList $res
}


# VFolderRead --
#
# Reads the list of vfolders from disk. If needed it calls FixVFolderList
# which upgrades the list from the old format to the new.
#
# Arguments:

proc VFolderRead {} {
    global vFolderDef vFolderVersion option vfolder_list vFolderInbox \
	vFolderStruct mailServer vFolderSave t vFolderSpecials \
	vFolderOutgoing vFolderHold

    if {[file readable $option(ratatosk_dir)/vfolderlist]} {
	source $option(ratatosk_dir)/vfolderlist
    }

    # Upgrade old versions
    if { 0 == [info exists vFolderVersion]} {
	if {[info exists vfolder_list]} {
	    FixVFolderList
	    set vFolderVersion 4
	} else {
	    set vFolderDef(0) {{} struct {} {1}}
	    set vFolderDef(1) $option(default_folder)
	    set vFolderVersion 7
	    set vFolderInbox 1
	}
    }
    if {3 == $vFolderVersion} {
	foreach id [array names vFolderDef] {
	    if { {} != [lindex $vFolderDef($id) 2]} {
		set vFolderDef($id) [lreplace $vFolderDef($id) 2 0 {}]
	    }
	}
	set vFolderVersion 4
    }
    if {4 == $vFolderVersion} {
	UpgradeVFolderList4to5
    }
    if {5 == $vFolderVersion} {
	UpgradeVFolderList5to6
    }
    if {6 == $vFolderVersion} {
	UpgradeVFolderList6to7
    }
    if {7 == $vFolderVersion} {
	UpgradeVFolderList7to8
    }
    if {8 != $vFolderVersion} {
	Popup $t(unsupported_folder_file)
	exit 1
    }
    if {![info exists vFolderInbox]} {
	set vFolderInbox 0
    }
    if {![info exists vFolderDef($vFolderInbox)]} {
	set vFolderInbox [lindex [lsort -integer [array names vFolderDef]] 0]
    }
    if {[info exists vFolderSave]} {
	foreach r $option(roles) {
	    set option($r,save_outgoing) $vFolderSave
	}
	unset vFolderSave
	# We have to save everything here since we have moved data from
	# one file to the other.
	SaveOptions
	VFolderWrite
    }
    if {![info exists vFolderHold]} {
	set id [VFolderGetID]
	set vFolderDef($id) [list $t(drafts) file {monitor 1} \
				 "$option(ratatosk_dir)/drafts.mbx"]
	set vFolderHold $id
    } else {
    	set vFolderDef($vFolderHold) [lreplace $vFolderDef($vFolderHold) 0 0 $t(drafts)]
    }
    if {![info exists vFolderOutgoing]} {
	set id [VFolderGetID]
	set vFolderDef($id) [list $t(outgoing) file {monitor 1} \
				 "$option(ratatosk_dir)/outgoing.mbx"]
	set vFolderOutgoing $id
    } else {
    	set vFolderDef($vFolderOutgoing) [lreplace $vFolderDef($vFolderOutgoing) 0 0 $t(outgoing)]
    }
    if {![info exists vFolderSpecials]} {
	set id [VFolderGetID]
	set vFolderDef($id) [list $t(specials) struct {} \
				 [list $vFolderOutgoing $vFolderHold]]
	set vFolderSpecials $id
    }
    CheckVFolderList

    RatCreateFolder -mbx $vFolderDef($vFolderHold)
    RatCreateFolder -mbx $vFolderDef($vFolderOutgoing)
}

# CheckVFolderList --
#
# Checks the consistency of the folder-list. It removes any unreferenced 
# entries and removes refewrences to bad entries.
#
# Arguments:

proc CheckVFolderList {} {
    global vFolderDef vFolderSpecials vFolderOutgoing vFolderHold
    set errors {}

    foreach v [array names vFolderDef] {
	set ref($v) 0
    }
    # Check that the root entry exists and is a struct
    incr ref(0)
    if {[info exists vFolderDef(0)]
	&& "struct" == [lindex $vFolderDef(0) 1]} {
	CheckVFDWalk 0
    } else {
	lappend errors "Missing struct entry 0"
	set vFolderDef(0) [list "Repaired" struct {} {}]
    }

    # Check that the specials entry exists and is a struct
    if {[info exists vFolderDef($vFolderSpecials)]
	&& "struct" == [lindex $vFolderDef($vFolderSpecials) 1]} {
	incr ref($vFolderSpecials)
	CheckVFDWalk $vFolderSpecials
    } else {
	puts "No specials"
	lappend errors "Missing struct entry for specials"
	set vFolderDef($vFolderSpecials) \
	    [list "Repaired" struct {} [list $vFolderOutgoing $vFolderHold]]
    }

    # Check that all entries are references exactly once
    foreach v [lsort -integer [array names ref]] {
	if {0 == $ref($v)} {
	    lappend errors "Folder $v not referenced, removing"
	    unset vFolderDef($v)
	}
    }
    foreach e $errors {
	puts $e
    }
}

# CheckVFDWalk --
#
# Walks the vfolderdef, we know that the id we are passed exists and contains
# a struct.
#
# Arguments:
# id - Id of struct to walk

proc CheckVFDWalk {id} {
    global vFolderDef
    upvar ref ref \
	errors errors

    # Walk through all referenced entries and see that they exists
    set nr {}
    set modified 0
    if {"struct" == [lindex $vFolderDef($id) 1]} {
	set pos 3
    } else {
	set pos 5
    }
    set ids [lindex $vFolderDef($id) $pos]
    foreach r $ids {
	if {![info exists vFolderDef($r)]} {
	    lappend errors "Referenced entry $r does not exist"
	    incr modified
	} elseif {0 != $ref($r)} {
	    lappend errors "Folder $r referenced multiple times"
	    incr modified
	} else {
	    lappend nr $r
	    incr ref($r)
	    if {"struct" == [lindex $vFolderDef($r) 1]
	        || "import" == [lindex $vFolderDef($r) 1]} {
		CheckVFDWalk $r
	    }
	}
    }
    if {0 < $modified} {
	set vFolderDef($id) [lreplace $vFolderDef($id) $pos $pos $nr]
    }
}

# SelectFileFolder --
#
# Presents a file selector to the user and opens the selected folder (if any)
#
# Arguments:
# parent - parent window

proc SelectFileFolder {parent} {
    global t option

    set fh [rat_fbox::run \
                -title $t(open_file) \
                -mode any \
                -initialdir $option(initialdir) \
                -ok $t(open) \
                -parent $parent]

    # Do open
    if {$fh != ""} {
        if {$option(initialdir) != [file dirname $fh]} {
            set option(initialdir) [file dirname $fh]
            SaveOptions
        }
	return [list RatOpenFolder [list $fh file {} $fh]]
    } else {
	return ""
    }
}

# SelectDbaseFolder --
#
# Lets the user search the database
#
# Arguments:
# parent - parent window

proc SelectDbaseFolder {parent} {
    global idCnt t b

    # Create identifier
    set id vfolderWinID[incr idCnt]
    set w .$id
    upvar \#0 $id hd
    set hd(done) 0
    set hd(op) and

    # Create toplevel
    toplevel $w -class TkRat -bd 5
    wm title $w $t(open_dbase)
    wm transient $w $parent

    # Fill in times
    set dinfo [RatDbaseInfo]
    set hd(int_from) [clock format [lindex $dinfo 1] -format "%Y-%m-%d 00:00"]
    set hd(int_to) [clock format [lindex $dinfo 2] -format "%Y-%m-%d 23:59"]

    # Populate window
    label $w.interval -text $t(time_interval): -anchor e
    frame $w.int
    entry $w.int.from -width 18 -textvariable ${id}(int_from)
    label $w.int.divider -text "-"
    entry $w.int.to -width 18 -textvariable ${id}(int_to)
    grid $w.int.from $w.int.divider $w.int.to -sticky ew
    grid columnconfigure $w.int 0 -weight 1
    grid columnconfigure $w.int 2 -weight 1
    set b($w.int.from) dbs_time_from
    set b($w.int.to) dbs_time_to
    grid $w.interval $w.int - -sticky ew -pady 5

    label $w.operation -text $t(operation): -anchor e
    radiobutton $w.and -text $t(op_and) -variable ${id}(op) -value and \
        -anchor w
    radiobutton $w.or -text $t(op_or) -variable ${id}(op) -value or -anchor w
    set b($w.and) dbs_and
    set b($w.or) dbs_or
    grid $w.operation $w.and -sticky ew
    grid x $w.or -sticky ew

    label $w.line_expl -text $t(line_expl)
    grid x $w.line_expl -sticky ew -pady 5

    foreach el {subject {all_addresses all_addr_detail} to from cc} {
        if {[llength $el] > 1} {
            label $w.${e}_rlabel -text "($t([lindex $el 1]))" -anchor w
            set l $w.${e}_rlabel
            set e [lindex $el 0]
        } else {
            set l "-"
            set e $el
        }
	label $w.${e}_label -text $t($e): -anchor e
	entry $w.${e}_entry -textvariable ${id}($e) -relief sunken
        grid $w.${e}_label $w.${e}_entry $l -sticky ew
	set b($w.${e}_entry) dbs_$e
    }

    frame $w.separator -height 4
    grid x $w.separator

    foreach el {keywords {complete_msg_text slow}} {
        if {[llength $el] > 1} {
            label $w.${e}_rlabel -text "($t([lindex $el 1]))" -anchor w
            set l $w.${e}_rlabel
            set e [lindex $el 0]
        } else {
            set l "-"
            set e $el
        }
	label $w.${e}_label -text $t($e): -anchor e
	entry $w.${e}_entry -textvariable ${id}($e) -relief sunken
        grid $w.${e}_label $w.${e}_entry $l -sticky ew
	set b($w.${e}_entry) dbs_$e
    }

    OkButtons $w $t(search) $t(cancel) "set ${id}(done)"
    grid $w.buttons - - -sticky we

    ::tkrat::winctl::SetGeometry selectDbaseFolder $w
    ::tkrat::winctl::ModalGrab $w $w.subject_entry

    set cont 1
    while {$cont} {
        tkwait variable ${id}(done)
        set cont 0
        if {1 == $hd(done)} {
            set start_s [clock format [clock seconds] -format "%Y-%m-%d 00:00"]
            set start_i [clock scan $start_s]
            set end_i [expr $start_i+24*60*60]
            if {[catch {clock scan $hd(int_from) -base $start_i} \
                     hd(int_from_parsed)]} {
                Popup $t(illegal_from_date) $w
                set cont 1
                continue
            }
            if {[catch {clock scan $hd(int_to) -base $end_i} \
                 hd(int_to_parsed)]} {
                Popup $t(illegal_to_date) $w
                set cont 1
                continue
            }
        }
    }

    # Do search
    set ret ""
    if {1 == $hd(done)} {
	set exp [list "int" $hd(int_from_parsed) $hd(int_to_parsed) $hd(op)]
	foreach e {keywords subject all_addresses to from cc} {
	    if {[string length $hd($e)]} {
                lappend exp $e $hd($e)
	    }
	}
        if {[string length $hd(complete_msg_text)]} {
            lappend exp "all" $hd(complete_msg_text)
        }
	if {[string compare $hd(op) $exp]} {
	    set ret [list RatOpenFolder \
		    [list "Dbase search" dbase {} {} {} $exp]]
	} else {
	    Popup $t(emptyexp) $parent
	}
    }

    foreach bn [array names b $w.*] {unset b($bn)}
    ::tkrat::winctl::RecordGeometry selectDbaseFolder $w
    destroy $w
    unset hd
    return $ret
}

# VFolderAddItem --
#
# Add an item to a menu
#
# Arguments:
# m	- Menu to add to
# id	- Id of item to add
# elem	- Item to add
# cmd	- Command to execute when an item is choosen
# write - If this argument is 1 the folders are going to be used for
#	  inserting messages.

proc VFolderAddItem {m id elem cmd write} {
    global openFolders option vFolderMonitorFH folderExists folderUnseen \
        currentColor

    if {[llength $currentColor] > 3} {
        set unreadBg [option get $m troughColor Color]
    } else {
        set unreadBg [$m cget -activebackground]
    }
    if {1 == [llength $elem]} {
	global vFolderDef
	set elem $vFolderDef($id)
    }
    if {![string compare dynamic [lindex $elem 1]] &&
	    (0 == $write || "expanded" == $option(dynamic_behaviour))} {
	regsub -all {[\. ]} $elem _ nid
	set nm $m.m$nid
	$m add cascade -label [lindex $elem 0] -menu $nm
		if {![winfo exists $nm]} {
	    menu $nm -postcommand [list VFolderBuildDynamic $nm \
		    $elem $cmd $write]
	}
    } elseif {[string compare pop3 [lindex $elem 1]] || 0==$write} {
	$m add command -label [lindex $elem 0] -command "$cmd [list $id]"
	if {[info exists vFolderMonitorFH($id)]} {
	    if {[info exists folderUnseen($vFolderMonitorFH($id))]} {
                set unseen $folderUnseen($vFolderMonitorFH($id))
                set exists $folderExists($vFolderMonitorFH($id))
                $m entryconfigure last -accelerator "($unseen/$exists)"
		if { $folderUnseen($vFolderMonitorFH($id)) > 0 } {
		    $m entryconfigure last \
                        -background $unreadBg
		}
	    } else {
		unset vFolderMonitorFH($id)
	    }
	}
	if {0 == $write && -1 != [lsearch -exact $openFolders $elem]} {
	    $m entryconfigure [$m index end] -state disabled
	}
    }
}

# VFolderBuildDynamic --
#
# Populate a dynamic menu
#
# Arguments:
# m	- The menu in which to insert the entries
# elem	- The folder definition
# cmd	- Command to execute when an item is choosen
# write	- If this argument is 1 the folders are going to be used for
#	  inserting messages.

proc VFolderBuildDynamic {m elem cmd write} {
    global t

    $m delete 0 end
    if {$write} {
	$m add command -label $t(auto_select) -command "$cmd [list $elem]"
    }
    foreach f [lsort [glob -nocomplain [lindex $elem 3]/*]] {
	if {[file isfile $f]} {
	    $m add command -label [file tail $f] \
		    -command "$cmd {[list [file tail $f] file \
					  [lindex $elem 2] $f]}"
	}
    }
    FixMenu $m
}


# VFolderBuildMenu --
#
# Constructs a menu of vfolders. When one item in the menu is choosen
# $cmd is executed and the vfolder id of the choosen folder is
# appended to $cmd. If the write argument is 1 then only those
# folders that can be written to are included in the menu.
#
# Arguments:
# m   -		The menu in which to insert the entries
# id  -		The id of the struct to start with
# cmd -		Command to execute when an item is choosen
# write -	If this argument is 1 the folders are going to be used for
#		inserting messages.

proc VFolderBuildMenu {m id cmd write} {
    global vFolderDef idCnt t idmap$m

    $m configure -tearoffcommand VFolderTearoffMenu
    if {![info exists vFolderDef($id)]} {
	return
    }
    
    switch [lindex $vFolderDef($id) 1] {
	struct {set i 3}
	import {set i 5}
    }
    foreach sid [lindex $vFolderDef($id) $i] {
	set name [lindex $vFolderDef($sid) 0]
	if {"struct" == [lindex $vFolderDef($sid) 1]
	    || "import" == [lindex $vFolderDef($sid) 1]} {
	    set nm $m.m$sid
	    $m add cascade -label $name -menu $nm
	    if {![winfo exists $nm]} {
		menu $nm -postcommand "$nm delete 0 end; \
					   VFolderBuildMenu \
					       $nm $sid [list $cmd] $write; \
					   FixMenu $nm"
	    }
	} else {
	    VFolderAddItem $m $sid $sid $cmd $write
	    set idmap${m}($sid) [$m index last]
	}
    }
}

# VFolderTearoffMenu --
#
# Add traces and bindings for torn off menus.
#
# Arguments:
# oldmenu - Old menu name
# menu	  - New menu

proc VFolderTearoffMenu {oldmenu menu} {
    global folderExists folderUnseen idmap$oldmenu idCnt

    set var idmap[incr idCnt]
    upvar \#0 $var v
    upvar \#0 idmap$oldmenu idmap
    foreach i [array names idmap] {
	set v($i) $idmap($i)
    }

    set cmd "after 100 VFolderTrace $var $menu"
    trace variable folderExists wu $cmd
    trace variable folderUnseen wu $cmd
    bind $menu <Destroy> "+
	trace vdelete folderExists wu \"$cmd\"
	trace vdelete folderUnseen wu \"$cmd\"
        unset $var
    "
}

# VFolderTrace --
#
# Trace function for vfolder menus
#
# Arguments:
# var	- Variable containing id-mappings
# menu	- Menu containig entry
# name1, name2, op

proc VFolderTrace {var menu name1 name2 op} {
    global vFolderDef vFolderMonitorID folderUnseen folderExists
    upvar \#0 $var v

    if {![info exists vFolderMonitorID($name2)]
        || ![info exists v($vFolderMonitorID($name2))]} {
        # Ignore this folder
        return
    }

    if {"w" == $op} {
        set a ($folderUnseen($name2)/$folderExists($name2))
    } else {
        set a ""
    }
    $menu entryconfigure $v($vFolderMonitorID($name2)) -accelerator $a
}

# VFolderDoOpen --
#
# Opens the specified vfolder
#
# Arguments:
# id -		Identity of folder
# vfolder -	The definition of the vfolder to be opened

proc VFolderDoOpen {id vfolder} {
    global vFolderWatch vFolderName vFolderDef vFolderHold option

    set f [RatOpenFolder $vfolder]
    array set flag [lindex $vfolder 2]
    set vFolderWatch($f) 0
    if {"" != $id} {
	global vFolderMonitorFH vFolderMonitorID folderUnseen
	set vFolderMonitorFH($id) $f
	set vFolderMonitorID($f) $id
    }
    if {[info exists flag(watch)] && $flag(watch)} {
	set vFolderWatch($f) 1
	set vFolderName($f) [lindex $vfolder 0]
    }

    if {$vfolder == $vFolderDef($vFolderHold)} {
        after [expr $option(compose_last_chance)*1000] VFolderPurgeBackups $f
    }
    return $f
}

# VFolderOpen --
#
# Opens the specified vfolder
#
# Arguments:
# handler -	The handler to the folder window which requested the open
# vfolder -	The definition of the vfolder to be opened. If it is only
#		one word then it is expected to the ID of a folder to open

proc VFolderOpen {handler vfolder} {
    global t inbox vFolderDef vFolderInbox option folderWindowList \
        vFolderLastUsedList
    upvar \#0 $handler fh

    set fh(special_folder) none
    if {1 == [llength $vfolder]} {
	global vFolderDef vFolderOutgoing vFolderHold

	set id $vfolder
	set vfolder $vFolderDef($id)
	if {$id == $vFolderOutgoing || $id == $vFolderHold} {
	    set fh(special_folder) drafts
	} else {
	    AddToLastUsedList $id
	}
    } else {
	set id ""
    }

    array set features [lindex $vfolder 2]
    # Initialize browse mode
    switch $option(browse) {
	normal {set mode 0}
	browse {set mode 1}
	folder {
	    if {![info exists features(browse)]} {
		    set features(browse) 0
		}
		set mode $features(browse)
	    }
    }
    set fh(browse) $mode

    set folder [FolderRead $handler [list VFolderDoOpen $id $vfolder] \
			   [lindex $vfolder 0]]
    if {[string length $folder]} {
	set folderWindowList($handler) $folder
    }
    if {![string compare $vfolder $vFolderDef($vFolderInbox)]} {
	set inbox $folder
    }
}

# VFolderInsert --
#
# Inserts the given messages into the given vfolder
#
# Arguments:
# handler  -	The handler to the folder window which requested the operation
# advance  -	1 if we should move to the next message on success
# delete   -    1 if we shoudl delete moved messages
# messages -	The messages to move
# vfolder  -	The definition of the vfolder to be opened. If it is only
#		one word then it is expected to the ID of a folder to open

proc VFolderInsert {handler advance delete messages vfolder} {
    if {1 == [llength $vfolder]} {
	global vFolderDef
	AddToLastUsedList $vfolder
	set vfolder $vFolderDef($vfolder)
    }
    RatBusy [list VFolderInsertDo $handler $advance $messages $vfolder $delete]
}
proc VFolderInsertDo {handler advance messages vfolder delete} {
    upvar \#0 $handler fh
    global option t

    if {![llength $vfolder]} {
	return
    }
    if {1 == [llength $vfolder]} {
	global vFolderDef
	set vfolder $vFolderDef($vfolder)
    }

    # There is no need to open dbase folders before inserting
    # messages. But other types of folders benefit from opening
    # because that means that each copied message does not need to
    # open it again.
    if {"dbase" != [lindex $vfolder 1]} {
        set f [RatOpenFolder append $vfolder]
    }
    set toDelete {}
    foreach msg $messages {
	if {[catch {$msg copy $vfolder} result]} {
	    RatLog 4 "$t(insert_failed): $result"
	    break
	} else {
            if {$delete} {
                lappend toDelete [$fh(folder_handler) find $msg]
            }
	}
    }
    if {[llength $toDelete] > 0} {
        # Do all the flag updates at once for performance reasons
        SetFlag $handler deleted 1 $toDelete
    }
    if { 1 == $advance } {
        FolderNext $handler
    }
    if {[info exists f]} {
        $f close
    }
}

# InsertIntoFile --
#
# Inserts the given message into a file. The user must specify the file.
#
# Arguments:
# parent - Parent window

proc InsertIntoFile {parent} {
    global t option

    set f [rat_fbox::run \
               -ok $t(ok) \
               -title $t(save_to_file) \
               -initialdir $option(initialdir) \
               -mode any \
               -parent $parent]

    if { $f != "" } {
        if {$option(initialdir) != [file dirname $f]} {
            set option(initialdir) [file dirname $f]
            SaveOptions
        }
	set result [list $f file {} $f]
    } else {
	set result {}
    }
    return $result
}

# InsertIntoDBase --
#
# Inserts the given message into the database. The user must specify
# some of the arguments for the insert operation.
#
# Arguments:
# parent - Parent window

proc InsertIntoDBase {parent} {
    global idCnt t b option

    # Create identifier
    set id f[incr idCnt]
    set w .$id
    upvar \#0 $id hd
    set hd(done) 0

    # Create toplevel
    toplevel $w -bd 5 -class TkRat
    wm title $w $t(insert_into_dbase)
    wm transient $w $parent

    # Populate window
    frame $w.keywords
    label $w.keywords.label -text $t(keywords):
    entry $w.keywords.entry -textvariable ${id}(keywords) -relief sunken \
	    -width 20
    pack $w.keywords.entry \
	 $w.keywords.label -side right
    set b($w.keywords.entry) keywords
    frame $w.extype
    label $w.extype.label -text $t(extype):
    frame $w.extype.b
    radiobutton $w.extype.b.none -text $t(none) -variable ${id}(extype) \
	    -value none
    set b($w.extype.b.none) exp_none
    radiobutton $w.extype.b.remove -text $t(remove) -variable ${id}(extype) \
	    -value remove
    set b($w.extype.b.remove) exp_remove
    radiobutton $w.extype.b.incoming -text $t(incoming) \
	    -variable ${id}(extype) -value incoming
    set b($w.extype.b.incoming) exp_incoming
    radiobutton $w.extype.b.backup -text $t(backup) -variable ${id}(extype) \
	    -value backup
    set b($w.extype.b.backup) exp_backup
    pack $w.extype.b.none \
	 $w.extype.b.remove \
	 $w.extype.b.incoming \
	 $w.extype.b.backup -side top -anchor w
    pack $w.extype.b \
	 $w.extype.label -side right -anchor nw
    frame $w.exdate
    label $w.exdate.label -text $t(exdate):
    entry $w.exdate.entry -textvariable ${id}(exdate) -relief sunken \
	    -width 20
    pack $w.exdate.entry \
	 $w.exdate.label -side right
    set b($w.exdate.entry) exp_date
    frame $w.buttons
    button $w.buttons.ok -default active -text $t(insert) \
	    -command "set ${id}(done) 1"
    button $w.buttons.cancel -text $t(cancel) -command "set ${id}(done) 0"
    pack $w.buttons.ok \
         $w.buttons.cancel -side left -expand 1 -pady 4
    pack $w.keywords \
	 $w.extype \
	 $w.exdate \
	 $w.buttons -side top -fill both

    set hd(extype) $option(def_extype)
    set hd(exdate) $option(def_exdate)
    bind $w <Return> "$w.buttons.ok invoke"
    bind $w <Escape> "$w.buttons.cancel invoke"
    wm protocol $w WM_DELETE_WINDOW "set ${id}(done) 0"
    ::tkrat::winctl::SetGeometry insertIntoDbase $w
    ::tkrat::winctl::ModalGrab $w $w.keywords.entry
    tkwait variable ${id}(done)

    # Do insert
    if { 1 == $hd(done) } {
	set exp [list and keywords $hd(keywords)]
	set result [list DBase dbase {} $hd(extype) $hd(exdate) $exp]
    } else {
	set result {}
    }
    ::tkrat::winctl::RecordGeometry insertIntoDbase $w
    foreach bn [array names b $w.*] {unset b($bn)}
    destroy $w
    unset hd
    return $result
}

# VFoldersUsesRole --
#
# Returns a list of names of folders which references the given role.
#
# Arguments:
# role  - Role to look for

proc VFoldersUsesRole {role} {
    global vFolderDef

    set results {}
    foreach id [array names vFolderDef] {
	set f(speed) ""
	unset f
	array set f [lindex $vFolderDef($id) 2]
	if {[info exists f(role)] && $role == $f(role)} {
	    lappend results [lindex $vFolderDef($id) 0]
	}
    }
    return $results
}

# VFolderPurgeBackups
#
# Removes old stale backups from a folder
#
# Arguments:
# fh - Folder handler

proc VFolderPurgeBackups {fh} {
    global option

    set cutoff [expr [clock seconds] - $option(compose_last_chance)]
    set deleted {}

    set num_msgs [lindex [$fh info] 1]
    for {set i 0} {$i <$num_msgs} {incr i} {
        set msg [$fh get $i]
        foreach h [$msg headers] {
            if {"X-TkRat-Internal-AutoBackup" == [lindex $h 0]} {
                if {[lindex $h 1] < $cutoff} {
                    lappend deleted $i
                }
                break
            }
        }
    }

    if {[llength $deleted]} {
        $fh setFlag $deleted deleted 1
        $fh update sync
    }
}
