# vfolderwizard.tcl -
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.
#
#
# Functions which implements the folder creation wizard
#

###############################################################################
# Generic functions

proc VFolderWizardStart {context } {
    global t propBigFont idCnt

    set id vfolderwizard[incr idCnt]
    set w .$id
    upvar \#0 $id hd
    set hd(history) {}
    set hd(top) $w
    set hd(context) $context

    # Create toplevel and insert infrastructure
    toplevel $w -class TkRat
    wm title $w $t(vfolderdef)
    label $w.title -textvariable ${id}(title) -font $propBigFont
    set hd(body) [rat_scrollframe::create $w.body -bd 10]
    frame $w.buttons
    button $w.buttons.prev -text "< $t(wiz_previous)" -state disabled \
	-command "set cmd \[lindex \$${id}(history) end-1\]; \
                  set ${id}(history) \[lreplace \$${id}(history) end-1 end\]; \
                  \$cmd $id"
    button $w.buttons.next -text "$t(wiz_next) >" -state disabled \
	-command "VFolderWizardStep1Done $id"
    button $w.buttons.cancel -text $t(wiz_cancel) \
	-command "::tkrat::winctl::RecordGeometry vFolderWizard $w $w.body; unset $id; destroy $w"
    pack $w.buttons.cancel -side right -padx 20 -pady 5
    pack $w.buttons.next $w.buttons.prev -side right
    pack $w.title -side top -fill x
    pack $w.buttons -side bottom -fill x
    pack $w.body -fill both -expand 1
    set hd(next) $w.buttons.next
    set hd(prev) $w.buttons.prev
    set hd(cancel) $w.buttons.cancel
    set hd(bodym) $w.body

    bind $w <Return> "$hd(next) invoke"
    bind $w <Escape> "$w.buttons.cancel invoke"
    wm protocol $w WM_DELETE_WINDOW "$w.buttons.cancel invoke"    

    grid propagate $w.body 0
    grid columnconfigure $w.body 0 -weight 1
    ::tkrat::winctl::SetGeometry vFolderWizard $w $w.body

    VFolderWizardStep1 $id
}

proc VFolderWizardReset {id} {
    upvar \#0 $id hd
    global t

    eval destroy [winfo children $hd(body)]
    grid columnconfigure $hd(body) 0 -weight 0
    grid columnconfigure $hd(body) 1 -weight 0
    $hd(next) configure -text "$t(wiz_next) >"
    # Work around bug in grid (reported and fixed Oct 2002)
    grid size $hd(body)
}

proc VFolderWizardStep1 {id} {
    upvar \#0 $id hd
    global t vFolderDef

    # Come up with name suggestion
    set max 0
    foreach i [array names vFolderDef] {
	if {[regexp "$t(folder) (\[0-9\]+)" \
		 [lindex $vFolderDef($i) 0] unused num]
	    && $num > $max} {
	    set max $num
	}
    }
    set hd(name) "Folder [incr max]"
    if {![info exists hd(type)]} {
        set hd(type) file
    }

    VFolderWizardReset $id
    lappend hd(history) VFolderWizardStep1

    # Setup the protocl selection
    set hd(title) $t(type)
    label $hd(body).label -text $t(select_type_of_folder)
    grid $hd(body).label -pady 10
    set a [list $hd(next) $hd(prev)]    
    foreach tt {{file VFolderWizardFile}
	{imap VFolderWizardIMAP}
	{dbase VFolderWizardDBase}
	{mh VFolderWizardMH}
	{pop VFolderWizardPOP}
	{dynamic VFolderWizardDynamic}} {
	set type [lindex $tt 0]
	radiobutton $hd(body).$type -anchor w \
	    -variable ${id}(type) -value $type -text $t(fw_$type) \
	    -command "$hd(next) configure -state normal; \
                      set ${id}(nextcmd) [lindex $tt 1]"
	grid $hd(body).$type -sticky we
	lappend a $hd(body).$type
        if {$type == $hd(type)} {
            set hd(nextcmd) [lindex $tt 1]
        }
    }
    $hd(prev) configure -state disabled
    $hd(next) configure -command "\$${id}(nextcmd) $id"
    if {"" != $hd(type)} {
	$hd(next) configure -state normal
    }
    grid rowconfigure $hd(body) 20 -weight 1
    SetupShortcuts $a
}


