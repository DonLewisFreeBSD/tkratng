# compose.tcl --
#
# This file contains the code which handles the composing of messages
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# List of compsoe windows
set composeWindowList {}

# List of header names we know of
set composeHeaderList {to subject cc bcc reply_to from}

# List of headers which contains addresses (OBS! must be lower case OBS!)
set composeAdrHdrList {from to cc bcc reply_to}

# Address book icon
set book_img [image create photo -data {
R0lGODdhEgAMAKUAANDU0KisqGBkYHB0cPj4+ICEgIiIiHh8eNDQ0MjMyJCQkIiMiLi4uLC0
sPj8+GBgYOjs6KCkoDg4OHBwcJiYmJCUkMjIyMDEwICAgKioqHh4eFBQUEhMSODg4PD08Jic
mKCgoLCwsNjY2FhcWDAwMP//////////////////////////////////////////////////
/////////////////////////////////////////////////////////ywAAAAAEgAMAAAG
f0CH0FHRjDQQh+bxAA2HA8kTQHqSpENBwUHqOjBcEuRgeA4CnqFnMewUMmaQp0vylLudwyee
FtrbenEEaltCeXtDEwwBGRUBHSEfChUUByFPBQYRFwgIBxsJAAgRAgpPFFUeHR2oIggdDgpY
QhwgIBkNFhEUCQkNFBSmT8PET0EAOw==}]

# Compose --
#
# Initialize the composing of a new letter
#
# Arguments:
# role - The role to do the composing as

proc Compose {role} {
    global idCnt
    set handler composeM[incr idCnt]

    return [DoCompose $handler $role 0 1]
}


# ComposeReply --
#
# Build a reply to the given message and let the user add his text
#
# Arguments:
# msg -		Message to reply to
# to -		Whom the reply should be sent to, 'sender' or 'all'.
# role -        The role to do the composing as

proc ComposeReply {msg to role} {
    global t option
    set msg [ComposeChoose $msg $t(reply_to_which)]
    if {![string length $msg]} {
	return 0
    }
    set handler [$msg reply $to]
    return [DoCompose $handler $role \
		[expr {($option(reply_bottom)) ? "1" : "-1"}] \
		[expr {($option(append_sig)) ? "1" : "0"}]]
}


# ComposeForwardInline --
#
# Forward a message and keep the first part inline if it is a text part.
#
# Arguments:
# msg -		Message to forward
# role -        The role to do the composing as

proc ComposeForwardInline {msg role} {
    global idCnt option t
    set handler composeM[incr idCnt]
    upvar #0 $handler mh

    set msg [ComposeChoose $msg $t(forward_which)]
    if {![string length $msg]} {
	return 0
    }

    foreach header [$msg headers] {
	set name [string tolower [lindex $header 0]]
	switch $name {
	subject		{ set mh(subject) "Fwd: [lindex $header 1]" }
	description	{ set hd(description) [lindex $header 1] }
	}
	if { -1 != [lsearch [string tolower $option(show_header_selection)] \
		$name]} {
	    regsub -all -- - $name _ name
	    set inline_header($name) [lindex $header 1]
	}
    }

    # Find if there is a bodypart which we can inline (a text part)
    set body [$msg body]
    set type [string tolower [$body type]]
    set inline {}
    set attach $body
    if { (![string compare text [lindex $type 0]]
	    && ![string compare plain [lindex $type 1]])
	|| (![string compare message [lindex $type 0]]
		&& ![string compare rfc822 [lindex $type 1]])} {
	set inline $body
	set attach {}
    } elseif {![string compare multipart [lindex $type 0]]} {
	set children [$body children]
	if {0 < [llength $children]} {
	    set type [string tolower [[lindex $children 0] type]]
	    if {![string compare text [lindex $type 0]]
		    && ![string compare plain [lindex $type 1]]} {
		set inline [lindex $children 0]
		set attach [lrange $children 1 end]
	    } else {
		set inline {}
		set attach $children
	    }
	}
    }

    # Now we are ready to start constructing the new message
    set bhandler composeM[incr idCnt]
    upvar #0 $bhandler bh
    set mh(body) $bhandler
    set bh(type) text
    set bh(subtype) plain
    if {[string length $inline]} {
	set bh(encoding) [$inline encoding]
	set bh(parameter) [$inline params]
	set bh(id) [$inline id]
	set bh(description) [$inline description]
	set preface "\n\n$option(forwarded_message)\n"
	set length 5
	foreach f $option(show_header_selection) {
	    if { $length < [string length $f]} {
		set length [string length $f]
	    }
	}
	foreach field $option(show_header_selection) {
	    regsub -all -- - [string tolower $field] _ f
	    if {[info exists inline_header($f)]} {
		if {[info exists t($f)]} {
		    set name $t($f)
		} else {
		    set name $field
		}
		set preface [format "%s%${length}s: %s\n" $preface $name \
			$inline_header($f)]
	    }
	}
	set mh(data) "${preface}\n[$inline data 0]"
	set mh(data_tags) "Cited noWrap no_spell"
    }
    foreach child $attach {
	set chandler composeC[incr idCnt]
	upvar #0 $chandler ch
	lappend bh(children) $chandler

	set type [string tolower [$child type]]
	set ch(type) [lindex $type 0]
	set ch(subtype) [lindex $type 1]
	set ch(encoding) [$child encoding]
	set ch(parameter) [$child params]
	set ch(disp_type) [$child disp_type]
	set ch(disp_parm) [$child disp_parm]
	set ch(id) [$child id]
	set ch(description) [$child description]
	if {![info exists mh(data)] && ![string compare text $ch(type)]} {
	    set mh(data) [$child data 0]
	    set mh(data_tags) "Cited noWrap no_spell"
	} else {
	    set ch(filename) [RatTildeSubst $option(send_cache)/rat.[RatGenId]]
	    set fh [open $ch(filename) w 0600]
	    $child saveData $fh 1 0
	    close $fh
	    set ch(removeFile) 1
	    lappend mh(attachmentList) $chandler
	}
    }

    return [DoCompose $handler $role 0 0]
}


# ComposeForwardAttachment --
#
# Forward the given message. The given message is included as an
# attachment of type message/rfc822
#
# Arguments:
# msg -		Message to reply to
# role -        The role to do the composing as

proc ComposeForwardAttachment {msg role} {
    global option t idCnt

    set msg [ComposeChoose $msg $t(forward_which)]
    if {![string length $msg]} {
	return 0
    }

    set handler composeM[incr idCnt]
    upvar #0 $handler mh

    #
    # Attach old message
    #
    set id compose[incr idCnt]
    upvar #0 $id hd

    set hd(description) $t(forwarded_message)
    foreach header [$msg headers] {
	switch [string tolower [lindex $header 0]] {
	subject		{ set mh(subject) "Fwd: [lindex $header 1]" }
	description	{ set hd(description) [lindex $header 1] }
	}
    }
    set hd(filename) [RatTildeSubst $option(send_cache)/rat.[RatGenId]]
    set fh [open $hd(filename) w 0600]
    fconfigure $fh -translation binary 
    regsub -all "\r\n" [$msg rawText] "\n" raw
    puts -nonewline $fh $raw
    close $fh
    set hd(type) message
    set hd(subtype) rfc822
    set hd(removeFile) 1
    set mh(attachmentList) $id

    return [DoCompose $handler $role 0 1]
}

# ComposeBounce --
#
# Bounce a message, i.e. remail it
#
# Arguments:
# msg -		Message to bounce
# role -        The role to do the composing as

proc ComposeBounce {msg role} {
    global idCnt option t
    set handler composeM[incr idCnt]
    upvar #0 $handler mh

    set msg [ComposeChoose $msg $t(forward_which)]
    if {![string length $msg]} {
	return 0
    }

    foreach header [$msg headers] {
	set name [string tolower [lindex $header 0]]
	set mh($name) [lindex $header 1]
    }

    # Find if there is a bodypart which we can inline (a text part)
    set body [$msg body]
    set type [string tolower [$body type]]
    set inline {}
    set attach $body
    if { (![string compare text [lindex $type 0]]
	    && ![string compare plain [lindex $type 1]])
	|| (![string compare message [lindex $type 0]]
		&& ![string compare rfc822 [lindex $type 1]])} {
	set inline $body
	set attach {}
    } elseif {![string compare multipart [lindex $type 0]]} {
	set children [$body children]
	if {0 < [llength $children]} {
	    set type [string tolower [[lindex $children 0] type]]
	    if {![string compare text [lindex $type 0]]
		    && ![string compare plain [lindex $type 1]]} {
		set inline [lindex $children 0]
		set attach [lrange $children 1 end]
	    } else {
		set inline {}
		set attach $children
	    }
	}
    }

    # Now we are ready to start constructing the new message
    set bhandler composeM[incr idCnt]
    upvar #0 $bhandler bh
    set mh(body) $bhandler
    set body [$msg body]
    set type [string tolower [$body type]]
    if {"multipart" == [lindex [string tolower [$body type]] 0]
	    && 0 < [llength [$body children]]} {
	set mh(data) [[lindex [$body children] 0] data 0]
        set mh(data_tags) "Cited noWrap no_spell"
	foreach child [lrange [$body children] 1 end] {
	    set chandler composeC[incr idCnt]
	    upvar #0 $chandler ch
	    lappend bh(children) $chandler

	    set type [string tolower [$child type]]
	    set ch(type) [lindex $type 0]
	    set ch(subtype) [lindex $type 1]
	    set ch(encoding) [$child encoding]
	    set ch(parameter) [$child params]
	    set ch(disp_type) [$child disp_type]
	    set ch(disp_parm) [$child disp_parm]
	    set ch(id) [$child id]
	    set ch(description) [$child description]
	    if {![info exists mh(data)] && ![string compare text $ch(type)]} {
		set mh(data) [$child data 0]
		set mh(data_tags) "Cited noWrap no_spell"
	    } else {
		set ch(filename) \
			[RatTildeSubst $option(send_cache)/rat.[RatGenId]]
		set fh [open $ch(filename) w 0600]
		$child saveData $fh 1 0
		close $fh
		set ch(removeFile) 1
		lappend mh(attachmentList) $chandler
	    }
	}
    } else {
	set mh(data) [$body data 0]
	set mh(data_tags) "Cited noWrap no_spell"
    }

    return [DoCompose $handler $role 0 0]
}


# ComposeHeld --
#
# Choose a message in the hold and continue compose it.
#
# Arguments:

proc ComposeHeld {} {
    global idCnt t b

    # First see if there are any held messages
    if {![llength [set content [RatHold list]]]} {
	Popup $t(no_held)
	return 0
    }

    # Create identifier
    set id gh[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(done) 0

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(getheld)

    # Populate window
    frame $w.list
    scrollbar $w.list.scroll -relief sunken -command "$w.list.list yview" \
	    -highlightthickness 0
    listbox $w.list.list -yscroll "$w.list.scroll set" -height 10 \
	    -width 80 -relief sunken -selectmode browse \
	    -exportselection false -highlightthickness 0
    set b($w.list.list) compose_held_pick
    bind $w.list.list <Double-1> "set ${id}(done) 1"
    pack $w.list.scroll -side right -fill y
    pack $w.list.list -side left -expand 1 -fill both
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"
    pack $w.buttons -side bottom -pady 5 -fill x
    pack $w.list -expand 1 -fill both -padx 5 -pady 5

    # Populate list
    foreach m $content {
	$w.list.list insert end $m
    }
    
    Place $w composeHeld
    ModalGrab $w
    tkwait variable ${id}(done)
    set index [$w.list.list curselection]
    RecordPos $w composeHeld
    destroy $w

    if {1 == $hd(done)} {
	if {[string length $index]} {
	    return [ComposeExtracted [RatHold extract $index]]
	} else {
	    Popup $t(must_select)
	}
    }
    unset b($w.list.list)
    unset hd
    return 0
}

# ComposeExtracted --
#
# Starts composing an extracted message
#
# Arguments:
# mgh - handler of the extracted message

proc ComposeExtracted {mgh} {
    global charsetMapping option

    upvar #0 $mgh mh
    if {[info exists mh(body)]} {
	upvar #0 $mh(body) bh
	if {![string compare "$bh(type)/$bh(subtype)" text/plain]} {
	    set edit $mh(body)
	    set children {}
	} elseif {![string compare "$bh(type)" multipart]} {
	    set children $bh(children)
	    upvar #0 [lindex $children 0] ch1
	    if {![string compare "$ch1(type)/$ch1(subtype)" text/plain]} {
		set edit [lindex $children 0]
		set children [lreplace $children 0 0]
	    } else {
		set edit {}
	    }
	} else {
	    set edit {}
	    set children $mh(body)
	}
	if {[info exists bh(pgp_sign)]} {
	    set mh(pgp_sign) $bh(pgp_sign)
	    set mh(pgp_encrypt) $bh(pgp_encrypt)
	}
	if {[string length $edit]} {
	    upvar #0 $edit bp
	    set fh [open $bp(filename) r]
	    if {[info exists bp(parameter)]} {
		set params $bp(parameter)
		lappend params {a a}
		array get p $params
	    }
	    if {[info exists p(charset)]} {
		set charset $p(charset)
	    } else {
		if {[info exists mh(charset)]} {
		    set charset $mh(charset)
		} else {
		    set charset auto
		}
	    }
	    if {"auto" == $charset} {
		set charset utf-8
	    } 
	    fconfigure $fh -encoding $charsetMapping($charset)
	    set mh(data) [read $fh]
	    set mh(data_tags) {}
	    close $fh
	    if {$bp(removeFile)} {
		catch "file delete -force -- $bp(filename)"
	    }
	}
	set mh(attachmentList) $children
    }
    return [DoCompose $mgh $mh(role) 0 0]
}

# ComposeClient --
#
# Executes the compose command from the client
#
# Arguments:
# hl	- List of presupplied header values

proc ComposeClient {hl} {
    global idCnt option

    set handler clientM[incr idCnt]
    upvar #0 $handler mh
    if {[llength $hl]} {
	array set mh $hl
    }
    return [DoCompose $handler $option(default_role) 0 1]
}

# DoCompose --
#
# Actually do the composition. This involves building a window in which
# the user may do a lot of things.
#
# Arguments:
# handler   -	The handler for the active compose session
# edit_text -	'1' if we should place the cursor in the text field.
#               '-1' if we should place the cursor at the top of the text field
# add_sig   -   '1' if we should add the signature
# role -        The role to do the composing as

proc DoCompose {handler role edit_text add_sig} {
    global option t b composeHeaderList composeWindowList defaultFontWidth \
	   tk_strictMotif env charsetName editors
    upvar #0 $handler mh

    # Initialize variables
    if {![info exists editors]} {
	EditorsRead
    }
    foreach i $composeHeaderList {
	set mh(O_$i) 0
    }
    regsub -all -- - [string tolower $option(compose_headers)] _ vars
    foreach i $vars {
	set mh(O_$i) 1
    }
    foreach adr {to cc bcc} {
	if {![info exists mh($adr)]} {
	    set mh($adr) {}
	}
    }
    set mh(role) $role
    set mh(redo) 0
    set mh(save_to) ""
    set mh(doWrap) 1
    set mh(doIndent) 1
    set mh(closing) 0
    set mh(eeditor) $option(eeditor)
    set mh(mark_nowrap) 0
    set mh(role_sig) 0
    if {![info exists mh(charset)]} {
	set mh(charset) auto
    }

    set mh(copy_attached) $option(copy_attached)
    if {![info exists mh(pgp_sign)]} {
	set mh(pgp_sign) $option(pgp_sign)
	set mh(pgp_encrypt) $option(pgp_encrypt)
    }

    # Create window
    set w .$handler
    set mh(toplevel) $w
    toplevel $w -class TkRat
    wm iconname $w $t(compose_name)

    # Menus
    FindAccelerators a {file edit role headers extra admin}

    frame $w.menu -relief raised -bd 1
    set m $w.menu.file.m
    menubutton $w.menu.file -menu $m -text $t(file) -underline $a(file)
    menu $m -tearoff 1
    $m add command -label $t(insert_file)... \
	    -command "ComposeInsertFile $handler"
    set b($m,[$m index end]) compose_insert_file
    $m add separator
    $m add checkbutton -label $t(automatic_wrap) -variable ${handler}(doWrap) \
	    -command "rat_edit::setWrap $w.body.text \$${handler}(doWrap)"
    set b($m,[$m index end]) automatic_wrap
    $m add separator
    $m add command -label $t(abort) \
	    -command "ComposeBuildStruct $handler abort;\
	              DoCompose2 $w $handler abort"
    set b($m,[$m index end]) abort_compose
    set mh(abort_menu) [list $m [$m index end]]
    lappend mh(eEditBlock) $w.menu.file

    set m $w.menu.edit.m
    menubutton $w.menu.edit -menu $m -text $t(edit) -underline $a(edit)
    menu $m -postcommand "ComposePostEdit $handler $m" -tearoff 1
    $m add command -label $t(undo) \
	    -command "event generate $w.body.text <<Undo>>"
    set b($m,[$m index end]) undo
    set mh(undo_menu) [list $m [$m index end]]
    $m add separator
    $m add command -label $t(cut) \
	    -command "event generate $w.body.text <<Cut>>"
    set b($m,[$m index end]) cut
    set mh(cut_menu) [list $m [$m index end]]
    $m add command -label $t(copy) \
	    -command "event generate $w.body.text <<Copy>>"
    set b($m,[$m index end]) copy
    set mh(copy_menu) [list $m [$m index end]]
    $m add command -label $t(paste) \
	    -command "event generate $w.body.text <<Paste>>"
    set b($m,[$m index end]) paste
    set mh(paste_menu) [list $m [$m index end]]
    $m add command -label $t(cut_all) \
	    -command "event generate $w.body.text <<CutAll>>"
    set b($m,[$m index end]) cut_all
    set mh(cut_all_menu) [list $m [$m index end]]
    $m add separator
    $m add command -label $t(wrap_paragraph) \
	    -command "event generate $w.body.text <<Wrap>>"
    set b($m,[$m index end]) wrap_paragraph
    set mh(wrap_menu) [list $m [$m index end]]
    $m add command -label $t(do_wrap_cited) \
	    -command "ComposeWrapCited $handler"
    set b($m,[$m index end]) do_wrap_cited
    $m add checkbutton -label $t(underline_nonwrap) \
	    -variable ${handler}(mark_nowrap) \
	    -command "$w.body.text tag configure noWrap \
				   -underline $${handler}(mark_nowrap)"
    set b($m,[$m index end]) mark_nowrap
    $m add command -label $t(check_spelling) \
	    -command "rat_ispell::CheckTextWidget $w.body.text"
    set b($m,[$m index end]) do_check_spelling
    $m add separator
    $m add command -label $t(run_through_command)... \
	    -command "ComposeSpecifyCmd $handler"
    set b($m,[$m index end]) run_through_command
    set mh(edit_end) [$m index end]

    set m $w.menu.role.m
    menubutton $w.menu.role -menu $m -text $t(role) -underline $a(role)
    menu $m -postcommand \
	    [list PostRoles $handler $m [list UpdateComposeRole $handler]]

    menubutton $w.menu.headers -menu $w.menu.headers.m -text $t(headers) \
			       -underline $a(headers)
    set b($w.menu.headers) headers_menu
    menu $w.menu.headers.m -tearoff 1
    foreach header $composeHeaderList {
	$w.menu.headers.m add checkbutton -label $t($header) \
		-variable ${handler}(O_$header) \
		-onvalue 1 -offvalue 0 \
		-command "ComposeBuildHeaderEntries $handler"
    }

    set m $w.menu.extra.m
    menubutton $w.menu.extra -menu $m -text $t(extra) -underline $a(extra)
    menu $m -tearoff 1
    $m add checkbutton \
	    -label $t(copy_attached_files) \
	    -variable ${handler}(copy_attached) \
	    -onvalue 1 -offvalue 0
    set b($m,[$m index end]) copy_attached_files
    $m add checkbutton \
	    -label $t(request_notification) \
	    -variable ${handler}(request_dsn) \
	    -onvalue 1 -offvalue 0
    set b($m,[$m index end]) request_notification
    if {0 < $option(pgp_version)} {
	$m add checkbutton \
		-label $t(sign) \
		-variable ${handler}(pgp_sign) \
		-onvalue 1 -offvalue 0
	set b($m,[$m index end]) pgp_sign
	$m add checkbutton \
		-label $t(encrypt) \
		-variable ${handler}(pgp_encrypt) \
		-onvalue 1 -offvalue 0
	set b($m,[$m index end]) pgp_encrypt
    }
    $m add cascade -label $t(charset) -menu $m.cm
    set b($m,[$m index end]) use_charset
    menu $m.cm
    $m.cm add radiobutton -label $t(auto) \
	    -variable ${handler}(charset) -value auto
    foreach c $option(charsets) {
	if {0 < [string length $charsetName($c)]} {
	    set name "$c ($charsetName($c))"
	} else {
	    set name $c
	}
	$m.cm add radiobutton -label $name -variable ${handler}(charset) \
		-value $c
    }
    
    set m $w.menu.admin.m
    menubutton $w.menu.admin -menu $m -text $t(admin) \
			     -underline $a(admin)
    menu $m -tearoff 1
    $m add command -label $t(define_keys)... -command {KeyDef compose}
    set b($m,[$m index end]) define_keys
    $m add command -label $t(editors)... -command EditorsList
    set b($m,[$m index end]) editors
    $m add command -label $t(command_list)... -command CmdList
    set b($m,[$m index end]) command_list
    $m add command -label $t(show_generated)... \
	    -command "ShowGeneratedHeaders $handler"
    set b($m,[$m index end]) show_generated_headers

    pack $w.menu.file \
	 $w.menu.edit \
	 $w.menu.role \
	 $w.menu.headers \
	 $w.menu.extra \
	 $w.menu.admin -side left -padx 5

    # Header fields
    set mh(headerFrame) $w.h
    frame $mh(headerFrame)

    # Message body
    frame $w.body
    scrollbar $w.body.scroll -relief sunken -bd 1 -takefocus 0 \
	    -command "$w.body.text yview" -highlightthickness 0
    text $w.body.text -relief sunken -bd 1 -setgrid true \
	    -yscrollcommand "$w.body.scroll set" -wrap none
    set b($w.body.text) compose_body
    Size $w.body.text compose
    pack $w.body.scroll -side right -fill y
    pack $w.body.text -side left -expand yes -fill both
    set mh(composeBody) $w.body.text
    if {[info exists mh(data)]} {
	$w.body.text insert end $mh(data) $mh(data_tags)
	if {[info exists mh(other_tags)]} {
	    foreach ot $mh(other_tags) {
		if {0 < [llength [lindex $ot 1]]} {
		    eval "$mh(composeBody) tag add [lindex $ot 0] \
			    [lindex $ot 1]"
		}
	    }
	}
	if { 1 == $edit_text } {
	    $w.body.text mark set insert end
	} elseif { -1 == $edit_text } {
	    $w.body.text insert 1.0 "\n\n" noWrap
	    $w.body.text mark set insert 1.0
	} else {
	    $w.body.text mark set insert 1.0
	}
    } else {
	$w.body.text mark set insert 1.0
    }
    if { 1 == $add_sig} {
	set pos [$w.body.text index insert]
	if {1 == [llength [info commands RatUP_Signature]]} {
	    if {[catch {RatUP_Signature $handler} sigtext]} {
		Popup "$t(sig_cmd_failed): $sigtext" $w
		unset sigtext
	    }
	} elseif {![file isdirectory $option($role,signature)]
		&& [file readable $option($role,signature)]} {
	    set fh [open $option($role,signature) r]
	    set sigtext [read -nonewline $fh]
	    close $fh
            set mh(role_sig) 1
	}
	if {[info exists sigtext]} {
	    if {$option(sigdelimit)} {
		$w.body.text insert end "\n" {} "-- " {noWrap no_spell}
	    }
	    $w.body.text insert end "\n$sigtext" {noWrap sig}
	}
	$w.body.text mark set insert $pos
    }
    $w.body.text see insert
    rat_edit::create $w.body.text

    # Calculate font width
    if {![info exists defaultFontWidth]} {
	CalculateFontWidth $w.body.text
    }

    # Attachments window
    frame $w.attach
    frame $w.attach.b
    label $w.attach.b.label -text $t(attachments)
    button $w.attach.b.attachf -text $t(attach_file) -command "Attach $handler"
    set b($w.attach.b.attachf) attach_file
    menubutton $w.attach.b.attachs -text $t(attach_special) -indicatoron 1 \
	    -menu $w.attach.b.attachs.m -relief raised
    set b($w.attach.b.attachs) attach_special
    set m $w.attach.b.attachs.m
    menu $m
    $m add command -label $t(attach_pgp_keys)... -command "AttachKeys $handler"
    set b($m,[$m index end]) attach_keys
    if { 0 == $option(pgp_version)} {
	$m entryconfigure [$m index end] -state disabled
    }
    button $w.attach.b.detach -text $t(detach) -state disabled \
	    -command "Detach $handler $w.attach.b.detach"
    set b($w.attach.b.detach) detach
    pack $w.attach.b.label -side left -padx 10
    pack $w.attach.b.attachf \
	 $w.attach.b.attachs \
	 $w.attach.b.detach -side left -padx 5
    frame $w.attach.list
    scrollbar $w.attach.list.scroll -relief sunken -takefocus 0 \
	    -command "$w.attach.list.list yview" -highlightthickness 0
    listbox $w.attach.list.list -yscroll "$w.attach.list.scroll set" \
	    -height 3 -relief sunken -bd 1 \
	    -exportselection false -highlightthickness 0 -selectmode extended
    set b($w.attach.list.list) attachments
    bind $w.attach.list.list <ButtonRelease-1> \
	    "if { 1 == \[llength \[%W curselection\]\]} {\
	         $w.attach.b.detach configure -state normal}"
    bind $w.attach.list.list <KeyRelease> \
	    "if { 1 == \[llength \[%W curselection\]\]} {\
	         $w.attach.b.detach configure -state normal}"
    pack $w.attach.list.scroll -side right -fill y
    pack $w.attach.list.list -side left -expand 1 -fill both
    pack $w.attach.b \
	 $w.attach.list -side top -fill x
    set mh(attachmentListWindow) $w.attach.list.list
    if {![info exists mh(attachmentList)]} {
	set mh(attachmentList) {}
    } else {
	foreach attachment $mh(attachmentList) {
	    upvar #0 $attachment bp
	    if { [info exists bp(description)] && "" != $bp(description) } {
		set desc $bp(description)
	    } else {
		set p(NAME) {}
		set p(FILENAME) {}
		foreach pp [concat $bp(parameter) $bp(disp_parm)] {
		    array set p $pp
		}
		if {[string length $p(NAME)]} {
		    set desc $p(NAME)
		} elseif {[string length $p(FILENAME)]} {
		    set desc $p(FILENAME)
		} else {
		    set desc "$bp(filename)"
		}
	    }
	    $mh(attachmentListWindow) insert end \
		    "$bp(type)/$bp(subtype) : $desc"
	}
	$w.attach.b.detach configure -state normal
    }

    # Buttons
    frame $w.buttons
    button $w.buttons.send -text $t(send) \
	    -command "DoCompose2 $w $handler send"
    set b($w.buttons.send) send
    lappend mh(eEditBlock) $w.buttons.send
    menubutton $w.buttons.sendsave -text $t(send_save) -indicatoron 1 \
	    -menu $w.buttons.sendsave.m -relief raised -underline 0
    set b($w.buttons.sendsave) sendsave
    menu $w.buttons.sendsave.m -tearoff 0 -postcommand \
	    "RatSendSavePostMenu $w $w.buttons.sendsave.m $handler"
    lappend mh(eEditBlock) $w.buttons.sendsave
    button $w.buttons.hold -text $t(hold)... -command "ComposeHold $w $handler"
    set b($w.buttons.hold) hold
    lappend mh(eEditBlock) $w.buttons.hold
    menubutton $w.buttons.edit -indicatoron 1 -menu $w.buttons.edit.m \
	    -relief raised -direction flush -textvariable ${handler}(eeditor)
    menu $w.buttons.edit.m -tearoff 0
    set mh(eeditm) $w.buttons.edit.m
    ComposeEEditorPopulate $handler
    trace variable editors w "ComposeEEditorPopulate $handler"
    set b($w.buttons.edit) eedit
    lappend mh(eEditBlock) $w.buttons.abort
    button $w.buttons.abort -text $t(abort) -command \
	    "ComposeBuildStruct $handler abort; DoCompose2 $w $handler abort"
    set b($w.buttons.abort) abort_compose
    lappend mh(eEditBlock) $w.buttons.edit
    pack $w.buttons.send \
	 $w.buttons.sendsave \
	 $w.buttons.hold -side left -padx 5
    pack $w.buttons.abort -side right -padx 5
    pack $w.buttons.edit -expand 1 -padx 5
    lappend mh(sendButtons) $w.buttons.send
    lappend mh(sendButtons) $w.buttons.sendsave

    # Populate headerlist and pack everything
    set first [ComposeBuildHeaderEntries $handler]
    pack $w.menu -side top -fill x
    pack $mh(headerFrame) -side top -fill x -padx 5 -pady 5
    pack $w.buttons -side bottom -fill x
    pack $w.attach -side bottom -fill x -padx 5 -pady 5
    pack $w.body -expand yes -fill both

    set mh(oldfocus) [focus]
    if { 1 == $edit_text || -1 == $edit_text } {
	focus $w.body.text
    } elseif {[string length $first]} {
	focus $first
    }
    Place $w compose
    lappend composeWindowList $handler
    ComposeBind $handler
    bind $mh(composeBody) <Destroy> \
	    "ComposeBuildStruct $handler abort; DoCompose2 $w $handler abort"

    if { 1 == $option(always_editor) } {
	ComposeEEdit $handler [lindex $editors 0]
    }

    UpdateComposeRole $handler

    return $handler
}
proc DoCompose2 {w handler do} {
    global composeWindowList option b editors folderWindowList \
	    vFolderDef
    upvar #0 $handler mh

    # Are we already doing this?
    if {0 == [info exists mh(closing)] || 1 == $mh(closing)} {
	return
    }

    # Check if a save folder is defined
    if {![string length $mh(save_to)] \
	    && "" != $option($mh(role),save_outgoing)} {
	set mh(save_to) $vFolderDef($option($mh(role),save_outgoing))
    }

    set mh(do) $do
    if { "send" == $do } {
	# Check that send is eanbled
	if {"disabled" == [[lindex $mh(sendButtons) 0] cget -state]} {
	    bell
	    return
	}

	set mh(closing) 1
	# By moving the focus to the text windge we force all HeaderEntries
	# to update their variables
	focus $mh(composeBody)
	update

	wm withdraw $w
	if {![ComposeSend $handler]} {
	    set sent 1
	    set doRemove 0
	} else {
	    set mh(closing) 0
	    wm deiconify $w
	    return
	}
    } elseif { "abort" == $do } {
	set sent 0
	set doRemove 1
    } elseif { "hold" == $do } {
	set sent 0
	set doRemove 0
    }

    if {[info exists mh(body)]} {
	ComposeFreeBody $mh(body) $doRemove
    }
    set index [lsearch $composeWindowList $handler]
    set composeWindowList [lreplace $composeWindowList $index $index]
    if {[winfo exists $w]} {
	RecordPos $w compose
	RecordSize $mh(composeBody) compose
	bind $mh(composeBody) <Destroy> { }
	foreach bn [array names b $w.*] {unset b($bn)}
	destroy $w
	if {![array size folderWindowList]} {
	    destroy .
	}
	catch "focus -force $mh(oldfocus)"
    }
    trace vdelete editors w "ComposeEEditorPopulate $handler"
    unset mh
}

# ComposeBind --
#
# Bind keyboard shortcuts for the compose window
#
# Arguments:
# handler -	The handler which identifies the context

proc ComposeBind {handler} {
    upvar #0 $handler mh

    foreach w "$mh(toplevel) $mh(composeBody)" {
	RatBind $w compose_key_send \
		"DoCompose2 $mh(toplevel) $handler send; break"
	RatBind $w compose_key_abort "ComposeBuildStruct $handler abort; \
		DoCompose2 $mh(toplevel) $handler abort; break" $mh(abort_menu)
	RatBind $w compose_key_editor \
		"ComposeEEdit $handler \[lindex \$editors 0\]"
	RatBind $mh(composeBody) compose_key_undo \
		"event generate $mh(composeBody) <<Undo>>; break" \
		$mh(undo_menu)
	RatBind $mh(composeBody) compose_key_cut \
		"event generate $mh(composeBody) <<Cut>>; break" $mh(cut_menu)
	RatBind $mh(composeBody) compose_key_copy \
		"event generate $mh(composeBody) <<Copy>>;break" $mh(copy_menu)
	RatBind $mh(composeBody) compose_key_wrap \
		"event generate $mh(composeBody) <<Wrap>>;break" $mh(wrap_menu)
	RatBind $mh(composeBody) compose_key_cut_all \
		"event generate $mh(composeBody) <<CutAll>>; break" \
		$mh(cut_all_menu)
	RatBind $mh(composeBody) compose_key_paste \
		"event generate $mh(composeBody) <<Paste>>; break" \
		$mh(paste_menu)
    }
}

# RatSendSavePostMenu --
#
# Create the want to save to menu
#
# Arguments:
# w	  -	Name of window
# m       -     Name of menu
# handler -     Handler of save window

proc RatSendSavePostMenu {w m handler} {
    global t

    $m delete 0 end
    VFolderBuildMenu $m 0 "RatSendSaveDo $w $handler" 1
    $m add separator
    $m add command -label $t(to_file)... \
	    -command "RatSendSaveDo $w $handler \
		      \[InsertIntoFile [winfo toplevel $w]\]"
    $m add command -label $t(to_dbase)... \
	    -command "RatSendSaveDo $w $handler \
		      \[InsertIntoDBase [winfo toplevel $w]\]"
}
proc RatSendSaveDo {w handler save_to} {
    upvar #0 $handler hd

    if {"" == $save_to} {
	return
    }
    if {1 == [llength $save_to]} {
	global vFolderDef
	set hd(save_to) $vFolderDef($save_to)
    } else {
	set hd(save_to) $save_to
    }
    DoCompose2 $w $handler send
}


# ComposeBuildHeaderEntries --
#
# Builds a list of header entries and packs them into the appropriate frame
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeBuildHeaderEntries {handler} {
    global composeHeaderList composeAdrHdrList t b
    upvar #0 $handler mh

    foreach slave [grid slaves $mh(headerFrame)] {
	destroy $slave
    }
    set mh(headerHandles) {}
    set first {}
    set row 0
    grid columnconfigure $mh(headerFrame) 1 -weight 1

    foreach header $composeHeaderList {
	if {0 == $mh(O_$header)} {
	    continue
	}
	label $mh(headerFrame).${header}_label -text $t($header):
	grid $mh(headerFrame).${header}_label -row $row -column 0 -sticky en
	set w $mh(headerFrame).${header}_entry
	if {-1 != [lsearch $composeAdrHdrList $header]} {
	    lappend mh(headerHandles) \
		    [ComposeBuildHE $w $handler ${handler}($header)]
	} else {
	    entry $w -textvariable ${handler}($header)
	}
	set b($w) compose_$header
	set b($w.t) compose_$header
	grid $w -row $row -column 1 -sticky we
	incr row
	if {0 == [string length $first]} {
	    set first $mh(headerFrame).${header}_entry
	}
    }

    return $first
}

# ComposeUpdateHeaderEntries --
#
# Updates all the header-entries from the variables
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeUpdateHeaderEntries {handler} {
    upvar #0 $handler mh

    foreach hh $mh(headerHandles) {
	set w [lindex $hh 0]
	set hd [lindex $hh 1]
	upvar #0 $hd hdr
	upvar #0 $hdr(varname) var

	$w delete 1.0 end
	$w insert end $var
	ComposeHandleHE $w $hd
    }
}

# ComposeBuildStruct --
#
# Builds the body structures needed by the RatSend and RatHold commands.
#
# Arguments:
# handler -	The handler for the active compose session
# mode -	The reason we are building the structure. Valid modes are:
#               abort - Do minimum
#               send  - Check charset
#               hold

proc ComposeBuildStruct {handler mode} {
    upvar #0 $handler mh
    global idCnt t option charsetMapping

    # Create identifier
    set id body[incr idCnt]
    upvar #0 $id bh

    set bh(type) text
    set bh(subtype) plain
    set bh(filename) [RatTildeSubst $option(send_cache)/rat.[RatGenId]]
    set fh [open $bh(filename) w 0600]
    set charset $mh(charset)
    if {[info exists bh(parameter)]} {
	set params $bh(parameter)
	lappend params {a a}
	array get p $params
	if {[info exists p(charset)]} {
	    set charset $p(charset)
	}
    }
    catch {$mh(composeBody) get 1.0 end-1c} bodydata
    if {"send" == $mode} {
	if {"auto" == $charset} {
	    set fallback $option(charset)
	    set charset [RatCheckEncodings bodydata \
			     $option(charset_candidates)]
	} else {
	    set fallback $charset
	    set charset [RatCheckEncodings bodydata $mh(charset)]
	}
	if {"" == $charset} {
	    if {0 != [RatDialog $mh(toplevel) $t(warning) $t(bad_charset) {} \
		    0 $t(continue) $t(abort)]} {
		return -1
	    }
	    set charset $fallback
	}
	if {![info exists p(charset)]} {
	    lappend bh(parameter) [list charset $charset]
	}
    } elseif {"hold" == $mode} {
	if {"auto" == $charset} {
	    set charset utf-8
	}
	set mh(other_tags) {}
	foreach tag {Cited noWrap no_spell sig} {
	    lappend mh(other_tags) \
		    [list $tag [$mh(composeBody) tag ranges $tag]]
	}
    }
    if {"abort" != $mode} {
	set mh(charset) $charset
	if {[info exists charsetMapping($charset)]} {
	    fconfigure $fh -encoding $charsetMapping($charset)
	} else {
	    fconfigure $fh -encoding $charset
	}
	puts -nonewline $fh $bodydata
	close $fh
	set bh(encoding) [RatGetCTE $bh(filename)]
	if {[info exists mh(content-description)]} {
	    if { 0 < [string length $mh(content-description)]} {
		set bh(description) $mh(content-description)
	    }
	}
    }
    set bh(removeFile) 1
    if {0 == [llength $mh(attachmentList)]} {
	set bh(pgp_sign) $mh(pgp_sign)
	set bh(pgp_encrypt) $mh(pgp_encrypt)
	set mh(body) $id
    } else {
	# Create identifier
	set mid body[incr idCnt]
	upvar #0 $mid ph
	set mh(body) $mid

	set ph(pgp_sign) $mh(pgp_sign)
	set ph(pgp_encrypt) $mh(pgp_encrypt)
	set ph(type) multipart
	set ph(subtype) mixed
	set ph(encoding) 7bit
	set ph(children) [linsert $mh(attachmentList) 0 $id]
	set ph(removeFile) 0
    }
    return 0
}

# ComposeSend --
#
# Actually send a message
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeSend {handler} {
    global t composeAdrHdrList
    upvar #0 $handler mh

    if { 0 == [string length "$mh(to)$mh(cc)$mh(bcc)"]} {
	Popup $t(need_to) $mh(toplevel)
	return 1
    }

    # Alias expansion and syntax error checking
    set err {}
    foreach e $composeAdrHdrList {
	if {[info exists mh($e)]} {
	    if {![catch {RatAlias expand2 $mh($e) $mh(role)} out]} {
		set mh($e) $out
	    } else {
		lappend err $t($e)
	    }
	}
    }
    if {0 < [llength $err]} {
	Popup "$t(adr_syntax_error): $err" $mh(toplevel)
	return 1
    }

    if { -1 == [ComposeBuildStruct $handler send]} {
	return 1
    }

    if {[catch "RatSend send $handler" message]} {
	RatLog 4 $message
	return 1
    } else {
	return 0
    }
}

# CompseEEdit --
#
# Run an external editor on the bodypart
#
# Arguments:
# handler -	The handler for the active compose session
# e	  -	Id of external editor to use

proc ComposeEEdit {handler e} {
    upvar #0 $handler mh
    global t idCnt editor charsetMapping rat_tmp

    if {[info exists mh(eedit_running)]} {
	return
    }

    set ehandler compose_E[incr idCnt]
    upvar #0 $ehandler eh

    # Write data, change text visible and edit
    set ecmd [lindex $editor($e) 0]
    set charset [lindex $editor($e) 1]
    if {[info exists charsetMapping($charset)]} {
	set charset $charsetMapping($charset)
    }
    set fname $rat_tmp/rat.[RatGenId]
    set fh [open $fname w 0600]
    if {"system" != $charset} {
	if {[catch {fconfigure $fh -encoding $charset} error]} {
	    Popup $error $mh(toplevel)
	    return
	}
    }
    puts -nonewline $fh [$mh(composeBody) get 0.0 end-1c]
    close $fh

    foreach block $mh(eEditBlock) {
	$block configure -state disabled
    }
    set mh(eedit_running) 1

    $mh(composeBody) delete 0.0 end
    $mh(composeBody) insert end "\n\n\n   $t(running_ext_editor)"
    if { 0 == [regsub "%s" $ecmd $fname cmd]} {
	set cmd "$ecmd $fname"
    }
    set pos "+[winfo rootx $mh(toplevel)]+[winfo rooty $mh(toplevel)]"
    regsub "%x" $cmd $pos cmd
    trace variable eh(status) w "ComposeEEdit2 $handler $fname $charset"
    RatBgExec ${ehandler}(status) $cmd
}
proc ComposeEEdit2 {handler fname charset name1 name2 op} {
    upvar #0 $handler mh
    upvar #1 $name1 eh

    # Check if still active, if then insert data
    if {[info exists mh]} {
	$mh(composeBody) delete 0.0 end
	set fh [open $fname r]
	if {"system" != $charset} {
	    catch {fconfigure $fh -encoding $charset}
	}
	while { -1 != [gets $fh line]} {
	    $mh(composeBody) insert end "$line\n" noWrap
	}
	close $fh
	catch "file delete -force -- $fname"
	foreach block $mh(eEditBlock) {
	    $block configure -state normal
	}
    }

    # Remove the trace
    trace vdelete ${name1}($name2) w "ComposeEEdit2 $handler $fname"
    unset mh(eedit_running)
}

# Attach --
#
# Attach a file to the message currently being composed
#
# Arguments:
# handler -	The handler for the active compose session

proc Attach {handler} {
    global idCnt t b option fixedNormFont
    upvar #0 $handler mh

    # Create identifier
    set id attach[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(done) 0

    set hd(filename) [rat_fbox::run -title $t(attach_file) -ok $t(open) \
	    -mode open -parent $mh(toplevel)]
    if {"" == $hd(filename)} {
	unset hd
	return
    }

    toplevel $w -class TkRat
    wm title $w $t(attach_file)
    wm transient $w $mh(toplevel)

    # Get default type $hd(filename)
    set type [RatType $hd(filename)]
    set hd(typestring) [lindex $type 0]
    set hd(encoding) [lindex $type 1]
    set hd(disp_fname) [file tail $hd(filename)]

    # Build specification window
    frame $w.file
    label $w.file.label -width 14 -anchor e -text $t(filename):
    label $w.file.name -textvariable ${id}(filename) \
        -anchor w -font $fixedNormFont -width 65
    pack $w.file.label \
	 $w.file.name -side left
	
    frame $w.type
    frame $w.type.r
    label $w.type.typelabel -width 14 -anchor e -text $t(type):
    menubutton $w.type.type -menu $w.type.type.m -anchor w -width 23 \
	    -textvariable ${id}(typestring) -relief raised -indicatoron 1
    set b($w.type.type) type_menu
    menu $w.type.type.m -tearoff 0
    foreach type { {text {plain enriched}}
		   {image {jpeg gif tiff bmp xpm png ppm pgm}}
		   {audio {basic}}
		   {video {mpeg}}
		   {message {rfc822 partial external-body}}
		   {application {octet-stream pdf postscript}}} {
	set typename [lindex $type 0]
	set submenu $w.type.type.m.$typename
	$w.type.type.m add cascade -menu $submenu -label $typename
	menu $submenu -tearoff 0
	foreach s [lindex $type 1] {
	    $submenu add command -label $s \
		    -command "set ${id}(typestring) $typename/$s"
	}
	$submenu add command -label $t(other)... \
		-command "SubtypeSpec ${id}(typestring) $typename $w"
    }
    label $w.type.r.enclabel -anchor e -text $t(current_encoding):
    menubutton $w.type.r.enc -menu $w.type.r.enc.m -anchor w -width 14 \
	    -textvariable ${id}(encoding) -relief raised -indicatoron 1
    set b($w.type.r.enc) encoding_menu
    menu $w.type.r.enc.m -tearoff 0
    # Make sure the user can only select the apropriate entries
    switch $hd(encoding) {
    8bit {
	    set bit7 disabled
	    set bit8 normal
	    set binary normal
	}
    binary {
	    set bit7 disabled
	    set bit8 disabled
	    set binary normal
	}
    default {
	    set bit7 normal
	    set bit8 normal
	    set binary normal
	}
    }
    $w.type.r.enc.m add command -label 7bit \
	    -command "set ${id}(encoding) 7bit" -state $bit7
    $w.type.r.enc.m add command -label 8bit \
	    -command "set ${id}(encoding) 8bit" -state $bit8
    $w.type.r.enc.m add command -label binary \
	    -command "set ${id}(encoding) binary" -state $binary
    $w.type.r.enc.m add command -label quoted-printable \
	    -command "set ${id}(encoding) quoted-printable" -state $bit7
    $w.type.r.enc.m add command -label base64 \
	    -command "set ${id}(encoding) base64" -state $bit7
    pack $w.type.typelabel \
	 $w.type.type -side left
    pack $w.type.r.enclabel \
	 $w.type.r.enc -side left
    pack $w.type.r -side right -padx 5

    frame $w.fname
    label $w.fname.label -width 14 -anchor e -text $t(filename):
    entry $w.fname.entry -width 65 -textvariable ${id}(disp_fname)
    pack $w.fname.label \
	 $w.fname.entry -side left
    set b($w.fname.entry) attach_fname

    frame $w.desc
    label $w.desc.label -width 14 -anchor e -text $t(description):
    entry $w.desc.entry -width 65 -textvariable ${id}(description)
    pack $w.desc.label \
	 $w.desc.entry -side left
    set b($w.desc.entry) attach_description

    frame $w.id
    label $w.id.label -width 14 -anchor e -text $t(id):
    entry $w.id.entry -width 65 -textvariable ${id}(id)
    set b($w.id.entry) attach_id
    pack $w.id.label \
	 $w.id.entry -side left
    
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"

    pack $w.file \
	 $w.type \
	 $w.fname \
	 $w.desc \
	 $w.id \
	 $w.buttons -side top -fill both -pady 2

    Place $w attach2
    ModalGrab $w $w.desc.entry
    $w.desc.entry icursor 0
    tkwait variable ${id}(done)

    if { 1  == $hd(done) } {
	set type [split $hd(typestring) /]
	set hd(type) [lindex $type 0]
	set hd(subtype) [lindex $type 1]
	set hd(removeFile) 0
	if {[string length $hd(disp_fname)]} {
	    set hd(disp_parm) \{[list filename $hd(disp_fname)]\}
	    set hd(parameter) \{[list name     $hd(disp_fname)]\}
	}
	set hd(disp_type) attachment
	if {$mh(copy_attached)} {
	    set fname [RatTildeSubst $option(send_cache)/[RatGenId]]
	    if {"link" == [file type $hd(filename)]} {
		set l [file readlink $hd(filename)]
		if {"relative" == [file pathtype $l]} {
		    set d [file dirname $hd(filename)]
		    if {"relative" == [file pathtype $d]} {
			set d [pwd]/$d
		    }
		    set l $d/$l
		}
		if {[catch {exec ln -s $l $fname} result]} {
		    Popup "$t(failed_to_make_copy): $result" $w
		}
	    } elseif {[catch {file copy -- $hd(filename) $fname} result]} {
		Popup "$t(failed_to_make_copy): $result" $w
	    } else {
		set hd(filename) $fname
		set hd(removeFile) 1
	    }
	}
	lappend mh(attachmentList) $id
	if { "" != $hd(description) } {
	    set desc $hd(description)
	} else {
	    set desc "$hd(typestring): $hd(disp_fname)"
	}
	$mh(attachmentListWindow) insert end $desc
    } else {
	unset hd
    }

    RecordPos $w attach2
    foreach bn [array names b $w.*] {unset b($bn)}
    destroy $w
}

# SubtypeSpec --
#
# Let the user specify an subtype
#
# Arguments:
# variable -	The name of a global variable in which the result is to be
#		left.
# type -	The primary type of the object.
# topwin -	The window we should be transient for

proc SubtypeSpec {variable type topwin} {
    upvar #0 $variable var
    global t idCnt fixedNormFont

    set id subtype[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(done) 0
    toplevel $w -class TkRat
    wm title $w $t(custom_type)
    wm transient $w $topwin

    frame $w.type
    label $w.type.label -width 10 -anchor e -text $t(type):
    label $w.type.name -text $type -anchor w -font $fixedNormFont
    pack $w.type.label \
	 $w.type.name -side left
    frame $w.subtype
    label $w.subtype.label -width 10 -anchor e -text $t(subtype):
    entry $w.subtype.entry -width 20 -textvariable ${id}(spec)
    pack $w.subtype.label \
	 $w.subtype.entry -side left
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"
    
    pack $w.type \
	 $w.subtype -side top -anchor w -padx 5
    pack $w.buttons -side top -pady 10 -fill x
    
    bind $w <Return> "set ${id}(done) 1"
    bind $w.subtype.entry <Tab> "set ${id}(done) 1"

    Place $w subtypeSpec
    ModalGrab $w $w.subtype.entry

    tkwait variable ${id}(done)

    if {1 == $hd(done)} {
	set var $type/$hd(spec)
    }

    RecordPos $w subtypeSpec
    destroy $w
    unset hd
}

# AttachKeys --
#
# Attach keys to the message currently being composed
#
# Arguments:
# handler -	The handler for the active compose session

proc AttachKeys {handler} {
    RatPGPGetIds AttachKeysDo $handler
}
proc AttachKeysDo {handler ids} {
    global idCnt t option
    upvar #0 $handler mh

    foreach keyid $ids {
	# Create identifier
	set id attach[incr idCnt]
	upvar #0 $id hd

	set hd(type) application
	set hd(subtype) pgp-keys
	set hd(encoding) 7bit
	set hd(description) \
		"$t(pgp_key) [lindex $keyid 0] $t(for) [lindex $keyid 1]"
	set hd(filename) [RatTildeSubst $option(send_cache)/[RatGenId]]
	set hd(removeFile) 1

	set f [open $hd(filename) w]
	puts $f [RatPGP extract [lindex $keyid 0]]
	close $f
	lappend mh(attachmentList) $id
	$mh(attachmentListWindow) insert end $hd(description)
    }
}

# Detach --
#
# Detach a previously attached attachment to a message
#
# Arguments:
# handler -	The handler for the active compose session
# button  -	The button which shall be disabled

proc Detach {handler button} {
    upvar #0 $handler mh
    $button configure -state disabled
    foreach element [lsort -integer -decreasing \
			    [$mh(attachmentListWindow) curselection]] {
	$mh(attachmentListWindow) delete $element
	ComposeFreeBody [lindex $mh(attachmentList) $element] 1
	set mh(attachmentList) [lreplace $mh(attachmentList) $element $element]
    }
}

# ComposeFreeBody --
#
# Free a bodypart from memory and remove any temporary files associated with it
#
# Arguments:
# handler -	The handler for the active compose session
# doRemove -	True if we should actually remove associated files

proc ComposeFreeBody {handler doRemove} {
    upvar #0 $handler bh

    if { "multipart" == $bh(type)} {
	if {[info exists bh(children)]} {
	    foreach body $bh(children) {
		ComposeFreeBody $body $doRemove
	    }
	}
    } 
    if {[info exists bh(removeFile)]} {
	if {$doRemove && $bh(removeFile)} {
	    catch {file delete -- $bh(filename)}
	}
    }
    unset bh
}

# ComposeHold --
#
# Insert the message being composed into the hold.
#
# Arguments:
# mainW   -	The main compose window
# handler -	The handler for the active compose session

proc ComposeHold {mainW handler} {
    upvar #0 $handler mh
    global idCnt t b

    # Create identifier
    set id hold[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(done) 0

    # Set default
    set hd(desc) ""
    if {[string length $mh(to)]} {
	set hd(desc) "$t(to): $mh(to)   "
    }
    if {[string length $mh(subject)]} {
	set hd(desc) "$hd(desc) $t(subject): $mh(subject)"
    }

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(hold_message)
    wm transient $w $mh(toplevel)

    # Populate window
    frame $w.desc
    label $w.desc.label -width 10 -anchor e -text $t(description):
    entry $w.desc.entry -width 80 -textvariable ${id}(desc)
    pack $w.desc.label \
	 $w.desc.entry -side left
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"
    pack $w.desc -side top -anchor w -padx 5 -pady 5
    pack $w.buttons -side top -pady 5 -fill x
    set b($w.desc.entry) hold_description
    
    Place $w composeHold
    ModalGrab $w $w.desc.entry

    tkwait variable ${id}(done)

    if {1 == $hd(done)} {
	ComposeBuildStruct $handler hold
	if {![catch [list RatHold insert $handler $hd(desc)] message]} {
	    DoCompose2 $mainW $handler hold
	} else {
	    Popup "$t(hold_failed); $message" $w
	}
    }

    RecordPos $w composeHold
    unset b($w.desc.entry)
    destroy $w
    unset hd
}


# ComposeChoose --
#
# This routine gets a message handler and scans it for embedded messages.
# If none are found the message handler is returned. If any are found the
# user may choose which message handler is to be returned.
#
# Arguments:
# msg  -	Message handler of message to reply to
# info -	An informative text which is to be displayed at the top
#		of the window

proc ComposeChoose {msg info} {
    global idCnt t b option fixedBoldFont

    set msgs [ComposeChooseDig [$msg body] $msg]
    while { -1 != [set i [lsearch -exact $msgs {}]]} {
	set msgs [lreplace $msgs $i $i]
    }
    if { 1 == [llength $msgs] } {
	return $msgs
    }

    # Create identifier
    set id cc[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(done) 0

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(reply_to)?

    # Populate window
    set mnum 0
    set hd(choosed) $msg
    label $w.label -text $info
    pack $w.label -side top -fill x
    frame $w.l -relief sunken -bd 1
    canvas $w.l.canvas \
	    -yscrollcommand "$w.l.scroll set" \
	    -highlightthickness 0
    scrollbar $w.l.scroll \
	    -relief sunken \
	    -bd 1 \
	    -command "$w.l.canvas yview" \
	    -highlightthickness 0
    pack $w.l.scroll -side right -fill y
    Size $w.l.canvas msgList
    pack $w.l.canvas -expand 1 -fill both
    frame $w.l.canvas.f
    set elemId [$w.l.canvas create window 0 0 -anchor nw -window $w.l.canvas.f]
    foreach m $msgs {
	set f $w.l.canvas.f.f$mnum
	incr mnum

	frame $f
	radiobutton $f.r -value $m -variable ${id}(choosed)
	text $f.t -relief flat
	set width 0
	set height 0
	$f.t tag configure HeaderName -font $fixedBoldFont
	foreach h [$m headers] {
	    set header([string tolower [lindex $h 0]]) [lindex $h 1]
	}
	foreach field [string tolower $option(show_header_selection)] {
	    if {[info exists header($field)]} {
		regsub -all -- - $field _ n
		if {[info exists t($n)]} {
		    set name $t($n)
		} else {
		    set name $field
		}
		$f.t insert end "$name: " HeaderName "$header($field)"
		set length [lindex [split [$f.t index insert] .] 1]
		if {$length >$width} {
		    set width $length
		}
		$f.t insert end "\n"
		incr height
	    }
	}
	$f.t configure -width $width -height $height -state disabled
	pack $f.r -side left -anchor n
	pack $f.t
	pack $f -side top -anchor w
	set b($f.r) choose_msg
	set b($f.t) choose_msg
    }
    pack $w.l -side top -expand 1 -fill both
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"
    pack $w.buttons -fill both -pady 5

    Place $w composeChoose
    update idletasks
    set bbox [$w.l.canvas bbox $elemId]
    eval {$w.l.canvas configure -scrollregion $bbox}

    ModalGrab $w
    tkwait variable ${id}(done)
    RecordSize $w.l.canvas msgList
    RecordPos $w composeChoose
    destroy $w

    if {1 == $hd(done)} {
	set r $hd(choosed)
    } else {
	set r {}
    }
    unset hd
    return $r
}


# ComposeChooseDig --
#
# Gets a bodypart handler and checks for embedded messages in it. The
# list of found messages is returned
#
# Arguments:
# body -	The bodypart to look in
# msgs -	The list of messages found so far

proc ComposeChooseDig {body msgs} {
    set type [$body type]
    if {![string compare message/rfc822 [lindex $type 0]/[lindex $type 1]]} {
	return [concat $msgs [$body message]]
    }
    foreach child [$body children] {
	set type [$child type]
	switch -glob [string tolower [lindex $type 0]/[lindex $type 1]] {
	message/rfc822 { set msg [$child message]
			 set msgs [concat $msgs $msg]
			 set msgs [ComposeChooseDig [$msg body] $msgs] }
	multipart/*    { set msgs [ComposeChooseDig $child $msgs] }
	}
    }
    return $msgs
}

# ComposeBuildHE --
#
# Build the header entry widget. The interface looks somewhat like the
# entry-widget, but we use a text-widget and have some special bindings.
#
# Arguments:
# w		- The window to build
# handler	- The handler for the active compose session
# textvariable	- The variable to keep and leave the result in

proc ComposeBuildHE {w mhandler textvariable} {
    global idCnt defaultFontWidth ISO_Left_Tab book_img
    upvar #0 $textvariable textvar
    upvar #0 $mhandler mh

    set handler compHE[incr idCnt]
    upvar #0 $handler hd

    # Build windows
    frame $w
    text $w.t -relief sunken -yscroll "$w.s set" -width 1 -height 1 -wrap none
    scrollbar $w.s -relief sunken -command "$w.t yview" -highlightthickness 0
    button $w.b -command "ComposeHandleHEAlias $w.t $handler" -bd 1 \
	    -image $book_img -padx 0 -pady 0 -takefocus 0
    pack $w.t -side left -expand yes -fill x
    pack $w.b -side right -anchor n

    # Initialize variables
    set hd(scrollbar) $w.s
    set hd(lines) 1
    set hd(varname) $textvariable
    set hd(scroll) 0
    set hd(width) 0
    set hd(mhandler) $mhandler

    # Do bindings
    bind $w <FocusIn> "focus $w.t"
    bind $w.t <Return> {focus [tk_focusNext %W]; break}
    bind $w.t <Tab> {focus [tk_focusNext %W]; break}
    bind $w.t <Shift-Tab> {focus [tk_focusPrev %W]; break}
    bind $w.t <$ISO_Left_Tab> {focus [tk_focusPrev %W]; break}
    bind $w.t <Shift-space> { }
    bind $w.t <KeyRelease-comma> "ComposeHandleHE %W $handler"
    bind $w.t <FocusOut> "ComposeHandleHE %W $handler"
    bind $w.t <Destroy> "unset $handler"
    bind $w.t <<PasteSelection>> "ComposeHandleHEPaste %W $handler; break"
    bind $w.t <<Paste>> "ComposeHandleHEPaste %W $handler; break"
    bind $w.t <Control-l> "ComposeHandleHEAlias %W $handler"
    bind $w.t <Configure> "ComposeHandleHEConfigure %W $handler %w"

    # Create error tag
    if {[winfo cells $w.t] > 2} {
	$w.t tag configure error -foreground red
    } else {
	$w.t tag configure error -underline 1
    }

    # Initialize
    if {![info exists textvar]} {
	set textvar {}
    } else {
	$w.t insert end $textvar
    }
    return [list $w.t $handler]
}

# ComposeHandleHEConfigure --
#
# Handle configure events in an address entry
#
# Arguments:
# w	  - The text widget
# handler - The handler which identifies this address widget
# pixwidth- The width of the text widget (in pixels)

proc ComposeHandleHEConfigure {w handler pixwidth} {
    global defaultFontWidth
    upvar #0 $handler hd

    if {![info exists hd(borders)]} {
	set hd(borders) [expr {2*([$w cget -borderwidth] \
		+[$w cget -highlightthickness])}]
    }
    set width [expr {($pixwidth-$hd(borders))/$defaultFontWidth}]
    if {$width == $hd(width)} {
	return
    }

    set hd(width) $width
    $w configure -tabs [expr {$defaultFontWidth*$hd(width)/2}]
    ComposeHandleHE $w $handler
}

# ComposeHandleHE --
#
# Handle events in an address entry
#
# Arguments:
# w	  - The text widget
# handler - The handler which identifies this address widget

proc ComposeHandleHE {w handler} {
    upvar #0 $handler hd
    upvar #0 $hd(varname) var
    upvar #0 $hd(mhandler) mh

    set sr [$w tag nextrange sel 1.0]
    if {[llength $sr]} {
	set sel [$w get [lindex $sr 0] [lindex $sr 1]]
    }
    set var [string trim [$w get 1.0 end]]

    $w delete 1.0 end
    set tempalist [RatSplitAdr $var]
    set alist {}
    set max 0
    set tot 0
    foreach adr $tempalist {	
	if {[catch {RatAlias expand1 $adr $mh(role)} adr2]} {
	    set tag($adr) error
	    lappend alist $adr
	} else {
	    set alist [concat $alist [RatSplitAdr $adr2]]
	}
    }
    foreach adr $alist {
	if {![info exists tag($adr)]} {
	    set tag($adr) {}
	}
	incr tot [string length $adr]
	incr tot 2
	if {[string length $adr] > $max} {
	    set max [string length $adr]
	}
    }
    if {$tot <= $hd(width)} {
	foreach adr $alist {
	    $w insert end $adr $tag($adr) ", "
	}
    } elseif {$max <= [expr {$hd(width)/2}]} {
	set c 1
	foreach adr $alist {
	    if {1 == $c} {
		$w insert end $adr $tag($adr) ",\t"
		set c 2
	    } else {
		$w insert end $adr $tag($adr) ",\n"
		set c 1
	    }
	}
    } else {
	foreach adr $alist {
	    $w insert end $adr $tag($adr) ",\n"
	}
    }
    if {[string length $var] && ![regexp {,$} $var]} {
	$w delete [$w search -backwards , end] end
    }
    set hd(lines) [expr {int([$w index end])-1}]

    if {$hd(lines) > 4 && !$hd(scroll)} {
	pack $hd(scrollbar) -side right -fill y
	$w configure -height 4
	set hd(scroll) 1
    } elseif {$hd(lines) <= 4} {
	pack forget $hd(scrollbar)
	$w configure -height $hd(lines)
	set hd(scroll) 0
    }
    $w see insert

    if {[llength $sr]} {
	set r [$w search -- $sel [lindex $sr 0]]
	if {"" == $r} {
	    set r [$w search -- $sel 1.0]
	}
	if {"" != $r} {
	    $w tag add sel $r $r+[string length $sel]c
	}
    }
}

# ComposeHandleHEPaste --
#
# Handle PasteSelection events in an address entry
#
# Arguments:
# w	  - The text widget
# handler - The handler which identifies this address widget

proc ComposeHandleHEPaste {w handler} {
    catch {
	regsub -all mailto: [selection get -displayof $w] {} var
	$w insert insert $var
    }
    ComposeHandleHE $w $handler
}

# ComposeHandleHEAlias --
#
# Handle the alias popup window
# w	  - The text widget
# handler - The handler which identifies this address widget

proc ComposeHandleHEAlias {w handler} {
    set alias [AliasChooser $w]

    if {[string length $alias]} {
	if {[string length [string trim [$w get 1.0 end]]]} {
	    $w insert end ,
	}
	$w insert end $alias
	ComposeHandleHE $w $handler
    }
}

# SendDeferred --
#
# Send deferred messages
#
# Arguments:

proc SendDeferred {} {
    global t numDeferred deferred ratSenderSending fixedNormFont

    set w .deferred

    # Allow only one window at a time.
    if {[winfo exists $w]} {
	incr deferred(to_send) [RatSend sendDeferred]
	return {}
    }

    set deferred(to_send) [RatSend sendDeferred]
    if {0 == $deferred(to_send)} {
	return {}
    }

    set deferred(w) $w
    set deferred(sent) 0
    set deferred(oldDeferred) $numDeferred

    toplevel $w -class TkRat
    wm title $w $t(send_deferred)

    frame $w.f
    message $w.f.message -aspect 500 -text $t(sending_deferred)...
    grid $w.f.message -columnspan 2 
    label $w.f.to_label -text $t(to_send): -anchor e
    label $w.f.to_num -textvariable deferred(to_send) -font $fixedNormFont
    grid $w.f.to_label $w.f.to_num -sticky ew
    label $w.f.sent_label -text $t(sent): -anchor e
    label $w.f.sent_num -textvariable deferred(sent) -font $fixedNormFont
    grid $w.f.sent_label $w.f.sent_num -sticky ew
    pack $w.f -padx 10 -pady 10

    Place $w sendDeferred
    trace variable numDeferred w SendDeferredUpdate
    trace variable ratSenderSending w SendDeferredUpdate

    return $w
}

# SendDeferredUpdate --
#
# Update the send deferred window
#
# Arguments:
# name1	- the variable that was updated
# name2	- notused
# op	- notused

proc SendDeferredUpdate {name1 name2 op} {
    global t numDeferred deferred ratSenderSending

    if {![string compare $name1 ratSenderSending]} {
	if {1 == $ratSenderSending} { return }
	RatSend init
	return
    }
    if {$numDeferred < $deferred(oldDeferred)} {
	incr deferred(sent) 1
	incr deferred(to_send) -1
    }
    set deferred(oldDeferred) $numDeferred
    if {0 == $deferred(to_send) || 0 == $ratSenderSending} {
	RecordPos $deferred(w) sendDeferred
	destroy $deferred(w)
	trace vdelete numDeferred w SendDeferredUpdate
	trace vdelete ratSenderSending w SendDeferredUpdate
    }
}

# ComposeInsertFile --
#
# Insert a file into the message currently being composed
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeInsertFile {handler} {
    global t
    upvar #0 $handler mh

    set filename [rat_fbox::run -ok $t(open) -title $t(insert_file) \
				 -parent [winfo toplevel $mh(composeBody)] \
				 -mode open]

    if {$filename != ""} {
	if {[catch {open $filename} fh]} {
	    Popup [format $t(failed_to_open_file) $fh] $mh(toplevel)
	} else {
	    set mh(undoText) {}
	    $mh(composeBody) mark set undoStart insert
	    $mh(composeBody) mark set undoEnd insert
	    $mh(composeBody) insert insert [read $fh] noWrap
	    close $fh
	}
    }
}

# ComposePostEdit --
#
# Post the edit menu. This routine may disable/enable apropriate entries
# in the menu
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposePostEdit {handler m} {
    global cmdList cmdName cmdCmd t
    upvar #0 $handler hd

    rat_edit::state $hd(composeBody) state

    $m entryconfigure 3 -state $state(selection)
    $m entryconfigure 4 -state $state(selection)
    $m entryconfigure 5 -state $state(paste)
    #$m entryconfigure 9 -state $state(selection)

    if {![info exist cmdList]} {
	CmdRead
    } elseif { $hd(edit_end) < [$m index end]} {
	$m delete [expr {$hd(edit_end)+1}] end
    }
    foreach i $cmdList {
	$m add command -label $cmdName($i) \
		-command "ComposeRunCmd $handler [list $cmdCmd($i)]"
    }
}

# ComposeSpecifyCmd --
#
# Let the user specify a program to run part of the text through
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeSpecifyCmd {handler} {
    global idCnt t b cmdArrayId cmdList cmdName cmdCmd
    upvar #0 $handler mh

    # Create identifier
    set id insert[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(done) 0

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(run_through_command)
    wm transient $w $mh(toplevel)

    # The save as line
    frame $w.s
    checkbutton $w.s.but -text $t(save_as) -variable ${id}(doSave)
    entry $w.s.entry -width 20 -textvariable ${id}(saveAs)
    bind $w.s.entry <KeyRelease> \
	    "if {0 < \[string length ${id}(saveAs)\]} { \
		 set ${id}(doSave) 1 \
	     } else { \
		 set ${id}(doSave) 0 \
	     }"
    label $w.s.label -text $t(command)
    pack $w.s.label -side left -padx 5 -anchor s
    pack $w.s.entry \
	 $w.s.but -side right -pady 5
    set b($w.s.but) save_cmd_as
    set b($w.s.entry) save_cmd_as

    # The text widget and the buttons
    text $w.t -relief sunken -bd 1 -wrap word -setgrid 1
    Size $w.t giveCmd
    set b($w.t) command_to_run_through
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"

    pack $w.s -side top -anchor w -fill x
    pack $w.t -side top -expand 1 -padx 5
    pack $w.buttons -fill both -pady 5

    Place $w giveCmd
    ModalGrab $w $w.s.entry
    tkwait variable ${id}(done)
    RecordPos $w giveCmd

    if {1 == $hd(done)} {
	if {$hd(doSave)} {
	    if {[string length $hd(saveAs)]} {
		lappend cmdList $cmdArrayId
		set cmdName($cmdArrayId) $hd(saveAs)
		set cmdCmd($cmdArrayId) [string trim [$w.t get 1.0 end]]
		incr cmdArrayId
		CmdWrite
	    } else {
		Popup $t(need_name) $w
	    }
	}
	ComposeRunCmd $handler [string trim [$w.t get 1.0 end]]
    }
    destroy $w
    unset hd
}

# ComposeRunCmd --
#
# Runs a command on a specified part of the text
#
# Arguments:
# handler -	The handler for the active compose session
# cmd	  -	The command to run

proc ComposeRunCmd {handler cmd} {
    upvar #0 $handler hd
    global t rat_tmp

    # Find area to work on
    if { 0 != [llength [$hd(composeBody) tag ranges sel]]} {
	set start sel.first
	set end sel.last
    } else {
	set start 1.0
	set end end-1c
    }

    # Remember things for undo
    set hd(undoText) [$hd(composeBody) get $start $end]
    set hd(undoTags) [$hd(composeBody) tag ranges noWrap]
    set hd(undoInsert) [$hd(composeBody) index insert]
    set hd(cmdStart) [$hd(composeBody) index $start]
    set hd(cmdEnd) [$hd(composeBody) index $end]
    set hd(text) [$hd(composeBody) get 1.0 end-1c]

    # Run command
    set name $rat_tmp/rat.[RatGenId]
    set fh [open $name.in w]
    puts $fh [$hd(composeBody) get $start $end]
    close $fh
    if {[regexp {%s} $cmd]} {
	regsub -all {%s} $cmd $name.in cmd
    } else {
	set cmd "cat $name.in | $cmd >$name.out"
    }

    # Replace the text with the message
    $hd(composeBody) delete 1.0 end
    $hd(composeBody) insert end "\n\n\n\t$t(command_is_running)..."

    # Disable compose window
    foreach block $hd(eEditBlock) {
	$block configure -state disabled
    }
    $hd(composeBody) configure -state disabled

    trace variable hd(status) w "ComposeRunCmdDone $handler $name"
    if {[catch {RatBgExec ${handler}(status) $cmd} result]} {
	Popup "$t(command_failed): $result" $hd(toplevel)
    }
}
proc ComposeRunCmdDone {handler name name1 name2 op} {
    upvar #0 $handler hd

    if {[info exists hd]} {
	foreach block $hd(eEditBlock) {
	    $block configure -state normal
	}
	$hd(composeBody) configure -state normal
	$hd(composeBody) delete 1.0 end
	$hd(composeBody) insert 1.0 $hd(text)
	$hd(composeBody) mark set undoStart $hd(cmdStart)
	$hd(composeBody) mark set undoEnd $hd(cmdEnd)
	if {0 == $hd(status)} {
	    $hd(composeBody) delete undoStart undoEnd
	    if {[file readable $name.out]} {
		set outfile $name.out
	    } else {
		set outfile $name.in
	    }
	    set fh [open $outfile r]
	    $hd(composeBody) insert undoStart [read -nonewline $fh]
	    close $fh
	}
    }
    catch {file delete -force -- $name.in $name.out}
    trace vdelete $name1($name2) w "ComposeRunCmdDone $handler $name"
}

# CmdWrite --
#
# Write the saved expressions to disk
#
# Arguments:

proc CmdWrite {} {
    global option cmdArrayId cmdList cmdName cmdCmd

    set f [open $option(ratatosk_dir)/commands w]
    puts $f "set cmdArrayId $cmdArrayId"
    puts $f "set cmdList [list $cmdList]"
    foreach c $cmdList {
	puts $f "set cmdName($c) [list $cmdName($c)]"
	puts $f "set cmdCmd($c) [list $cmdCmd($c)]"
    }
    close $f
}

# CmdRead --
#
# Read the saved expressions
#
# Arguments:

proc CmdRead {} {
    global option cmdArrayId cmdList cmdName cmdCmd

    if {[file readable $option(ratatosk_dir)/commands]} {
	source $option(ratatosk_dir)/commands
    } else {
	set cmdArrayId 0
	set cmdList {}
    }
}

# CmdList --
#
# Lets the user view/modify the list of commands
#
# Arguments:

proc CmdList {} {
    global cmdList cmdName idCnt t b

    # Create identifier
    set id ccmd[incr idCnt]
    upvar #0 $id hd
    set w .$id
    set hd(changed) 0
    set hd(list) $w.l.list
    set hd(text) $w.t
    set hd(apply) $w.b.apply
    set hd(delete) $w.b.delete

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(command_list)

    # The list
    frame $w.l
    listbox $hd(list) \
	    -yscroll "$w.l.scroll set" \
	    -exportselection false \
	    -highlightthickness 0 \
	    -selectmode single \
	    -setgrid 1
    scrollbar $w.l.scroll \
	    -command "$hd(list) yscroll" \
	    -highlightthickness 0
    pack $w.l.scroll -side right -fill y
    pack $hd(list) -expand 1 -fill both
    Size $hd(list) cmdList
    set b($hd(list)) saved_commands

    # The buttons
    frame $w.b
    button $hd(apply) -text $t(apply) -command "CmdApply $w $id" \
	    -state disabled
    button $hd(delete) -text $t(delete) -command "CmdDelete $w $id" \
	    -state disabled
    button $w.b.close -text $t(close) -command "CmdClose $w $id"
    pack $hd(apply) \
	 $w.b.delete \
	 $w.b.close -side top -pady 5 -padx 5
    set b($hd(apply)) apply_changes_to_cmd
    set b($hd(delete)) delete_command
    set b($w.b.close) dismiss

    # The command content
    text $hd(text) \
	    -relief sunken \
	    -bd 1 \
	    -wrap word \
	    -width 40 \
	    -height 4 \
	    -state disabled
    set b($hd(text)) command_content

    # Pack them all
    pack $hd(text) -side bottom
    pack $w.b -side right -padx 5 -pady 5
    pack $w.l -fill both -expand 1 -padx 5 -pady 5

    # Make sure we have the list in memory
    if {![info exist cmdList]} {
	CmdRead
    }
    # no commands defined so no list
    if {[llength $cmdList] == 0} {
	return
    }

    # Populate the list
    foreach c $cmdList {
	$hd(list) insert end $cmdName($c)
    }

    # Bind the listbox and text
    bind $hd(list) <ButtonRelease-1> "\
	    $hd(apply) configure -state disabled; \
	    $hd(delete) configure -state normal; \
	    $hd(text) configure -state normal; \
	    $hd(text) delete 1.0 end; \
	    $hd(text) insert 1.0 \
		    \$cmdCmd(\[lindex \$cmdList \[%W index @%x,%y\]\])"
    bind $hd(text) <KeyRelease> "CmdTextCheck $w $id"
    wm protocol $w WM_DELETE_WINDOW "CmdClose $w $id"

    Place $w cmdList
}

# CmdDelete --
#
# Delete a command
#
# Arguments:
# w       -	The command list window
# handler -	The changes variable

proc CmdDelete {w handler} {
    global cmdList cmdName
    upvar #0 $handler hd

    set index [$hd(list) curselection]
    unset cmdName([lindex $cmdList $index])
    set cmdList [lreplace $cmdList $index $index]
    set hd(changed) 1

    # Populate the list
    $hd(list) delete 0 end
    foreach c $cmdList {
	$hd(list) insert end $cmdName($c)
    }

    # Clear the text
    $hd(text) delete 1.0 end
    $hd(text) configure -state disabled

    # Disable the buttons
    $hd(apply) configure -state disabled
    $hd(delete) configure -state disabled
}

# CmdTextCheck --
#
# Check if the command text has been changed
#
# Arguments:
# w -	The command list window
# handler -	The changes variable

proc CmdTextCheck {w handler} {
    global cmdList cmdCmd
    upvar #0 $handler hd

    if {[string compare [$hd(text) get 1.0 end-1c] \
	    $cmdCmd([lindex $cmdList [$hd(list) curselection]])]} {
	$hd(apply) configure -state normal
    } else {
	$hd(apply) configure -state disabled
    }
}

# CmdApply --
#
# Apply the current change
#
# Arguments:
# w       -	The command list window
# handler -	The changes variable

proc CmdApply {w handler} {
    global cmdList cmdCmd
    upvar #0 $handler hd

    set cmdCmd([lindex $cmdList [$hd(list) curselection]]) \
	    [$hd(text) get 1.0 end-1c]
    $hd(apply) configure -state disabled
    set hd(changed) 1
}

# CmdClose --
#
# Closes the command window
#
# Arguments:
# w       -	The command list window
# handler -	The changes variable

proc CmdClose {w handler} {
    upvar #0 $handler hd

    if { 1 == $hd(changed)} {
	CmdWrite
    }

    catch {focus $hd(oldfocus)}
    unset hd
    destroy $w
}

# ShowGeneratedHeaders --
#
# Show the generated headers window
#
# Arguments:
# handler - Handler identifying the folder window

proc ShowGeneratedHeaders {handler} {
    global idCnt t $handler fixedNormFont fixedBoldFont

    # Create identifier
    set id iw[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(generated_header)

    # Message part
    frame $w.b
    button $w.b.update -text $t(update) -command "UpdateGH $handler $w.text"
    button $w.b.dismiss -text $t(dismiss) -command "\
	    RecordPos $w showGH; \
	    RecordSize $w.text showGH; \
	    destroy $w"
    pack $w.b.update $w.b.dismiss -side left -padx 5 -expand 1
    text $w.text -yscroll "$w.scroll set" -relief sunken -bd 1 -wrap none
    Size $w.text showGH
    scrollbar $w.scroll -relief raised -bd 1 \
	    -command "$w.text yview"
    pack $w.b -side bottom -pady 5 -fill x 
    pack $w.scroll -side right -fill y
    pack $w.text -expand 1 -fill both

    $w.text tag configure name -font $fixedBoldFont
    $w.text tag configure value -font $fixedNormFont -lmargin2 20

    UpdateGH $handler $w.text

    Place $w showGH
}

# FixAddress --
#
# Mangles the given addresses into the formats used when speaking SMTP.
# Returns a string containing the mangled addresses
#
# Arguments:
# role - Role to do work under
# al   - String containing addresses
# mode - Way of printing address (rfc822 or mail)

proc FixAddress {role al mode} {
    set result {}
    foreach adr [split $al ,] {
	set a [RatCreateAddress $adr $role]
	lappend result [$a get $mode]
	rename $a ""
    }
    return [join $result ", "]
}

# UpdateGH --
#
# Update a show generated headers window
#
# Arguments:
# handler - Handler identifying the folder window
# w	  - Name of the text widget
# args    - Ignored trace variables

proc UpdateGH {handler w args} {
    global option
    upvar #0 $handler hd

    $w configure -state normal
    $w delete 1.0 end

    set helo [RatGetCurrent smtp_helo $hd(role)]
    set host [RatGetCurrent host $hd(role)]
    set gen [RatGenerateAddresses $handler]

    # EHLO
    $w insert end "(SMTP)      EHLO: " name $helo value "\n"

    # Envelope from
    set from [RatCreateAddress [lindex $gen 0] $hd(role)]
    $w insert end "(SMTP) MAIL FROM: " name [$from get mail] value "\n"
    rename $from {}

    # Envelope rcpt to
    $w insert end "(SMTP)   RCPT TO: " name
    set al ""
    foreach f {to cc bcc} {
	if {[string length $hd($f)]} {
	    if {[string length $al]} {
		set al "$al, $hd($f)"
	    } else {
		set al $hd($f)
	    }
	}
    }
    set first 1
    RatAlias list alist
    foreach a [split [FixAddress $hd(role) [RatAlias expand2 $al $hd(role)] \
	    mail] ,] {
	if {$first} {
	    set first 0
	} else {
	    $w insert end "                  " name
	}
	$w insert end "<[string trim $a]>\n" value
    }
    $w insert end "\n"

    # From:
    # Possible Sender:
    $w insert end "    From: " name [lindex $gen 0] value "\n"
    if {"" != [lindex $gen 1]} {
	$w insert end "  Sender: " name [lindex $gen 1] value "\n"
    }

    # Reply-to
    if { "" != $hd(reply_to)} {
	$w insert end "Reply-To: " name \
		"[FixAddress $hd(role) [RatAlias expand2 $hd(reply_to) $hd(role)] rfc822]\n" \
		value
    }

    # To
    if { "" != $hd(to)} {
	$w insert end "      To: " name \
		"[FixAddress $hd(role) [RatAlias expand2 $hd(to) $hd(role)] rfc822]\n" \
		value
    }

    # CC
    if { "" != $hd(cc)} {
	$w insert end "      cc: " name \
		"[FixAddress $hd(role) [RatAlias expand2 $hd(cc) $hd(role)] rfc822]\n" \
		value
    }

    $w configure -state disabled
}


# EditorsRead --
#
# Read the editors file
#
# Arguments:

proc EditorsRead {} {
    global option editors editor ratCurrent t charsetMapping editorsChanged \
	   charsetReverseMapping

    if {[file readable $option(ratatosk_dir)/editors]} {
	source $option(ratatosk_dir)/editors
	foreach e [array names editor] {
	    if {[info exists charsetMapping([lindex $editor($e) 1])]} {
		set editor($e) [list [lindex $editor($e) 0] \
			$charsetMapping([lindex $editor($e) 1])]
	    }
	}
    } else {
	set editors [list $t(external_editor)]
	if {[info exists charsetReverseMapping($ratCurrent(charset))]} {
	    set charset $charsetReverseMapping($ratCurrent(charset))
	} else {
	    set charset $ratCurrent(charset)
	}
	set editor($t(external_editor)) \
		[list $option(editor) $charset]
    }
    if {![info exists option(eeditor)]} {
	set option(eeditor) [lindex $editors 0]
    }
    set editorsChanged 0
}


# EditorsWrite --
#
# Write the editors file
#
# Arguments:

proc EditorsWrite {} {
    global option editors editor editorsChanged

    if {0 == $editorsChanged} {
	return
    }
    set f [open $option(ratatosk_dir)/editors w]
    puts $f "set editors [list $editors]" 
    foreach e $editors {
	puts $f [list set editor($e) $editor($e)]
    }
    close $f
    set editorsChanged 0
}


# EditorsList --
#
# Show the editors list window
#
# Arguments:

proc EditorsList {} {
    global t

    rat_list::create editors editorList "EditorsEdit add" "EditorsEdit edit" \
	    EditorsDelete EditorsWrite \
	    $t(editors) $t(add) $t(edit) $t(delete) $t(dismiss)
}

# EditorsDelete --
#
# Delete an editor
#
# Arguments:
# name - Name of editor to delete

proc EditorsDelete {name} {
    global editor editorsChanged

    unset editor($name)
    incr editorsChanged
}

# EditorsEdit --
#
# Edit an editor definition
#
# Arguments:
# mode	 - determines what to do. Valud values are: "add", "edit"
# arg1,2 - Argument depending on mode

proc EditorsEdit {mode arg1 {arg2 {}}} {
    global editors editor idCnt t b option charsetName charsetReverseMapping

    # Create identifier
    set id eedit[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(w) $w
    set hd(mode) $mode
    set hd(oldfocus) [focus]

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(editors)

    label $w.lname -text $t(name): -anchor e
    entry $w.ename -textvariable ${id}(name)
    set b($w.ename) name_of_editor
    label $w.lcmd -text $t(command): -anchor e
    entry $w.ecmd -textvariable ${id}(cmd) -width 40
    set b($w.ecmd) editor_command
    label $w.lcharset -text $t(charset): -anchor e
    menubutton $w.mcharset \
	    -textvariable ${id}(charset_name) \
	    -indicatoron 1 \
	    -relief raised \
	    -menu $w.mcharset.m
    set b($w.mcharset) editor_charset
    menu $w.mcharset.m -tearoff 0
    set width 4
    foreach c [concat system [lsort [encoding names]]] {
	if {[info exists charsetReverseMapping($c)]
	    && 0 < [string length $charsetReverseMapping($c)]} {
	    set name $charsetReverseMapping($c)
	} else {
	    set name $c
	}
	set hd(chname,$c) $name
	$w.mcharset.m add command -label $name -command \
		"set ${id}(charset_name) [list $hd(chname,$c)]; \
		 set ${id}(charset) $c"
	if {[string length $hd(chname,$c)] > $width} {
	    set width [string length $hd(chname,$c)]
	}
    }
    $w.mcharset configure -width $width
    FixMenu $w.mcharset.m
    OkButtons $w $t(ok) $t(cancel) "EditorsEditDone $id"
    grid $w.lname $w.ename -sticky we
    grid $w.lcmd $w.ecmd -sticky we
    grid $w.lcharset -sticky we
    grid $w.mcharset -column 1 -row 2 -sticky w
    grid $w.buttons - -sticky we -pady 5

    if {"edit" == $mode} {
	set hd(name) $arg1
	set hd(cmd) [lindex $editor($arg1) 0]
	set hd(charset) [lindex $editor($arg1) 1]
	if {![info exists hd(chname,$hd(charset))]} {
	    set hd(charset) system
	}
	set hd(changeproc) $arg2
	set hd(oldname) $arg1
	focus $w.ecmd
    } else {
	focus $w.ename
	set hd(charset) system
	set hd(addproc) $arg1
    }
    set hd(charset_name) $hd(chname,$hd(charset))

    Place $w editorsEdit
}

# EditorsEditDone --
# 
# Called when EditorsEdit window is done
#
# Arguments:
# handler - Handler describing the windo
# ok	  - Boolean indicating if ok was pressed

proc EditorsEditDone {handler ok} {
    upvar #0 $handler hd
    global editor editorsChanged b t

    if {$ok} {
	if {"" == $hd(name)} {
	    Popup $t(need_name) $hd(w)
	    return
	}
	if {"edit" != $hd(mode) && [info exists editor($hd(name))]} {
	    Popup $t(name_occupied) $hd(w)
	    return
	}
	if {![IsExecutable [lindex $hd(cmd) 0]]} {
	    Popup $t(illegal_file_spec) $hd(w)
	    return
	}
	set editor($hd(name)) [list $hd(cmd) $hd(charset)]
	if {"edit" == $hd(mode)} {
	    if {$hd(name) != $hd(oldname)} {
		eval $hd(changeproc) [list $hd(name)]
		unset editor($hd(oldname))
	    }
	} else {
	    eval $hd(addproc) [list $hd(name)]
	}
	incr editorsChanged
    }
    RecordPos $hd(w) editorsEdit
    foreach bn [array names b $hd(w).*] {unset b($bn)}
    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    unset hd
}

# ComposeEEditorPopulate --
#
# Populate the external editor menu
#
# Arguments:
# handler - Handler identifying the compose window
# args	  - Arguments provided by trace

proc ComposeEEditorPopulate {handler args} {
    upvar #0 $handler hd
    global editors

    $hd(eeditm) delete 0 end
    foreach e $editors {
	$hd(eeditm) add command -label $e \
		-command "ComposeEEdit $handler [list $e] ; \
			        set ${handler}(eeditor)(eeditor) [list $e]"
    }
}

# ComposeWrapCited --
#
# Wraps the cited message
#
# Arguments:
# handler - Handler identifying the compose window

proc ComposeWrapCited {handler} {
    upvar #0 $handler hd

    set s 1.0

    rat_edit::initUndo $hd(composeBody) 1.0 end
    while {[llength [set r [$hd(composeBody) tag nextrange Cited $s]]]} {
	set start [lindex $r 0]
	set end [lindex $r 1]
	set n [RatWrapCited [$hd(composeBody) get $start $end]]
	$hd(composeBody) delete $start $end
	$hd(composeBody) insert $start $n {noWrap Cited no_spell}
	set s [lindex $r 1]
    }
}

# UpdateComposeRole
#
# Updates variables depeneding on role
#
# Arguments:
# handler - Handler identifying the compose window

proc UpdateComposeRole {handler} {
    global t option
    upvar #0 $handler mh

    set role $mh(role)
    set mh(request_dsn) $option($role,dsn_request)
    foreach v {from reply_to bcc} {
	set mh($v) $option($role,$v)
    }
    set gen [RatGenerateAddresses $handler]
    set mh(from) [lindex $gen 0]
    set mh(sender) [lindex $gen 1]

    ComposeUpdateHeaderEntries $handler

    wm title $mh(toplevel) "$t(compose_name) ($option($mh(role),name))"

    if {1 >= [llength [split $mh(from) .]]
        && 0 == $option(force_send)} {
	set state disabled
	Popup $t(no_send_bad_host) $mh(toplevel)
    } else {
	set state normal
    }
    foreach sb $mh(sendButtons) {
	$sb configure -state $state
    }

    if {$mh(role_sig)
        && ![file isdirectory $option($role,signature)]
        && [file readable $option($role,signature)]} {
	set fh [open $option($role,signature) r]
	set sigtext [read -nonewline $fh]
	close $fh
	
	set insert [$mh(composeBody) index insert]
	if {{} != [$mh(composeBody) tag ranges sig]} {
	    $mh(composeBody) delete sig.first sig.last
	} elseif {$option(sigdelimit)} {
	    $mh(composeBody) insert end "\n" {} "-- " {noWrap no_spell}
	}
	$mh(composeBody) insert end "\n$sigtext" {noWrap sig}
	$mh(composeBody) mark set insert $insert
    }
}
