# alias.tcl --
#
# Code which handles aliases.
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén.
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# List of alias windows
set aliasWindows {}

# True if the aliases have been modified
set aliasMod 0

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
    upvar #0 $id hd

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(aliases)
    set hd(w) $w
    set hd(old,default_book) $option(default_book)

    # The menus
    frame $w.mbar -relief raised -bd 1
    FindAccelerators a {show addrbooks import}
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

    set m $w.mbar.import.m
    menubutton $w.mbar.import -text $t(import) -menu $m -underline $a(import)
    set b($w.mbar.import) import_aliases_from_pgm
    menu $m
    $m add command -label mail -command "AddrbookImport mail"
    $m add command -label elm -command "AddrbookImport elm"
    $m add command -label pine -command "AddrbookImport pine"

    pack $w.mbar.show \
	 $w.mbar.book \
	 $w.mbar.import -side left -padx 5

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
	    -selectmode extended
    Size $w.l.list aliasList
    set hd(listbox) $w.l.list
    pack $w.l.scroll -side right -fill y
    pack $w.l.list -expand 1 -fill both
    bind $w.l.list <ButtonRelease-1> "SetAliasesState $id"
    bind $w.l.list <KeyRelease> "SetAliasesState $id"
    bind $w.l.list <Double-1> "AliasDetail $id edit \$${id}(current)"
    set b($w.l.list) alias_list

    # Buttons
    frame $w.b
    button $w.b.new -text $t(new)... -command "AliasDetail $id new"
    set b($w.b.new) new_alias
    button $w.b.edit -text $t(edit) -state disabled \
	    -command "AliasDetail $id edit \$${id}(current)"
    set b($w.b.edit) edit_alias
    button $w.b.delete -text $t(delete) -state disabled \
	    -command "AliasDelete $id"
    set b($w.b.delete) delete_alias
    menubutton $w.b.move -text $t(move_to) -state disabled -menu $w.b.move.m \
	    -indicatoron 1 -relief raised
    set b($w.b.move) move_alias
    menu $w.b.move.m
    set hd(movemenu) $w.b.move.m
    button $w.b.close -text $t(close) -command "AliasClose $id"
    set b($w.b.close) dismiss
    pack $w.b.new \
	 $w.b.edit \
	 $w.b.delete \
	 $w.b.move \
	 $w.b.close -side left -expand 1 -pady 5

    # Pack it all
    pack $w.mbar -side top -fill x
    pack $w.b -side bottom -fill x
    pack $w.l -expand 1 -fill both

    # Create the booklist
    AliasesUpdateBooklist $id

    # Populate list
    AliasesPopulate $id

    # Place window
    Place $w aliases
    lappend aliasWindows $id

    wm protocol $w WM_DELETE_WINDOW "AliasClose $id"

    # Set up traces
    trace variable option(use_system_aliases) w "AliasesUpdateBooklist {}"
    trace variable option(addrbooks) w "AliasesUpdateBooklist {}"
}

# AliasesUpdateBooklist --
#
# Update the list of known books for an alias window
#
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
	upvar #0 $handler hd

	$hd(showmenu) delete 0 end
	$hd(movemenu) delete 0 end
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
		$hd(movemenu) add command -label $name \
			-command [list AliasMoveTo move $handler $name]
		$hd(defaultmenu) add radiobutton -label $name \
			-variable option(default_book) -value $name \
			-command {set bookMod 1}
	    }
	}
    }
}

# AliasesPopulate --
#
# Populate the aliases window
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
	upvar #0 $handler hd

	set old {}
	foreach sel [$hd(listbox) curselection] {
	    lappend old $sel
	}
	RatAlias list alias
	set top [lindex [$hd(listbox) yview] 0]
	$hd(listbox) delete 0 end
	set hd(aliasIds) ""
	foreach a [lsort [array names alias]] {
	    set book [lindex $alias($a) 0]
	    if {$hd(show,$book)} {
		lappend hd(aliasIds) $a
		$hd(listbox) insert end [format "%-10s  %-10s  %-20s  %s" \
			$book $a [lindex $alias($a) 1] \
			[lindex $alias($a) 2]]
	    }
	}
	$hd(listbox) yview moveto $top
	foreach o $old {
	    set i [lsearch -exact $hd(aliasIds) $o]
	    if { -1 != $i } {
		$hd(listbox) selection set $i
		$hd(listbox) see $i
	    }
	}
	SetAliasesState $handler
    }
}

# SetAliasesState --
#
# Update the status of the buttons
#
# Arguments:
# handler	- The id of this window