proc VFolderWizardTestImport {id} {
    upvar \#0 $id hd
    global mailServer idCnt t

    # Create window
    set w .w[incr idCnt]
    toplevel $w -class TkRat
    wm title $w $t(test_import)
    if {"imap" == $hd(type)} {
	label $w.server_l -text $t(server): -anchor e
	label $w.server -text $hd(server) -anchor w
	grid $w.server_l $w.server -sticky ew
    }
    label $w.path_l -text $t(path): -anchor e
    label $w.path -text $hd(path) -anchor w
    grid $w.path_l $w.path -sticky ew
    frame $w.f
    scrollbar $w.f.scroll \
	    -relief sunken \
	    -bd 1 \
	    -highlightthickness 0 \
	    -command "$w.f.list yview"
    listbox $w.f.list \
	    -yscroll "$w.f.scroll set" \
	    -relief sunken \
	    -bd 1 \
	    -exportselection false \
	    -highlightthickness 0
    pack $w.f.scroll -side right -fill y
    pack $w.f.list -expand 1 -fill both
    grid $w.f - -pady 10 -padx 5 -sticky nsew

    button $w.close -text $t(dismiss) -command "destroy $w"
    grid $w.close - -pady 10
    grid columnconfigure $w 0 -weight 1
    grid columnconfigure $w 1 -weight 1
    grid rowconfigure $w 2 -weight 1
    bind $w <Escape> "$w.close invoke"
    ::tkrat::winctl::SetGeometry testImportResult $w $w.f.list
    bind $w.f.list <Destroy> \
        "::tkrat::winctl::RecordGeometry testImportResult $w $w.f.list"

    # Do test import
    set cleanup ""
    switch $hd(type) {
	imap {
	    if {"new" == $hd(server_action)} {
		set mailServer($hd(server)) $hd(mailServer)
		set cleanup "unset mailServer($hd(server))"
	    }
	    set def [list $hd(name) imap {} $hd(server) $hd(path)]
	}
	file {
	    set def [list $hd(name) file {} $hd(path)]
	}
	mh {
	    set def [list $hd(name) mh {} $hd(path)]
	}
    }
    set r [RatBusy [list RatTestImport "%" $def]]
    if {"" != $cleanup} {
	eval $cleanup
    }

    # Build result
    set result {}
    foreach i $r {
	set f "[lindex $i 2]"
	if {-1 == [lsearch -exact [lindex $i 0] noinferiors]} {
	    set f "$f[lindex $i 1]"
	}
	lappend result $f
    }

    # Show result
    eval $w.f.list insert end [lsort $result]
}

proc VFolderWizardShowImport {id} {
    upvar \#0 $id hd
    global t

    VFolderWizardReset $id
    $hd(next) configure -text $t(close) \
	-command "$hd(cancel) configure -state normal; $hd(cancel) invoke"
    $hd(prev) configure -state disable
    $hd(cancel) configure -state disable
    grid columnconfigure $hd(body) 1 -weight 1

    set hd(num_found) 0
    set hd(last_found) {}

    set hd(title) $t(importing)...
    rat_flowmsg::create $hd(body).message -text "$t(currently_importing)"
    grid $hd(body).message - -pady 10

    frame $hd(body).space1 -height 20
    grid $hd(body).space1

    label $hd(body).found_l -text "   $t(found):" -anchor e
    label $hd(body).counter -textvariable ${id}(num_found) -anchor w
    grid $hd(body).found_l $hd(body).counter -pady 5 -sticky ew

    label $hd(body).last_l -text "   $t(last):" -anchor e
    label $hd(body).last -textvariable ${id}(last_found) -anchor w
    grid $hd(body).last_l $hd(body).last -pady 5 -sticky ew

    update
    set hd(next_update) [expr [clock clicks -milliseconds]+100]
    RatBusy {RatImport $hd(folder_id) "VFolderWizardImportCallback $id"}
    VFolderRedrawSubtree $hd(folder_id)
}

proc VFolderWizardImportCallback {id folder flags} {
    upvar \#0 $id hd

    incr hd(num_found)
    set hd(last_found) $folder

    set now [clock clicks -milliseconds]
    if {$now > $hd(next_update)} {
	update
	set hd(next_update) [expr [clock clicks -milliseconds]+100]
    }
}

proc VFolderWizardServerCheck {id} {
    upvar \#0 $id hd
    global t

    set state normal

    if {"" == $hd(host)
	|| "" == $hd(name)
	|| "" == $hd(user)
	|| ("tcp_custom" == $hd(method) && "" == $hd(port))
	|| ("rsh" == $hd(method) && "" == $hd(ssh_cmd))} {
	set state disabled
    }

    $hd(next) configure -state $state
}

