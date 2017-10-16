# vfolderdef.tcl -
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.
#
# Create and edit vfolders
#
# Folders are stored in the vfolderlist file. This file is a valid tcl-file
# and is read by sourcing it. It should define four variables and two-three
# arrays of lists. The variables are:
# vFolderVersion  - Version of this file, this should be '8'
# vFolderInbox    - The identifier of the default folder, this is the first
#                   folder which is opened upon startup.
# vFolderSpecials - List of special folders. Currently only the following two
# vFolderOutgoing - Id of folder holding the outqueue
# vFolderHold     - Id of folder where held messagaes are kept
#
# The first array is vFolderStruct which defines the layout of the folder
# menu. Index '0' is the top and must always exist. Each item in this
# array is a list {name contents} where contents is a list of indexes.
# Positive indexes refers to the vFolderDef array and negative indexes
# to other entries in the vFolderStruct array.
#
# Each virtual folder is described by one entry in the vFolderDef array.
# Each entry in the array is a list. The three first elements in this list
# are the same for all folder types:
#  {NAME TYPE FLAGS TYPE_SPECIFIC_ELEMENTS}
# Flags is a list of flags and their values, suitable for the "array set" cmd.
# Valid flags are: sort, browse, monitor, watch, subscribed
# The following is a list of all the different folder types and their elems
#
# file:		{name file flags filename}
# mh:		{name mh flags path_to_mh_dir}
# dbase:	{name dbase flags extype exdate expression}
# imap:		{name imap flags imap_host folder}
# pop3:		{name pop3 flags pop3_host}
# dynamic:	{name dynamic flags path_to_dir policy}
# disconnected:	{name dis flags imap_server folder}
#
# The following are entries which does not refer to any physical folders
# Instead they contain references and structures.
# Menu struct:  {name struct flags {ids}}
# import:       {name import flags folderdef pattern {ids}}
#
#
# The final array is mailServer. Indexes into this array are, for imap servers
# the display name of the server and for pop3 servers an integer.
# Each entry in the array contains the following list:
#    {host port flags user}
# Valid flags are: pop3 ssl validate-cert
#
#
# OLD FORMATS
# imap:		{name imap flags {{{host:port}folder}} user}
# pop3:		{name pop3 flags {{{host/pop3}}} user}
# disconnected:	{name dis flags {{{host:port}folder}} user}

# VFolderDef --
#
# Create the vfolderdef window
#
# Arguments:

proc VFolderDef {} {
    global t b vf vfd_old vFolderDef vFolderSpecials

    # Create identifier
    set id vfolderdef
    set w .$id
    if {[winfo exists $w]} {
	wm deiconify $w
	raise $w
	return
    }
    upvar #0 $id hd
    set vf(w) $w
    set vf(done) 0
    set vf(dragging) 0
    set vf(oldfocus) [focus]
    set vf(drag_after) {}
    set vf(w) $w
    set vf(selected) {}
    set vf(changed) 0
    set vf(unapplied_changes) 0
    set vfd_old(marker) {}

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(vfolderdef)

    # Populate window
    frame $w.mbar -relief raised -bd 1
    set m $w.mbar.a.m
    menubutton $w.mbar.a -menu $m -text $t(actions) -underline 0
    menu $m
    $m add command -label $t(new_folder_wizard)... \
	-command "VFolderWizardStart menu"
    $m add command -label $t(new_submenu)... -command "VFolderNewStruct menu"
    $m add separator
    $m add command -label $t(reimport_all) -command VFolderReimportAll    
    set b($m,[$m index end]) reimport_all
    $m add separator
    $m add command -label $t(close) -command "VFolderWinClose 0"
    set b($m,[$m index end]) dismiss
    pack $w.mbar.a -side left -padx 5

    # This has to be created after the menubar to make the Destroy logic work
    frame $w.d

    # Paning button
    frame $w.d.handle -width 10 -height 10 \
	    -relief raised -borderwidth 2 \
	    -cursor sb_h_double_arrow
    set b($w.d.handle) pane_button

    # Folder tree
    set vf(tree) [rat_tree::create $w.d.t -sizeid vFolderDef \
		      -selectcallback VFolderSelect \
		      -movenotify VFolderCheckChanges]
    set vf(top) [$vf(tree) gettopnode]

    # Find max width of labels
    set vf(mw) 10
    foreach l {name type pathname keywords extype mail_server mbox role
               pattern reimport_when sort_order  host user connect flags} {
	if {[string length $t($l)] > $vf(mw)} {
	    set vf(mw) [string length $t($l)]
	}
    }
    # Font is proportional
    set vf(mw) [expr {int($vf(mw)*0.9)}]

    # Folder details
    frame $w.d.r -relief sunken
    set vf(detframe) $w.d.r.details
    set vf(details) [rat_scrollframe::create $vf(detframe) -bd 10 \
			 -highlightthickness 0]
    label $vf(details).name_lab -width $vf(mw)
    entry $vf(details).name -width 40 -state disabled -relief flat
    grid $vf(details).name_lab $vf(details).name -sticky ew
    frame $w.d.r.buttons -bd 10
    button $w.d.r.buttons.apply -text $t(apply_changes) -state disabled \
	    -command VFolderApply
    button $w.d.r.buttons.restore -text $t(restore_values) -state disabled \
	    -command VFolderRestore
    pack $w.d.r.buttons.apply $w.d.r.buttons.restore \
	    -side left -expand 1 -anchor s
    set vf(but_apply) $w.d.r.buttons.apply
    set vf(but_restore) $w.d.r.buttons.restore
    grid $w.d.r.details -column 1 -row 1 -sticky nsew	 
    grid $w.d.r.buttons -column 1 -row 2 -sticky nsew
    grid columnconfigure $w.d.r 1 -weight 1
    grid rowconfigure $w.d.r 1 -weight 1
    grid rowconfigure $vf(details) 100 -weight 1
    pack $w.mbar -side top -fill x
    pack $w.d -fill both -expand 1


    # Do packing of paning window
    VFolderPane [::tkrat::winctl::GetPane vFolderDef]
    place $w.d.t -relheight 1
    place $w.d.r -relheight 1 -relx 1 -anchor ne
    place $w.d.handle -anchor s
    raise $w.d.handle
    bind $w.d <Configure> \
	    "set ${id}(W) \[winfo width %W\]; \
	     set ${id}(X0) \[winfo rootx %W\]; \
	     set y \[expr \[winfo height %W\] - 10\];\
             place configure $w.d.handle -y \$y"
    bind $w.d.handle <B1-Motion> \
	"VFolderPane \[expr (%X-\$${id}(X0))/\$${id}(W).0\]"

    menu $w.mf -tearoff 0
    $w.mf add command -label $t(delete)... -command "VFolderDeleteFolder"
    set vf(folder_menu_delete) [$w.mf index end]
    $w.mf add command -label $t(new_folder_wizard)... \
	-command "VFolderWizardStart tree"
    $w.mf add command -label $t(new_submenu)... \
	-command "VFolderNewStruct tree"
    set vf(folder_menu) $w.mf

    menu $w.mm -tearoff 0
    $w.mm add command -label $t(delete)... -command "VFolderDeleteServer"
    set b($w.mm,[$w.mm index end]) vd_delete
    set vf(mailserver_menu) $w.mm

    # Create images (images lent from tk sources)
    if {![info exists vf(folder)]} {
        set vf(folder) [image create photo -data {
R0lGODlhEAAMAKEAAAD//wAAAPD/gAAAACH5BAEAAAAALAAAAAAQAAwAAAIghINhyycvVFsB
QtmS3rjaH1Hg141WaT5ouprt2HHcUgAAOw==}]
        set vf(file)   [image create photo -data {
R0lGODlhDAAMAKEAALLA3AAAAP//8wAAACH5BAEAAAAALAAAAAAMAAwAAAIgRI4Ha+IfWHsO
rSASvJTGhnhcV3EJlo3kh53ltF5nAhQAOw==}]
        set vf(dbase)   [image create photo -data {
R0lGODlhEAAMAKEAAAD//wAAAPD/gP///yH+Dk1hZGUgd2l0aCBHSU1QACH5BAEAAAAALAAA
AAAQAAwAAAImhIMZxhcCo0DtyTtZwpMeqGzZx0ULWY6AlZ5rCmqwurKS19RhDhQAOw==}]
        set vf(imap)   [image create photo -data {
R0lGODlhEAAMAKEAAAD//wAAAPD/gP///yH5BAEAAAAALAAAAAAQAAwAAAIkhA+hi50CRXAo
SDupsTGfmWFY9T3dt6SXRG4m5LlhGmv2jdsFADs=}]
        set vf(pop3)   [image create photo -data {
R0lGODlhEAAMAKEAAAD//wAAAPD/gP///yH5BAEAAAAALAAAAAAQAAwAAAIihA+hi50CRXAo
SDupsTG3jGHaxEHegobS+HQUmGryTNdAAQA7}]
    }

    bind $w.mbar <Destroy> VFolderWinCleanup
    bind $w <Return> {
	if {"normal" == [[winfo toplevel %W].d.r.buttons.apply cget -state]} {
	    VFolderApply
	}
    }
    bind $vf(but_restore) <Return> {%W invoke; break}

    $vf(tree) autoredraw 0
    # Add IMAP-servers
    set vf(imapitem) [$vf(top) add folder -image $vf(folder) \
	    -label $t(imap_servers) -state closed]
    VFolderAddMailServers
    # Add special folders
    set item [$vf(top) add folder -image $vf(folder) -label $t(specials) \
	    -state closed -zone specials]
    foreach sid [lindex $vFolderDef($vFolderSpecials) 3] {
	VFDInsert $item end $sid specials
    }
    # Add normal folders
    set item [$vf(top) add folder -image $vf(folder) -label $t(folders) \
	    -state open -zone folders]
    foreach sid [lindex $vFolderDef(0) 3] {
	VFDInsert $item end $sid folders
    }
    set vf(folderitem) $item
    $vf(tree) autoredraw 1
    $vf(tree) redraw
    ::tkrat::winctl::SetGeometry vFolderDef $w $w.d
}

