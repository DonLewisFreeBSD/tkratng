# alias.tcl --
#
# Code which handles aliases.
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén.
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# List of alias windows
set aliasWindows {}

# True if the address books have been modified
set bookMod 0


# Aliases --
#
# Display the aliases window
#
# Arguments:

proc Aliases {} {
    global idCnt t b option aliasWindows

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar \#0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(addressbook)
    set hd(w) $w
    set hd(old,default_book) $option(default_book)
    set hd(ignore_changes) 1

    # The menus
    frame $w.mbar -relief raised -bd 1
    FindAccelerators a {file edit show addrbooks}

    set m $w.mbar.file.m
    menubutton $w.mbar.file -text $t(file) -menu $m -underline $a(file)
    menu $m
    $m add command -label $t(new)... -command "AliasNew $id"
    set hd(new_menu) [list $m [$m index end]]
    $m add separator
    $m add command -label $t(reread_addresses) -command AliasRead
    set b($m,[$m index end]) reread_addresses
    set hd(read_menu) [list $m [$m index end]]
    $m add command -label $t(import_addresses) -command ScanAliases
    set hd(scan_menu) [list $m [$m index end]]
    set b($m,[$m index end]) import_aliases
    $m add separator
    $m add command -label $t(close) -command "AliasClose $id"
    set hd(close_menu) [list $m [$m index end]]

    set m $w.mbar.edit.m
    menubutton $w.mbar.edit -text $t(edit) -menu $m -underline $a(edit)
    menu $w.mbar.edit.m
    $m add command -label $t(edit) -command "AliasEdit $id"
    set hd(edit_menu) [list $m [$m index end]]
    $m add command -label $t(delete) -command "AliasDelete $id"
    set hd(delete_menu) [list $m [$m index end]]

    menubutton $w.mbar.show -text $t(show) -menu $w.mbar.show.m \
	    -underline $a(show)
    menu $w.mbar.show.m
    set hd(showmenu) $w.mbar.show.m
    set b($w.mbar.show) show_adrbook_menu

    menubutton $w.mbar.book -text $t(addrbooks) -menu $w.mbar.book.m \
	    -underline $a(addrbooks)
    set m $w.mbar.book.m
    menu $m
    $m add checkbutton -label $t(use_system_aliases) \
	    -variable option(use_system_aliases)
    set b($m,[$m index end]) use_system_aliases
    $m add separator
    $m add command -label $t(add)... -command AddrbookAdd
    set b($m,[$m index end]) add_addrbook
    $m add command -label $t(delete)... -command AddrbookDelete
    set b($m,[$m index end]) delete_addrbook
    $m add cascade -label $t(set_default)... -menu $m.sd
    set b($m,[$m index end]) set_default_addrbook
    menu $m.sd
    set hd(defaultmenu) $m.sd

    pack $w.mbar.file \
	 $w.mbar.edit \
	 $w.mbar.show \
	 $w.mbar.book -side left -padx 5

    RatBindMenus $w $id alias {new_menu read_menu close_menu
        delete_menu edit_menu}

    # List of aliases
    frame $w.l
    scrollbar $w.l.scroll \
	    -relief raised \
	    -bd 1 \
	    -highlightthickness 0 \
	    -command "$w.l.list yview"
    listbox $w.l.list \
	    -yscroll "$w.l.scroll set" \
	    -relief raised \
	    -bd 1 \
	    -exportselection false \
	    -highlightthickness 0 \
	    -selectmode extended \
            -setgrid true
    set hd(listbox) $w.l.list
    pack $w.l.scroll -side right -fill y
    pack $w.l.list -expand 1 -fill both
    set b($w.l.list) alias_list

    # Buttons
    frame $w.b
    button $w.b.new -text $t(new)... -command "AliasNew $id"
    button $w.b.edit -text $t(edit)... -command "AliasEdit $id"
    button $w.b.delete -text $t(delete) -command "AliasDelete $id"
    button $w.b.close -text $t(close) -command "destroy $w"
    pack $w.b.new $w.b.edit $w.b.delete $w.b.close -side left -padx 20 -pady 5
    set hd(edit_button) $w.b.edit
    set hd(delete_button) $w.b.delete

    # Pack it all
    pack $w.mbar -side top -fill x
    pack $w.b -side bottom -anchor center
    pack $w.l -expand 1 -fill both

    # Create the booklist
    AliasesUpdateBooklist $id

    # Populate list
    AliasesPopulate $id

    # Track list slections
    bind $hd(listbox) <<ListboxSelect>> "AliasUpdateState $id"
    bind $hd(listbox) <Double-1> "AliasEdit $id"
    bind $w <Escape> "$w.b.close invoke"

    ::tkrat::winctl::SetGeometry alias $w $hd(listbox)
    lappend aliasWindows $id

    bind $hd(listbox) <Destroy> "AliasClose $id"

    # Set up traces
    trace variable option(use_system_aliases) w "AliasesUpdateBooklist {}"
    trace variable option(addrbooks) w "AliasesUpdateBooklist {}"
}

# AliasUpdateState --
#
# Update the enabled/disabled state of edit and delete buttons
#
# Arguments:
# handler - The handler of the alias window

proc AliasUpdateState {handler} {
    upvar \#0 $handler hd
    global aliasBook

    # Edit is possible if exactly one element is selected
    set edit_state disabled
    if {[llength [$hd(listbox) curselection]] == 1} {
        set id [lindex $hd(aliasIds) [$hd(listbox) curselection]]
        set def [RatAlias get $id]
        if {$aliasBook(writable,[lindex $def 0])} {
            set edit_state normal
        }
    }
    $hd(edit_button) configure -state $edit_state
    [lindex $hd(edit_menu) 0] entryconfigure [lindex $hd(edit_menu) 1] \
        -state $edit_state

    # Delete is possible if at least one element is selected
    if {[llength [$hd(listbox) curselection]] > 0} {
        set delete_state normal
    } else {
        set delete_state disabled
    }
    $hd(delete_button) configure -state $delete_state
    [lindex $hd(delete_menu) 0] entryconfigure [lindex $hd(delete_menu) 1] \
        -state $delete_state
}