proc VFolderWizardBuildNewServer {id prot} {
    upvar \#0 $id hd
    global option

    set port ""
    set flags {}
    if {"pop3" == $prot} {
	lappend flags pop3
	set defaultPort 110
    } else {
	set defaultPort 143
    }
    switch $hd(method) {
	tcp_default {
	    set port $defaultPort
	}
	tcp_custom {
	    set port $hd(port)
	}
	rsh {
	    set port {}
	}
    }
    foreach f {ssl notls novalidate-cert} {
	if {$hd($f)} {
	    lappend flags $f
	}
    }
    if {$hd(ssh_cmd) != $option(ssh_template)} {
	lappend flags [list ssh-cmd $hd(ssh_cmd)]
    }
    return [list $hd(host) $port $flags $hd(user)]
}

proc CheckCClientFolder {id def} {
    upvar \#0 $id hd
    global t mailServer

    if {"new" == $hd(server_action)} {
	set mailServer($hd(server)) $hd(mailServer)
    }
    # Test import just this name
    #   not-exist -> new folder
    #   selectable -> insert as existing
    #   inferiors -> import
    set import_result [RatBusy [list RatTestImport "" $def]]
    if {0 == [llength $import_result]} {
	# Did not exist, create it
	RatBusy {catch {RatCreateFolder $def} r}
	if {"1" != $r} {
	    return
	}
	set hd(folder_id) [VFolderAddFolder $hd(context) $def]
    } else {
	set flags [lindex [lindex $import_result 0] 0]
	if {-1 == [lsearch -exact $flags noselect]} {
	    # Selectable, add it
	    set hd(folder_id) [VFolderAddFolder $hd(context) $def]
	}
	if {-1 == [lsearch -exact $flags noinferiors]} {
	    # Can have inferiors, see if it does
	    set import_result [RatBusy [list RatTestImport "%" $def]]
	    if {0 < [llength $import_result]} {
		# There are inferiors, import them
		set def [list $hd(name) import {} $def * {}]
		set hd(folder_id) [VFolderAddFolder $hd(context) $def]
	    }
	}
    }

    if {"new" == $hd(server_action)} {
	VFolderAddMailServers
    }
    if {"import" == [lindex $def 1]} {
	VFolderWizardShowImport $id
    } else {
	$hd(cancel) invoke
    }
}

###############################################################################
# File folders

proc VFolderWizardFile {id} {    
    upvar \#0 $id hd
    global t

    VFolderWizardReset $id
    grid columnconfigure $hd(body) 1 -weight 1
    lappend hd(history) VFolderWizardFile

    set hd(title) $t(define_file_folder)
    rat_flowmsg::create $hd(body).message -text "$t(why_filefolder)"
    grid $hd(body).message - -pady 10

    label $hd(body).name_l -text $t(name): -anchor e
    entry $hd(body).name_e -textvariable ${id}(name)
    grid $hd(body).name_l $hd(body).name_e -sticky we

    frame $hd(body).space -height 20
    grid $hd(body).space

    if {![info exists hd(path)]} {
	set hd(path) "[pwd]/"
    }
    label $hd(body).file_l -text $t(path): -anchor e
    entry $hd(body).file_e -textvariable ${id}(path)
    button $hd(body).browse -text $t(browse)... \
	-command "Browse $hd(body) ${id}(path) dirok"
    grid $hd(body).file_l $hd(body).file_e -sticky we
    grid x $hd(body).browse -sticky e

    frame $hd(body).space2 -height 20
    grid $hd(body).space2

    $hd(next) configure -text $t(wiz_finish) \
	-command "VFolderWizardFileDone $id"

    rat_scrollframe::recalc $hd(bodym)
    $hd(prev) configure -state normal
    SetupShortcuts [list $hd(next) $hd(prev) $hd(body).browse]
    $hd(body).name_e selection range 0 end
    $hd(body).name_e icursor end
    focus $hd(body).name_e
}

proc VFolderWizardFileDone {id} {
    upvar \#0 $id hd
    global t
    
    set def [list $hd(name) file {} $hd(path)]
    if {[file isdirectory $hd(path)]} {
	set def [list $hd(name) import {} $def * {}]
    } elseif {[file exists $hd(path)]} {
	RatBusy {set fail [catch {RatCheckFolder $def} i]}
	if {$fail} {
	    return
	}
    } else {
	RatBusy {catch {RatCreateFolder $def} r}
	if {"1" != $r} {
	    return
	}
    }

    set hd(folder_id) [VFolderAddFolder $hd(context) $def]
    if {[file isdirectory $hd(path)]} {
	VFolderWizardShowImport $id
    } else {
	$hd(cancel) invoke
    }
}