# VFolderWinClose --
#
# Close a vfolderdef window
#
# Arguments:
# force - True if we can not abort

proc VFolderWinClose {force} {
    global b vf t

    if {!$force} {
        # Check if it is ok
        if {0 == [VFolderChangeOk]} {
            return
        }

        # Do we have a wizard up?
        set wizards 0
        foreach w [winfo children .] {
            if {[string match ".vfolderwizard*" $w]} {
                wm deiconify $w
                incr wizards
            }
        }
        if { 0 != $wizards} {
            Popup $t(cant_close_while_wizards)
            return
        }
    }
    destroy $vf(w)
}

# VFolderWinCleanup --
#
# Cleanup when closing VFolderWindow
#
# Arguments:

proc VFolderWinCleanup {} {
    global vf b

    VFolderCheckChanges
    ::tkrat::winctl::RecordGeometry vFolderDef $vf(w) $vf(w).d $vf(pane)
    catch {focus $vf(oldfocus)}
    foreach bn [array names b $vf(w).*] {unset b($bn)}
    unset vf
}

# VFolderPane --
#
# Pane the vfolderdef window
#
# Arguments:
# x       - X position of dividing line

proc VFolderPane {x} {
    global vf

    if {$x < 0.01 || 0.99 < $x}  return
        # Prevents placing into inaccessibility (off the window).

    set w $vf(w)
    set vf(pane) $x

    place $w.d.t -relwidth $x
    place $w.d.r -relwidth [expr {1.0 - $x}]
    place $w.d.handle -relx $x
}


# VFolderAddMailServers --
#
# Adds the mail-servers to the tree.
#
# Arguments:
# im - Imap top item

proc VFolderAddMailServers {} {
    global mailServer vf

    $vf(imapitem) clear
    foreach m [lsort -dictionary [array names mailServer]] {
	if {-1 != [lsearch -exact [lindex $mailServer($m) 2] pop3]} {
	    continue
	}
	set iid [list imap $m]
	$vf(imapitem) add item -image $vf(imap) -label $m -id $iid
	$vf(tree) bind $iid <3> \
		[list VFolderPostMenu %X %Y $iid $vf(mailserver_menu)]
    }
}

# VFolderGetItemName --
#
# Gets the name to insert into the tree.
#
# Arguments:
# id - folder id

proc VFolderGetItemName {id} {
    global vFolderDef vFolderInbox t

    set d $vFolderDef($id)
    array set f [lindex $d 2]
    set ta {}
    if {![string compare $vFolderInbox $id]} {
	lappend ta INBOX
    }
    if {[info exists f(monitor)] && $f(monitor)} {
	lappend ta $t(monitored)
    }
    if {[info exists f(watch)] && $f(watch)} {
	lappend ta $t(watched)
    }
    return "[lindex $d 0]        [join $ta ,]"
}

# VFolderGetItemImage --
#
# Returns the image to use for a certain item
#
# Arguments:
# id - folder id

proc VFolderGetItemImage {id} {
    global vFolderDef vf

    switch -regexp [lindex $vFolderDef($id) 1] {
	imap|dis { return $vf(imap) }
	pop3 { return $vf(pop3) }
	import { return $vf(folder) }
	dbase { return $vf(dbase) }
	default { return $vf(file) }	
    }
}

# VFDInsert --
#
# Insert folder items into the tree
#
# Arguments:
# item - Item command
# pos  - Position to insert in
# id   - ID of vFolderDef to insert
# zone - Zone to drop folders in

proc VFDInsert {item pos id zone} {
    global vFolderDef vf

    set d $vFolderDef($id)
    set text [VFolderGetItemName $id]
    set iid [list folder $id]
    if {"struct" == [lindex $d 1] || "import" == [lindex $d 1]} {
	if {"struct" == [lindex $d 1]} {
	    set loc 3
	    set z2 $zone
	} else {
	    set loc 5
	    set z2 {}
	}
	set i [$item add folder -image $vf(folder) -label $text -id $iid \
		-position $pos -zone $z2 -dropin $zone]
	set vf(item,$id) $i
	foreach sid [lindex $d $loc] {
	    VFDInsert $i end $sid $z2
	}
    } else {
	set image [VFolderGetItemImage $id]
	$item add entry -image $image -label $text -id $iid -position $pos \
		-dropin $zone
    }
    if {"folders" == $zone} {
	$vf(tree) bind $iid <3> \
	    [list VFolderPostMenu %X %Y $iid $vf(folder_menu)]
    }
}

# VFolderChangeOk --
#
# Check if it is ok to change the details part. Returns '0' if not.
#
# Arguments:

proc VFolderChangeOk {} {
    global vf vfd t

    if {$vf(unapplied_changes)} {
	set value [RatDialog $vf(w) $t(unapplied_changes_title) \
		$t(unapplied_changes) {} 0 $t(apply) $t(discard) $t(cancel)]
	if {0 == $value} {
	    VFolderApply
	} elseif {2 == $value} {
	    return 0
	}
	set vf(unapplied_changes) 0
	if {1 == $vfd(new)} {
	    VFolderRemoveNew
	}
    }
    return 1
}

# VFolderSelect --
#
# Callback, called when the user selects something in the folder-list
#
# Arguments:
# id - Id of selected item

