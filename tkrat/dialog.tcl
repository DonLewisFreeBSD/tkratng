#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

# RatLogin
# See ../doc/interface
proc RatLogin {host trial user prot port} {
    global t idCnt m

    set id login[incr idCnt]
    set w .$id
    upvar \#0 $id hd
    set hd(user) $user
    set hd(store) 0

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(login)

    # Populate window
    label $w.label -text "$t(opening) $prot $t(mailbox_on) $host:$port"
    frame $w.user
    label $w.user.label -text $t(user): -width 10 -anchor e
    entry $w.user.entry -textvariable ${id}(user) -width 20
    if {[string length $hd(user)]} {
	 $w.user.entry configure -state disabled
    }
    pack $w.user.label $w.user.entry -side left
    frame $w.passwd
    label $w.passwd.label -text $t(passwd): -width 10 -anchor e
    entry $w.passwd.entry -textvariable ${id}(passwd) -width 20 -show {-}
    pack $w.passwd.label $w.passwd.entry -side left
    checkbutton $w.store -text $t(store_passwd) -variable ${id}(store)
    set m($w.store) store_passwd

    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"

    pack $w.label  -side top -padx 5 -pady 5
    pack $w.user \
	 $w.passwd \
	 $w.store \
	 $w.buttons -side top -fill both -pady 2
    
    ::tkrat::winctl::SetGeometry ratLogin $w
    ::tkrat::winctl::ModalGrab $w $w.passwd.entry
    
    tkwait variable ${id}(done)

    ::tkrat::winctl::RecordGeometry ratLogin $w
    destroy $w
    unset m($w.store)
    update idletasks
    if { 1 == $hd(done) } {
	set r [list $hd(user) $hd(passwd) $hd(store)]
    } else {
	set r {{} {} 0}
    }
    unset hd
    return $r
}

# Popup --
#
# Show a message which the user has to acknowledge
#
# Arguments:
# message -	The message to show
# parent -	Parent window

proc Popup {message {parent {}}} {
    global t

    RatDialog $parent ! $message {} 0 $t(continue)
    update idletasks
}

# RatDialog --
#
# This looks almost like the tk dialog, except that it uses a message
# instead of a label and it doesn't set the font.
#
# This procedure displays a dialog box, waits for a button in the dialog
# to be invoked, then returns the index of the selected button.
#
# Arguments:
# parent -	Parent window
# title -	Title to display in dialog's decorative frame.
# text -	Message to display in dialog.
# bitmap -	Bitmap to display in dialog (empty string means none).
# default -	Index of button that is to display the default ring
#		(-1 means none).
# args -	One or more strings to display in buttons across the
#		bottom of the dialog box.

proc RatDialog {parent title text bitmap default args} {
    global tkPriv idCnt

    # 1. Create the top-level window and divide it into top
    # and bottom parts.

    set w .dialog[incr idCnt]
    catch {destroy $w}
    toplevel $w -class TkRat
    wm title $w $title
    wm iconname $w Dialog
    wm protocol $w WM_DELETE_WINDOW { }
    wm transient $w $parent

    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both -expand 1

    if {80 > [string length $text] && -1 == [string first $text "\n"]} {
        set aspect 3000
    } else {
        set aspect 600
    }

    # 2. Fill the top part with bitmap and message (use the option
    # database for -wraplength so that it can be overridden by
    # the caller).

    option add *Dialog.msg.wrapLength 3i widgetDefault
    message $w.msg -justify left -text $text -aspect $aspect
    pack $w.msg -in $w.top -side right -expand 1 -fill both -padx 3m -pady 3m
    if {$bitmap != ""} {
	label $w.bitmap -bitmap $bitmap
	pack $w.bitmap -in $w.top -side left -padx 3m -pady 3m
    }

    # 3. Create a row of buttons at the bottom of the dialog.

    set i 0
    foreach but $args {
	button $w.button$i -text $but -command "set tkPriv(button) $i"
	if {$i == $default} {
	    $w.button$i configure -default active
	}
	pack $w.button$i -in $w.bot -side left -expand 1 \
		-padx 3m -pady 2m
	bind $w.button$i <Return> "
	    $w.button$i configure -state active -relief sunken
	    update idletasks
	    after 100
	    set tkPriv(button) $i
            break
	"
	incr i
    }

    # 4. Create a binding for <Return> on the dialog if there is a
    # default button.

    if {$default >= 0} {
	bind $w <Return> "
	    $w.button$default configure -state active -relief sunken
	    update idletasks
	    after 100
	    set tkPriv(button) $default
	"
    }

    # 5. Withdraw the window, then update all the geometry information
    # so we know how big it wants to be, then center the window in the
    # display and de-iconify it.

    wm withdraw $w
    update idletasks
    set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
	    - [winfo vrootx [winfo parent $w]]}]
    set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
	    - [winfo vrooty [winfo parent $w]]}]
    wm geom $w +$x+$y
    wm deiconify $w

    # 6. Set a grab and claim the focus too.
    if {$default >= 0} {
	set f $w.button$default
    } else {
	set f $w
    }
    ::tkrat::winctl::ModalGrab $w $f

    # 7. Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    tkwait variable tkPriv(button)
    destroy $w
    return $tkPriv(button)
}