###############################################################################
# IMAP folders

proc VFolderWizardIMAP {id} {    
    upvar \#0 $id hd
    global t mailServer

    VFolderWizardReset $id
    lappend hd(history) VFolderWizardIMAP

    set imaps {}
    foreach ms [lsort -dictionary [array names mailServer]] {
        if { -1 == [lsearch -exact [lindex $mailServer($ms) 2] pop3]} {
            lappend imaps $ms
        }
    }
    if {![info exists hd(server)]} {
	if {0 == [llength $imaps]} {
	    set hd(server_action) new
	    VFolderWizardIMAPStep1Done $id
	    return
	} else {
	    set hd(server) [lindex $imaps 0]
	    set hd(server_action) reuse
	}
    }

    grid columnconfigure $hd(body) 1 -weight 1

    set hd(title) $t(define_imap_folder)
    rat_flowmsg::create $hd(body).message -text "$t(imap_step1)"
    grid $hd(body).message - -pady 10

    frame $hd(body).space1 -height 20
    grid $hd(body).space1

    radiobutton $hd(body).new -text $t(define_new) -value new -anchor w \
	-variable ${id}(server_action) -command "VFolderWizardIMAPCheck $id"
    grid $hd(body).new -sticky we

    frame $hd(body).space2 -height 10
    grid $hd(body).space2

    radiobutton $hd(body).reuse -text $t(reuse_imap) \
	-variable ${id}(server_action) \
	-value reuse -command "VFolderWizardIMAPCheck $id"
    menubutton $hd(body).sel -indicatoron 1 -relief raised -bd 1 \
	-menu $hd(body).sel.m -textvariable ${id}(server) -width 20 \
	-takefocus 1 -highlightthickness 1
    menu $hd(body).sel.m
    foreach m $imaps {
	$hd(body).sel.m add command -label $m -command \
	    [list VFolderWizardIMAPSelServer $id $m]
    }
    grid $hd(body).reuse $hd(body).sel -sticky w

    $hd(next) configure -command "VFolderWizardIMAPStep1Done $id"

    VFolderWizardIMAPCheck $id
    rat_scrollframe::recalc $hd(bodym)
    SetupShortcuts [list $hd(next) $hd(prev) $hd(body).new $hd(body).reuse]
    $hd(prev) configure -state normal
}

proc VFolderWizardIMAPCheck {id} {
    upvar \#0 $id hd

    set state normal
    if {"reuse" == $hd(server_action) && "" == $hd(server)} {
	set state disabled
    }
    $hd(next) configure -state $state
}

proc VFolderWizardIMAPSelServer {id server} {
    upvar \#0 $id hd

    set hd(server_action) reuse
    set hd(server) $server
    VFolderWizardIMAPCheck $id
}

proc VFolderWizardIMAPStep1Done {id} {
    upvar \#0 $id hd

    if {"reuse" == $hd(server_action)} {
	VFolderWizardIMAPPath $id
    } else {
	VFolderWizardIMAPServer $id
    }
}