proc VFolderSelect {ident} {
    global vf t

    # Check if changed
    if {0 == [VFolderChangeOk]} {
	return cancel
    }

    switch [lindex $ident 0] {
	imap   { VFolderSetupMailServer imap [lindex $ident 1] 0}
	folder { VFolderSetupDetails [lindex $ident 1] 0 {} }
	default {
	    foreach c [winfo children $vf(details)] {
		destroy $c
	    }
	}
    }
    rat_scrollframe::recalc $vf(detframe)
    return ok
}

# VFolderClearWindow --
#
# Clears the window
#
# Arguments:

proc VFolderClearWindow {} {
    global vf vfd vfd_old

    foreach c [winfo children $vf(details)] {
	destroy $c
    }
    # Work around bug in tk (reported & fixed Oct 2002)
    grid size $vf(details)
    unset vfd_old
    set vfd_old(marker) {}
    trace vdelete vfd w VFolderDefChange
}

# VFolderSetupDetails --
#
# Setup the details frame
#
# Arguments:
# id  - Id of element to show
# new - Boolean indicating if this is a new object or not
# pos - Where to insert a new object {struct-id index}

proc VFolderSetupDetails {id new pos} {
    global vf vfd vfd_old vFolderDef vFolderInbox t b mailServer \
	    option env vFolderHold vFolderOutgoing

    VFolderClearWindow

    set w $vf(details)
    set vfd(olddef) $vFolderDef($id)
    set vfd(name) [lindex $vfd(olddef) 0]
    set vfd(mode) folder
    set vfd(type) [lindex $vfd(olddef) 1]
    set vfd(typename) $t([lindex $vfd(olddef) 1])
    set vfd(id) $id
    set vfd(new) $new
    set vfd(import_on_create) 0

    # Top information
    if {$id == $vFolderHold} {
	label $w.is_hold -text $t(is_hold_folder) -pady 10 -anchor n
	grid $w.is_hold - -sticky ew
    } elseif {$id == $vFolderOutgoing} {
	label $w.is_outgoing -text $t(is_outgoing_folder) -pady 10 -anchor n
	grid $w.is_outgoing - -sticky ew
    }
    label $w.name_lab -text $t(name): -width $vf(mw) -anchor e
    entry $w.name -width 40 -textvariable vfd(name)
    grid $w.name_lab $w.name -sticky ew
    label $w.type_lab -text $t(type): -anchor e
    set m $w.type_mb.m
    menubutton $w.type_mb -textvariable vfd(typename) -anchor w -menu $m \
	    -highlightthickness 1
    grid $w.type_lab $w.type_mb -sticky ew -pady 5
    if {$new} {
	set imaps {}
	set popi 0
	foreach ms [lsort -dictionary [array names mailServer]] {
	    if { -1 == [lsearch -exact [lindex $mailServer($ms) 2] pop3]} {
		lappend imaps $ms
	    } elseif {[string is integer $ms] && $ms > $popi} {
		set popi $ms
	    }
	}
	set def(file) [list {} file {} {}]
	set def(mh) [list {} mh {} {}]
	set def(dbase) [list {} dbase {} $option(def_extype) \
		$option(def_exdate) {and keywords {}}]
	set def(imap) [list {} imap {} [lindex $imaps 0] {}]
	set def(pop3) [list {} pop3 {} [expr {$popi + 1}]]
	set def(dynamic) [list {} dynamic {} {} sender]
	menu $m
	foreach ft {file mh dbase imap pop3 dynamic} {
	    $m add command -label $t($ft) -command \
		    "set vFolderDef($id) \[lreplace [list $def($ft)] 0 0 \
		                                    \$vfd(name)\]; \
		    [list VFolderSetupDetails $id $new $pos]; \
		    update; focus \[tk_focusNext $w.type_mb\]"
	    if {"imap" == $ft && 0 == [llength $imaps]} {
		$m entryconfigure end -state disabled
	    }
	}
	foreach ft {file mh imap} {
	    $m add command -label "$t(import) ($t($ft))" -command \
		    "set vFolderDef($id) \[lreplace \
		                  \[list {} import {reimport manually} \
				  [list $def($ft)] {*}\] 0 0 \$vfd(name)\]; \
		     [list VFolderSetupDetails $id $new $pos]; \
		     update; focus \[tk_focusNext $w.type_mb\]"
	    if {"imap" == $ft && 0 == [llength $imaps]} {
		$m entryconfigure end -state disabled
	    }
	}
	if {"struct" != $vfd(type)} {
	    $w.type_mb configure -bd 2 -indicatoron 1 -relief raised \
		    -takefocus 1
	}
	if {"pop3" == $vfd(type) && "" == [lindex $vfd(olddef) 3]} {
	    # Find unique name
	    set i 1
	    while {[info exists mailServer($i)]} {
		incr i
	    }
	    set n $i

	    set mailServer($n) [list {} {} {} $env(USER)]
	    set d [lreplace $vfd(olddef) 3 3 $n]
	}
	$vf(but_apply) configure -text $t(create) -state normal
	$vf(but_restore) configure -text $t(cancel) -state normal
	set vf(unapplied_changes) 1
    } else {
	$w.type_mb configure -state disabled -indicatoron 0
	$vf(but_apply) configure -text $t(apply_changes) -state disabled
	$vf(but_restore) configure -text $t(restore_values) -state disabled
    }

    # Do magic if we are looking at an import folder
    if {"import" == [lindex $vfd(olddef) 1]} {
	set vfd(is_import) 1
	set vfd(pattern) [lindex $vfd(olddef) 4]
	array set vfd [lindex $vfd(olddef) 2]
	set d [lindex $vfd(olddef) 3]
	set vfd(typename) "$vfd(typename) ($t([lindex $d 1]))"
	set vfd(type) [lindex $d 1]
	set vfd(manage) 0
    } else {
	set d $vfd(olddef)
	set vfd(is_import) 0
	if {"pop3" == [lindex $d 1]} {
	    set vfd(manage) 0
	} else {
	    set vfd(manage) $vfd(new)
	}
    }

    # Do type specific things
    switch -regexp [lindex $d 1] {
	file|mh|dynamic {
	    set vfd(filename) [RatDecodeQP system [lindex $d 3]]
	    label $w.file_label -text $t(pathname): -anchor e
	    entry $w.file_entry -textvariable vfd(filename) -width 40
	    set b($w.file_entry) vd_pathname
	    grid $w.file_label $w.file_entry -sticky ew
	    button $w.fbrowse -text $t(browse)...
	    if {"dynamic" == [lindex $d 1]} {
		$w.fbrowse configure -command "Browse $w vfd(filename) dirok"
	    } else {
		$w.fbrowse configure -command "Browse $w vfd(filename) any"
	    }
	    set b($w.fbrowse) file_browse
	    grid x $w.fbrowse -sticky e
	}
	dbase {
	    set vfd(keywords) [lindex [lindex $d 5] 2]
	    set vfd(extype) [lindex $d 3]
	    set vfd(exdate) [lindex $d 4]
	    label $w.kw_lab -text $t(keywords): -anchor e
	    entry $w.kw_entry -textvariable vfd(keywords) -width 20
	    set b($w.kw_entry) keywords
	    label $w.extype_lab -text $t(extype): -anchor e
	    frame $w.extype
	    foreach et {none remove incoming backup} {
		radiobutton $w.extype.$et -anchor w -text $t($et) \
			-variable vfd(extype) -value $et
		set b($w.extype.$et) exp_$et
		pack $w.extype.$et -side top -fill x
	    }
	    label $w.exdate_lab -text $t(exdate): -anchor e
	    entry $w.exdate_entry -textvariable vfd(exdate)
	    set b($w.exdate_entry) exp_date
	    grid $w.kw_lab $w.kw_entry -sticky we
	    grid $w.extype_lab $w.extype -sticky wen
	    grid $w.exdate_lab $w.exdate_entry -sticky we
	}
	imap|dis {
	    set vfd(mail_server) [lindex $d 3]
	    set vfd(mailbox_path) [RatDecodeQP system [lindex $d 4]]
	    if {"dis" == [lindex $d 1]} {
		set vfd(disconnected) 1
	    } else {
		set vfd(disconnected) 0
	    }
	    label $w.ms_lab -text $t(mail_server): -anchor e
	    set m $w.ms_but.m
	    menubutton $w.ms_but -textvariable vfd(mail_server) \
		    -relief raised -indicatoron 1 -menu $m -takefocus 1 \
		    -highlightthickness 1
	    menu $m
	    foreach ms [lsort -dictionary [array names mailServer]] {
		if { -1 == [lsearch -exact [lindex $mailServer($ms) 2] pop3]} {
		    $m add command -label $ms \
			    -command [list set vfd(mail_server) $ms]
		}
	    }
	    set b($w.ms_but) mail_server
	    grid $w.ms_lab $w.ms_but -sticky ew -pady 5
	    label $w.mp_lab -text $t(mbox): -anchor e
	    entry $w.mp_entry -textvariable vfd(mailbox_path) -width 20
	    set b($w.mp_entry) vd_mbox
	    grid $w.mp_lab $w.mp_entry -sticky we
	    checkbutton $w.disconnected -text $t(enable_offline) \
		    -variable vfd(disconnected)
	    set b($w.disconnected) use_as_disconnected
	    grid x $w.disconnected -sticky w
	    if {0 == $vfd(is_import)} {
		checkbutton $w.create -text $t(create_mailbox_on_server) \
			-variable vfd(manage)
		set b($w.create) create_mailbox_on_server
		grid x $w.create -sticky w
	    }
	}
	pop3 {	    
	    set vfd(mail_server) [lindex $d 3]
	    label $w.ms_lab -text $t(mail_server): -anchor e

	    VFolderSetupMailServerDetails $w $vfd(mail_server)
	}
    }

    if {1 == $vfd(is_import)} {
	frame $w.sp3 -height 16
	grid $w.sp3
	label $w.pl -text $t(pattern): -anchor e
	entry $w.pe -textvariable vfd(pattern)
	set b($w.pe) vd_pattern
	grid $w.pl $w.pe -sticky ew
	checkbutton $w.sub -text $t(subscribed_only) \
		-variable vfd(subscribed)
	set b($w.sub) vd_subscribed
	grid x $w.sub -sticky w
	frame $w.sp4 -height 10
	grid $w.sp4
	label $w.when_lab -text $t(reimport_when): -anchor e
	radiobutton $w.rei_manually -variable vfd(reimport) -value manually \
		-text $t(reimport_manually) -anchor w
	grid $w.when_lab $w.rei_manually -sticky ew
	set b($w.rei_manually) vd_rei_manually
	radiobutton $w.rei_session -variable vfd(reimport) -value session \
		-text $t(reimport_session)
	grid x $w.rei_session -sticky w
	set b($w.rei_session) vd_rei_session
	if {0 == $new} {
	    button $w.reimport_now -text $t(reimport_now) -pady 0 \
		    -command "VFolderReimport $id"
	    grid x $w.reimport_now -sticky w
	} else {
	    frame $w.sp5 -height 10
	    grid $w.sp5
	    set vfd(import_on_create) 1
	    checkbutton $w.import_on_create -text $t(import_on_create) \
		    -variable vfd(import_on_create)
	    grid x $w.import_on_create -sticky w
	    set b($w.import_on_create) vd_import_on_create
	}
    }

    # Show flags for everything except menus
    if {"struct" != [lindex $vfd(olddef) 1]} {
	set vfd(sort) default
	set vfd(role) default
	foreach v {browse monitor watch trace subscribed inbox} {
	    set vfd($v) 0
	}
	if {$id == $vFolderInbox} {
	    set vfd(inbox) 1
	}
	array set vfd [lindex $vfd(olddef) 2]
	set vfd(sort_l) $t(sort_$vfd(sort))
	if {"default" == $vfd(role)} {
	    set vfd(role_l) $t(default)
	} else {
	    set vfd(role_l) $option($vfd(role),name)
	}

	label $w.role_label -text $t(role): -anchor e
	set m $w.role_m.m
	menubutton $w.role_m -textvariable vfd(role_l) \
		-relief raised -indicatoron 1 -menu $m -highlightthickness 1 \
		-takefocus 1
	set b($w.role_m) vd_role
	menu $m -tearoff 0
	$m add command -label $t(default) -command "set vfd(role) default; \
		set vfd(role_l) [list $t(default)]"
	foreach r $option(roles) {
	    $m add command -label $option($r,name) \
		    -command "set vfd(role) $r; \
		    set vfd(role_l) [list $option($r,name)]"
	}

	label $w.sort_label -text $t(sort_order): -anchor e
	set m $w.sort_m.m
	menubutton $w.sort_m -textvariable vfd(sort_l) \
		-relief raised -indicatoron 1 -menu $m -highlightthickness 1 \
		-takefocus 1
	set b($w.sort_m) vd_sort
	menu $m -tearoff 0
	foreach o {default threaded folder reverseFolder date reverseDate \
		size reverseSize subject subjectonly} {
	    $m add command -label $t(sort_$o) \
		    -command "set vfd(sort) $o; \
		    set vfd(sort_l) [list $t(sort_$o)]"
	}
	checkbutton $w.browse -text $t(browse_mode) -variable vfd(browse)
	set b($w.browse) browse_mode
	checkbutton $w.monitor -text $t(monitor_mbox) -variable vfd(monitor) \
		-command "if !\$vfd(monitor) {set vfd(watch) 0}"
	set b($w.monitor) vd_monitor
	checkbutton $w.watch -text "    $t(watch_mbox)" -variable vfd(watch) \
		-command "if \$vfd(watch) {set vfd(monitor) 1}"
	set b($w.watch) vd_watch
	checkbutton $w.inbox -text $t(incom_mbox) -variable vfd(inbox)
	set b($w.inbox) vd_setinbox
	if {$vfd(inbox)} {
	    $w.inbox configure -state disabled
	}
	frame $w.sp1 -height 16
	frame $w.sp2 -height 16
	grid x $w.sp1
	grid $w.role_label $w.role_m -sticky ew -pady 5
	grid $w.sort_label $w.sort_m -sticky ew -pady 5
	grid x $w.browse -sticky w
	grid x $w.monitor -sticky w
	grid x $w.watch -sticky w
	if {1 != $vfd(is_import)} {
	    grid x $w.inbox -sticky w
	}
	grid x $w.sp2
    } else {
	set vfd(mode) struct
    }

    # Create copy of values
    foreach v [array names vfd] {
	set vfd_old($v) $vfd($v)
    }

    trace variable vfd w VFolderDefChange

    focus $w.name
    $w.name icursor end
    if {$new} {
	$w.name selection range 0 end
    }
}