# AliasNew --
#
# Create a new alias and select it for editing.
#
# Arguments:
# handler -	The handler identifying the window

proc AliasNew {handler} {
    upvar \#0 $handler hd
    global t option aliasBook

    set result [AliasEditPanel $handler "" \
                    [list [lindex [lindex $option(addrbooks) 0] 0] \
                         "" "" {} {} {}]]
    if {[llength $result] != 7} {
        return
    }
    eval "RatAlias add $result"
    set aliasBook(changed,[lindex $result 0]) 1

    # Find out where it should be inserted
    set id [lindex $result 1]
    set hd(aliasIds) [lsort [concat $hd(aliasIds) $id]]
    set i [lsearch -exact $hd(aliasIds) $id]

    # Update entry in list of addresses
    set ne [AliasFormat $id [list \
                                 [lindex $result 0] \
                                 [lindex $result 2] \
                                 [lindex $result 3] \
                                 [lindex $result 4] \
                                 [lindex $result 5] \
                                 [lindex $result 6]]]
    set old_top \
	[expr int([lindex [$hd(listbox) yview] 0] * [llength $hd(aliasIds)])]
    $hd(listbox) insert $i $ne
    $hd(listbox) yview $old_top
    $hd(listbox) see $i
}

# AliasEdit --
#
# Handle editing of aliases
#
# Arguments:
# handler - The handler of the alias window

proc AliasEdit {handler} {
    upvar \#0 $handler hd
    global aliasBook

    # Only one element may be selected
    if {[llength [$hd(listbox) curselection]] != 1} {
        return
    }

    set id [lindex $hd(aliasIds) [$hd(listbox) curselection]]
    set def [RatAlias get $id]
    if {!$aliasBook(writable,[lindex $def 0])} {
        return
    }

    set result [AliasEditPanel $handler $id $def]
    if {[llength $result] != 7} {
        return
    }
    RatAlias delete $id
    eval "RatAlias add $result"

    set book_old [lindex $def 0]
    set book_new [lindex $result 0]
    if {$book_old != $book_new} {
	set aliasBook(changed,$book_old) 1
    }
    set aliasBook(changed,$book_new) 1

    # Possibly resort list of addresses
    set id_new [lindex $result 1]
    set d [lsearch -exact $hd(aliasIds) $id]
    if {$id != $id_new} {
	set hd(aliasIds) [lsort [lreplace $hd(aliasIds) $d $d $id_new]]
	set i [lsearch -exact $hd(aliasIds) $id_new]
    } else {
	set i $d
    }
    # Update entry in list of addresses
    set ne [AliasFormat $id_new [list \
                                     [lindex $result 0] \
                                     [lindex $result 2] \
                                     [lindex $result 3] \
                                     [lindex $result 4] \
                                     [lindex $result 5] \
                                     [lindex $result 6]]]
    set old_top \
	[expr int([lindex [$hd(listbox) yview] 0] * [llength $hd(aliasIds)])]
    $hd(listbox) delete $d
    $hd(listbox) insert $i $ne
    $hd(listbox) yview $old_top
    $hd(listbox) selection set $i
    $hd(listbox) see $i
}

# AliasEditPanel --
#
# Show alias edit window
#
# Arguments:
# handler - The handler of the alias window
# name    - Name of address book entry
# def     - Definition of address book entry