proc VFolderWizardIMAPServer {id {mode wizard}} {
    global t option env
    upvar \#0 $id hd

    if {"wizard" == $mode} {
	VFolderWizardReset $id
	lappend hd(history) VFolderWizardIMAPServer
    }

    # Defaults
    if {![info exists hd(user)]} {
	set hd(user) $env(USER)
    }
    if {![info exists hd(method)]} {
	set hd(method) tcp_default
	set hd(ssh_cmd) $option(ssh_template)
	set hd(priv) tls
	set hd(ssl) 0
	set hd(notls) 0
    }
    
    rat_flowmsg::create $hd(body).message -text "$t(imap_def)"
    grid $hd(body).message - -pady 10

    label $hd(body).host_l -text $t(host): -anchor e
    entry $hd(body).host_e -textvariable ${id}(host)
    grid $hd(body).host_l $hd(body).host_e -sticky ew
    bind $hd(body).host_e <FocusOut> "VFolderWizardServerCheck $id"
    
    label $hd(body).user_l -text $t(user): -anchor e
    entry $hd(body).user_e -textvariable ${id}(user)
    grid $hd(body).user_l $hd(body).user_e -sticky ew
    bind $hd(body).user_e <FocusOut> "VFolderWizardServerCheck $id"

    frame $hd(body).msp2 -height 10
    grid $hd(body).msp2
    
    label $hd(body).conn_l -text $t(connect): -anchor e
    radiobutton $hd(body).conn_tcpdef -text $t(tcp_default) \
	-variable ${id}(method) -value tcp_default -anchor w \
	-command "VFolderWizardServerCheck $id"
    frame $hd(body).conn_tcpcust
    radiobutton $hd(body).conn_tcpcust.b -text $t(tcp_custom): \
	-variable ${id}(method) -value tcp_custom \
	-command "VFolderWizardServerCheck $id"
    entry $hd(body).conn_tcpcust.e -width 6 -textvariable ${id}(port)
    pack $hd(body).conn_tcpcust.b $hd(body).conn_tcpcust.e -side left
    radiobutton $hd(body).conn_rsh -text $t(rsh_ssh) -variable ${id}(method) \
	-value rsh -command "VFolderWizardServerCheck $id"
    grid $hd(body).conn_l $hd(body).conn_tcpdef - -sticky ew
    grid x $hd(body).conn_tcpcust -sticky w
    grid x $hd(body).conn_rsh -sticky w

    label $hd(body).ssh_l -text $t(ssh_command): -anchor e
    entry $hd(body).ssh_e -width 40 -textvariable ${id}(ssh_cmd)
    grid $hd(body).ssh_l $hd(body).ssh_e -sticky ew
    bind $hd(body).ssh_e <FocusOut> "VFolderWizardServerCheck $id"

    frame $hd(body).msp3 -height 10
    grid $hd(body).msp3

    label $hd(body).priv_l -text $t(privacy): -anchor e
    radiobutton $hd(body).priv_ssl -text $t(use_ssl) -anchor w \
	    -variable ${id}(priv) -value ssl \
	    -command "set ${id}(ssl) 1;set ${id}(notls) 1"
    radiobutton $hd(body).priv_tls -text $t(try_tls) \
	    -variable ${id}(priv) -value tls \
	    -command "set ${id}(ssl) 0;set ${id}(notls) 0"
    radiobutton $hd(body).priv_none -text $t(no_encryption) \
	    -variable ${id}(priv) -value none \
	    -command "set ${id}(ssl) 0;set ${id}(notls) 1"
    grid $hd(body).priv_l $hd(body).priv_ssl -sticky ew
    grid x $hd(body).priv_tls -sticky w
    grid x $hd(body).priv_none -sticky w

    frame $hd(body).msp4 -height 10
    grid $hd(body).msp4

    label $hd(body).flags_l -text $t(flags): -anchor e
    checkbutton $hd(body).flag_checkc -text $t(ssl_check_cert) \
	-variable ${id}(novalidate-cert) -onvalue 0 -offvalue 1 -anchor w
    grid $hd(body).flags_l $hd(body).flag_checkc -sticky ew

    if {"wizard" == $mode} {
	$hd(next) configure -state disabled \
	    -command "VFolderWizardIMAPPath $id"
	rat_scrollframe::recalc $hd(bodym)
    }

    VFolderWizardServerCheck $id
    focus $hd(body).host_e
}

proc VFolderWizardIMAPPath {id} {
    upvar \#0 $id hd
    global t mailServer option

    # Possibly setup server
    if {"new" == $hd(server_action)} {
	set hd(mailServer) [VFolderWizardBuildNewServer $id imap]
	set hd(server) $hd(host)
	set i 1
	while {[info exists mailServer($hd(server))]} {
	    set hd(server) "$hd(host)-[incr i]"
	}
    }
    if {![info exists hd(path)]} {
	set hd(path) ""
    }

    # Populate window
    VFolderWizardReset $id
    lappend hd(history) VFolderWizardIMAPPath
    grid columnconfigure $hd(body) 1 -weight 1

    rat_flowmsg::create $hd(body).message -text "$t(why_imap_folder)"
    grid $hd(body).message - -pady 10

    label $hd(body).name_l -text $t(name): -anchor e
    entry $hd(body).name_e -textvariable ${id}(name)
    grid $hd(body).name_l $hd(body).name_e -sticky we

    frame $hd(body).space -height 20
    grid $hd(body).space

    label $hd(body).path_l -text $t(path): -anchor e
    entry $hd(body).path_e -textvariable ${id}(path)
    grid $hd(body).path_l $hd(body).path_e -sticky we

    frame $hd(body).space2 -height 20
    grid $hd(body).space2

    rat_flowmsg::create $hd(body).dismess -text "$t(imap_offline)"
    grid $hd(body).dismess -
    checkbutton $hd(body).use_dis -variable ${id}(use_dis) \
	-text $t(enable_offline)
    grid $hd(body).use_dis - -sticky w -padx 10

    frame $hd(body).space3 -height 20
    grid $hd(body).space3

    rat_flowmsg::create $hd(body).testmsg -text "$t(imap_test_import)"
    grid $hd(body).testmsg -
    button $hd(body).test -text $t(test_import)... \
	-command "VFolderWizardTestImport $id"
    grid $hd(body).test -

    $hd(next) configure -text $t(wiz_finish) \
	-command "VFolderWizardIMAPDone $id"

    rat_scrollframe::recalc $hd(bodym)
    SetupShortcuts [list $hd(next) $hd(prev)]
    $hd(body).name_e selection range 0 end
    $hd(body).name_e icursor end
    focus $hd(body).name_e
}