# VFolderSetupMailerverDetails --
#
# Adds the widgets to a details frame
#
# Argument:
# w  - window to add to
# id - Id of mail server

proc VFolderSetupMailServerDetails {w id} {
    global b t vfd mailServer option

    if {[info exists mailServer($id)]} {
	set ms $mailServer($id)
    } else {
	set ms [list $option(remote_host) {} pop3 $option(remote_user)]
    }
    set flags [lindex $ms 2]
    foreach f {ssl notls novalidate-cert secure debug} {
	if {-1 != [lsearch -exact $flags $f]} {
	    set vfd($f) 1
	} else {
	    set vfd($f) 0
	}
    }
    set i [lsearch -glob $flags ssh-cmd*]
    if {-1 != $i} {
	set vfd(ssh_cmd) [lindex [lindex $flags $i] 1]
    } else {
	set vfd(ssh_cmd) $option(ssh_template)
    }
    set vfd(host) [lindex $ms 0]
    set vfd(user) [lindex $ms 3]
    set vfd(port) {}
    set port [lindex $ms 1]
    if {"" == $port} {
	set vfd(method) rsh
    } elseif {("imap" == $vfd(type) && !$vfd(ssl) && 143 == $port)
	      || ("pop3" == $vfd(type) && !$vfd(ssl) && 110 == $port)
              || ("imap" == $vfd(type) && $vfd(ssl) && 993 == $port)
	      || ("pop3" == $vfd(type) && $vfd(ssl) && 995 == $port)} {
	set vfd(method) tcp_default
    } else {
	set vfd(method) tcp_custom
	set vfd(port) $port
    }
    if {$vfd(ssl)} {
	set vfd(priv) ssl
	set vfd(notls) 1
    } elseif {0 == $vfd(notls)} {
	set vfd(priv) tls
    } else {
	set vfd(priv) none
    }

    frame $w.msp1 -height 16
    grid $w.msp1

    label $w.host_lab -text $t(host): -anchor e
    entry $w.host -width 40 -textvariable vfd(host)
    set b($w.host) vd_host
    grid $w.host_lab $w.host -sticky ew

    label $w.user_lab -text $t(user): -anchor e
    entry $w.user -width 40 -textvariable vfd(user)
    set b($w.user) vd_user
    grid $w.user_lab $w.user -sticky ew

    frame $w.msp2 -height 10
    grid $w.msp2

    label $w.conn_lab -text $t(connect): -anchor e
    radiobutton $w.conn_tcpdef -text $t(tcp_default) -variable vfd(method) \
	    -value tcp_default -anchor w
    set b($w.conn_tcpdef) vd_tcp_default
    frame $w.conn_tcpcust
    radiobutton $w.conn_tcpcust.b -text $t(tcp_custom): -variable vfd(method) \
	    -value tcp_custom
    entry $w.conn_tcpcust.e -width 6 -textvariable vfd(port)
    pack $w.conn_tcpcust.b $w.conn_tcpcust.e -side left
    set b($w.conn_tcpcust.b) vd_tcp_custom
    set b($w.conn_tcpcust.e) vd_tcp_custom
    radiobutton $w.conn_rsh -text $t(rsh_ssh) -variable vfd(method) \
	    -value rsh
    set b($w.conn_rsh) vd_rsh
    grid $w.conn_lab $w.conn_tcpdef - -sticky ew
    grid x $w.conn_tcpcust -sticky w
    grid x $w.conn_rsh -sticky w

    label $w.ssh_lab -text $t(ssh_command): -anchor e
    entry $w.ssh_cmd -width 40 -textvariable vfd(ssh_cmd)
    grid $w.ssh_lab $w.ssh_cmd -sticky ew
    set b($w.ssh_cmd) ssh_command

    frame $w.msp3 -height 10
    grid $w.msp3

    label $w.priv_lab -text $t(privacy): -anchor e
    radiobutton $w.priv_ssl -text $t(use_ssl) -anchor w \
	    -variable vfd(priv) -value ssl \
	    -command "set vfd(ssl) 1;set vfd(notls) 1"
    radiobutton $w.priv_tls -text $t(try_tls) \
	    -variable vfd(priv) -value tls \
	    -command "set vfd(ssl) 0;set vfd(notls) 0"
    radiobutton $w.priv_none -text $t(no_encryption) \
	    -variable vfd(priv) -value none \
	    -command "set vfd(ssl) 0;set vfd(notls) 1"
    set b($w.priv_ssl) vd_priv_ssl
    set b($w.priv_tls) vd_priv_tls
    set b($w.priv_none) vd_priv_none
    grid $w.priv_lab $w.priv_ssl -sticky ew
    grid x $w.priv_tls -sticky w
    grid x $w.priv_none -sticky w

    frame $w.msp4 -height 10
    grid $w.msp4

    label $w.flags_lab -text $t(flags): -anchor e
    checkbutton $w.flag_secure -text $t(imap_secure) -variable vfd(secure) \
	    -anchor w
    set b($w.flag_secure) vd_secure
    checkbutton $w.flag_checkc -text $t(ssl_check_cert) \
	    -variable vfd(novalidate-cert) -onvalue 0 -offvalue 1
    set b($w.flag_checkc) vd_flag_checkc
    checkbutton $w.flag_debug -text $t(debug_cclient) -variable vfd(debug)
    set b($w.flag_debug) vd_debug_cclient
    grid $w.flags_lab $w.flag_secure -sticky ew
    grid x $w.flag_checkc -sticky w
    grid x $w.flag_debug -sticky w

    VFolderMailServerSetupState {}
}