proc AliasEditPanel {handler name def} {
    upvar \#0 $handler hd
    global t aliasBook idCnt tk_version

    # Create toplevel
    set w .al[incr idCnt]
    toplevel $w -class TkRat -bd 2 -relief flat
    wm title $w $t(edit_address)

    # The alias fields
    label $w.alias_lab -text $t(alias): -anchor e
    entry $w.alias -textvariable ${handler}(alias)
    set b($w.alias) alias_alias
    set hd(alias_focus) $w.alias
    label $w.fullname_lab -text $t(fullname): -anchor e
    entry $w.fullname -textvariable ${handler}(fullname)
    set b($w.fullname) alias_fullname
    label $w.email_lab -text $t(email_address): -anchor e
    text $w.email -height 3
    set b($w.email) alias_content
    set hd(email_address_text) $w.email
    label $w.comment_lab -text $t(comment): -anchor e
    text $w.comment -height 3
    set b($w.comment) alias_comment
    set hd(comment_text) $w.comment
    label $w.addrbook_lab -text $t(addressbook): -anchor e
    set m $w.addrbook.m
    menubutton $w.addrbook -textvariable ${handler}(addrbook) \
	-indicatoron 1 -menu $m -bd 2 -relief raised -anchor w -justify left
    set b($w.addrbook) alias_adr_book
    menu $m -postcommand "PopulateAddrbookMenu $m ${handler}(addrbook)"
    label $w.pgp_actions_lab -text $t(pgp_actions): -anchor e
    set m $w.pgp_actions.m
    menubutton $w.pgp_actions -textvariable ${handler}(pgp_actions_t) \
	-indicatoron 1 -menu $m -bd 2 -relief raised -anchor w -justify left
    set b($w.pgp_actions) alias_pgp_actions
    menu $m
    foreach v {none sign encrypt sign_encrypt} {
	$m add command -label $t($v) \
	    -command "[list set ${handler}(pgp_actions) $v]; \
                            UpdatePGPState $handler"
    }
    label $w.pgp_key_lab -text $t(pgp_key): -anchor e
    frame $w.pgp_key
    set m $w.pgp_key.mb.m
    menubutton $w.pgp_key.mb -textvariable ${handler}(pgp_key_t) \
        -indicatoron 1 -menu $m -bd 2 -relief raised -anchor w -justify left
    set b($w.pgp_key.mb) alias_pgp_key
    menu $m -postcommand "PopulatePGPKeyMenu $m ${handler}"
    place $w.pgp_key.mb -relwidth 1.0
    set hd(infowidgets) [list $w.alias $w.fullname $w.email \
			     $w.comment  $w.pgp_actions $w.pgp_key.mb]
    set hd(addrbookwidget) $w.addrbook

    OkButtons $w $t(ok) $t(cancel) "set ${handler}(done)"
    set hd(ok_button) $w.buttons.ok

    grid $w.alias_lab $w.alias -sticky ew -padx 2 -pady 2
    grid $w.fullname_lab $w.fullname -sticky ew -padx 2 -pady 2
    grid $w.email_lab $w.email -sticky ewn -padx 2 -pady 2
    grid rowconfigure $w [expr [lindex [grid size $w] 1] - 1] -weight 1
    grid $w.comment_lab $w.comment -sticky ewn -padx 2 -pady 2
    grid rowconfigure $w [expr [lindex [grid size $w] 1] - 1] -weight 1
    grid $w.addrbook_lab $w.addrbook -sticky ew -padx 2 -pady 2
    grid $w.pgp_actions_lab $w.pgp_actions -sticky ew -padx 2 -pady 2
    grid $w.pgp_key_lab $w.pgp_key -sticky nsew -padx 2 -pady 2
    grid $w.buttons - -sticky ew

    grid configure $w.email -sticky nsew
    grid configure $w.comment -sticky nsew
    grid columnconfigure $w 1 -weight 1

    # Initialize values
    set hd(alias) $name
    set hd(addrbook) [lindex $def 0]
    set hd(fullname) [lindex $def 1]
    $hd(email_address_text) insert end [lindex $def 2]
    $hd(comment_text) insert end [lindex $def 3]
    set sign [expr [lsearch -exact [lindex $def 5] pgp_sign] != -1]
    set encrypt [expr [lsearch -exact [lindex $def 5] pgp_encrypt] != -1]
    if {$sign & $encrypt} {
        set hd(pgp_actions) sign_encrypt
    } elseif {$sign} {
        set hd(pgp_actions) sign
    } elseif {$encrypt} {
        set hd(pgp_actions) encrypt
    } else {
        set hd(pgp_actions) none
    }
    set hd(pgp_key) {}
    trace variable hd(pgp_key) w [list UpdatePGPState $handler]
    set hd(pgp_key) [lindex $def 4]

    # Handle ok button
    trace variable hd(alias) w [list AliasWinUpdateOk $handler]
    bind $hd(email_address_text) <KeyRelease> [list AliasWinUpdateOk $handler]
    bind $hd(email_address_text) <<Paste>> [list AliasWinUpdateOk $handler]
    bind $hd(email_address_text) <<Cut>> [list AliasWinUpdateOk $handler]
    AliasWinUpdateOk $handler

    # Show and wait for window
    wm protocol $w WM_DELETE_WINDOW "set ${handler}(done) 0"
    ::tkrat::winctl::SetGeometry aliasDetail $w
    after idle $hd(alias_focus) selection range 0 end
    focus $hd(alias_focus)
    tkwait variable ${handler}(done)
    trace vdelete hd(alias) w [list AliasWinUpdateOk $handler]
    trace vdelete hd(pgp_key) w [list UpdatePGPState $handler]

    if {$hd(done)} {
        set flags {}
        switch $hd(pgp_actions) {
            sign_encrypt {
                lappend flags pgp_sign
                lappend flags pgp_encrypt
            }
            sign {
                lappend flags pgp_sign
            }
            encrypt {
                lappend flags pgp_encrypt
            }
        }
        set content [string trim [$hd(email_address_text) get 1.0 end]]
        set comment [string trim [$hd(comment_text) get 1.0 end]]
        set r [list $hd(addrbook) $hd(alias) $hd(fullname) $content $comment \
                   $hd(pgp_key) $flags]
    } else {
        set r {}
    }
    destroy $w
    return $r
}

# AliasWinUpdateOk --
#
# Update state of ok button
#
# Arguments:
# handler - The handler of the alias window
# args    - Extra trace arguments

proc AliasWinUpdateOk {handler args} {
    upvar \#0 $handler hd
    global tk_version

    set content [string trim [$hd(email_address_text) get 1.0 end]]
    if {"" == $hd(alias) || "" == $content} {
        $hd(ok_button) configure -state disabled
    } else {
        $hd(ok_button) configure -state normal
    }
}

# PopulateAddrbookMenu --
#
# Populate the address book menu
#
# Arguments:
# m   - Menu to populate
# var - Variable to store selections in

proc PopulateAddrbookMenu {m var} {
    global aliasBook

    $m delete 0 end
    foreach book [array names aliasBook writable,*] {
	regsub writable, $book {} name
	$m add command -label $name -command [list set $var $name]
	if {!$aliasBook(writable,$name)} {
	    $m entryconfigure end -state disabled
	}
    }
}

# PopulatePGPKeyMenu --
#
# Populate the pgp key menu.
#
# Arguments:
# m       - Menu to populate
# handler - The handler of the alias window