proc SetAliasesState {handler} {
    upvar #0 $handler hd
    global aliasBook

    set writable 1
    foreach selected [$hd(listbox) curselection] {
	set a [RatAlias get [lindex $hd(aliasIds) $selected]]
	if {!$aliasBook(writable,[lindex $a 0])} {
	    set writable 0
	    break
	}
    }
    set editState disabled
    set moveState disabled
    set deleteState disabled
    set l [llength [$hd(listbox) curselection]]
    set hd(current) {}
    if {$writable} {
	if {$l == 1} {
	    set hd(current) [lindex $hd(aliasIds) [$hd(listbox) curselection]]
	    set editState normal
	    set deleteState normal
	    set moveState normal
	} elseif {$l > 0} {
	    set deleteState normal
	    set moveState normal
	}
	
    }
    $hd(w).b.edit configure -state $editState
    $hd(w).b.move configure -state $moveState
    $hd(w).b.delete configure -state $deleteState
}

# AliasSave --
#
# Save the aliases (if needed)
#
# Arguments:

proc AliasSave {} {
    global option aliasMod bookMod aliasBook

    if {$aliasMod} {
	set books $option(addrbooks)
	if {$option(use_system_aliases)} {
	    lappend books $option(system_aliases)
	}
	foreach book $books {
	    if {$aliasBook(changed,[lindex $book 0])} {
		RatAlias save [lindex $book 0] [lindex $book 2]
	    }
	}
	set aliasMod 0
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
    global aliasWindows aliasMod b
    upvar #0 $handler hd
    
    set i [lsearch -exact $aliasWindows $handler]
    if {-1 != $i} {
	set aliasWindows [lreplace $aliasWindows $i $i]
    }
    RecordSize $hd(listbox) aliasList
    RecordPos $hd(w) aliases
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    destroy $hd(w)
    AliasSave
}           

# AliasDetail --
#
# Show the alias detail window
#
# Arguments:
# handler	- The handler of the alias window
# mode		- What to do on OK
# template	- Template alias to use

proc AliasDetail {handler mode {template {}}} {
    global idCnt t b aliasBook option aliasDetail
    upvar #0 $handler ahd

    # Sanity check
    if {"edit" == $mode && "" == $template} {
	return
    }

    # Check if we have another window active first
    if {[string length $template] && [info exists aliasDetail($template)]} {
	wm deiconify $aliasDetail($template)
	return
    }

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(mode) $mode
    set hd(handler) $handler

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(alias)
    set hd(w) $w

    # Create fields
    label $w.book_label -text $t(addrbook):
    set hd(book) $option(default_book)
    menubutton $w.book_but -textvariable ${id}(book) -relief raised \
	    -menu $w.book_but.m
    set b($w.book_but) alias_adr_book
    menu $w.book_but.m
    foreach book [array names aliasBook writable,*] {
	if {!$aliasBook($book)} {
	    continue
	}
	regsub writable, $book {} name
	$w.book_but.m add radiobutton -label $name -value $name \
		-variable ${id}(book)
    }
    grid $w.book_label -sticky e
    grid $w.book_but -row 0 -column 1 -sticky ew -padx 2 -pady 2

    set line 1
    foreach f {alias fullname} {
	label $w.${f}_label -text $t($f):
	entry $w.${f}_entry -textvariable ${id}($f)
	set b($w.${f}_entry) alias_$f
	grid $w.${f}_label -sticky e
	grid $w.${f}_entry -row $line -column 1 -sticky ew -padx 2 -pady 2
	incr line
    }
    label $w.content_label -text $t(content):
    text $w.content_text -wrap word -setgrid true
    set b($w.content_text) alias_content
    grid $w.content_label -sticky ne
    grid $w.content_text -row $line -column 1 -sticky nsew  -padx 2 -pady 2
    Size $w.content_text aliasText
    set hd(content_text) $w.content_text
    incr line
    label $w.comment_label -text $t(comment):
    text $w.comment_text -wrap word
    set b($w.comment_text) alias_comment
    grid $w.comment_label -sticky ne
    grid $w.comment_text -row $line -column 1 -sticky nsew  -padx 2 -pady 2
    Size $w.comment_text aliasText
    set hd(comment_text) $w.comment_text

    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 3 -weight 1

    bind $w.alias_entry <space> {bell; break}
    bindtags $w.content_text [list Text $w.content_text . all]
    bind $w.content_text <Key-Return> {break}
    bindtags $w.comment_text [list Text $w.comment_text . all]
    bind $w.comment_text <Key-Return> {break}

    # Buttons
    OkButtons $w $t(ok) $t(cancel) "AliasDetailDone $id"
    grid $w.buttons - -pady 5 -sticky ew

    if {[string length $template]} {
	set aliasDetail($template) $w
	set hd(template) $template
	set a [RatAlias get $template]
	set hd(alias) $template
	set hd(book) [lindex $a 0]
	set hd(fullname) [lindex $a 1]
	$hd(content_text) insert 1.0 [lindex $a 2]
	$hd(comment_text) insert 1.0 [lindex $a 3]
	set hd(old,book) $hd(book)
    }
    set hd(old,alias) $hd(alias)

    set hd(oldfocus) [focus]
    focus $w.alias_entry

    Place $w aliasDetail
}

# AliasDetailDone --
#
# Script called when the alias detail window may be done
#
# Arguments:
# handler -	The handler identifying the window
# action  -	The action we should take

proc AliasDetailDone {handler action} {
    global t b aliasMod aliasBook aliasDetail
    upvar #0 $handler hd

    if {1 == $action} {
	if {[regexp " |\t" $hd(alias)]} {
	    Popup $t(alias_may_only_contain_chars) $hd(w)
	    return
	}
	set hd(content) [string trim [$hd(content_text) get 1.0 end]]
	set hd(comment) [string trim [$hd(comment_text) get 1.0 end]]
	if { 0 == [string length $hd(alias)] || \
		0 == [string length $hd(content)]} {
	    Popup $t(need_alias_and_content) $hd(w)
	    return
	}
	if {"edit" == $hd(mode)} {
	    RatAlias delete $hd(old,alias)
	    set aliasBook(changed,$hd(old,book)) 1
	}
	RatAlias add $hd(book) $hd(alias) $hd(fullname) $hd(content) \
		$hd(comment) {}
	set aliasMod 1
	set aliasBook(changed,$hd(book)) 1
	AliasesPopulate
    }
    RecordPos $hd(w) aliasDetail
    RecordSize $hd(content_text) aliasText
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    if {[info exists hd(template)]} {
	unset aliasDetail($hd(template))
    }
    unset hd
}

# AliasDelete --
#
# Deletes aliases from the alias list --
#
# Arguments:
# handler -	The handler identifying the window

proc AliasDelete {handler} {
    upvar #0 $handler hd
    global aliasBook aliasMod

    foreach a [$hd(listbox) curselection] {
	set alias [lindex $hd(aliasIds) $a]
	set aliasBook(changed,[lindex [RatAlias get $alias] 0]) 1
	set aliasMod 1
	RatAlias delete $alias
    }
    AliasesPopulate
}

# AddrbookImport --
#
# Import an address book in a different format
#
# Arguments:
# format -	The format of the file

proc AddrbookImport {format} {
    global idCnt t b

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar #0 $id hd

    # Create toplevel
    toplevel $w -bd 5 -class TkRat
    wm title $w $t(import)
    set hd(w) $w
    set hd(format) $format

    label $w.n_label -text $t(name): -anchor e
    entry $w.n_entry -textvariable ${id}(name)
    set b($w.n_entry) name_of_adrbook
    grid $w.n_label $w.n_entry -sticky ew

    label $w.f_label -text $t(filename): -anchor e
    entry $w.f_entry -textvariable ${id}(file) -width 40
    set b($w.f_entry) name_of_adrbook_file
    grid $w.f_label $w.f_entry -sticky ew

    button $w.browse -text $t(browse) -command \
	    "set ${id}(file) \[rat_fbox::run -parent $w -title $t(import) \
	     -ok $t(ok) -mode open\]"
    set b($w.browse) file_browse
    grid x $w.browse -sticky e

    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 1 -weight 1

    # Buttons
    OkButtons $w $t(ok) $t(cancel) "AddrbookImportDone $id"
    grid $w.buttons - -pady 5 -sticky ew
    bind $w <Return> {}

    Place $w addrbookImport
    update
    set hd(oldfocus) [focus]
    focus $w.n_entry
}

# AddrbookImportDone
#
# Called when an address book import window is done
#
# Arguments
# handler - The handler of the import window
# action  - What to do

proc AddrbookImportDone {handler action} {
    upvar #0 $handler hd
    global option t b bookMod aliasBook

    if {$action} {
	if {![string length $hd(name)]} {
	    Popup $t(need_name) $hd(w)
	    return
	}
	if {[info exists aliasBook(writable,$hd(name))]} {
	    Popup $t(book_already_exists) $hd(w)
	    return
	}
	if {![file isfile $hd(file)] || ![file readable $hd(file)]} {
	    Popup "$t(illegal_file_spec): $hd(file)" $hd(w)
	    return
	}
	set aliasBook(writable,$hd(name)) 0
	set aliasBook(changed,$hd(name)) 0
	set bookMod 1
	lappend option(addrbooks) [list $hd(name) $hd(format) $hd(file)]
	switch $hd(format) {
	    mail {ReadMailAliases $hd(file) $hd(name)}
	    elm  {ReadElmAliases $hd(file) $hd(name)}
	    pine {ReadPineAliases $hd(file) $hd(name)}
	}
	AliasesPopulate
    }

    RecordPos $hd(w) addrbookAdd
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    unset hd
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
    upvar #0 $id hd

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

    button $w.browse -text $t(browse) -command \
	    "set ${id}(file) \[rat_fbox::run -parent $w -title $t(new) \
	     -ok $t(save) -mode save\]"
    set b($w.browse) file_browse
    grid x $w.browse -sticky e

    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 1 -weight 1

    # Buttons
    OkButtons $w $t(ok) $t(cancel) "AddrbookAddDone $id"
    bind $w <Return> ""
    grid $w.buttons - -pady 5 -sticky ew

    Place $w addrbookAdd
    update
    set hd(oldfocus) [focus]
    focus $w.n_entry
}

# AddrbookAddDone
#
# Called when an address book add window is done
#
# Arguments
# handler - The handler of the add window
# action  - What to do

proc AddrbookAddDone {handler action} {
    upvar #0 $handler hd
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
	    RatAlias read $hd(file)
	}
    }

    RecordPos $hd(w) addrbookAdd
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
    upvar #0 $id hd

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
    Size $w.l.list bookList
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

    Place $w addrBookDelete
}