# RatText --
#
# Display a text to the user
#
# Arguments:
# title -	Title to display in text's decorative frame.
# text -	Message to display in text.

proc RatText {title text} {
    global idCnt t

    set text [string map [list "\a" ""] $text]

    # Create identifier
    set id rattext[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $title

    # Message part
    button $w.button -text $t(close) -command "destroy $w"
    text $w.text -yscroll "$w.scroll set" -relief sunken -bd 1
    scrollbar $w.scroll -relief sunken -bd 1 \
	    -command "$w.text yview"
    pack $w.button -side bottom -padx 5 -pady 5
    pack $w.scroll -side right -fill y
    pack $w.text -expand 1 -fill both
    $w.text insert end $text\n
    $w.text configure -state disabled

    bind $w <Escape> "$w.button invoke"
    bind $w.text <Destroy> "::tkrat::winctl::RecordGeometry ratText $w $w.text"
    ::tkrat::winctl::SetGeometry ratText $w $w.text
}

# bgerror --
#
# This is a modified version of bgerror. It allows one to include the
# stack trace in a bug report message.
#
# Arguments:
# err -			The error message.

proc bgerror {err} {
    global errorInfo t
    set info $errorInfo
    set button [tk_dialog .bgerrorDialog "Error in Tcl Script" \
	    "Error: $err" error 0 OK $t(send_bug) "Skip Messages" \
	    "Stack Trace"]
    if {$button == 0} {
	return -code ok
    } elseif {$button == 1} {
	SendBugReport [list [list "Stack Trace: $err" "$info"]]
	return -code ok
    } elseif {$button == 2} {
	return -code break
    }

    set w .bgerrorTrace
    catch {destroy $w}
    toplevel $w -class TkRat
    wm minsize $w 1 1
    wm title $w "Stack Trace for Error"
    wm iconname $w "Stack Trace"
    button $w.ok -text OK -command "destroy $w"
    text $w.text -relief sunken -bd 2 -yscrollcommand "$w.scroll set" \
	    -setgrid true -width 60 -height 20
    scrollbar $w.scroll -relief sunken -command "$w.text yview"
    pack $w.ok -side bottom -padx 3m -pady 2m
    pack $w.scroll -side right -fill y
    pack $w.text -side left -expand yes -fill both
    $w.text insert 0.0 $info
    $w.text mark set insert 0.0

    bind $w <Escape> "$w.ok invoke"

    # Center the window on the screen.

    wm withdraw $w
    update idletasks
    set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
	    - [winfo vrootx [winfo parent $w]]}]
    set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
	    - [winfo vrooty [winfo parent $w]]}]
    wm geom $w +$x+$y
    wm deiconify $w

    # Be sure to release any grabs that might be present on the
    # screen, since they could make it impossible for the user
    # to interact with the stack trace.

    if {[grab current .] != ""} {
	grab release [grab current .]
    }
    return -code ok
}

