# keydef.tcl --
#
# This file contains code which handles the key definitions window
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# The order of the definitions
set keyDefOrder(folder) {folder_key_find folder_key_compose folder_key_replya
	folder_key_replys folder_key_forward_i folder_key_forward_a
	folder_key_bounce folder_key_sync folder_key_netsync folder_key_update
        folder_key_delete folder_key_undelete folder_key_markunread
        folder_key_flag folder_key_nextu folder_key_next folder_key_prev
        folder_key_home folder_key_bottom folder_key_pagedown
        folder_key_pageup folder_key_linedown folder_key_lineup
        folder_key_cycle_header folder_key_print folder_key_close
        folder_key_openfile folder_key_online folder_key_quit}
set keyDefOrder(compose) {compose_key_send compose_key_abort 
	compose_key_editor compose_key_undo compose_key_cut
	compose_key_cut_all compose_key_copy compose_key_paste}

# KeyDef --
#
# Create a key definition window
#
# Arguments:
# area	-	Identifies the area of keys to define

proc KeyDef {area} {
    global option t b keyDefOrder

    # Create identifier
    set id kd
    upvar #0 $id hd
    set w .$id
    if {[winfo exists $w]} {
	destroy $w
	unset hd
    }
    set hd(do) 0
    set hd(state) ""
    set hd(w) $w

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(define_keys)

    # Buttons
    frame $w.but
    button $w.but.ok -text $t(ok) -command "KeyDefApply $area $id"
    button $w.but.delete -text $t(delete) \
	-command "set ${id}(state) delete; set ${id}(message) \"$t(do_delete)\""
    button $w.but.cancel -text $t(cancel) -command "unset $id; destroy $w"
    pack $w.but.ok \
	 $w.but.delete \
	 $w.but.cancel -side left -expand 1
    set b($w.but.ok) ok_and_apply
    set b($w.but.delete) keydef_delete
    set b($w.but.cancel) cancel

    pack $w.but -side bottom -fill x -pady 5

    # State line
    label $w.msg -textvariable ${id}(message) -relief raised -bd 1
    pack $w.msg -side bottom -fill x -pady 5 -padx 10

    # The canvas
    frame $w.f -relief sunken -bd 1
    scrollbar $w.f.scroll \
	    -relief sunken \
	    -bd 1 \
	    -command "$w.f.canvas yview" \
	    -highlightthickness 0
    set hd(canvas) $w.f.canvas
    canvas $w.f.canvas -yscrollcommand "$w.f.scroll set" -highlightthickness 0
    Size $w.f.canvas keyCanvas
    frame $w.f.canvas.f
    set hd(cid) [$w.f.canvas create window 0 0 \
	    -anchor nw \
	    -window $w.f.canvas.f]
    set fr $w.f.canvas.f
    pack $w.f.scroll -side right -fill y
    pack $w.f.canvas -side left -expand 1 -fill both
    pack $w.f -fill both

    # Create key windows
    foreach n $keyDefOrder($area) {
	set hd($n) $option($n)
	label ${fr}.${n}_label -text $t($n) -anchor e
	button ${fr}.${n}_button -text $t(add_key) \
		-command "AddKey ${fr}.${n}_f $n $id"
	set b(${fr}.${n}_button) keydef_add
	frame ${fr}.${n}_f -relief sunken -bd 1
	set b(${fr}.${n}_f) keydef_def
	grid ${fr}.${n}_label ${fr}.${n}_f ${fr}.${n}_button -sticky we -pady 5
	set hd(w_${n}) ${fr}.${n}_f
	PopulateKeyDef ${fr}.${n}_f $n $id
	if {![llength $hd($n)]} {
	    button ${fr}.${n}_f.b -relief flat -state disabled
	    pack ${fr}.${n}_f.b
	}
    }
    grid columnconfigure $w.f.canvas 1 -weight 1
    wm protocol $w WM_DELETE_WINDOW "unset $id; destroy $w"

    Place $w keydef

    # Resize canvas
    update idletasks
    set bbox [$hd(canvas) bbox $hd(cid)]
    eval {$hd(canvas) configure -scrollregion $bbox}
}

# PopulateKeyDef --
#
# Populates one keydef function
#
# Arguments:
# w	  - The frame to add the keys in
# name    - The name of the definitions
# handler - The handler for this keydef window

