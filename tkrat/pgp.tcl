# pgp.tcl --
#
# This file contains code which handles pgp interaction
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.


# RatGetPGPPassPhrase --
#
# Get the pgp pass phrase from the user
#
# Arguments:

proc RatGetPGPPassPhrase {} {
    global idCnt t

    # Create identifier
    set id pgpPass[incr idCnt]
    set w .$id
    upvar #0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(pgp_pass_phrase) 

    # Populate window
    label $w.label -text "$t(pgp_pass_phrase):"
    entry $w.entry -textvariable ${id}(phrase) -width 32 -show -
    button $w.button_ok -text $t(ok) -command "set ${id}(done) ok"
    button $w.button_cancel -text $t(abort) -command "set ${id}(done) abort"
    grid $w.label $w.entry -pady 5 -padx 5
    grid $w.button_ok $w.button_cancel
    bind $w.entry <Return> "set ${id}(done) ok"
    wm protocol $w WM_DELETE_WINDOW "set ${id}(done) abort"

    Place $w pgpPhrase
    ModalGrab $w $w.entry

    tkwait variable ${id}(done)

    RecordPos $w pgpPhrase
    destroy $w
    set ret [list $hd(done) $hd(phrase)]
    unset hd
    return $ret
}

# RatPGPError --
#
# Report an PGP error to the user. It should return either "ABORT" or
# "RETRY".
#
# Arguments:
# error	-	An error message

proc RatPGPError {error} {
    global idCnt t

    # Create identifier
    set id pgpProblem[incr idCnt]
    set w .$id
    upvar #0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm transient $w .
    wm title $w $t(pgp_problem) 

    # Populate window
    frame $w.f
    pack $w.f -padx 5 -pady 5 -fill both -expand 1

    label $w.f.label -text "$t(pgp_problem):"
    pack $w.f.label -side top -anchor w

    frame $w.f.t -relief sunken -bd 1
    scrollbar $w.f.t.scroll \
	-relief sunken \
	-command "$w.f.t.text yview" \
	-highlightthickness 0
    text $w.f.t.text \
	-yscroll "$w.f.t.scroll set" \
	-setgrid 1 \
	-relief raised \
	-bd 0 \
	-highlightthickness 0
    Size $w.f.t.text pgpError
    pack $w.f.t.scroll -side right -fill y
    pack $w.f.t.text -side left -expand yes -fill both
    regsub -all "\a" $error {} errmsg
    $w.f.t.text insert 1.0 $errmsg
    $w.f.t.text configure -state disabled
    pack $w.f.t -expand 1 -fill both

    frame $w.f.b
    button $w.f.b.retry -text $t(retry) -command "set ${id}(done) RETRY"
    button $w.f.b.abort -text $t(abort) -command "set ${id}(done) ABORT"
    pack $w.f.b.retry $w.f.b.abort -side left -expand 1 -pady 5
    pack $w.f.b -side bottom -fill x
    wm protocol $w WM_DELETE_WINDOW "set ${id}(done) ABORT"

    Place $w pgpError

    ModalGrab $w
    tkwait variable ${id}(done)

    RecordSize $w.f.t.text pgpError
    RecordPos $w pgpError
    destroy $w
    set action $hd(done)
    unset hd
    return $action
}

# RatPGPGetIds --
#
# Let the user select keys from her keyrings
#
# Arguments:
# proc	- procedure to call when done
# arg	- argument to procedure (before list of ids)

