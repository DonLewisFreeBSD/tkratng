# delattachments.tcl --
#
# Contains code for deleting attachments
#
#  TkRat software and its included text is Copyright 1996-2005 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

namespace eval ::tkrat::delattachments {
    namespace export delete
}

# ::tkrat::delattachments::delete --
#
# Show the delet attachments window
#
# Arguments:
# msg     - Message to perform delete attachments from
# handler - Handler of the folder window we are called from

proc ::tkrat::delattachments::delete {msg handler} {
    global idCnt t

    upvar \#0 $handler fh

    set id ::tkrat::delattachments::state[incr idCnt]
    upvar \#0 $id hd

    # Create toplevel
    set w .customcopy_[incr idCnt]
    toplevel $w -class TkRat -bd 5
    wm title $w $t(delete_attachments)
    wm transient $w $fh(w)

    # Initialize defaults
    set hd(msg) $msg
    set hd(folder_handler) $handler
    set hd(w) $w

    # Populate window
    message $w.msg -text $t(delete_attachments_expl) -aspect 350

    # Attachments
    labelframe $w.f -text $t(attachments_to_delete)
    add_attachments $w.f $id [$msg body] {} ""

    # Buttons
    frame $w.buttons
    button $w.buttons.delete -text $t(delete) -default active -state disabled \
        -command "::tkrat::delattachments::do $id"
    button $w.buttons.cancel -text $t(cancel) -command "destroy $w"
    pack $w.buttons.delete \
	 $w.buttons.cancel -side left -expand 1
    bind $w <Return> "$w.buttons.delete invoke"
    set hd(delete_button) $w.buttons.delete

    pack $w.msg $w.f $w.buttons -side top -fill x -expand 1

    # Place it
    ::tkrat::winctl::SetGeometry DeleteAttachments $w
    bind $w.buttons.delete <Destroy> \
        "::tkrat::delattachments::cleanup $id"
}

# ::tkrat::delattachments::cleanup --
#
# Cleans things up when the window is destroyed
#
# Arguments:
# handler - Hander of the delete attachments window

proc ::tkrat::delattachments::cleanup {handler} {
    upvar \#0 $handler hd

    ::tkrat::winctl::RecordGeometry DeleteAttachments $hd(w)
    unset hd
}

# ::tkrat::delattachments::add_attachments --
#
# Adds the attachments buttons
#
# Arguments:
# w       - Frame to add to
# handler - Hander of the delete attachments window
# body    - Body to add children of
# parent  - Parent specification
# leader  - Leading string

proc ::tkrat::delattachments::add_attachments {w handler body parent leader} {
    upvar \#0 $handler hd
    global idCnt

    set i 0
    foreach c [$body children] {
        set id [incr idCnt]
        lappend hd(buttons) $id
        set hd(id_$id) [concat $parent $i]
        set hd(state_$id) 0

        # Description of bodypart
        set type [join [string tolower [$c type]] /]
        set size [RatMangleNumber [$c size]]
        set desc " $type ($size)"
        if {[$c description] != ""} {
            set desc "$desc\n [$c description]"
        }
        if {[$c filename] != ""} {
            set desc "$desc\n [$c filename]"
        }

        # Widgets
        set f $w.a$id
        frame $f
        label $f.l -text $leader
        checkbutton $f.c -text $desc -justify left -anchor nw \
            -variable ${handler}(state_$id) \
            -command "::tkrat::delattachments::update_state $handler"
        pack $f.l $f.c -side left
        pack $f -side top -anchor w

        # Does this have children?
        if {"MULTIPART" != [lindex [$c type] 0]} {
            add_attachments $w $handler $c $hd(id_$id) "    $leader"
        }
        incr i
    }
}

# ::tkrat::delattachments::update_state --
#
# Update the state of the delete button
#
# Arguments:
# handler - Hander of the delete attachments window

proc ::tkrat::delattachments::update_state {handler} {
    upvar \#0 $handler hd

    set state disabled

    foreach b $hd(buttons) {
        if {$hd(state_$b)} {
            set state normal
            break
        }
    }
    $hd(delete_button) configure -state $state
}

# ::tkrat::delattachments::do --
#
# Actually do the deletion
#
# Arguments:
# handler - Hander of the delete attachments window

proc ::tkrat::delattachments::do {handler} {
    upvar \#0 $handler hd
    upvar \#0 $hd(folder_handler) fh
    global t

    # Create list of attachments to delete
    set attachments {}
    foreach b $hd(buttons) {
        if {$hd(state_$b)} {
            lappend attachments $hd(id_$b)
        }
    }

    # Perform deletion and insert new message
    if {[catch {$hd(msg) delete_attachments $attachments} nmsg]} {
        Popup $t(message_deleted)
    } else {
        $fh(folder_handler) insert $nmsg

        # Mark original message for deletion
        set index [$fh(folder_handler) find $hd(msg)]
        if {-1 != $index} {
            $fh(folder_handler) setFlag $index deleted 1
        }

        Sync $hd(folder_handler) update
    }

    # Destroy window
    destroy $hd(w)
}