proc PopulateKeyDef {w name handler} {
    global idCnt b
    upvar #0 $handler hd

    foreach s [pack slaves $w] {
	destroy $s
    }
    foreach k $hd($name) {
	set hd($k) $name
	regsub {Key-} $k {} key
	set bn $w.b[incr idCnt]
	button $bn -text [string trim $key {<>}] -bd 1 \
	    -command "DeleteKeyDef $k $handler"
	pack $bn -side left -pady 2 -padx 2
	set b($bn) keydef_def
    }
}

# AddKey --
#
# Add a new key combination
#
# Arguments:
# w	  - The frame to add the keys in
# name    - The name of the definitions
# handler - The handler for this keydef window

proc AddKey {w name handler} {
    upvar #0 $handler hd
    global t

    set hd(mod) ""
    set hd(state) ""
    set hd(message) ""
    pack propagate $w 0
    foreach s [pack slaves $w] {
	destroy $s
    }
    label $w.label -text $t(press_key)
    set hd(state) $t(press_key)
    pack $w.label -expand 1
    bind $w.label <KeyPress> "KeyEvent p %K $w $name $handler; break"
    bind $w.label <KeyRelease> "KeyEvent r %K $w $name $handler; break"
    focus $w.label
}

# KeyEvent --
#
# Handle a key press or key release.
#
# Arguments:
# e	  - Which event
# key	  - The keysym
# w	  - The frame to add the keys in
# name    - The name of the definitions
# handler - The handler for this keydef window

proc KeyEvent {e key w name handler} {
    upvar #0 $handler hd
    global t

    if {[regexp {(Shift|Control|Alt|Mod[1-5]|Meta)(_[LR])?} $key tot mod]} {
	if {[string compare p $e]} {
	    regsub "$mod-" $hd(mod) {} hd(mod)
	} else {
	    set hd(mod) "$mod-$hd(mod)"
	}
    } elseif {[string compare r $e]} {
	set event "<$hd(mod)Key-$key>"
	set hd(state) ""
	if {[info exists hd($event)]} {
	    # The key already exists
	    set oname $hd($event)
	    if { 0 == [RatDialog [winfo toplevel $w] $t(add_key) \
			   "$t(key_defined) $t($oname)" {} 0 \
			   $t(replace_key) $t(cancel)]} {
	        # Remove old definition and update it
		set i [lsearch $hd($oname) $event]
		set hd($oname) [lreplace $hd($oname) $i $i]
		PopulateKeyDef $hd(w_$oname) $oname $handler
		lappend hd($name) $event
		set hd($event) $name
	    }
	} else {
	    lappend hd($name) $event
	    set hd($event) $name
	}
	destroy [pack slaves $w]
	PopulateKeyDef $w $name $handler
	pack propagate $w 1
    }
}

# DeleteKeyDef --
#
# Delete a key definition
#
# Arguments:
# event   - The key event that should be deleted
# handler - The handler for the keydef window

proc DeleteKeyDef {event handler} {
    upvar #0 $handler hd

    if {[string compare delete $hd(state)]} {
	return
    }
    set name $hd($event)
    set i [lsearch $hd($name) $event]
    set hd($name) [lreplace $hd($name) $i $i]
    PopulateKeyDef $hd(w_$name) $name $handler
    unset hd($event)
    set hd(state) ""
    set hd(message) ""
}

# KeyDefApply --
#
# Apply the key definitions
#
# Arguments:
# area	-	Identifies the area of keys to define
# handler - The handler for this keydef window

proc KeyDefApply {area handler} {
    upvar #0 $handler hd
    global option b

    set changed 0
    set remove {}
    foreach n [array names hd ${area}_key_*] {
	if {[string compare $option($n) $hd($n)]} {
	    foreach e $option($n) {
		if { -1 == [lsearch $hd($n) $e]} {
		    lappend remove $e
		}
	    }
	    set option($n) $hd($n)
	    set changed 1
	}
    }
    destroy $hd(w)
    foreach bn [array names b $hd(w).*] {unset b($bn)}
    unset hd

    if {$changed} {
	switch $area {
	folder {
		global folderWindowList

		foreach f [array names folderWindowList] {
		    upvar #0 $f fh
		    foreach e $remove {
			bind $fh(w) $e {}
		    }
		    FolderBind $f
		}
	    }
	compose {
		global composeWindowList

		foreach m $composeWindowList {
		    upvar #0 $m mh
		    foreach e $remove {
			bind $mh(toplevel) $e {}
		    }
		    ComposeBind $m
		}
	    }
	}
	SaveOptions
    }
}