# VFolderSetupMailServer --
#
# Setup the details frame
#
# Arguments:
# type - Type of mail-server
# id   - Id of element to show
# new  - Boolean indicating if this is a new object or not

proc VFolderSetupMailServer {type id new} {
    global vf vfd vfd_old vFolderDef vFolderInbox t b

    VFolderClearWindow

    set w $vf(details)
    set vfd(new) $new
    set vfd(mode) mail_server
    set vfd(type) $type
    set vfd(id) $id
    set vfd(name) $id

    # Top information
    label $w.name_lab -text $t(name): -width $vf(mw) -anchor e
    entry $w.name -width 40 -textvariable vfd(name)
    grid $w.name_lab $w.name -sticky ew

    VFolderSetupMailServerDetails $w $id


    # Create copy of values
    foreach v [array names vfd] {
	set vfd_old($v) $vfd($v)
    }

    if {$vfd(new)} {
	$vf(but_apply) configure -text $t(create) -state normal
	$vf(but_restore) configure -text $t(cancel) -state normal
    } else {
	$vf(but_apply) configure -text $t(apply_changes) -state disabled
	$vf(but_restore) configure -text $t(restore_values) -state disabled
    }
    trace variable vfd w VFolderDefChange

    if {$new} {
	focus $w.name
	$w.name selection range 0 end
	$w.name icursor end
    } else {
	focus $w.host
	$w.host icursor end
    }
}

# VFolderMailServerSetupState --
#
# Setup the state of the buttons in the MailServer pane
#
# Arguments:
# elem - Name of changed element in vfs

proc VFolderMailServerSetupState {elem} {
    global vf vfd ratHaveOpenSSL

    set w $vf(details)

    if {"method" == $elem && "tcp_custom" == $vfd(method)} {
	focus $w.conn_tcpcust.e
    }

    if {"tcp_custom" == $vfd(method)} {
	$w.conn_tcpcust.e configure -state normal -takefocus 1
    } else {
	$w.conn_tcpcust.e configure -state disabled -takefocus 0
    }

    if {0 == $ratHaveOpenSSL || "rsh" == $vfd(method)} {
	set ssl_state disabled
	set checkc_state disabled
	set secure_state disabled
	set vfd(ssl) 0
	set vfd(notls) 1
	set vfd(novalidate-cert) 1
	set vfd(priv) none
    } else {
	set ssl_state normal
	if {"none" != $vfd(priv)} {
	    set secure_state normal
	    set checkc_state normal
	} else {
	    set secure_state disabled
	    set vfd(secure) 0
	    set checkc_state disabled
	    set vfd(novalidate-cert) 1
	}
    }
    if {"rsh" == $vfd(method)} {
	set ssh_state normal
    } else {
	set ssh_state disabled
    }

    $w.priv_ssl configure -state $ssl_state
    $w.priv_tls configure -state $ssl_state
    $w.flag_secure configure -state $secure_state
    $w.flag_checkc configure -state $checkc_state
    if {1 > [string compare "8.3.1" [info patchlevel]]} {
	$w.ssh_lab configure -state $ssh_state
    }
    if {"normal" == $ssh_state} {
	set col [option get $w.ssh_cmd activeForeground Color]
    } else {
	set col [option get $w.ssh_cmd disabledForeground Color]
    }
    $w.ssh_cmd configure -state $ssh_state -fg $col
}