proc PopulatePGPKeyMenu {m handler} {
    global t

    $m delete 0 end
    foreach k [lindex [RatPGP listkeys] 1] {
	if {[lindex $k 5]} {
	    set desc "[join [lindex $k 3] {, }]; [lindex $k 2]"
	    set id($desc) [lindex $k 0]
	}
    }
    $m add command -label "- $t(auto) -" \
	-command [list set ${handler}(pgp_key) {}]
    foreach d [lsort [array names id]] {
	$m add command -label "$d" \
	    -command [list set ${handler}(pgp_key) [list $id($d) $d]]
    }
    FixMenu $m
}

# AliasesUpdateBooklist --
#
# Update the list of known books for an alias window
# Arguments:
# handler - The handler of the alias window

proc AliasesUpdateBooklist {handler args} {
    global aliasWindows option aliasBook

    if {"" == $handler} {
	set hds $aliasWindows
    } else {
	set hds $handler
    }

    foreach handler $hds {
	upvar \#0 $handler hd

	$hd(showmenu) delete 0 end
	$hd(defaultmenu) delete 0 end

	foreach a $option(addrbooks) {
	    set book [lindex $a 0]
	    if {![info exists hd(show,$book)]} {
		set hd(show,$book) 1
	    }
	}
	set sysbook [lindex $option(system_aliases) 0]
	if {$option(use_system_aliases)} {
	    if {![info exists hd(show,$sysbook)]} {
		set hd(show,$sysbook) 1
	    }
	} else {
	    catch {unset hd(show,$sysbook)}
	}

	foreach book [array names aliasBook writable,*] {
	    regsub writable, $book {} name
	    $hd(showmenu) add checkbutton -label $name \
		    -variable ${handler}(show,$name) \
		    -command "AliasesPopulate $handler"
	    if {$aliasBook(writable,$name)} {
		$hd(defaultmenu) add radiobutton -label $name \
			-variable option(default_book) -value $name \
			-command {set bookMod 1}
	    }
	}
    }
}

# AliasesFormat --
#
# Format a single alias for the list
#
# Arguments:
# a  - Name of alias
# ac - Alias content to format

proc AliasFormat {a ac} {
    return [format "%-13s  %-20s  %s" $a [lindex $ac 1] [lindex $ac 2]]
}

# AliasesPopulate --
#
# Populate the list of addresses
#
# Arguments:
# handler - The handler of the alias window

proc AliasesPopulate {{handler {}}} {
    global aliasWindows

    if {"" == $handler} {
	set hds $aliasWindows
    } else {
	set hds $handler
    }

    foreach handler $hds {
	upvar \#0 $handler hd

	set old [$hd(listbox) curselection]
	RatAlias list alias
	set top [lindex [$hd(listbox) yview] 0]
	$hd(listbox) delete 0 end
	set hd(aliasIds) {}
	foreach a [lsort [array names alias]] {
	    set book [lindex $alias($a) 0]
	    if {$hd(show,$book)} {
		lappend hd(aliasIds) $a
		$hd(listbox) insert end [AliasFormat $a $alias($a)]
	    }
	}
	$hd(listbox) yview moveto $top
	if {"" != $old} {
	    set i [lsearch -exact $hd(aliasIds) $old]
	    if { -1 != $i } {
		$hd(listbox) selection set $i
		$hd(listbox) see $i
	    }
	}
        AliasUpdateState $handler
    }
}

# UpdatePGPState --
#
# Update the pgp state in address view
#
# Arguments:
# handler - The handler of the alias window

proc UpdatePGPState {handler args} {
    upvar \#0 $handler hd
    global t

    set hd(pgp_actions_t) $t($hd(pgp_actions))
    if {0 < [llength $hd(pgp_key)]} {
	set hd(pgp_key_t) [lindex $hd(pgp_key) 1]
    } else {
	set hd(pgp_key_t) "- $t(auto) -"
    }
}

# AliasSave --
#
# Save the aliases (if needed)
#
# Arguments:

proc AliasSave {} {
    global option bookMod aliasBook

    set books $option(addrbooks)
    if {$option(use_system_aliases)} {
	lappend books $option(system_aliases)
    }
    foreach book $books {
	if {$aliasBook(changed,[lindex $book 0])} {
	    RatAlias save [lindex $book 0] [lindex $book 2]
	}
    }

    if {$bookMod} {
	SaveOptions
	set bookMod 0
    }
}

# AliasClose --
#   
# Close an aliases window.
#   
# Arguments:
# handler - The handler of the alias window
    
proc AliasClose {handler} {
    global aliasWindows b
    upvar \#0 $handler hd
    
    bind $hd(listbox) <Destroy> {}
    set i [lsearch -exact $aliasWindows $handler]
    if {-1 != $i} {
	set aliasWindows [lreplace $aliasWindows $i $i]
    }
    ::tkrat::winctl::RecordGeometry alias $hd(w) $hd(listbox)
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    destroy $hd(w)
    unset hd
    AliasSave
}           

# AliasDelete --
#
# Deletes aliases from the alias list.
#
# Arguments:
# handler -	The handler identifying the window

proc AliasDelete {handler} {
    upvar \#0 $handler hd
    global aliasBook

    foreach a [$hd(listbox) curselection] {
	set alias [lindex $hd(aliasIds) $a]
	set aliasBook(changed,[lindex [RatAlias get $alias] 0]) 1
	RatAlias delete $alias
    }
    AliasesPopulate
}

# AddrbookAdd --
#
# Create a new address book
#
# Arguments:

proc AddrbookAdd {} {
    global idCnt t b

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar \#0 $id hd

    # Create toplevel
    toplevel $w -bd 5 -class TkRat
    wm title $w $t(new)
    set hd(w) $w

    label $w.n_label -text $t(name): -anchor e
    entry $w.n_entry -textvariable ${id}(name)
    set b($w.n_entry) name_of_adrbook
    grid $w.n_label $w.n_entry -sticky ew

    label $w.f_label -text $t(filename): -anchor e
    entry $w.f_entry -textvariable ${id}(file) -width 40
    set b($w.f_entry) name_of_adrbook_file
    grid $w.f_label $w.f_entry -sticky ew

    button $w.browse -text $t(browse) -command "AddrbookBrowse $id"
    set b($w.browse) file_browse
    grid x $w.browse -sticky e

    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 1 -weight 1

    # Buttons
    OkButtons $w $t(ok) $t(cancel) "AddrbookAddDone $id"
    bind $w <Return> ""
    bind $w.n_label <Destroy> "AddrbookAddDone $id 0"
    grid $w.buttons - -pady 5 -sticky ew

    ::tkrat::winctl::SetGeometry addrbookAdd $hd(w)
    update
    set hd(oldfocus) [focus]
    focus $w.n_entry
}

# AddrbookBrowse --
#
# Browse for addrbook file
#
# Arguments:
# handler - The handler of the add window

proc AddrbookBrowse {handler} {
    upvar \#0 $handler hd
    global t option
    
    set hd(file) [rat_fbox::run \
                      -parent $hd(w) \
                      -title $t(new) \
                      -initialdir $option(initialdir) \
                      -ok $t(save) \
                      -mode save]
    if {"" != $hd(file) && $option(initialdir) != [file dirname $hd(file)]} {
        set option(initialdir) [file dirname $hd(file)]
        SaveOptions
    }
}

# AddrbookAddDone
#
# Called when an address book add window is done
#
# Arguments
# handler - The handler of the add window
# action  - What to do

proc AddrbookAddDone {handler action} {
    upvar \#0 $handler hd
    global option t b bookMod aliasBook env

    if {$action} {
	if {![string length $hd(name)]} {
	    Popup $t(need_name) $hd(w)
	    return
	}
	if {[info exists aliasBook(writable,$hd(name))]} {
	    Popup $t(book_already_exists) $hd(w)
	    return
	}
	set dir [file dirname $hd(file)]
	if {("/" != [string index $hd(file) 0]
	     && "~" != [string index $hd(file) 0])
	    || ([file exists $hd(file)] && ![file readable $hd(file)])
	    || [file isdirectory $hd(file)]
	    || (![file exists $hd(file)] && ![file writable $dir])} {
	    Popup "$t(illegal_file_spec): $hd(file)" $hd(w)
	    return
	}
	if {([file isfile $hd(file)] && [file writable $hd(file)])
		|| (![file exists $hd(file)] && [file isdirectory $dir]
		    && [file writable $dir])} {
	    set aliasBook(writable,$hd(name)) 1
	} else {
	    set aliasBook(writable,$hd(name)) 0
	}
	set aliasBook(changed,$hd(name)) 0
	set bookMod 1
	lappend option(addrbooks) [list $hd(name) tkrat $hd(file)]
	if {[file exists $hd(file)]} {
	    RatAlias read $hd(name) $hd(file)
            AliasesPopulate
	}
    }

    bind $hd(w).n_label <Destroy> {}
    ::tkrat::winctl::RecordGeometry addrbookAdd $hd(w)
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    unset hd
}

# AddrbookDelete --
#
# Delete a address book
#
# Arguments:

proc AddrbookDelete {} {
    global idCnt t b option

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar \#0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(delete)
    set hd(w) $w

    # List of books
    frame $w.l
    scrollbar $w.l.scroll \
	    -bd 1 \
	    -highlightthickness 0 \
	    -command "$w.l.list yview"
    listbox $w.l.list \
	    -yscroll "$w.l.scroll set" \
	    -bd 1 \
	    -exportselection false \
	    -highlightthickness 0 \
	    -selectmode extended
    set hd(listbox) $w.l.list
    pack $w.l.scroll -side right -fill y
    pack $w.l.list -expand 1 -fill both
    set b($w.l.list) list_of_books_to_delete

    # Buttons
    OkButtons $w $t(delete) $t(cancel) "AddrbookDeleteDone $id"

    # Pack it
    pack $w.l \
	 $w.buttons -side top -padx 5 -pady 5 -expand 1 -fill both

    # Populate list
    foreach book [lsort $option(addrbooks)] {
	$hd(listbox) insert end [lindex $book 0]
    }

    bind $hd(listbox) <Destroy> "AddrbookDeleteDone $id 0"
    ::tkrat::winctl::SetGeometry addrBookDelete $w $w.l.list
}

# AddrbookDeleteDone
#
# Called when an address book delete window is done
#
# Arguments
# handler  -	The handler of the delete window
# action   -	What to do

proc AddrbookDeleteDone {handler action} {
    upvar \#0 $handler hd
    global option aliasBook bookMod t b

    if {$action} {
	foreach s [$hd(listbox) curselection] {
	    lappend del [$hd(listbox) get $s]
	}
	set keep {}
	set remove {}
	foreach book $option(addrbooks) {
	    if {-1 == [lsearch -exact $del [lindex $book 0]]} {
		lappend keep $book
	    } else {
		lappend remove [lindex $book 0]
	    }
	}
	if {-1 != [lsearch -exact $remove $option(default_book)]} {
	    set newDefault {}
	    foreach book [array names aliasBook writable,*] {
		if {!$aliasBook($book)} {
		    continue
		}
		regsub writable, $book {} name
		if {-1 == [lsearch -exact $remove $name]} {
		    set newDefault $name
		    break
		}
	    }
	    if {![string length $newDefault]} {
		Popup $t(need_writable_book) $hd(w)
		return
	    }
	    set option(default_book) $newDefault
	}
	foreach r $remove {
	    unset aliasBook(writable,$r)
	    unset aliasBook(changed,$r)
	}
	set option(addrbooks) $keep
	RatAlias list alias
	foreach a [array names alias] {
	    if {-1 != [lsearch -exact $del [lindex $alias($a) 0]]} {
		RatAlias delete $a
	    }
	}
	AliasesPopulate
	set bookMod 1
    }

    bind $hd(listbox) <Destroy> {}
    ::tkrat::::winctl::RecordGeometry addrBookDelete $hd(w) $hd(listbox)
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    destroy $hd(w)
    unset hd
}