proc VFolderWizardIMAPDone {id} {
    upvar \#0 $id hd

    if $hd(use_dis) {
	set proto dis
    } else {
	set proto imap
    }
    set def [list $hd(name) $proto {} $hd(server) $hd(path)]
    CheckCClientFolder $id $def
}

###############################################################################
# MH folders

proc VFolderWizardMH {id} {
    upvar \#0 $id hd
    global t

    VFolderWizardReset $id
    grid columnconfigure $hd(body) 1 -weight 1
    lappend hd(history) VFolderWizardMH

    if {![info exists hd(path)]} {
	set hd(path) ""
    }
    set hd(server_action) none

    rat_flowmsg::create $hd(body).message -text "$t(why_mhfolder)"
    grid $hd(body).message - -pady 10

    label $hd(body).name_l -text $t(name): -anchor e
    entry $hd(body).name_e -textvariable ${id}(name)
    grid $hd(body).name_l $hd(body).name_e -sticky we -pady 10

    label $hd(body).path_l -text $t(path): -anchor e
    entry $hd(body).path_e -textvariable ${id}(path)
    grid $hd(body).path_l $hd(body).path_e -sticky we -pady 10

    rat_flowmsg::create $hd(body).testmsg -text "$t(mh_test_import)"
    grid $hd(body).testmsg -
    button $hd(body).test -text $t(test_import)... \
	-command "VFolderWizardTestImport $id"
    grid $hd(body).test -

    $hd(next) configure -text $t(wiz_finish) \
	-command "VFolderWizardMHDone $id"

    rat_scrollframe::recalc $hd(bodym)
    SetupShortcuts [list $hd(next) $hd(prev)]
    $hd(body).name_e selection range 0 end
    $hd(body).name_e icursor end
    focus $hd(body).name_e
    $hd(prev) configure -state normal
}

proc VFolderWizardMHDone {id} {
    upvar \#0 $id hd

    set def [list $hd(name) mh {} $hd(path)]
    CheckCClientFolder $id [list $hd(name) mh {} $hd(path)]
}

###############################################################################
# DBase folders

proc VFolderWizardDBase {id} {
    upvar \#0 $id hd
    global t option

    VFolderWizardReset $id
    grid columnconfigure $hd(body) 1 -weight 1
    lappend hd(history) VFolderWizardDBase

    set hd(title) $t(define_dbase_folder)

    rat_flowmsg::create $hd(body).message -text $t(why_dbase_folder)
    grid $hd(body).message - - -pady 10

    label $hd(body).name_l -text $t(name): -anchor e
    entry $hd(body).name_e -textvariable ${id}(name)
    grid $hd(body).name_l $hd(body).name_e - -sticky we

    frame $hd(body).space -height 20
    grid $hd(body).space

    if {![info exists hd(extype)]} {
	set hd(extype) $option(def_extype)
	set hd(exdate) $option(def_exdate)
    }

    label $hd(body).kw_lab -text $t(keywords): -anchor e
    entry $hd(body).kw_entry -textvariable ${id}(keywords) -width 20
    grid $hd(body).kw_lab $hd(body).kw_entry - -sticky we -pady 10

    label $hd(body).exdate_lab -text $t(exdate): -anchor e
    entry $hd(body).exdate_entry -textvariable ${id}(exdate)   
    label $hd(body).exdate_unit -text ($t(days)) -anchor w
    grid $hd(body).exdate_lab $hd(body).exdate_entry \
	$hd(body).exdate_unit -sticky we -pady 10

    label $hd(body).extype_lab -text $t(extype): -anchor e
    frame $hd(body).extype
    foreach et {none remove incoming backup} {
	radiobutton $hd(body).extype.$et -anchor w -text $t($et) \
	    -variable ${id}(extype) -value $et
	pack $hd(body).extype.$et -side top -fill x
    }
    grid $hd(body).extype_lab $hd(body).extype - -sticky wen

    $hd(next) configure -text $t(finish) \
	-command "VFolderWizardDBaseDone $id"

    rat_scrollframe::recalc $hd(bodym)
    SetupShortcuts [list $hd(next) $hd(prev)]
    $hd(body).name_e selection range 0 end
    $hd(body).name_e icursor end
    focus $hd(body).name_e
    $hd(prev) configure -state normal
}