proc RatPGPGetIds {proc arg} {
    global idCnt t option

    # List keys
    if {[catch {RatPGP listkeys} keylist]} {
	Popup $keylist
	return
    }

    # Create identifier
    set id pgpGet[incr idCnt]
    set w .$id
    upvar #0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(select_keys) 

    # Add text
    set hd(list) $w.l
    rat_textlist::create $hd(list) [lindex $keylist 0]
    Size [rat_textlist::textwidget $hd(list)] pgpGet
    pack $w.l -side top -fill both -expand 1

    # Buttons
    frame $w.buttons
    button $w.buttons.ok -text $t(ok) \
	    -command "RatPGPGetIdsDone $w $id 1 [list $proc $arg]"
    button $w.buttons.sel -text $t(select_all) \
	    -command "rat_textlist::selection $hd(list) set 0 end"
    button $w.buttons.unsel -text $t(deselect_all) \
	    -command "rat_textlist::selection $hd(list) clear"
    button $w.buttons.cancel -text $t(cancel) \
	    -command "RatPGPGetIdsDone $w $id 0 [list $proc $arg]"
    pack $w.buttons.ok \
	 $w.buttons.sel \
	 $w.buttons.unsel \
	 $w.buttons.cancel -side left -expand 1
    pack $w.buttons -side bottom -pady 5 -fill x

    # Populate text widget
    set hd(keys) {}
    foreach e [lindex $keylist 1] {
	lappend hd(keys) [lrange $e 0 1]
	rat_textlist::insert $hd(list) end [lindex $e 2]
    }

    wm protocol $w WM_DELETE_WINDOW \
	    "RatPGPGetIdsDone $w $id 0 [list $proc $arg]"

    Place $w pgpGet
}

# RatPGPGetIdsDone --
#
# Calls when the selection is done
#
# Arguments:
# w	  -	The id selection window
# handler -	The handler which identifies the session window
# done    -	The users selection (1=ok, 0=cancel)
# proc	  -	procedure to call when done
# arg	  -	argument to procedure (before list of ids)

proc RatPGPGetIdsDone {w handler done proc arg} {
    upvar #0 $handler hd
    global option

    if {$done} {
	set ids {}
	foreach s [rat_textlist::selection $hd(list) get] {
	    lappend ids [lrange [lindex $hd(keys) $s] 0 1]
	}
	$proc $arg $ids
    }
    RecordPos $w pgpGet
    RecordSize [rat_textlist::textwidget $hd(list)] pgpGet
    catch {focus $hd(oldfocus)}
    destroy $w
    unset hd
}

# RatPGPAddKeys --
#
# Add keys to keyring
#
# Arguments:
# keys	  - Keys to add
# keyring - Keyring to add them to

proc RatPGPAddKeys {keys {keyring ""}} {
    global idCnt option t rat_tmp

    # Create identifier
    set id pgpInt[incr idCnt]
    upvar #0 $id hd

    # Setup file
    set hd(fileName) $rat_tmp/rat.[RatGenId]
    set f [open $hd(fileName) w]
    puts $f $keys
    close $f

    # Create command and run it
    if {"" != $option(pgp_path)} {
	set dir $option(pgp_path)/
    } else {
	set dir ""
    }
    if {$option(pgp_version) == 2} {
        set cmd "${dir}pgp -ka $hd(fileName) $keyring"
    } elseif {$option(pgp_version) == 5} {
        set cmd "${dir}pgpk -ka $hd(fileName) $keyring"
    } elseif {$option(pgp_version) == "gpg-1"} {
        set cmd "${dir}gpg --no-secmem-warning -q --import $hd(fileName)"
    } elseif {$option(pgp_version) == 6} {
        set cmd "${dir}pgp -ka $hd(fileName) $keyring"
    }
    set cmd "$cmd; echo '$t(press_return_to_dismiss)'; read FOO"

    RatBgExec ${id}(existStatus) "$option(terminal) \"$cmd\""

    trace variable hd(existStatus) w RatPGPAddKeysDone
}

# RatPGPAddKeysDone
#
# This gets called when the add command has run and should clean
# things up.
#
# Arguments:
# name1, name2 -        Variable specifiers
# op           -        Operation

proc RatPGPAddKeysDone {name1 name2 op} {
    upvar #0 $name1 hd

    file delete -force -- $hd(fileName) &
    unset hd
}

