# rat_list --
#
# Create a list of some things and let the user modify this list.

package provide rat_list 1.0

namespace eval rat_list {
    namespace export create
    variable openLists
    variable idCnt 0
}


# rat_list::create --
#
# Create a list window
#
# Arguments:
# listvar	- variable containing list
# identifier	- identifier for size of window etc
# addproc	- procedure to add new items, this will be called with a
#		  procedure name as argument. The add proc should call this
#		  procedure with the name of the new item as argument.
# editproc	- procedure to edit items Two arguments will be added:
#		      * name of item to edit
#		      * procedure to call if the name has changed
#			(with new name as argument)
# deleteproc	- proc to delete items (item to delete will be added as arg)
# dismissproc	- proc to call when dismissing
# title		- title string of window
# add		- text of add button
# edit		- text of edit button
# delete	- text of delete button
# dismiss	- text of dismiss button

proc rat_list::create {listvar identifier addproc editproc deleteproc \
	dismissproc title add edit delete dismiss} {
    variable openLists
    variable idCnt
    upvar #0 $listvar list
    
    # Make sure the list exists
    if {![info exists list]} {
	set list {}
    }

    # Check for already open window
    if {[info exists openLists($listvar)]} {
	wm deiconify $openLists($listvar)
	raise $openLists($listvar)
	puts foo
	return
    }

    # Create identifier
    set id l[incr idCnt]
    set w .rat_list_$id
    upvar #0 rat_list::$id hd

    set hd(identifier) $identifier
    set hd(w) $w
    set hd(listvar) $listvar
    set hd(editproc) $editproc
    set hd(deleteproc) $deleteproc
    set hd(dismissproc) $dismissproc
    set openLists($listvar) $w

    # Create window
    toplevel $w -class TkRat
    wm title $w $title

    frame $w.f
    listbox $w.f.list \
	-yscroll "$w.f.scroll set" \
	-exportselection false \
	-highlightthickness 0 \
	-selectmode extended \
	-relief sunken \
	-setgrid true
    scrollbar $w.f.scroll \
	-relief sunken \
	-command "$w.f.list yview" \
	-highlightthickness 0
    pack $w.f.scroll -side right -fill y
    pack $w.f.list -side left -expand 1 -fill both

    button $w.add -text $add -command "eval $addproc {{rat_list::add $id}}"
    button $w.edit -text $edit -command "rat_list::edit $id"
    button $w.delete -text $delete -command "rat_list::delete $id"
    button $w.dismiss -text $dismiss -command "rat_list::dismiss $id"

    pack $w.f -side left -expand 1 -fill both
    pack $w.add \
	 $w.edit \
	 $w.delete \
	 $w.dismiss -side top -padx 5 -fill x -expand 1

    bind $w.f.list <Double-1> "rat_list::edit $id"
    bind $w.f.list <ButtonRelease-1> "rat_list::setState $id"
    bind $w.f.list <KeyPress> "rat_list::setState $id"

    foreach elem [lsort -dictionary $list] {
	$w.f.list insert end $elem
    }

    set hd(list) $w.f.list
    rat_list::setState $id

    wm protocol $w WM_DELETE_WINDOW "rat_list::dismiss $id"

    ::tkrat::winctl::SetGeometry $identifier $w $w.f.list
}

proc rat_list::setState {id} {
    upvar #0 rat_list::$id hd

    set w $hd(w)
    set l [llength [$hd(list) curselection]]
    if {0 == $l} {
	set editState disabled
	set delState disabled
    } elseif {1 == $l} {
	set editState normal
	set delState normal
    } else {
	set editState disabled
	set delState normal
    }
    $w.edit configure -state $editState
    $w.delete configure -state $delState
}

proc rat_list::changeName {id old new} {
    upvar #0 rat_list::$id hd
    upvar #0 $hd(listvar) list

    set i [lsearch -exact $list $old]
    $hd(list) delete $i
    set list [lsort -dictionary [concat [lreplace $list $i $i] [list $new]]]
    set i [lsearch -exact $list $new]
    $hd(list) insert $i $new
}

proc rat_list::add {id elem} {
    upvar #0 rat_list::$id hd
    upvar #0 $hd(listvar) list

    if {"" != $elem} {
	lappend list $elem
	set list [lsort -dictionary $list]
	set i [lsearch -exact $list $elem]
	$hd(list) insert $i $elem
    }
}

proc rat_list::edit {id} {
    upvar #0 rat_list::$id hd

    set current [$hd(list) get [$hd(list) curselection]]
    eval $hd(editproc) [list $current [list rat_list::changeName $id $current]]
}

proc rat_list::delete {id} {
    upvar #0 rat_list::$id hd
    upvar #0 $hd(listvar) list

    foreach elemIndex [lsort -decreasing -integer [$hd(list) curselection]] {
	eval $hd(deleteproc) [list [$hd(list) get $elemIndex]]
	set list [lreplace $list $elemIndex $elemIndex]
	$hd(list) delete $elemIndex
    }
    rat_list::setState $id
}

proc rat_list::dismiss {id} {
    upvar #0 rat_list::$id hd
    variable openLists

    if {[string length $hd(dismissproc)]} {
	eval $hd(dismissproc)
    }
    unset openLists($hd(listvar))
    ::tkrat::winctl::RecordGeometry $hd(identifier) $hd(w) $hd(list)
    destroy $hd(w)
    unset hd
}