proc VFolderWizardDBaseDone {id} {
    upvar \#0 $id hd
    global t
    
    set def [list $hd(name) dbase {} $hd(extype) $hd(exdate) \
		 [list and keywords $hd(keywords)]]

    set hd(folder_id) [VFolderAddFolder $hd(context) $def]
    $hd(cancel) invoke
}

###############################################################################
# POP3 folders

proc VFolderWizardPOP {id {mode wizard}} {
    global t option env
    upvar \#0 $id hd

    # Defaults
    if {![info exists hd(user)]} {
	set hd(user) $env(USER)
    }
    if {![info exists hd(method)]} {
	set hd(method) tcp_default
	set hd(ssh_cmd) $option(ssh_template)
	set hd(priv) tls
	set hd(ssl) 0
	set hd(notls) 0
    }
    
    set hd(title) $t(define_pop_folder)

    if {"wizard" == $mode} {
	VFolderWizardReset $id
	lappend hd(history) VFolderWizardPop

	rat_flowmsg::create $hd(body).message -text "$t(why_pop)"
	grid $hd(body).message - -pady 10

	label $hd(body).name_l -text $t(name): -anchor e
	entry $hd(body).name_e -textvariable ${id}(name)
	grid $hd(body).name_l $hd(body).name_e - -sticky we

	frame $hd(body).space -height 20
	grid $hd(body).space
    }

    label $hd(body).host_l -text $t(host): -anchor e
    entry $hd(body).host_e -textvariable ${id}(host)
    grid $hd(body).host_l $hd(body).host_e -sticky ew
    
    label $hd(body).user_l -text $t(user): -anchor e
    entry $hd(body).user_e -textvariable ${id}(user)
    grid $hd(body).user_l $hd(body).user_e -sticky ew

    frame $hd(body).msp2 -height 10
    grid $hd(body).msp2
    
    label $hd(body).conn_l -text $t(connect): -anchor e
    radiobutton $hd(body).conn_tcpdef -text $t(tcp_default) \
	-variable ${id}(method) -value tcp_default -anchor w \
	-command "VFolderWizardServerCheck $id"
    frame $hd(body).conn_tcpcust
    radiobutton $hd(body).conn_tcpcust.b -text $t(tcp_custom): \
	-variable ${id}(method) -value tcp_custom \
	-command "VFolderWizardServerCheck $id"
    entry $hd(body).conn_tcpcust.e -width 6 -textvariable ${id}(port)
    pack $hd(body).conn_tcpcust.b $hd(body).conn_tcpcust.e -side left
    radiobutton $hd(body).conn_rsh -text $t(rsh_ssh) -variable ${id}(method) \
	-value rsh -command "VFolderWizardServerCheck $id"
    grid $hd(body).conn_l $hd(body).conn_tcpdef - -sticky ew
    grid x $hd(body).conn_tcpcust -sticky w
    grid x $hd(body).conn_rsh -sticky w

    label $hd(body).ssh_l -text $t(ssh_command): -anchor e
    entry $hd(body).ssh_e -width 40 -textvariable ${id}(ssh_cmd)
    grid $hd(body).ssh_l $hd(body).ssh_e -sticky ew

    frame $hd(body).msp3 -height 10
    grid $hd(body).msp3

    label $hd(body).priv_l -text $t(privacy): -anchor e
    radiobutton $hd(body).priv_ssl -text $t(use_ssl) -anchor w \
	    -variable ${id}(priv) -value ssl \
	    -command "set ${id}(ssl) 1;set ${id}(notls) 1"
    radiobutton $hd(body).priv_tls -text $t(try_tls) \
	    -variable ${id}(priv) -value tls \
	    -command "set ${id}(ssl) 0;set ${id}(notls) 0"
    radiobutton $hd(body).priv_none -text $t(no_encryption) \
	    -variable ${id}(priv) -value none \
	    -command "set ${id}(ssl) 0;set ${id}(notls) 1"
    grid $hd(body).priv_l $hd(body).priv_ssl -sticky ew
    grid x $hd(body).priv_tls -sticky w
    grid x $hd(body).priv_none -sticky w

    frame $hd(body).msp4 -height 10
    grid $hd(body).msp4

    label $hd(body).flags_l -text $t(flags): -anchor e
    checkbutton $hd(body).flag_checkc -text $t(ssl_check_cert) \
	-variable ${id}(novalidate-cert) -onvalue 0 -offvalue 1 -anchor w
    grid $hd(body).flags_l $hd(body).flag_checkc -sticky ew

    if {"wizard" == $mode} {
	rat_flowmsg::create $hd(body).conn -text "$t(check_on_finish)"
	grid $hd(body).conn - -pady 10
	
	$hd(next) configure -state disabled -text $t(wiz_finish) \
	    -command "VFolderWizardPOPDone $id"
	rat_scrollframe::recalc $hd(bodym)
	focus $hd(body).name_e
        SetupShortcuts [list $hd(next) $hd(prev)]
        $hd(prev) configure -state normal
    } else {
	focus $hd(body).host_e
    }

    foreach w [list $hd(body).host_e $hd(body).user_e $hd(body).ssh_e \
                  $hd(body).conn_tcpcust.e] {
	bind $w <FocusOut> "VFolderWizardServerCheck $id"
	bind $w <Return> "VFolderWizardServerCheck $id"
    }
    VFolderWizardServerCheck $id
}