# VFolderDefChange --
#
# Trace procedure called whenever the user changes the vfolder definition
#
# Arguments:
# name1, name2, op - Trace arguments

proc VFolderDefChange {name1 name2 op} {
    global vf vfd

    if {"mail_server" == $vfd(mode) || "pop3" == $vfd(type)} {
	VFolderMailServerSetupState $name2
    }
    $vf(but_apply) configure -state normal
    $vf(but_restore) configure -state normal
    set vf(changed) 1
    set vf(unapplied_changes) 1
}

# VFolderWrite --
#
# Writes the list of vfolders to disk
#
# Arguments:

proc VFolderWrite {} {
    global option vFolderDef vFolderVersion vFolderInbox \
	    mailServer vFolderOutgoing vFolderHold vFolderSpecials

    # Do nothing on errors
    if {[catch {open $option(ratatosk_dir)/vfolderlist w} fh]} {
	return
    }

    puts $fh "set vFolderVersion $vFolderVersion"
    if {"" != $vFolderInbox} {
	puts $fh "set vFolderInbox $vFolderInbox"
    }
    puts $fh "set vFolderHold $vFolderHold"
    puts $fh "set vFolderOutgoing $vFolderOutgoing"
    puts $fh "set vFolderSpecials $vFolderSpecials"
    foreach s [array names mailServer] {
	puts $fh [list set mailServer($s) $mailServer($s)]
    }
    foreach elem [array names vFolderDef] {
	puts $fh "set vFolderDef($elem) [list $vFolderDef($elem)]"
    }
    close $fh
}

# VFolderCheckChanges --
#
# Checks if the definitions have changed in any way and if so rewrites
# them.
#
# Arguments:

proc VFolderCheckChanges {} {
    global vf vFolderDef vFolderSpecials

    # Has the tree-structure changed
    if {0 != [$vf(tree) getnumchanges]} {
	incr vf(changed)
	foreach i [array names vFolderDef] {
	    if {"struct" == [lindex $vFolderDef($i) 1]
	        && $vFolderSpecials != $i} {
		unset vFolderDef($i)
	    }
	}
	VFolderReconstructStruct 0 {} $vf(folderitem)
    }
    if {$vf(changed)} {
	VFolderWrite
    }
    set vf(changed) 0
}

# VFolderReconstructStruct --
#
# Reconstruct a struct entry in the vFolderDef array from the tree widget
#
# Arguments:
# nid  - Node id in vFolderDef
# name - name of nore
# node - Node in tree to read
 
proc VFolderReconstructStruct {nid name node} {
    global vFolderDef

    set contents {}
    foreach i [$node list] {
	set id [lindex [lindex $i 2] 1]
	lappend contents $id
	if {"node" == [lindex $i 0]} {
	    VFolderReconstructStruct $id [lindex $i 1] [lindex $i 3]
	}
    }
    if {[info exists vFolderDef($nid)]} {
	switch [lindex $vFolderDef($nid) 1] {
	    struct {set index 3}
	    import {set index 5}
	}
	set vFolderDef($nid) \
	    [lreplace $vFolderDef($nid) $index $index $contents]
    } else {
	set vFolderDef($nid) [list [string trim $name] struct {} $contents]
    }
}

# VFolderConstructDef --
#
# Reconstruct the folder definition
#
# Arguments:

proc VFolderConstructDef {} {
    global vfd vfd_old vf t env

    set flags [list sort $vfd(sort) browse $vfd(browse) \
	    monitor $vfd(monitor) watch $vfd(watch) role $vfd(role)]
    switch -regexp $vfd(type) {
	file {
	    if {"" == $vfd(filename)} {
		Popup "$t(illegal_file_spec): $vfd(filename)" $vf(w)
		return ""
	    }
	    regsub {/$} $env(HOME) {} home
	    regsub ^~/ $vfd(filename) $home/ path
	    set path [RatEncodeQP system $path]
	    set def [list $vfd(name) file $flags $path]
	}
	mh {
	    if {"" == $vfd(filename)} {
		Popup $t(need_mh_name) $vf(w)
		return ""
	    }
	    set path [RatEncodeQP system $vfd(filename)]
	    set def [list $vfd(name) mh $flags $path]
	}
	dbase {
	    if {"" == $vfd(keywords)} {
		Popup $t(need_keyword) $vf(w)
		return ""
	    }
	    set def [list $vfd(name) dbase $flags $vfd(extype) \
		    $vfd(exdate) [list and keywords $vfd(keywords)]]
	}
	imap|dis {
	    if {"" == $vfd(mail_server)} {
		Popup $t(need_mail_server) $vf(w)
		return ""
	    }
	    if {1 == $vfd(disconnected)} {
		set vfd(type) dis
	    } else {
		set vfd(type) imap
	    }
	    if {$vfd(new) && $vfd(manage)} {
		set vfd(mailbox_path) [RatEncodeMutf7 $vfd(mailbox_path)]
	    }
	    set path [RatEncodeQP system $vfd(mailbox_path)]
	    set def [list $vfd(name) $vfd(type) $flags $vfd(mail_server) $path]
	    if {!$vfd(is_import) && "dis" == $vfd(type) \
		    && $vfd_old(mailbox_path) != $vfd(mailbox_path)} {
		set f [RatOpenFolder $def]
		$f close
	    }
	}
	pop3 {
	    if {"" == $vfd(mail_server)} {
		Popup $t(need_mail_server) $vf(w)
		return ""
	    }
	    set def [list $vfd(name) pop3 $flags $vfd(mail_server)]
	}
	dynamic {
	    if {![file isdirectory $vfd(filename)]} {
		Popup "$t(illegal_file_spec): $vfd(filename)" $vf(w)
		return ""
	    }
	    set path [RatEncodeQP system $vfd(filename)]
	    set def [list $vfd(name) dynamic $flags $path sender]
	}
    }
    if {$vfd(is_import)} {
	set flags [list subscribed $vfd(subscribed) \
		reimport $vfd(reimport)]
	set def [list $vfd(name) import $flags $def $vfd(pattern) {}]
    }
    return $def
}

# VFolderGetID --
#
# Get an ID to use for a new vFolderDef
#
# Arguments:

proc VFolderGetID {} {
    global vFolderDef
    set max [lindex [lsort -integer -decreasing [array names vFolderDef]] 0]
    return [expr $max + 1]
}

# VFolderApply --
#
# Apply the changes in the current details view
#
# Arguments:

proc VFolderApply {} {
    global vf vfd vfd_old vFolderDef mailServer t vFolderInbox env option \
	folderWindowList

    if {0 == [string length $vfd(name)]} {
	Popup $t(need_name) $vf(w)
	return
    }

    set redraw 0
    $vf(tree) autoredraw 0

    if {"mail_server" == $vfd(mode) || "pop3" == $vfd(type)} {
	if {"" == $vfd(host) || "" == $vfd(user)} {
	    Popup $t(need_host_and_user) $vf(w)
	    $vf(tree) autoredraw 1
	    return
	}

	if {$vfd(id) != $vfd(name) && "mail_server" == $vfd(mode)} {
	    if {[info exists mailServer($vfd(name))]} {
		Popup "$t(a_mailserver_named) $vfd(name) $t(already_exists)"
		$vf(tree) autoredraw 1
		return
	    }
	    foreach id [array names vFolderDef] {
		if {"import" == [lindex $vFolderDef($id) 1]} {
		    set d [lindex $vFolderDef($id) 3]
		    set i 1
		} else {
		    set d $vFolderDef($id)
		    set i 0
		}
		if {[regexp {imap|pop3|dis} [lindex $d 1]]
	            && $vfd(id) == [lindex $d 3]} {
		    set d [lreplace $d 3 3 $vfd(name)]
		    if {$i} {
			set vFolderDef($id) [lreplace $vFolderDef($id) 3 3 $d]
		    } else {
			set vFolderDef($id) $d
		    }
		}
	    }
	    unset mailServer($vfd(id))
	}

	set port ""
	set flags {}
	switch $vfd(method) {
	    tcp_default {
                if {"pop" == $vfd(type)} {
                    if {$vfd(ssl)} {
                        set port 995
                    } else {
                        set port 110
                    }
                } else {
                    if {$vfd(ssl)} {
                        set port 993
                    } else {
                        set port 143
                    }
                }
	    }
	    tcp_custom {
		set port $vfd(port)
	    }
	    rsh {
		set port {}
	    }
	}
	if {"pop3" == $vfd(type)} { lappend flags pop3 }
	foreach f {ssl notls novalidate-cert secure debug} {
	    if {$vfd($f)} {
		lappend flags $f
	    }
	}
	if {$vfd(ssh_cmd) != $option(ssh_template)} {
	    lappend flags [list ssh-cmd $vfd(ssh_cmd)]
	}
	if {"mail_server" == $vfd(mode)} {
	    set n $vfd(name)
	} else {
	    set n $vfd(mail_server)
	}
	set mailServer($n) [list $vfd(host) $port $flags $vfd(user)]
	if {"mail_server" == $vfd(mode)} {
	    VFolderAddMailServers
	    set redraw 1
	    $vf(tree) select [list $vfd(type) $vfd(name)]
	}
    }
    if {"folder" == $vfd(mode)} {
	if {$vfd(name) != $vfd_old(name) || $vfd(new)} {
	    set redraw 1
	}
	set def [VFolderConstructDef]
	if {"" == $def} {
	    $vf(tree) autoredraw 1
	    return
	}
	# If we have changed any significant parts of the folder definition
	# and we have any open instances then close all open instances.
	if {![VFolderSameBase $vFolderDef($vfd(id)) $def]
	    && "" != [set oh [RatGetOpenHandler $vFolderDef($vfd(id))]]} {
	    $oh close 1
	    foreach fhd [array names folderWindowList] {
		if {"$oh" == $folderWindowList($fhd)} {
		    FolderWindowClear $fhd
		}
	    }
	}
	if {$vfd(monitor) != $vfd_old(monitor)} {
	    global vFolderMonitorFH vFolderMonitorID folderExists
	    incr redraw
	    if {$vfd(monitor)} {
		if {![catch {RatOpenFolder $def} nhd]} {
		    set vFolderMonitorFH($vfd(id)) $nhd
		    set vFolderMonitorID($nhd) $vfd(id)
		    set folderExists($nhd) $folderExists($nhd)
		}
	    } else {
		catch {$vFolderMonitorFH($vfd(id)) close}
		catch {unset vFolderMonitorID($vFolderMonitorFH)}
		catch {unset vFolderMonitorFH($vfd(id))}
	    }
	}
	if {$vfd(watch) != $vfd_old(watch)} {
	    incr redraw
	}
	if {$vfd_old(inbox) != $vfd(inbox)} {
	    incr redraw
	    if {$vfd(inbox)} {
		set oldId $vFolderInbox
		set vFolderInbox $vfd(id)
		set text [VFolderGetItemName $oldId]
		$vf(tree) itemchange [list folder $oldId] -label $text
	    } else {
		set vFolderInbox {}
	    }
	}
        if {$vfd(type) == "imap" && $vfd_old(type) == "dis"} {
	    RatDeleteDisconnected $vfd(olddef)
        }
	set vFolderDef($vfd(id)) $def
	if {$redraw} {
	    set text [VFolderGetItemName $vfd(id)]
	    set image [VFolderGetItemImage $vfd(id)]
	    set iid [list folder $vfd(id)]
	    if {"import" == [lindex $def 1]} {
		set pos [$vf(tree) getpos $iid]
		# We can't use itemchange here since we must change
		# from folder to struct
		$vf(tree) delete $iid
		VFDInsert [lindex $pos 0] [lindex $pos 1] $vfd(id) folders
	    } else {
		$vf(tree) itemchange $iid -label $text -image $image
	    }
	}
	if {$vfd(manage)} {
	    RatBusy {
		if {$vfd(new)} {
		    if {[catch {RatCreateFolder $def} err]} {
			if {1 == [RatDialog $vf(w) ! \
				      "$t(mailbox_create_failed) $err" \
				      {} 0 $t(continue) $t(abort)]} {
			    trace vdelete vfd w VFolderDefChange
			    $vf(but_apply) configure -state disabled
			    $vf(but_restore) configure -state disabled
			    set vf(unapplied_changes) 0
			    $vf(tree) delete [list folder $vfd(id)]
			    $vf(tree) redraw
			    unset vFolderDef($vfd(id))
			    foreach c [winfo children $vf(details)] {
				destroy $c
			    }
			    return
			}
		    }
		}
	    }
	}

    } elseif {"struct" == $vfd(mode)} {
	if {$vfd(name) != $vfd_old(name)} {
	    $vf(tree) itemchange [list folder $vfd(id)] -label $vfd(name)
	    set redraw 1
	    set vFolderDef($vfd(id)) \
		    [lreplace $vFolderDef($vfd(id)) 0 0 $vfd(name)]
	}
    }

    $vf(tree) autoredraw 1
    if {$redraw} {
	$vf(tree) redraw
    }

    $vf(but_apply) configure -state disabled
    $vf(but_restore) configure -state disabled
    set vf(unapplied_changes) 0

    if {$vfd(new)} {
	if {"mail_server" == $vfd(mode)} {
	    VFolderSetupMailServer $vfd(type) $vfd(name) 0
	} else {
	    if {1 == $vfd(import_on_create)} {
		VFolderReimport $vfd(id)
	    }
	    VFolderSetupDetails $vfd(id) 0 {}
	}
    }

    # Create copy of values
    foreach v [array names vfd] {
	set vfd_old($v) $vfd($v)
    }

    VFolderCheckChanges
    return
}

# VFolderSameBase --
#
# Check if the two given folder definitions points to the same base folder
#
# Arguments:
# def1, def2 - Two folder definitions

proc VFolderSameBase {def1 def2} {
    if {[lindex $def1 1] != [lindex $def2 1]} {
	return 0
    }
    for {set i 3} {$i <= [llength $def1]} {incr i} {
	if {[lindex $def1 $i] != [lindex $def2 $i]} {
	    return 0
	}
    }
    return 1
}

# VFolderRemoveNew --
#
# Invoked when canceling creating a folder
#
# Arguments:

proc VFolderRemoveNew {} {
    global vfd vf mailServer vFolderDef

    if {"mail_server" == $vfd(mode)} {
	$vf(tree) delete [list $vfd(type) $vfd(id)]
	unset mailServer($vfd(name))
    } else {
	$vf(tree) delete [list folder $vfd(id)]
	unset vFolderDef($vfd(id))
    }
    foreach c [winfo children $vf(details)] {
	destroy $c
    }
    if {[info exists vfd(old)]} {
	unset vfd_old
	set vfd_old(marker) {}
    }
    trace vdelete vfd w VFolderDefChange
}

# VFolderRestore --
#
# Restore values in folder definition
#
# Arguments:

proc VFolderRestore {} {
    global vfd vfd_old vf vFolderDef

    if {0 == $vfd(new)} {
	foreach v [array names vfd_old] {
	    set vfd($v) $vfd_old($v)
	}
    } else {
	VFolderRemoveNew
    }
    set vf(unapplied_changes) 0
    $vf(but_apply) configure -state disabled
    $vf(but_restore) configure -state disabled
    VFolderCheckChanges
}