# AliasExtract --
#
# Extracts aliases from the current message
#
# Arguments:
# handler - The handler of the folder window
# msgs    - Messages to extract from

proc AliasExtract {handler msgs} {
    global idCnt t b aliasBook option
    upvar \#0 $handler fh

    # Get list of known addresses
    RatAlias list alias
    foreach a [array names alias] {
	foreach adr [split [lindex $alias($a) 2]] {
	    set present([string tolower $adr]) 1
	}
    }

    # Extract the addresses
    set adrlist {}
    foreach msg $msgs {
        foreach a [$msg get from reply_to sender cc bcc to] {
            if {[$a isMe]} {
                continue
            }
            set good 1
            foreach a2 $adrlist {
                if {![$a compare $a2]} {
                    set good 0
                    break
                }
            }
            if {[info exists present([string tolower [$a get mail]])]} {
                set good 0
            }
            if {$good} {
                lappend adrlist $a
            }
        }
    }

    # Check that we found something
    if {![llength $adrlist]} {
	Popup $t(could_not_find_adr) $fh(toplevel)
	return
    }

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar \#0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(extract_adr)
    set hd(w) $w

    # Create address book menu
    frame $w.book
    label $w.book.label -text $t(addrbook):
    set hd(book) $option(default_book)
    menubutton $w.book.menu -textvariable ${id}(book) -relief raised \
	    -menu $w.book.menu.m -width 20 -indicatoron 1
    set b($w.book.menu) aliases_adr_book
    menu $w.book.menu.m
    foreach book [array names aliasBook writable,*] {
	if {!$aliasBook($book)} {
	    continue
	}
	regsub writable, $book {} name
	$w.book.menu.m add radiobutton -label $name -value $name \
		-variable ${id}(book)
    }
    pack $w.book.label \
         $w.book.menu -side left
    pack $w.book -side top -pady 5

    # Create frame with aliases to add
    frame $w.f
    label $w.f.use -text $t(use)
    label $w.f.name -text $t(alias) -width 8
    label $w.f.fname -text $t(fullname) -width 20
    label $w.f.content -text $t(content) -width 35
    label $w.f.comment -text $t(comment) -width 30
    grid $w.f.use $w.f.name $w.f.fname $w.f.content $w.f.comment -sticky w
    canvas $w.f.c
    frame $w.f.c.f
    set f $w.f.c.f
    set totlist ""
    foreach a $adrlist {
	if {[string length $totlist]} {
	    set totlist "$totlist,\n[string tolower [$a get mail]]"
	} else {
	    set totlist [string tolower [$a get mail]]
	}
	incr idCnt
	set hd($idCnt,use) 1
	set name [string tolower [lindex [$a get name] 0]]
	if {![string length $name] || [info exists alias($name)]} {
	    set name2 [string tolower [lindex [split [$a get mail] @.] 0]]
	    if {![string length $name]} {
		set name $name2
	    }
	    if {[info exist alias($name2)]} {
		for {set i 2} {[info exists alias($name2)]} {incr i} {
		    set name2 $name$i
		}
	    }
	    set name $name2
	}
	set alias($name) ""
	set hd($idCnt,name) $name
	set hd($idCnt,fname) [$a get name]
	set hd($idCnt,content) [string tolower [$a get mail]]
	checkbutton $f.c$idCnt -variable ${id}($idCnt,use)
	entry $f.en$idCnt -textvariable ${id}($idCnt,name) -width 8
	bind $f.en$idCnt <space> {bell; break}
	entry $f.ef$idCnt -textvariable ${id}($idCnt,fname) -width 20
	entry $f.ec$idCnt -textvariable ${id}($idCnt,content) -width 35
	entry $f.ek$idCnt -textvariable ${id}($idCnt,comment) -width 30
	set b($f.c$idCnt) aliasadd_use
	set b($f.en$idCnt) alias_alias
	set b($f.ef$idCnt) alias_fullname
	set b($f.ec$idCnt) alias_content
	set b($f.ek$idCnt) alias_comment
	grid $f.c$idCnt \
	     $f.en$idCnt \
	     $f.ef$idCnt \
	     $f.ec$idCnt \
	     $f.ek$idCnt -sticky we
	set idw $idCnt
    }
    set num [llength $adrlist]
    if {$num > 1} {
	incr idCnt
	set hd($idCnt,use) 0
	set hd($idCnt,content) $totlist
	checkbutton $f.c$idCnt -variable ${id}($idCnt,use)
	entry $f.en$idCnt -textvariable ${id}($idCnt,name) -width 8
	entry $f.ef$idCnt -textvariable ${id}($idCnt,fname) -width 20
	if {$num > 10} {
	    frame $f.ec$idCnt
	    scrollbar $f.ec$idCnt.scroll -relief sunken \
		    -command "$f.ec$idCnt.text yview" -highlightthickness 0
	    text $f.ec$idCnt.text -width 35 -height 10 -wrap none \
		    -yscroll "$f.ec$idCnt.scroll set"
	    pack $f.ec$idCnt.scroll -side right -fill y 
	    pack $f.ec$idCnt.text -expand yes -fill both
	    set hd(listcmd) $f.ec$idCnt.text
	} else {
	    text $f.ec$idCnt -width 35 -height $num -wrap none
	    set hd(listcmd) $f.ec$idCnt
	}
	$hd(listcmd) insert 1.0 $totlist
	set hd(listvar) $idCnt,content
	entry $f.ek$idCnt -textvariable ${id}($idCnt,comment) -width 30
	set b($f.c$idCnt) aliasadd_use
	set b($f.en$idCnt) alias_alias
	set b($f.ef$idCnt) alias_fullname
	set b($f.ec$idCnt) alias_content
	set b($f.ek$idCnt) alias_comment
	grid $f.c$idCnt \
	     $f.en$idCnt \
	     $f.ef$idCnt \
	     $f.ec$idCnt \
	     $f.ek$idCnt -sticky wen
    }
    grid columnconfigure $f 1 -weight 1
    grid columnconfigure $f 2 -weight 1
    grid columnconfigure $f 3 -weight 1
    grid columnconfigure $f 4 -weight 1
    set wid [$w.f.c create window 0 0 -anchor nw -window $f]
    update idletasks
    set bbox [$w.f.c bbox $wid]
    $w.f.c configure -scrollregion $bbox
    set height [lindex $bbox 3]
    set width [lindex $bbox 2]
    set maxheight [expr {[winfo screenheight $w] - 200}]
    if {$height > $maxheight} {
	scrollbar $w.f.s -command "$w.f.c yview" \
		  -highlightthickness 0 -bd 1
	$w.f.c configure -yscrollcommand "$w.f.s set"
	set height $maxheight
	set scroll $w.f.s
    } else {
	set scroll -
    }
    grid $w.f.c - - - - $scroll -sticky ns
    $w.f.c configure -width $width -height $height
    pack $w.f -side top -fill both

    # Create buttons
    frame $w.buttons
    button $w.buttons.add -text $t(add_aliases) \
	-command "AliasExtractDone $id 1" -default active
    button $w.buttons.unmark -text $t(clear_selection) \
	-command "AliasExtractClearSelection $id"
    button $w.buttons.cancel -text $t(cancel) \
	-command "AliasExtractDone $id 0"
    pack $w.buttons.add \
	$w.buttons.unmark \
	$w.buttons.cancel -side left -expand 1
    bind $w <Return> "AliasExtractDone $id 1"

    pack $w.buttons -side bottom -pady 5 -fill x

    bind $w.buttons.add <Destroy> "AliasExtractDone $id 0"
    ::tkrat::winctl::SetGeometry extractAlias $w
}