proc VFolderWizardPOPDone {id} {
    upvar \#0 $id hd
    global t option mailServer

    # Find a free pop-server-index
    set popi 0
    foreach ms [array names mailServer] {
	if { -1 != [lsearch -exact [lindex $mailServer($ms) 2] pop3]
	     && [string is integer $ms] && $ms > $popi} {
	    set popi $ms
	}
    }
    incr popi

    # Create folder definition
    set def [list $hd(name) pop3 {} $popi]

    # Create server definition
    set mailServer($popi) [VFolderWizardBuildNewServer $id pop3]
    VFolderAddMailServers

    RatBusy {set fail [catch {RatCheckFolder $def} i]}
    if {$fail} {
	Popup "$t(failed_to_open_mailbox): $i" $hd(top)
	unset mailServer($popi)
	return
    }

    set hd(folder_id) [VFolderAddFolder $hd(context) $def]
    $hd(cancel) invoke
}

###############################################################################
# Dynamic folders

proc VFolderWizardDynamic {id} {    
    upvar \#0 $id hd
    global t

    VFolderWizardReset $id
    grid columnconfigure $hd(body) 1 -weight 1
    lappend hd(history) VFolderWizardDynamic

    set hd(title) $t(define_dynamic_folder)
    rat_flowmsg::create $hd(body).message -text "$t(why_dynamic_folder)"
    grid $hd(body).message - -pady 10

    label $hd(body).name_l -text $t(name): -anchor e
    entry $hd(body).name_e -textvariable ${id}(name)
    grid $hd(body).name_l $hd(body).name_e -sticky we

    frame $hd(body).space -height 20
    grid $hd(body).space

    if {![info exists hd(path)]} {
	set hd(path) "[pwd]/"
    }
    label $hd(body).dir_l -text $t(path): -anchor e
    entry $hd(body).dir_e -textvariable ${id}(path)
    button $hd(body).browse -text $t(browse)... \
	-command "Browse $hd(body) ${id}(path) dirok"
    grid $hd(body).dir_l $hd(body).dir_e -sticky we
    grid x $hd(body).browse -sticky e

    $hd(next) configure -text $t(finish) \
	-command "VFolderWizardDynamicDone $id"

    rat_scrollframe::recalc $hd(bodym)
    SetupShortcuts [list $hd(next) $hd(prev) $hd(body).browse]
    $hd(body).name_e selection range 0 end
    $hd(body).name_e icursor end
    focus $hd(body).name_e
    $hd(prev) configure -state normal
}

proc VFolderWizardDynamicDone {id} {
    upvar \#0 $id hd
    global t
    
    # Create directory if nonexisting was specified in create mode
    if {![file exists $hd(path)]} {
	if {[catch {file mkdir $hd(path)} err]} {
	    Popup "$t(failed_create) '$hd(path)': $err"
	    return
	}
    }

    # Make sure we have a directory name
    if {![file isdirectory $hd(path)]} {
	set hd(path) [file dirname $hd(path)]
    }

    set def [list $hd(name) dynamic {} $hd(path)]

    set hd(folder_id) [VFolderAddFolder $hd(context) $def]
    $hd(cancel) invoke
}