# AddrbookDeleteDone
#
# Called when an address book delete window is done
#
# Arguments
# handler  -	The handler of the delete window
# action   -	What to do

proc AddrbookDeleteDone {handler action} {
    upvar #0 $handler hd
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

    RecordPos $hd(w) addrBookDelete
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    destroy $hd(w)
    unset hd
}

# AliasMoveTo --
#
# Move selected aliases to address book
#
# Arguments:
# op	  - Which operation to perform
# handler - The handler of the alias window
# dest    - Name of destination folder

proc AliasMoveTo {op handler dest} {
    upvar #0 $handler hd
    global aliasMod aliasBook

    foreach i [$hd(listbox) curselection] {
	set a [lindex $hd(aliasIds) $i]
	set alias [RatAlias get $a]
	RatAlias delete $a
	eval RatAlias add [lreplace $alias 0 0 $dest $a]
	set aliasBook(changed,[lindex $alias 0]) 1
	set aliasBook(changed,$dest) 1
    }
    AliasesPopulate
    incr aliasMod
}

# AliasExtract --
#
# Extracts aliases from the current message
#
# Arguments:
# handler - The handler of the folder window

proc AliasExtract {handler} {
    global idCnt t b aliasBook option
    upvar #0 $handler fh

    # Extract the addresses
    set adrlist {}
    foreach a [$fh(current) get from reply_to sender cc bcc to] {
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
	if {$good} {
	    lappend adrlist $a
	}
    }

    # Check that we found something
    if {![llength $adrlist]} {
	Popup $t(could_not_find_adr) $fh(toplevel)
	return
    }

    RatAlias list alias
    foreach a [array names alias] {
	foreach adr [split [lindex $alias($a) 2]] {
	    set present($adr) 1
	}
    }

    # Create identifier
    set id al[incr idCnt]
    set w .$id
    upvar #0 $id hd

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
	    set totlist "$totlist,\n[$a get mail]"
	} else {
	    set totlist [$a get mail]
	}
	if {[info exists present([$a get mail])]} {
	    continue
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
	set hd($idCnt,content) [$a get mail]
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
    if {![info exists idw]} {
	Popup $t(could_not_find_adr) $fh(toplevel)
	destroy $w
	unset hd
	return
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
    OkButtons $w $t(add_aliases) $t(cancel) "AliasExtractDone $id"
    pack $w.buttons -side bottom -pady 5 -fill x

    Place $w extractAlias
}

# AliasExtractDone --
#
# The alias extract window is now done.
#
# Arguments:
# handler - The handler of the extract window
# action  - Which action we should take

proc AliasExtractDone {handler action} {
    upvar #0 $handler hd
    global aliasMod t b aliasBook

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
	    set aliasMod 1
	    set aliasBook(changed,$hd(book)) 1
	}
	if { 1 == $aliasMod } {
	    AliasesPopulate
	}
	AliasSave
    }

    RecordPos $hd(w) extractAlias
    foreach bn [array names b $hd(w)*] {unset b($bn)}
    destroy $hd(w)
    unset hd
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
    upvar #0 $id hd

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
    Size $w.list aliasChooser
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

    ModalGrab $w

    # Wait for action
    tkwait variable ${id}(done)
    RecordSize $w.list aliasChooser
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
    upvar #0 $handler hd

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
    upvar #0 $handler hd

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