# AliasExtractDone --
#
# The alias extract window is now done.
#
# Arguments:
# handler - The handler of the extract window
# action  - Which action we should take

proc AliasExtractDone {handler action} {
    upvar \#0 $handler hd
    global t b aliasBook

    # Find which entries we should use
    set ids {}
    foreach i [array names hd *,use] {
	if {$hd($i)} {
	    lappend ids [lindex [split $i ,] 0]
	}
    }

    if { 1 == $action} {
	if {[info exists hd(listcmd)]} {
	    set hd($hd(listvar)) [$hd(listcmd) get 1.0 end]
	}
	# Add the aliases
	foreach id $ids {
	    if {![string length $hd($id,name)]} {
		Popup $t(missing_alias_name) $fh(toplevel)
		continue
	    }
	    RatAlias add $hd(book) $hd($id,name) $hd($id,fname) \
		    $hd($id,content) $hd($id,comment) {}
	    set aliasBook(changed,$hd(book)) 1
	}
	if {0 < [llength $ids]} {
	    AliasesPopulate
	}
	AliasSave
    }

    bind $hd(w).buttons.add <Destroy> {}
    ::tkrat::winctl::RecordGeometry extractAlias $hd(w)
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    destroy $hd(w)
    unset hd
}

# AliasExtractClearSelection --
#
# Unmarks all aliases in the alias extract list
#
# Arguments:
# handler - The handler of the extract window

proc AliasExtractClearSelection {handler} {
    upvar \#0 $handler hd

    foreach i [array names hd *,use] {
	set hd($i) 0
    }
}

# AliasChooser --
#
# Pops up a window where the user may select an alias. This alias is
# then returned.
#
# Arguments:
# master    - The text widget that is to be master for this window

proc AliasChooser {master} {
    global idCnt t b

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar \#0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(alias_chooser)
    wm transient $w [winfo toplevel $master]
    set hd(w) $w
    set hd(search) ""

    # Find coordinates for window
    set bbox [$master bbox insert]
    set x [expr {[winfo rootx $master]+[lindex $bbox 0]+5}]
    set y [expr {[winfo rooty $master]+[lindex $bbox 1]-20}]
    wm geom $w +$x+$y

    # Build the list
    scrollbar $w.scroll \
	    -relief raised \
	    -bd 1 \
	    -highlightthickness 0 \
	    -command "$w.list yview"
    listbox $w.list \
	    -yscroll "$w.scroll set" \
	    -relief raised \
	    -bd 1 \
	    -exportselection false \
	    -highlightthickness 0 \
	    -selectmode single
    set b($w.list) alias_chooser
    ::tkrat::winctl::Size aliasChooser $w.list
    pack $w.scroll -side right -fill y
    pack $w.list -expand 1 -fill both
    set hd(list) $w.list

    # Bind keys
    bind $w <Control-c> "set ${id}(done) 0"
    bind $w <Key-Escape> "set ${id}(done) 0"
    bind $w <Key-Return> "set ${id}(done) 1"
    bind $w <Key-Tab> "set ${id}(done) 1"
    bind $w.list <ButtonRelease-1> "set ${id}(done) 1"
    bind $w <Key-Up> "AliasChooserMoveSel $id up"
    bind $w <Key-Down> "AliasChooserMoveSel $id down"
    bind $w <Key> "AliasChooserSearch $id %A"
    wm protocol $w WM_DELETE_WINDOW "set ${id}(done) 0"

    # Populate list
    RatAlias list alias
    set hd(aliasIds) [lsort [array names alias]]
    foreach a $hd(aliasIds) {
	$hd(list) insert end [format "%-8s %-20s" $a [lindex $alias($a) 1]]
    }
    $hd(list) selection set 0

    ::tkrat::winctl::ModalGrab $w

    # Wait for action
    tkwait variable ${id}(done)
    ::tkrat::winctl::RecordSize aliasChooser $w.list
    if {1 == $hd(done)} {
	set ret [lindex $hd(aliasIds) [$hd(list) curselection]]
    } else {
	set ret ""
    }
    unset b($w.list)
    destroy $w
    unset hd
    return $ret
}

