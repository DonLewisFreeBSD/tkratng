# firstwizard.tcl -
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.
#
#
# Functions which implements the first use wizard

proc FirstUseWizard {} {
    global t propBigFont idCnt env

    set id firstusewizard[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(history) {}
    set hd(top) $w

    # Create toplevel and insert infrastructure
    toplevel $w -class TkRat
    wm title $w $t(first_use_wizard)
    label $w.title -textvariable ${id}(title) -font $propBigFont
    set hd(scrollbody) $w.body
    set hd(body) [rat_scrollframe::create $w.body -bd 10]
    frame $w.buttons
    button $w.buttons.prev -text "< $t(wiz_previous)" -state disabled \
	-command "set cmd \[lindex \$${id}(history) end-1\]; \
                  set ${id}(history) \[lreplace \$${id}(history) end-1 end\]; \
                  \$cmd $id"
    button $w.buttons.next -text "$t(wiz_next) >" -state disabled 
    button $w.buttons.cancel -text $t(cancel) -command "destroy $w"
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

    grid propagate $w.body 0
    grid columnconfigure $w.body 0 -weight 1
    grid rowconfigure $hd(body) 20 -weight 1
    bind $w.body <Destroy> "if {\"%W\" == \"$w.body\"} {FirstUseWizardClose $id}"
    ::tkrat::winctl::SetGeometry firstUseWizard $w $w.body

    # Initialize values
    set hd(fullname) "$env(GECOS)"
    if {[string match *.* [info hostname]]} {
	set guess [join [lrange [split [info hostname] .] 1 end] .]
    } else {
	set guess [info hostname]
    }
    set hd(email_address) "$env(USER)@$guess"
    set hd(sendprot) smtp
    set hd(smtp_hosts) localhost
    set hd(imap_host) localhost
    set hd(imap_user) $env(USER)
    set hd(pop3_host) localhost
    set hd(pop3_user) $env(USER)
    if {[catch {exec which sendmail} hd(sendprog)]} {
	if {[file executable /usr/lib/sendmail]} {
	    set hd(sendprog) /usr/lib/sendmail
	}
    }
    set hd(inproto) file
    set hd(filename) $env(MAIL)

    FirstUseWizardIdent $id

    tkwait window $hd(top)
}

proc FirstUseWizardClose {id} {
    upvar \#0 $id hd

    ::tkrat::winctl::RecordGeometry $hd(top) $hd(top).body
    unset hd
}

proc FirstUseWizardReset {id} {
    upvar #0 $id hd
    global t

    eval destroy [winfo children $hd(body)]
    grid columnconfigure $hd(body) 0 -weight 0
    grid columnconfigure $hd(body) 1 -weight 0
    $hd(next) configure -text "$t(wiz_next) >"
    # Work around bug in grid (reported and fixed 200210??)
    grid size $hd(body)
}

proc FirstUseWizardIdent {id} {
    upvar #0 $id hd
    global t

    # Setup the first step
    FirstUseWizardReset $id
    lappend hd(history) FirstUseWizardIdent
    set hd(title) $t(identity)

    rat_flowmsg::create $hd(body).message1 -text "$t(firstuse_info)"
    grid $hd(body).message1 - -sticky w -pady 10

    rat_flowmsg::create $hd(body).message2 -text "$t(why_identity)"
    grid $hd(body).message2 - -sticky w

    frame $hd(body).space1 -height 20
    grid $hd(body).space1

    label $hd(body).name_l -text $t(fullname): -anchor e
    entry $hd(body).name_e -textvariable ${id}(fullname)
    grid $hd(body).name_l $hd(body).name_e -sticky we

    frame $hd(body).space2 -height 20
    grid $hd(body).space2

    label $hd(body).email_l -text $t(email_address): -anchor e
    entry $hd(body).email_e -textvariable ${id}(email_address)
    grid $hd(body).email_l $hd(body).email_e -sticky we

    $hd(prev) configure -state disabled
    $hd(next) configure -command "FirstUseWizardSend $id" -state normal
    grid columnconfigure $hd(body) 1 -weight 1
    SetupShortcuts [list $hd(next) $hd(prev)]

    focus $hd(body).email_e
    $hd(body).email_e selection range 0 end
    rat_scrollframe::recalc $hd(scrollbody)
}

proc FirstUseWizardSend {id} {
    upvar #0 $id hd
    global t

    # Setup the first step
    FirstUseWizardReset $id
    lappend hd(history) FirstUseWizardSend
    set hd(title) $t(sending)

    set a [list $hd(next) $hd(prev)]

    rat_flowmsg::create $hd(body).message -text "$t(why_sending)"
    grid $hd(body).message - -sticky w

    frame $hd(body).space1 -height 20
    grid $hd(body).space1

    radiobutton $hd(body).smtp -text $t(use_mail_server) \
	-variable ${id}(sendprot) -value smtp \
	-command "FirstUseWizardSendSetup $id"
    grid $hd(body).smtp - -sticky w
    lappend a $hd(body).smtp

    label $hd(body).smtpserver_l -text $t(smtp_hosts): -anchor e
    entry $hd(body).smtpserver_e -textvariable ${id}(smtp_hosts)
    grid $hd(body).smtpserver_l $hd(body).smtpserver_e -sticky we

    frame $hd(body).space2 -height 20
    grid $hd(body).space2

    radiobutton $hd(body).prog -text $t(use_prog) \
	-variable ${id}(sendprot) -value prog \
	-command "FirstUseWizardSendSetup $id"
    grid $hd(body).prog - -sticky w
    lappend a $hd(body).prog

    label $hd(body).prog_l -text $t(sendprog): -anchor e
    entry $hd(body).prog_e -textvariable ${id}(sendprog)
    grid $hd(body).prog_l $hd(body).prog_e -sticky we

    button $hd(body).file_browse -text $t(browse)... \
	-command "Browse $hd(top) ${id}(sendprog) any"
    grid x $hd(body).file_browse -sticky e

    $hd(prev) configure -state normal
    $hd(next) configure -command "FirstUseWizardInbox $id" -state normal
    grid columnconfigure $hd(body) 1 -weight 1
    SetupShortcuts $a
    FirstUseWizardSendSetup $id
    rat_scrollframe::recalc $hd(scrollbody)
}

proc FirstUseWizardSendSetup {id} {
    upvar #0 $id hd

    switch $hd(sendprot) {
	"smtp" {set s 1; set p 0; set fw $hd(body).smtpserver_e}
	"prog" {set s 0; set p 1; set fw $hd(body).prog_e}
    }
    rat_ed::enabledisable $s [list $hd(body).smtpserver_l \
				  $hd(body).smtpserver_e]
    rat_ed::enabledisable $p [list $hd(body).prog_l $hd(body).prog_e]
    if {[focus] != $fw} {
	focus $fw
	$fw selection range 0 end
    }
}

proc FirstUseWizardInbox {id} {
    upvar #0 $id hd
    global t

    # Setup the first step
    FirstUseWizardReset $id
    lappend hd(history) FirstUseWizardInbox
    set hd(title) $t(incom_mbox)

    set a [list $hd(next) $hd(prev)]

    rat_flowmsg::create $hd(body).message -text "$t(why_inbox)"
    grid $hd(body).message - -sticky w

    frame $hd(body).space1 -height 10
    grid $hd(body).space1

    radiobutton $hd(body).file -text $t(file) \
	-variable ${id}(inproto) -value file \
	-command "FirstUseWizardInboxSetup $id"
    grid $hd(body).file - -sticky w
    lappend a $hd(body).file

    label $hd(body).file_l -text $t(pathname): -anchor e
    entry $hd(body).file_e -textvariable ${id}(filename)
    grid $hd(body).file_l $hd(body).file_e -sticky we

    button $hd(body).file_browse -text $t(browse)... \
	-command "Browse $hd(top) ${id}(filename) any"
    grid x $hd(body).file_browse -sticky e

    radiobutton $hd(body).imap -text $t(imap) \
	-variable ${id}(inproto) -value imap \
	-command "FirstUseWizardInboxSetup $id"
    grid $hd(body).imap - -sticky w
    lappend a $hd(body).imap

    label $hd(body).imap_l -text $t(host): -anchor e
    entry $hd(body).imap_e -textvariable ${id}(imap_host)
    grid $hd(body).imap_l $hd(body).imap_e -sticky we

    label $hd(body).iuser_l -text $t(user): -anchor e
    entry $hd(body).iuser_e -textvariable ${id}(imap_user)
    grid $hd(body).iuser_l $hd(body).iuser_e -sticky we

    button $hd(body).imap_adv -text $t(advanced_imap_conf)... \
	-command "FirstUseWizardInboxAdv $id imap"
    grid x $hd(body).imap_adv -sticky e

    radiobutton $hd(body).pop -text $t(pop3) \
	-variable ${id}(inproto) -value pop3 \
	-command "FirstUseWizardInboxSetup $id"
    grid $hd(body).pop - -sticky w
    lappend a $hd(body).pop

    label $hd(body).pop_l -text $t(host): -anchor e
    entry $hd(body).pop_e -textvariable ${id}(pop3_host)
    grid $hd(body).pop_l $hd(body).pop_e -sticky we

    label $hd(body).puser_l -text $t(user): -anchor e
    entry $hd(body).puser_e -textvariable ${id}(pop3_user)
    grid $hd(body).puser_l $hd(body).puser_e -sticky we

    button $hd(body).pop_adv -text $t(advanced_pop_conf)... \
	-command "FirstUseWizardInboxAdv $id pop3"
    grid x $hd(body).pop_adv -sticky e

    $hd(prev) configure -state normal
    $hd(next) configure -command "FirstUseWizardImport $id" -state normal
    grid columnconfigure $hd(body) 1 -weight 1
    SetupShortcuts $a
    FirstUseWizardInboxSetup $id
    rat_scrollframe::recalc $hd(scrollbody)
}

proc FirstUseWizardInboxSetup {id} {
    upvar #0 $id hd

    switch $hd(inproto) {
	"imap" {set i 1; set p 0; set f 0; set fw $hd(body).imap_e}
	"pop3" {set i 0; set p 1; set f 0; set fw $hd(body).pop_e}
	"file" {set i 0; set p 0; set f 1; set fw $hd(body).file_e}
    }
    rat_ed::enabledisable $i [list $hd(body).imap_l $hd(body).imap_e \
				  $hd(body).iuser_l $hd(body).iuser_e \
				  $hd(body).imap_adv]
    rat_ed::enabledisable $p [list $hd(body).pop_l $hd(body).pop_e \
				  $hd(body).puser_l $hd(body).puser_e \
				  $hd(body).pop_adv]
    rat_ed::enabledisable $f [list $hd(body).file_l $hd(body).file_e \
				  $hd(body).file_browse]
    if {[focus] != $fw} {
	focus $fw
	$fw selection range 0 end
    }
}

proc FirstUseWizardInboxAdv {id prot} {
    upvar #0 $id hd
    global idCnt t propBigFont

    set id2 imapadv[incr idCnt]
    set w .$id2
    upvar #0 $id2 hd2
    set hd2(host) $hd(${prot}_host)
    set hd2(name) Dummy

    toplevel $w -class TkRat
    wm title $w $t(first_use_wizard)
    label $w.title -textvariable ${id}(title) -font $propBigFont
    
    set hd2(body) [frame $w.body -bd 10]
    frame $w.buttons
    button $w.buttons.done -text "$t(done)" -state disabled \
	-command "set ${id}(action) done; destroy $w"
    button $w.buttons.cancel -text "$t(cancel)" -state disabled \
	-command "set ${id}(action) cancel; destroy $w"
    set hd2(next) $w.buttons.done
    pack $w.buttons.done -pady 10
    pack $w.title -side top -fill x
    pack $w.buttons -side bottom -fill x
    pack $w.body -fill both -expand 1

    bind $w <Escape> "destroy $w"
    wm protocol $w WM_DELETE_WINDOW "destroy $w"

    if {"imap" == $prot} {
	set hd2(user) $hd(imap_user)
	VFolderWizardIMAPServer $id2 standalone
    } else {
	set hd2(user) $hd(pop3_user)
	VFolderWizardPOP $id2 standalone
    }

    ::tkrat::winctl::SetGeometry firstUseAdv $w $w

    tkwait window $w

    if {"done" == $hd(action)} {
	set hd(mailServer) [VFolderWizardBuildNewServer $id2 $prot]
    }
    unset hd2
}

proc FirstUseWizardImport {id} {
    upvar #0 $id hd
    global t

    FirstUseWizardReset $id
    lappend hd(history) FirstUseWizardImport
    set hd(title) $t(import_info)

    rat_flowmsg::create $hd(body).message -text "$t(import_info_text)"
    grid $hd(body).message - -sticky w -pady 10

    $hd(prev) configure -state normal
    $hd(next) configure -command "FirstUseWizardDone $id" -text $t(finish) \
	-state normal
    grid columnconfigure $hd(body) 1 -weight 1
    SetupShortcuts [list $hd(next) $hd(prev)]

    rat_scrollframe::recalc $hd(scrollbody)
}

proc FirstUseWizardDone {id} {
    upvar #0 $id hd
    global option mailServer vFolderDef vFolderInbox t

    set r $option(default_role)
    set option($r,from) "$hd(fullname) <$hd(email_address)>"
    set option($r,sendprot) $hd(sendprot)
    set option($r,sendprog) $hd(sendprog)
    set option($r,smtp_hosts) $hd(smtp_hosts)

    # Setup incoming mailbox
    if {"pop3" == $hd(inproto) || "imap" == $hd(inproto)} {
	if {"pop3" == [lindex $vFolderDef($vFolderInbox) 1]
	    || "imap" == [lindex $vFolderDef($vFolderInbox) 1]} {
	    set si [lindex $vFolderDef($vFolderInbox) 3]
	} else {
	    if {"imap" == $hd(inproto)} {
		set si $hd(imap_host)
		set i 1		
		while {[info exists mailServer($si)]} {
		    set si "$hd(imap_host)-[incr i]"
		}
	    } else {
		set si 0
		foreach ms [array names mailServer] {
		    if { -1 != [lsearch -exact \
				    [lindex $mailServer($ms) 2] pop3]
			 && [string is integer $ms] && $ms > $si} {
			set si $ms
		    }
		}
		incr si
	    }
	}
	if {![info exists hd(mailServer)]} {
	    if {"pop3" == $hd(inproto)} {
		set hd(mailServer) \
		    [list $hd(pop3_host) 110 {pop3} $hd(pop3_user)]
	    } else {
		set hd(mailServer) [list $hd(imap_host) 143 {} $hd(imap_user)]
	    }
	}
	set mailServer($si) $hd(mailServer)
    }
    switch $hd(inproto) {
	"pop3" {
	    set def [list $t(inbox) pop3 {} $si]
	}
	"imap" {
	    set def [list $t(inbox) imap {} $si INBOX]
	}
	"file" {
	    set def [list $t(inbox) file {} $hd(filename)]
	}
    }
    set vFolderDef($vFolderInbox) $def

    SaveOptions
    VFolderWrite

    destroy $hd(top)
}