# VFolderNewStruct --
#
# Initiate creation of a new folder structure object
#
# Arguments:
# context - menu or tree, describes from where we were invoked

proc VFolderNewStruct {context} {
    global vf vFolderDef option t

    # Check if it is ok
    if {0 == [VFolderChangeOk]} {
	return
    }

    # Determine where to place it
    if {"menu" == $context} {
	set vf(into_item) $vf(folderitem)
	set vf(into_pos) end
    }

    # Create template
    set id [VFolderGetID]
    set vFolderDef($id) [list $t(new_submenu) struct {} {}]
    $vf(tree) autoredraw 0
    VFDInsert $vf(into_item) $vf(into_pos) $id folders
    $vf(tree) select [list folder $id]
    $vf(tree) autoredraw 1
    $vf(tree) redraw
    VFolderSetupDetails $id 1 {}
}

# VFolderAddFolder --
#
# Adds a folder created by the wizard
#
# Arguments:
# context - Add context
# def     - Folder definition

proc VFolderAddFolder {context def} {
    global vf vFolderDef

    # Determine where to place it
    if {"menu" == $context} {
	set vf(into_item) $vf(folderitem)
	set vf(into_pos) end
    }

    set id [VFolderGetID]
    set vFolderDef($id) $def
    $vf(tree) autoredraw 0
    VFDInsert $vf(into_item) $vf(into_pos) $id folders
    $vf(tree) select [list folder $id]
    $vf(tree) autoredraw 1
    $vf(tree) redraw
    VFolderSetupDetails $id 0 {}
    return $id
}

# VFolderPostMenu:
#
# Post a the menu for a given entry.
#
# Arguments:
# x, y   - Position of pointer, this is where we will popup the menu
# id     - Id of selected element
# m      - Menu to popup

proc VFolderPostMenu {x y id m} {
    global vf vFolderInbox b option

    set pos [$vf(tree) getpos $id]
    set vf(into_item) [lindex $pos 0]
    set vf(into_pos) [expr {[lindex $pos 1]+1}]
    set vf(template) [lindex $id 1]
    set vf(iid) $id
    set delete_state normal
    set delete_bal vd_delete
    set rid [lindex $id 1]
    if {"folder" == [lindex $id 0] 
	&& $rid == $vFolderInbox} {
	set delete_state disabled
	set delete_bal cant_delete_inbox
    } else {
	foreach r $option(roles) {
	    if {$rid == $option($r,save_outgoing)} {
		set delete_state disabled
		set delete_bal "cant_delete_used"
		break
	    }
	}
    }

    $m entryconfigure $vf(folder_menu_delete) -state $delete_state
    set b($m,$vf(folder_menu_delete)) $delete_bal
    tk_popup $m $x $y
}

# VFolderReimport
#
# Reimport a folder now. First we must get the definition and then we can do
# the import
#
# Arguments:
# id - Id of folder to reimport.

proc VFolderReimport {id} {
    global vFolderDef vf

    if {![VFolderChangeOk]} {
	return
    }
    RatBusy {RatImport $id}

    VFolderRedrawSubtree $id
}

# VFolderRedrawSubtree
#
# Redraws a subtree in the list. Useful after reimporting stuff etc.
#
# Arguments:
# id - Id of folder to redraw the children of

proc VFolderRedrawSubtree {id} {
    global vFolderDef vf

    $vf(tree) autoredraw 0
    $vf(item,$id) clear
    foreach sid [lindex $vFolderDef($id) 5] {
	VFDInsert $vf(item,$id) end $sid {}
    }
    $vf(tree) autoredraw 1
    $vf(tree) redraw
    VFolderCheckChanges
}

# VFolderDeleteServer --
#
# Perhaps delete the current server object
#
# Arguments:

proc VFolderDeleteServer {} {
    global t vf vfd vFolderDef mailServer

    set u ""
    foreach id [array names vFolderDef] {
	if {"import" == [lindex $vFolderDef($id) 1]} {
	    set f [lindex $vFolderDef($id) 3]
	} else {
	    set f $vFolderDef($id)
	}
	if {[regexp "imap|pop3|dis" [lindex $f 1]]
	    && [lindex $vf(iid) 1] == [lindex $f 3]} {
	    lappend ids $id
	    set u "$u\n\t[lindex $f 0]"
	}
    }
    if {"" != $u} {
	Popup "$t(mailserver_used): $u" $vf(w)
	return
    }
    $vf(tree) delete $vf(iid)
    unset mailServer($vf(template))

    if {[info exists vfd(id)] && $vfd(id) == [lindex $vf(iid) 1]} {
	VFolderClearWindow
    }

    VFolderCheckChanges
}

# VFolderDeleteFolder --
#
# Perhaps delete the current folder object
#
# Arguments:

proc VFolderDeleteFolder {} {
    global t vf vfd vFolderDef option mailServer

    set id [lindex $vf(iid) 1]
    if {"struct" == [lindex $vFolderDef($id) 1]} {
	set children [lindex $vFolderDef($id) 3]
    } elseif {"import" == [lindex $vFolderDef($id) 1]} {
	set children [lindex $vFolderDef($id) 5]
    } else {
	set children {}
    }
    if {0 < [llength $children]} {
	if {[RatDialog $vf(w) ! $t(item_not_empty) {} 0 $t(delete) \
		 $t(cancel)]} {
	    return
	}
    }
    set keep [RatDialog $vf(w) ! $t(delete_what_folder) {} 1 \
		  $t(delete_both) $t(only_in_tkrat)]
    $vf(tree) delete $vf(iid)
    set idlist $id
    for {set i 0} {$i < [llength $idlist]} {incr i} {
	set id [lindex $idlist $i]
	if {0 == $keep} {
	    RatDeleteFolder $vFolderDef($id)
	}
	if {"struct" == [lindex $vFolderDef($id) 1]} {
	    set idlist [concat $idlist [lindex $vFolderDef($id) 3]]
	} elseif {"import" == [lindex $vFolderDef($id) 1]} {
	    set idlist [concat $idlist [lindex $vFolderDef($id) 5]]
	} elseif {"pop3" == [lindex $vFolderDef($id) 1]} {
	    unset mailServer([lindex $vFolderDef($id) 3])
	} elseif {"dis" == [lindex $vFolderDef($id) 1]} {
	    RatDeleteDisconnected $vFolderDef($id)
	}
	unset vFolderDef($id)
    }

    if {[info exists vfd(id)] && $vfd(id) == $id} {
	VFolderClearWindow
    }
    
    VFolderCheckChanges
}

# VFolderReimportAll -
#
# Reimport all import-folders
#
# Arguments:

proc VFolderReimportAll {} {
    global vFolderDef vf

    if {![info exists vf(w)]} {
	set vf(w) ""
    }
    if {[winfo exists $vf(w)]} {
	if {![VFolderChangeOk]} {
	    return
	}
	$vf(tree) autoredraw 0
    }

    RatBusy {
	foreach id [array names vFolderDef] {
            if {![info exists vFolderDef($id)]} {
                continue
            }
	    if {"import" != [lindex $vFolderDef($id) 1]} {
		continue
	    }
	    
	    RatImport $id
	    if {[winfo exists $vf(w)] && [info exists vf(item,$id)]} {
		$vf(item,$id) clear
		foreach sid [lindex $vFolderDef($id) 5] {
		    VFDInsert $vf(item,$id) end $sid {}
		}
	    }
	}
    }
    if {[winfo exists $vf(w)]} {
	$vf(tree) autoredraw 1
	$vf(tree) redraw
    }
}