# AliasChooserMoveSel --
#
# Move the selection in the chooser.
#
# Arguments:
# handler   - The handler which defines this selection window
# direction - Which direction we should move the selection.

proc AliasChooserMoveSel {handler direction} {
    upvar \#0 $handler hd

    set cur [$hd(list) curselection]
    $hd(list) selection clear $cur

    if {[string compare up $direction]} {
	if {[incr cur] >= [$hd(list) size]} {
	    incr cur -1
	}
	if {[expr {$cur/double([$hd(list) size])}]
	       >= [lindex [$hd(list) yview] 1]} {
	    $hd(list) yview $cur
	}
    } else {
	if {$cur > 0} {
	    incr cur -1
	    if {[expr {$cur/double([$hd(list) size])}] < \
		    [lindex [$hd(list) yview] 0]} {
		$hd(list) yview scroll -1 pages
	    }
	}
    }
    $hd(list) selection set $cur
    set hd(search) ""
}

# AliasChooserSearch --
#
# Searches the chooser list.
#
# Arguments:
# handler   - The handler which defines this selection window
# key	    - The pressed key

proc AliasChooserSearch {handler key} {
    upvar \#0 $handler hd

    if {1 != [scan $key "%s" key2]} {
	return
    }
    set hd(search) "$hd(search)$key2"
    set i [lsearch -glob $hd(aliasIds) "$hd(search)*"]
    if {-1 == $i} {
	bell
	set hd(search) ""
    } else {
	$hd(list) selection clear [$hd(list) curselection]
	$hd(list) selection set $i
	$hd(list) see $i
    }
}

# ElmGets --
#
# Fix elm alias file reading to handle multiple line aliases
#
# Arguments:
# fh      - File handle
# linevar - variable to store line in

proc ElmGets {fh linevar} {
    upvar $linevar line
    set haveline 0
    set line ""
    while {$haveline <= 0 && -1 != [gets $fh sline]} {
        set sline [string trim $sline]
        if {[string match {#*} $sline] || 0==[string length $sline]} {
            continue
        }
        set line "${line}${sline} "
        if {![string match {?*=?*=?* } $line] || [string match {*, } $line]} {
            set haveline 0
        } else {
            set haveline 1
        }
    }
    if {$haveline <= 0} {
       return $haveline
    } else {
       return [string length $line]
    }
}


# ReadElmAliases --
#
# Read aliases.text files generated by elm
#
# Arguments:
# file -	Filename to read aliases from
# book -	Address book to insert them into

proc ReadElmAliases {file book} {
    set n 0
    set fh [open $file r]
    while { 0 < [ElmGets $fh line]} {
	if {[string match {*=*=*} $line] && [string length [lindex $line 0]]} {
	    set a [split $line =]
	    RatAlias add $book \
			 [string trim [lindex $a 0]] \
			 [string trim [lindex $a 1]] \
			 [string trim [lindex $a 2]] {} {}
	    incr n
	}
    }
    close $fh
    return $n
}


# ReadMailAliases --
#
# Get aliases out of mailrc files generated by mail and others
#
# Arguments:
# file -	Filename to read aliases from
# book -	Address book to insert them into

proc ReadMailAliases {file book} {
    set n 0
    set fh [open $file r]
    while { -1 != [gets $fh line]} {
	while { 1 == [regexp {\\$} $line]} {
	    if {-1 == [gets $fh cont]} {
		break
	    }
	    set line [join [list [string trimright $line \\] $cont] ""]
	}
	if {[string match {alias *} $line]} {
	    if {[regexp "^alias\[ \t\]+(\[a-zA-Z0-9_-\]+)\[ \t\]+(.+)$" $line \
		    {} name content]} {
		RatAlias add $book $name $name $content {} {}
		incr n
	    }
	}
    }
    close $fh
    return $n
}

# ReadPineAliases --
#
# Read the .addressbook files generated by pine
#
# Arguments:
# file -        Filename to read aliases from
# book -	Address book to insert them into
 
proc ReadPineAliases {file book} {
    if {[catch {open $file r} fh]} {
	Popup $fh
	return 0
    }
    set aliases {}
    while { -1 != [gets $fh line]} {
        if {[regsub {^ } $line "" cont]} {
            set aliases [lreplace $aliases end end \
		    "[lindex $aliases end] $cont"]
        } else {
            lappend aliases $line
        }
    }
    close $fh

    set n 0
    foreach a $aliases {
        if {[regexp {^#DELETED} $a]} {
            continue
        }
        set sa [split $a "\t"]
	if {[string length [lindex $sa 0]]} {
	    incr n
	    set content [lindex $sa 2]
	    regexp {^\((.+)\)$} $content notUsed content
	    RatAlias add $book [lindex $sa 0] [lindex $sa 1] $content {} {}
	}
    }
    return $n
}

# AliasWeAreQuitting --
#
# Save aliases if needed since tkrat is quitting
#
# Arguments:

proc AliasWeAreQuitting {} {
    global aliasWindows

    if {[llength $aliasWindows]} {
	AliasSave
    }
}
