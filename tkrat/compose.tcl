# compose.tcl --
#
# This file contains the code which handles the composing of messages
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# List of compsoe windows
set composeWindowList {}

# List of header names we know of
set composeHeaderList {to subject cc bcc reply_to from}

# List of headers which are not editable
set composeAutoHeaderList {in_reply_to references date}

# List of headers which contains addresses (OBS! must be lower case OBS!)
set composeAdrHdrList {from to cc bcc reply_to}

# Fields allowed in mailto links
set mailtoAllowedHdrs {to cc bcc reply_to subject in_reply_to references body}

# Address book icon
set book_img [image create photo -data {
R0lGODdhEgAMAKUAANDU0KisqGBkYHB0cPj4+ICEgIiIiHh8eNDQ0MjMyJCQkIiMiLi4uLC0
sPj8+GBgYOjs6KCkoDg4OHBwcJiYmJCUkMjIyMDEwICAgKioqHh4eFBQUEhMSODg4PD08Jic
mKCgoLCwsNjY2FhcWDAwMP//////////////////////////////////////////////////
/////////////////////////////////////////////////////////ywAAAAAEgAMAAAG
f0CH0FHRjDQQh+bxAA2HA8kTQHqSpENBwUHqOjBcEuRgeA4CnqFnMewUMmaQp0vylLudwyee
FtrbenEEaltCeXtDEwwBGRUBHSEfChUUByFPBQYRFwgIBxsJAAgRAgpPFFUeHR2oIggdDgpY
QhwgIBkNFhEUCQkNFBSmT8PET0EAOw==}]

# ComposeLoad --
#
# Dummy proc used to force the loading of this file
#
# Arguments:

proc ComposeLoad {} {
}

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
# msg-		Message to reply to
# to -		Whom the reply should be sent to, 'sender' or 'all'.
# role -        The role to do the composing as
# notify -      Command to run when message has been sent

proc ComposeReply {msg to role notify} {
    global t option
    set msg [ComposeChoose $msg $t(reply_to_which)]
    if {![string length $msg]} {
	return 0
    }
    set handler [$msg reply $to $role]
    upvar \#0 $handler mh
    set mh(notify) $notify
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
    global idCnt option t rat_tmp
    set handler composeM[incr idCnt]
    upvar \#0 $handler mh

    set msg [ComposeChoose $msg $t(forward_which)]
    if {![string length $msg]} {
	return 0
    }

    foreach header [$msg headers] {
	set name [string tolower [lindex $header 0]]
	switch $name {
	subject		    { set mh(subject) "Fwd: [lindex $header 1]" }
	content_description { set hd(content_description) [lindex $header 1] }
	}
	if { -1 != [lsearch [string tolower $option(show_header_selection)] \
		$name]} {
	    set name [string map {- _} $name]
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
    upvar \#0 $bhandler bh
    set mh(body) $bhandler
    set bh(type) text
    set bh(subtype) plain
    if {[string length $inline]} {
	set bh(encoding) [$inline encoding]
	set bh(parameter) [$inline params]
	set bh(content_id) [$inline id]
	set bh(content_description) [$inline description]
	set preface "\n\n$option(forwarded_message)\n"
	set length 5
	foreach f $option(show_header_selection) {
	    if { $length < [string length $f]} {
		set length [string length $f]
	    }
	}
	foreach field $option(show_header_selection) {
	    set f [string map {- _} [string tolower $field]]
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
	upvar \#0 $chandler ch
	lappend bh(children) $chandler

	set type [string tolower [$child type]]
	set ch(type) [lindex $type 0]
	set ch(subtype) [lindex $type 1]
	set ch(encoding) [$child encoding]
	set ch(parameter) [$child params]
	set ch(disp_type) [$child disp_type]
	set ch(disp_parm) [$child disp_parm]
	set ch(content_id) [$child id]
	set ch(content_description) [$child description]
	if {![info exists mh(data)] && ![string compare text $ch(type)]} {
	    set mh(data) [$child data 0]
	    set mh(data_tags) "Cited noWrap no_spell"
	} else {
	    set ch(filename) $rat_tmp/rat.[RatGenId]
	    set fh [open $ch(filename) w]
	    $child saveData $fh 1 0
	    close $fh
	    set ch(removeFile) 1
	    lappend mh(attachmentList) $chandler
            set ch(size) [file size $ch(filename)]
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
    global option t idCnt rat_tmp

    set msg [ComposeChoose $msg $t(forward_which)]
    if {![string length $msg]} {
	return 0
    }

    set handler composeM[incr idCnt]
    upvar \#0 $handler mh

    #
    # Attach old message
    #
    set id compose[incr idCnt]
    upvar \#0 $id hd

    set hd(content_description) $t(forwarded_message)
    foreach header [$msg headers] {
	switch [string tolower [lindex $header 0]] {
	subject		    { set mh(subject) "Fwd: [lindex $header 1]" }
	content_description { set hd(content_description) [lindex $header 1] }
	}
    }
    set hd(filename) $rat_tmp/rat.[RatGenId]
    set fh [open $hd(filename) w]
    fconfigure $fh -translation binary 
    set raw [string map {"\r\n" "\n"}  [$msg rawText]]
    puts -nonewline $fh $raw
    close $fh
    set hd(type) message
    set hd(subtype) rfc822
    set hd(removeFile) 1
    set hd(size) [file size $hd(filename)]
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
    global t idCnt

    set msg [ComposeChoose $msg $t(bounce_which)]
    if {"" == $msg} {
        return
    }

    set handler composeM[incr idCnt]
    upvar \#0 $handler mh
    set mh(msgs) $msg
    return [DoBounce $handler $role]
}

# ComposeContinue --
#
# Continue to compose a message
#
# Arguments:
# msg - Message to continue composing

proc ComposeContinue {msg} {
    global idCnt option t rat_tmp
    set handler composeM[incr idCnt]
    upvar \#0 $handler mh

    set role $option(default_role)
    set mh(data_tags) ""
    set mh(other_tags) ""
    set replacements {}
    foreach header [$msg headers] {
	set name [string tolower [lindex $header 0]]
        if {[regexp -nocase {x-tkrat-original-([a-z]+)} $name unused f]} {
            lappend replacements [list $f [lindex $header 1]]
            continue
        }
        # Ignore address headers
        if {-1 != [lsearch -exact {to from cc bcc} $name]} {
            continue
        }
	switch $name {
	    x-tkrat-internal-tags {
		set mh(other_tags) [lindex $header 1]
	    }
	    x-tkrat-internal-role {
		set role [lindex $header 1]
	    }
	    x-tkrat-internal-pgpactions {
		set a [lindex $header 1]
                set mh(pgp_sign) [lindex $a 0]
                set mh(pgp_encrypt) [lindex $a 1]
                set mh(pgp_sign_explicit) 1
	    }
	    x-tkrat-internal-bcc {
		set mh(bcc) [lindex $header 1]
	    }
	    default {
		set mh($name) [lindex $header 1]
	    }
	}
    }
    foreach r $replacements {
        set mh([lindex $r 0]) [lindex $r 1]
    }

    # Find if there is a bodypart which we can inline (a text part)
    set body [$msg body]
    set type [string tolower [$body type]]
    set inline {}
    set attach $body
    if { ("text" == [lindex $type 0] && "plain" == [lindex $type 1])
	 || ("message" == [lindex $type 0] && "rfc822" == [lindex $type 1])} {
	set inline $body
	set attach {}
    } elseif {"multipart" == [lindex $type 0]} {
	set children [$body children]
	if {0 < [llength $children]} {
	    set type [string tolower [[lindex $children 0] type]]
	    if {"text" == [lindex $type 0] && "plain" == [lindex $type 1]} {
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
    upvar \#0 $bhandler bh
    set mh(body) $bhandler
    set body [$msg body]
    set type [string tolower [$body type]]
    set bh(type) [lindex $type 0]
    set bh(subtype) [lindex $type 1]
    if {"multipart" == $bh(type) && 0 < [llength [$body children]]} {
	set mh(data) [[lindex [$body children] 0] data 0]
	foreach child [lrange [$body children] 1 end] {
	    set chandler composeC[incr idCnt]
	    upvar \#0 $chandler ch
	    lappend bh(children) $chandler

	    set type [string tolower [$child type]]
	    set ch(type) [lindex $type 0]
	    set ch(subtype) [lindex $type 1]
	    set ch(encoding) [$child encoding]
	    set ch(parameter) [$child params]
	    set ch(disp_type) [$child disp_type]
	    set ch(disp_parm) [$child disp_parm]
	    set ch(content_id) [$child id]
	    set ch(content_description) [$child description]
	    if {![info exists mh(data)] && ![string compare text $ch(type)]} {
		set mh(data) [$child data 0]
	    } else {
		set ch(filename) $rat_tmp/rat.[RatGenId]
		set fh [open $ch(filename) w]
		$child saveData $fh 1 0
		close $fh
		set ch(removeFile) 1
                set ch(size) [file size $ch(filename)]
		lappend mh(attachmentList) $chandler
	    }
	}
    } else {
	set mh(data) [$body data 0]
    }
    

    return [DoCompose $handler $role 0 0]
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
    upvar \#0 $handler mh
    if {[llength $hl]} {
	array set mh $hl
    }
    return [DoCompose $handler $option(default_role) 0 1]
}

# MailtoClient --
#
# Executes the mailto command from the client
#
# Arguments:
# m     - mailto link

proc MailtoClient {mailto} {
    global idCnt option t composeAdrHdrList mailtoAllowedHdrs

    regsub "^mailto:" $mailto "" mailto

    set handler clientM[incr idCnt]
    upvar \#0 $handler mh

    set s [split $mailto "?"]
    if {2 < [llength $s]} {
        Popup "$t(bad_mailto_url): $mailto"
        return
    }

    # The initial implied to field
    if {"" != [lindex $s 0]} {
        set mh(to) [RatDecodeUrlc [lindex $s 0] 1]
    }

    # The rest of the fields
    foreach f [split [lindex $s 1] "&"] {
        set l [split $f "="]
        if {2 != [llength $l]} {
            Popup "$t(bad_mailto_url): $mailto"
            return
        }
        regsub -all -- {-} [string tolower [lindex $l 0]] "_" field
        set addr [expr -1 == [lsearch -exact $composeAdrHdrList $field]]
        if {-1 == [lsearch -exact $mailtoAllowedHdrs $field]} {
            Popup "$t(bad_mailto_url): $mailto"
            return
        }

        if {"body" == $field} {
            set field data
            set mh(data_tags) {}
        }
        set mh($field) [RatDecodeUrlc [lindex $l 1] $addr]
    }

    return [DoCompose $handler $option(default_role) 0 1]
}

# ForwardGroupSeparately --
#
# Forwards a group of messages as separate emails
#
# Arguments:
# msgs - List of messages to forward
# role - The role to do the composing as

proc ForwardGroupSeparately {msgs role} {
    global t idCnt

    if {0 == [llength $msgs]} {
	return 0
    }

    set handler composeM[incr idCnt]
    upvar \#0 $handler mh
    set mh(subject) $t(forwarded_message)
    set mh(special) forward_group
    set mh(msgs) $msgs
    return [DoCompose $handler $role 0 1]
}

# ForwardGroupInOne --
#
# Forwards a group of messages as attachments in one email
#
# Arguments:
# msgs - List of messages to forward
# role - The role to do the composing as

proc ForwardGroupInOne {msgs role} {
    global t idCnt rat_tmp

    if {0 == [llength $msgs]} {
	return 0
    }

    set handler composeM[incr idCnt]
    upvar \#0 $handler mh
    set mh(subject) $t(forwarded_messages)

    #
    # Attach messages
    #
    foreach msg $msgs {
        set id compose[incr idCnt]
        upvar \#0 $id hd

        set hd(content_description) $t(forwarded_message)
        foreach header [$msg headers] {
            switch [string tolower [lindex $header 0]] {
                content_description {
                    set hd(content_description) [lindex $header 1]
                }
            }
        }
        set hd(filename) $rat_tmp/rat.[RatGenId]
        set fh [open $hd(filename) w]
        fconfigure $fh -translation binary 
        set raw [string map {"\r\n" "\n"}  [$msg rawText]]
        puts -nonewline $fh $raw
        close $fh
        set hd(type) message
        set hd(subtype) rfc822
        set hd(removeFile) 1
        set hd(size) [file size $hd(filename)]
        lappend mh(attachmentList) $id
    }

    return [DoCompose $handler $role 0 1]
}

# BounceMessages --
#
# Bounces all the specified messages
#
# Arguments:
# msgs - List of messages to forward
# role - The role to do the composing as

proc BounceMessages {msgs role} {
    global t idCnt

    if {0 == [llength $msgs]} {
	return 0
    }

    set handler composeM[incr idCnt]
    upvar \#0 $handler mh
    set mh(special) bounce_group
    set mh(msgs) $msgs
    return [DoBounce $handler $role]
}

# ComposeInit --
#
# Initialize a number of compose variables
#
# Arguments:
# handler   -	The handler for the active compose session
# role -        The role to do the composing as

proc ComposeInit {handler role} {
    global composeHeaderList option
    upvar \#0 $handler mh

    foreach i $composeHeaderList {
	set mh(O_$i) 0
    }
    foreach adr {from reply_to to cc bcc} {
	if {![info exists mh($adr)]} {
	    set mh($adr) {}
	}
    }
    foreach f {from reply_to bcc role} {
        set mh(orig,$f) ""
    }

    set mh(role) $role
    set mh(save_to) ""
    set mh(closing) 0
    set mh(role_sig) 0
    if {![info exists mh(pgp_sign)]} {
	set mh(pgp_sign) $option($role,sign_outgoing)
        set mh(pgp_sign_explicit) 0
	set mh(pgp_encrypt) $option(pgp_encrypt)
    }
    set mh(pgp_signer) $option($mh(role),sign_as)
    set mh(final_backup_done) 0
    if {![info exists mh(special)]} {
	set mh(special) none
    }
    if {![info exists mh(charset)]} {
	set mh(charset) auto
    }
}

# DoCompose --
#
# Actually do the composition. This involves building a window in which
# the user may do a lot of things.
#
# Arguments:
# handler   -	The handler for the active compose session
# role -        The role to do the composing as
# edit_text -	'1' if we should place the cursor in the text field.
#               '-1' if we should place the cursor at the top of the text field
# add_sig   -   '1' if we should add the signature

proc DoCompose {handler role edit_text add_sig} {
    global option t b composeHeaderList composeWindowList defaultFontWidth \
	   tk_strictMotif env charsetName editors fixedItalicFont
    upvar \#0 $handler mh

    # Initialize variables
    ComposeInit $handler $role
    if {![info exists editors]} {
	EditorsRead
    }
    set vars [string map {- _} [string tolower $option(compose_headers)]]
    foreach i $vars {
	set mh(O_$i) 1
    }

    set mh(send_handler) ComposeSend
    set mh(window_id) compose
    set mh(redo) 0
    set mh(do_wrap) $option(do_wrap)
    set mh(eeditor) $option(eeditor)
    set mh(mark_nowrap) $option(mark_nowrap)
    set mh(autospell) $option(autospell)
    set mh(dict) $option(def_spell_dict)

    # Create window
    set w .$handler
    set mh(toplevel) $w
    set mh(title) $t(compose_name)
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
    $m add command -label $t(store_snapshot) \
	    -command "ComposeStoreSnapshot $handler"
    $m add separator
    $m add command -label $t(abort) \
	    -command "DoComposeCleanup $w $handler backup"
    set b($m,[$m index end]) abort_compose
    set mh(abort_menu) [list $m [$m index end]]
    lappend mh(eEditBlock) $w.menu.file

    set m $w.menu.edit.m
    menubutton $w.menu.edit -menu $m -text $t(edit) -underline $a(edit)
    menu $m -postcommand "ComposePostEdit $handler $m" -tearoff 1
    $m add command -label $t(undo) \
        -command "event generate $w.body.text <<RatUndo>>"
    set b($m,[$m index end]) undo
    set mh(undo_menu) [list $m [$m index end]]
    $m add command -label $t(redo) \
        -command "event generate $w.body.text <<RatRedo>>"
    set b($m,[$m index end]) redo
    set mh(redo_menu) [list $m [$m index end]]
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
    $m add checkbutton -label $t(show_addr_history) \
        -variable option(show_autocomplete) -command SaveOptions
    set b($m,[$m index end]) show_addr_history
    $m add separator
    $m add checkbutton -label $t(automatic_wrap) \
        -variable ${handler}(do_wrap) \
        -command [list ComposeSetWrap $handler]
    set b($m,[$m index end]) automatic_wrap
    $m add command -label $t(wrap_paragraph) \
	    -command "event generate $w.body.text <<Wrap>>"
    set b($m,[$m index end]) wrap_paragraph
    set mh(wrap_menu) [list $m [$m index end]]
    $m add command -label $t(do_wrap_cited) \
	    -command "ComposeWrapCited $handler"
    set b($m,[$m index end]) do_wrap_cited
    $m add checkbutton -label $t(underline_nonwrap) \
	    -variable ${handler}(mark_nowrap) \
            -command "ComposeSetMarkWrap $handler"
    set b($m,[$m index end]) mark_nowrap
    $m add separator
    $m add command -label $t(check_spelling)... \
	    -command "rat_ispell::CheckTextWidget $w.body.text"
    set b($m,[$m index end]) do_check_spelling
    $m add checkbutton -label $t(mark_misspellings) \
        -variable ${handler}(autospell) -onvalue 1 -offvalue 0 \
        -command "MarkMisspellings $handler"
    set mh(autospell_menu) [list $m [$m index end]]
    $m add cascade -label $t(language) -menu $m.lang
    menu $m.lang -postcommand "BuildComposeLang $handler $m.lang"
    set mh(language_menu) [list $m [$m index end]]
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
	$m add command -label "$t(pgp_details)..." \
            -command "PGPDetails $handler"
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
	    $w.body.text insert end "\n$sigtext" {noWrap no_spell sig}
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
        "CheckDeatchStatus $handler $w.attach.b.detach"
    bind $w.attach.list.list <KeyRelease> \
        "CheckDeatchStatus $handler $w.attach.b.detach"
    pack $w.attach.list.scroll -side right -fill y
    pack $w.attach.list.list -side left -expand 1 -fill both
    pack $w.attach.b \
	 $w.attach.list -side top -fill x
    set mh(attachmentListWindow) $w.attach.list.list
    if {"forward_group" == $mh(special)} {
        $mh(attachmentListWindow) insert end \
            "-- $t(forwarded_msg_goes_here) --"
    }
    if {![info exists mh(attachmentList)]} {
	set mh(attachmentList) {}
    } else {
	foreach attachment $mh(attachmentList) {
	    upvar \#0 $attachment bp
	    if { [info exists bp(content_description)]
                 && "" != $bp(content_description) } {
		set desc $bp(content_description)
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
            if {[info exists bp(size)]} {
                set size " ([RatMangleNumber $bp(size)])"
            } else {
                set size ""
            }
	    $mh(attachmentListWindow) insert end \
		    "$bp(type)/$bp(subtype) : $desc$size"
	}
    }

    # Buttons
    frame $w.buttons
    button $w.buttons.send -text $t(send) -command "ComposeSend $w $handler"
    set b($w.buttons.send) send
    lappend mh(eEditBlock) $w.buttons.send
    menubutton $w.buttons.sendsave -text $t(send_save) -indicatoron 1 \
	    -menu $w.buttons.sendsave.m -relief raised -underline 0
    set b($w.buttons.sendsave) sendsave
    menu $w.buttons.sendsave.m -tearoff 0 -postcommand \
	    "RatSendSavePostMenu $w $w.buttons.sendsave.m $handler"
    lappend mh(eEditBlock) $w.buttons.sendsave
    button $w.buttons.hold -text $t(postpone) \
        -command "ComposeHold $w $handler"
    set b($w.buttons.hold) postpone
    lappend mh(eEditBlock) $w.buttons.hold
    menubutton $w.buttons.edit -indicatoron 1 -menu $w.buttons.edit.m \
	    -relief raised -direction flush -textvariable ${handler}(eeditor)
    set mh(eeditb) $w.buttons.edit
    menu $w.buttons.edit.m -tearoff 0
    set mh(eeditm) $w.buttons.edit.m
    ComposeEEditorPopulate $handler
    trace variable editors w "ComposeEEditorPopulate $handler"
    set b($w.buttons.edit) eedit
    lappend mh(eEditBlock) $w.buttons.abort
    button $w.buttons.abort -text $t(abort) -command \
	    "DoComposeCleanup $w $handler backup"
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
    ::tkrat::winctl::SetGeometry compose $w $w.body.text
    lappend composeWindowList $handler
    ComposeBind $handler
    wm protocol $w WM_DELETE_WINDOW "DoComposeCleanup $w $handler backup"

    if { 1 == $option(always_editor)} {
        if {0 == [llength $editors]} {
            Popup $t(always_eeditor_but_none) $mh(toplevel)
        } else {
            ComposeEEdit $handler [lindex $editors 0]
        }
    }

    UpdateComposeRole $handler
    MarkMisspellings $handler

    if {$option(compose_backup) > 0} {
        set mh(next_backup) [after [expr $option(compose_backup)*1000] \
                                 [list ComposeStoreBackup $handler 1]]
    }

    return $handler
}
proc DoComposeCleanup {w handler backup} {
    global composeWindowList b editors folderWindowList
    upvar \#0 $handler mh

    # Are we already doing this?
    if {0 == [info exists mh(closing)] || 1 == $mh(closing)} {
	return
    }
    set mh(closing) 1

    if {$backup != "noback"} {
        ComposeDoFinalBackup $handler
    }

    if {[info exists mh(body)]} {
	ComposeFreeBody $mh(body)
    }
    foreach a $mh(attachmentList) {
        upvar \#0 $a ah
        if {[info exists ah]} {
            if {[info exists ah(removeFile)] && $ah(removeFile)} {
                catch {file delete -- $ah(filename)}
            }
            unset ah
        }
    }
    set index [lsearch $composeWindowList $handler]
    set composeWindowList [lreplace $composeWindowList $index $index]
    if {[winfo exists $w]} {
        if {$mh(window_id) == "compose"} {
            if {[info exists mh(composeBody)]} {
                ::tkrat::winctl::RecordGeometry compose $w $mh(composeBody)
            } else {
                ::tkrat::winctl::RecordGeometry compose $w
            }
        } elseif {$mh(window_id) == "bounce"} {
            ::tkrat::winctl::RecordGeometry bounce $w
        }
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

# MarkMisspellings --
#
# Enable/disable marking of misspelled words
#
# Aruments:
# handler -	The handler which identifies the context

proc MarkMisspellings {handler} {
    upvar \#0 $handler mh
    global option

    set option(autospell) $mh(autospell)
    SaveOptions

    if {$mh(autospell)} {
        rat_textspell::init $mh(composeBody) $mh(dict)
        set lang_state normal
    } else {
        rat_textspell::uninit $mh(composeBody)
        set lang_state disabled
    }
    [lindex $mh(language_menu) 0] entryconfigure \
        [lindex $mh(language_menu) 1] -state $lang_state
}

# BuildComposeLang --
#
# Build the compose language menu
#
# Arguments:
# handler -	The handler which identifies the context
# m       -     Nam eof the menu to build

proc BuildComposeLang {handler m} {
    upvar \#0 $handler mh
    global t

    $m configure -postcommand ""
    foreach l [concat auto [rat_textspell::get_dicts]] {
        if {"auto" == $l} {
            set label $t(auto)
        } else {
            set label [string totitle $l]
        }
        $m add radiobutton -label $label -variable ${handler}(dict) -value $l \
            -command [list rat_textspell::set_dict $mh(composeBody) $l]
    }
    FixMenu $m
}

# CheckDetachStatus --
#
# Update status of detach button
#
# Arguments:
# handler -	The handler which identifies the context
# detach  -     The detach button

proc CheckDeatchStatus {handler detach} {
    upvar \#0 $handler mh

    set state disabled
    if {0 < [llength [$mh(attachmentListWindow) curselection]]} {
        set state normal
    }
    if {"forward_group" == $mh(special)
        && 1 == [$mh(attachmentListWindow) selection includes 0]} {
        set state disabled
    }
    $detach configure -state $state
}

# ComposeBind --
#
# Bind keyboard shortcuts for the compose window
#
# Arguments:
# handler -	The handler which identifies the context

proc ComposeBind {handler} {
    upvar \#0 $handler mh

    set wins $mh(toplevel)
    if {[info exists mh(composeBody)]} {
        lappend wins $mh(composeBody)
	RatBindMenu $mh(composeBody) compose_key_cut $mh(cut_menu)
	RatBindMenu $mh(composeBody) compose_key_copy $mh(copy_menu)
	RatBindMenu $mh(composeBody) compose_key_wrap $mh(wrap_menu)
	RatBindMenu $mh(composeBody) compose_key_cut_all $mh(cut_all_menu)
	RatBindMenu $mh(composeBody) compose_key_paste $mh(paste_menu)
	RatBindMenu $mh(composeBody) compose_key_undo $mh(undo_menu)
	RatBindMenu $mh(composeBody) compose_key_redo $mh(redo_menu)
    }
    foreach w $wins {
	RatBindMenu $w compose_key_abort $mh(abort_menu)
	RatBind $w compose_key_send \
		"ComposeSend $mh(toplevel) $handler; break"
	RatBind $w compose_key_editor \
		"ComposeEEdit $handler \[lindex \$editors 0\]"
    }
}


# DoBounce --
#
# Actually do the bouncing. This involves building a window in which
# the user may specify recipients
#
# Arguments:
# handler   -	The handler for the active compose session
# role -        The role to do the composing as

proc DoBounce {handler role} {
    global option t b composeHeaderList composeWindowList defaultFontWidth \
	   tk_strictMotif env charsetName editors fixedItalicFont
    upvar \#0 $handler mh

    # Initialize variables
    ComposeInit $handler $role

    foreach h {{to 1} {cc 1} {bcc 0}} {
        lappend headerList [lindex $h 0]
        set mh(O_[lindex $h 0]) [lindex $h 1]
    }
    set mh(send_handler) ComposeBounceSend
    set mh(window_id) bounce

    set mh(attachmentList) {}

    # Create window
    set w .$handler
    set mh(toplevel) $w
    set mh(title) $t(bounce_name)
    toplevel $w -class TkRat
    wm iconname $w $t(bounce_name)

    # Menus
    FindAccelerators a {file role headers}

    frame $w.menu -relief raised -bd 1
    set m $w.menu.file.m
    menubutton $w.menu.file -menu $m -text $t(file) -underline $a(file)
    menu $m -tearoff 1
    $m add command -label $t(abort) \
	    -command "DoComposeCleanup $w $handler noback"
    set b($m,[$m index end]) abort_compose
    set mh(abort_menu) [list $m [$m index end]]

    set m $w.menu.role.m
    menubutton $w.menu.role -menu $m -text $t(role) -underline $a(role)
    menu $m -postcommand \
	    [list PostRoles $handler $m [list UpdateComposeRole $handler]]

    menubutton $w.menu.headers -menu $w.menu.headers.m -text $t(headers) \
			       -underline $a(headers)
    set b($w.menu.headers) headers_menu
    menu $w.menu.headers.m -tearoff 1
    foreach header $headerList {
	$w.menu.headers.m add checkbutton -label $t($header) \
		-variable ${handler}(O_$header) \
		-onvalue 1 -offvalue 0 \
		-command "ComposeBuildHeaderEntries $handler"
    }

    pack $w.menu.file \
	 $w.menu.role \
	 $w.menu.headers -side left -padx 5

    # Header fields
    set mh(headerFrame) $w.h
    frame $mh(headerFrame)

    # Buttons
    frame $w.buttons
    button $w.buttons.send -text $t(send) \
        -command "ComposeBounceSend $w $handler"
    set b($w.buttons.send) send
    menubutton $w.buttons.sendsave -text $t(send_save) -indicatoron 1 \
	    -menu $w.buttons.sendsave.m -relief raised -underline 0
    set b($w.buttons.sendsave) sendsave
    menu $w.buttons.sendsave.m -tearoff 0 -postcommand \
	    "RatSendSavePostMenu $w $w.buttons.sendsave.m $handler"
    button $w.buttons.abort -text $t(abort) -command \
	    "DoComposeCleanup $w $handler noback"
    set b($w.buttons.abort) abort_compose
    pack $w.buttons.send \
	 $w.buttons.sendsave -side left -padx 5
    pack $w.buttons.abort -side right -padx 5
    lappend mh(sendButtons) $w.buttons.send
    lappend mh(sendButtons) $w.buttons.sendsave

    bind $w <Escape> "$w.buttons.abort invoke"

    # Populate headerlist and pack everything
    set first [ComposeBuildHeaderEntries $handler]
    pack $w.menu -side top -fill x
    pack $mh(headerFrame) -side top -fill x -padx 5 -pady 5
    pack $w.buttons -side bottom -fill x -padx 5 -pady 5

    set mh(oldfocus) [focus]
    if {[string length $first]} {
	focus $first
    }
    ::tkrat::winctl::SetGeometry bounce $w
    lappend composeWindowList $handler
    ComposeBind $handler

    UpdateComposeRole $handler

    return $handler
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
    FixMenu $m
}
proc RatSendSaveDo {w handler save_to} {
    upvar \#0 $handler hd

    if {"" == $save_to} {
	return
    }
    if {1 == [llength $save_to]} {
	global vFolderDef
	set hd(save_to) $vFolderDef($save_to)
    } else {
	set hd(save_to) $save_to
    }
    $hd(send_handler) $w $handler
}


# ComposeBuildHeaderEntries --
#
# Builds a list of header entries and packs them into the appropriate frame
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeBuildHeaderEntries {handler} {
    global composeHeaderList composeAdrHdrList t b
    upvar \#0 $handler mh

    set oldfocus [focus]
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

    catch {focus $oldfocus}
    return $first
}

# ComposeUpdateHeaderEntries --
#
# Updates all the header-entries from the variables
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeUpdateHeaderEntries {handler} {
    upvar \#0 $handler mh

    foreach hh $mh(headerHandles) {
	set w [lindex $hh 0]
	set hd [lindex $hh 1]
	upvar \#0 $hd hdr
	upvar \#0 $hdr(varname) var

	$w delete 1.0 end
	$w insert end $var
	ComposeHandleHE $w $hd
    }
}

# ComposeSend --
#
# Actually send a message
#
# Arguments:
# mainW   -	The main compose window
# handler -	The handler for the active compose session

proc ComposeSend {mainW handler} {
    global t composeAdrHdrList vFolderDef vFolderOutgoing option \
	folderWindowList idCnt rat_tmp
    upvar \#0 $handler mh

    # Update all header entries
    foreach hh $mh(headerHandles) {
	set w [lindex $hh 0]
	set hhd [lindex $hh 1]
	ComposeHandleHE $w $hhd
    }

    # Check that we have at least one recipient
    if { 0 == [string length "$mh(to)$mh(cc)$mh(bcc)"]} {
	Popup $t(need_to) $mh(toplevel)
	return
    }

    # Extract potential pgp keys to use
    foreach e {to cc} {
	if {![catch {RatAlias expand pgp $mh($e) $mh(role)} out]} {
	    set mh_pgp($e) $out
	} else {
	    set mh_pgp($e) ""
	}
    }

    # Alias expansion and syntax error checking
    set err {}
    foreach e $composeAdrHdrList {
	if {[info exists mh($e)] && [string length $mh($e)]} {
	    if {![catch {RatAlias expand sending $mh($e) $mh(role)} out]} {
                AddrListAdd $mh($e)
		set mh($e) $out
	    } else {
		lappend err $t($e)
	    }
	}
    }
    SaveAddrList
    if {0 < [llength $err]} {
	Popup "$t(adr_syntax_error): $err" $mh(toplevel)
	return
    }

    # Check if a save folder is defined
    if {![string length $mh(save_to)]
        && "" != $option($mh(role),save_outgoing)
        && [info exists vFolderDef($option($mh(role),save_outgoing))]} {
	set mh(save_to) $vFolderDef($option($mh(role),save_outgoing))
    }

    # Prepare pgp recipients
    if {$mh(pgp_signer) == ""} {
	set mh(pgp_signer) [RatExtractAddresses $mh(role) $mh(from)]
    }
    set mh(pgp_rcpts) {}
    foreach p [concat $mh_pgp(to) $mh_pgp(cc)] {
        lappend mh(pgp_rcpts) [lindex $p 0]
    }

    # Generate date header
    set mh(date) [RatGenerateDate]

    set fh [RatOpenFolder $vFolderDef($vFolderOutgoing)]
    if {"forward_group" == $mh(special)} {
        foreach a $mh(msgs) {
            set id compose[incr idCnt]
            upvar \#0 $id hd

            set hd(content_description) $t(forwarded_message)
            foreach header [$a headers] {
                switch -- [string tolower [lindex $header 0]] {
                    content_description	{
                        set hd(content_description) [lindex $header 1]
                    }
                }
            }
            set hd(filename) $rat_tmp/rat.[RatGenId]
            set fileh [open $hd(filename) w]
            fconfigure $fileh -translation binary 
            puts -nonewline $fileh [string map {"\r\n" "\n"}  [$a rawText]]
            close $fileh
            set hd(type) message
            set hd(subtype) rfc822
            set hd(removeFile) 1
            set oldAttachments $mh(attachmentList)
            lappend mh(attachmentList) $id

            set msg [ComposeCreateMsg $handler]
            if {$mh(pgp_sign) || $mh(pgp_encrypt)} {
                if {[catch {$msg pgp $mh(pgp_sign) $mh(pgp_encrypt) $mh(role) \
                                $mh(pgp_signer) $mh(pgp_rcpts)}]} {
                    $fh close
                    return
                }
            }
            $fh insert $msg
            rename $msg ""
            file delete $hd(filename)
            unset hd
            set mh(attachmentList) $oldAttachments
        }
    } else {
        # Create message and insert into outgoing queue
        set msg [ComposeCreateMsg $handler]
        if {$mh(pgp_sign) || $mh(pgp_encrypt)} {
            if {[catch {$msg pgp $mh(pgp_sign) $mh(pgp_encrypt) $mh(role) \
                            $mh(pgp_signer) $mh(pgp_rcpts)}]} {
                $fh close
                return
            }
        }

        $fh insert $msg
        rename $msg ""
    }
    foreach i [$fh flagged seen 0] {
        $fh setFlag $i seen 1
    }

    foreach h [array names folderWindowList] {
	if {$folderWindowList($h) == $fh} {
	    Sync $h update
	}
    }
    $fh close

    # Inform sender (if online)
    if {$option(online)} {
        RatNudgeSender
    }

    # Possibly inform folder window
    if {[info exists mh(notify)]} {
	eval $mh(notify)
    }

    # Get compose window to clean up
    DoComposeCleanup $mainW $handler backup
}

# ComposeBounceSend --
#
# Actually bounce a message
#
# Arguments:
# mainW   -	The main bounce window
# handler -	The handler for the active bounce session

proc ComposeBounceSend {mainW handler} {
    global t idCnt composeAdrHdrList option vFolderDef vFolderOutgoing
    upvar \#0 $handler mh

    # Update all header entries
    foreach hh $mh(headerHandles) {
	set w [lindex $hh 0]
	set hhd [lindex $hh 1]
	ComposeHandleHE $w $hhd
    }

    # Check that we have at least one recipient
    if { 0 == [string length "$mh(to)$mh(cc)$mh(bcc)"]} {
	Popup $t(need_to) $mh(toplevel)
	return
    }

    # Alias expansion and syntax error checking
    set err {}
    foreach e $composeAdrHdrList {
	if {[info exists mh($e)]} {
	    if {![catch {RatAlias expand sending $mh($e) $mh(role)} out]} {
                AddrListAdd $mh($e)
		set mh($e) $out
	    } else {
		lappend err $t($e)
	    }
	}
    }
    SaveAddrList
    if {0 < [llength $err]} {
	Popup "$t(adr_syntax_error): $err" $mh(toplevel)
	return
    }

    # Check if a save folder is defined
    if {![string length $mh(save_to)] \
	    && "" != $option($mh(role),save_outgoing)} {
	set mh(save_to) $vFolderDef($option($mh(role),save_outgoing))
    }

    set fh [RatOpenFolder $vFolderDef($vFolderOutgoing)]

    set good 0
    set fail 0
    foreach msg $mh(msgs) {
        if {0 == [llength [info commands $msg]]} {
            incr fail
            continue
        }
        incr good
        set envelope {}
        set mh(files) {}
        foreach h {to cc bcc} {
            if {"" != $mh($h)} {
                lappend envelope [list $h $mh($h)]
                regsub -all "\n" $mh($h) "\n    " value
                lappend envelope [list X-TkRat-Original-$h $value]
            }
        }
        foreach header [$msg headers] {
            set name [string map {- _} [string tolower [lindex $header 0]]]
            if {"subject" == $name
                || "from" == $name
                || "reply_to" == $name} {
                lappend envelope [list $name [lindex $header 1]]
            }
        }
        lappend envelope [list message_id [RatGenerateMsgId $mh(role)]]

        lappend envelope [list X-TkRat-Internal-Role $mh(role)]
        lappend envelope [list X-TkRat-Internal-PGPActions \
                              [list $mh(pgp_sign) $mh(pgp_encrypt)]]
        if {[string length $mh(save_to)]} {
            lappend envelope [list X-TkRat-Internal-Save-To $mh(save_to)]
        }

        set bmsg [RatCreateMessage $mh(role) \
                      [list $envelope \
                           [ComposeCreateBody ${handler}(files) [$msg body]]]]
        $fh insert $bmsg
        rename $bmsg ""
        foreach f $mh(files) {
            file delete -force $f
        }
    }
    if {$fail > 0} {
        if {$fail == 1 && $good == 0} {
            Popup $t(message_deleted)
        } else {
            Popup $t(messages_deleted)
        }
    }

    foreach i [$fh flagged seen 0] {
        $fh setFlag $i seen 1
    }

    foreach h [array names folderWindowList] {
	if {$folderWindowList($h) == $fh} {
	    Sync $h update
	}
    }
    $fh close

    # Inform sender (if online)
    if {$option(online)} {
        RatNudgeSender
    }

    # Possibly inform folder window
    if {[info exists mh(notify)]} {
	eval $mh(notify)
    }

    # Get compose window to clean up
    DoComposeCleanup $mainW $handler backup
}

# ComposeCreateBody --
#
# Create a body for for sending to ratCreateMessage
#
# Arguments:
# flist - Name of list which will contain names of used files
# body  - Body command

proc ComposeCreateBody {flist body} {
    global rat_tmp
    upvar \#0 $flist filelist

    set type [$body type]
    set desc [$body description]
    set header {}
    if {"" != $desc} {
        lappend header [list content_description $desc]
    }
    set ltype [string tolower [lindex $type 0]]
    if {"multipart" == $ltype} {
        set bodydata {}
        foreach c [$body children] {
            lappend bodydata [ComposeCreateBody $flist $c]
        }
    } elseif {"text" == $ltype} {
        set bodydata [list utfblob [$body data 0 utf-8]]
    } else {
        set filename $rat_tmp/rat.[RatGenId]
        set fh [open $filename w]
        $body saveData $fh 1 0
        close $fh
        set bodydata [list file $filename]
        lappend filelist $filename
    }
    return [list \
                [lindex $type 0] \
                [lindex $type 1] \
                [$body params] \
                [$body encoding] \
                [$body disp_type] \
                [$body disp_parm] \
                $header \
                $bodydata]
}

# CompseEEdit --
#
# Run an external editor on the bodypart
#
# Arguments:
# handler -	The handler for the active compose session
# e	  -	Id of external editor to use

proc ComposeEEdit {handler e} {
    upvar \#0 $handler mh
    global t idCnt editor charsetMapping rat_tmp

    if {[info exists mh(eedit_running)]} {
	return
    }

    set ehandler compose_E[incr idCnt]
    upvar \#0 $ehandler eh

    # Write data, change text visible and edit
    set ecmd [lindex $editor($e) 0]
    set charset [lindex $editor($e) 1]
    if {[info exists charsetMapping($charset)]} {
	set charset $charsetMapping($charset)
    }
    set fname $rat_tmp/rat.[RatGenId]
    set fh [open $fname w]
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
    upvar \#0 $handler mh
    upvar \#1 $name1 eh

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
    upvar \#0 $handler mh

    # Create identifier
    set id attach[incr idCnt]
    set w .$id
    upvar \#0 $id hd
    set hd(done) 0

    set hd(filename) [rat_fbox::run \
                          -title $t(attach_file) \
                          -ok $t(open) \
                          -initialdir $option(initialdir) \
                          -mode open \
                          -parent $mh(toplevel)]
    if {"" == $hd(filename)} {
	unset hd
	return
    }
    if {$option(initialdir) != [file dirname $hd(filename)]} {
        set option(initialdir) [file dirname $hd(filename)]
        SaveOptions
    }

    toplevel $w -class TkRat
    wm title $w $t(attach_file)
    wm transient $w $mh(toplevel)

    # Get default type $hd(filename)
    set type [RatType $hd(filename)]
    set hd(typestring) [lindex $type 0]
    set hd(encoding) [lindex $type 1]
    set hd(disp_fname) [file tail $hd(filename)]
    set hd(size) [file size $hd(filename)]

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
		   {application {octet-stream pdf postscript msword}}} {
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
    entry $w.desc.entry -width 65 -textvariable ${id}(content_description)
    pack $w.desc.label \
	 $w.desc.entry -side left
    set b($w.desc.entry) attach_description

    frame $w.id
    label $w.id.label -width 14 -anchor e -text $t(id):
    entry $w.id.entry -width 65 -textvariable ${id}(content_id)
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

    ::tkrat::winctl::SetGeometry attach2 $w
    ::tkrat::winctl::ModalGrab $w $w.desc.entry
    $w.desc.entry icursor 0
    tkwait variable ${id}(done)

    if { 1  == $hd(done) } {
	set type [split $hd(typestring) /]
	set hd(type) [lindex $type 0]
	set hd(subtype) [lindex $type 1]
	set hd(removeFile) 0
        if {"text" == $hd(type)} {
            set f [open $hd(filename) r]
            set bd [read $f 32768]
            close $f
            set charset [RatCheckEncodings bd $option(charset_candidates)]
            if {"" == $charset} {
                set charset $option(charset)
            }
            set hd(parameter) \{[list charset $charset]\}
        }
	if {[string length $hd(disp_fname)]} {
	    set hd(disp_parm) \{[list filename $hd(disp_fname)]\}
            if {[info exists hd(parameter)]} {
                lappend hd(parameter) [list name $hd(disp_fname)]
            } else {
                set hd(parameter) \{[list name $hd(disp_fname)]\}
            }
	}
	set hd(disp_type) attachment
	lappend mh(attachmentList) $id
	if { "" != $hd(content_description) } {
	    set desc $hd(content_description)
	} else {
	    set desc "$hd(typestring): $hd(disp_fname) ([RatMangleNumber $hd(size)])"
	}
	$mh(attachmentListWindow) insert end $desc
    } else {
	unset hd
    }

    ::tkrat::winctl::RecordGeometry attach2 $w
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
    upvar \#0 $variable var
    global t idCnt fixedNormFont

    set id subtype[incr idCnt]
    set w .$id
    upvar \#0 $id hd
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

    ::tkrat::winctl::SetGeometry subtypeSpec $w
    ::tkrat::winctl::ModalGrab $w $w.subtype.entry

    tkwait variable ${id}(done)

    if {1 == $hd(done)} {
	set var $type/$hd(spec)
    }

    ::tkrat::winctl::RecordGeometry subtypeSpec $w
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
    global idCnt t option rat_tmp
    upvar \#0 $handler mh

    foreach keyid $ids {
	# Create identifier
	set id attach[incr idCnt]
	upvar \#0 $id hd

	set hd(type) application
	set hd(subtype) pgp-keys
	set hd(encoding) 7bit
	set hd(content_description) \
		"$t(pgp_key) [lindex $keyid 0] $t(for) [lindex $keyid 1]"
	set hd(filename) $rat_tmp/[RatGenId]
	set hd(removeFile) 1

	set f [open $hd(filename) w]
	puts $f [RatPGP extract [lindex $keyid 0]]
	close $f
	lappend mh(attachmentList) $id
	$mh(attachmentListWindow) insert end $hd(content_description)
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
    upvar \#0 $handler mh
    $button configure -state disabled
    foreach element [lsort -integer -decreasing \
			    [$mh(attachmentListWindow) curselection]] {
	$mh(attachmentListWindow) delete $element
        if {"forward_group" == $mh(special)} {
            incr element -1
        }
	ComposeFreeBody [lindex $mh(attachmentList) $element]
	set mh(attachmentList) [lreplace $mh(attachmentList) $element $element]
    }
}

# ComposeFreeBody --
#
# Free a bodypart from memory and remove any temporary files associated with it
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeFreeBody {handler} {
    upvar \#0 $handler bh

    if {![info exists bh(type)]} {
	return
    }
    if { "multipart" == $bh(type)} {
	if {[info exists bh(children)]} {
	    foreach body $bh(children) {
		ComposeFreeBody $body
	    }
	}
    } 
    if {[info exists bh(removeFile)] && $bh(removeFile)} {
	catch {file delete -- $bh(filename)}
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
    upvar \#0 $handler mh
    global t vFolderDef vFolderHold

    # Update all header entries
    foreach hh $mh(headerHandles) {
	set w [lindex $hh 0]
	set hhd [lindex $hh 1]
	ComposeHandleHE $w $hhd
    }

    # Cancel any pending backups
    if {[info exists mh(next_backup)]} {
        catch {after cancel $mh(next_backup)}
    }

    # Create message and insert into hold
    set msg [ComposeCreateMsg $handler]
    set fh [RatOpenFolder $vFolderDef($vFolderHold)]
    $fh insert $msg
    rename $msg ""

    # Mark all messages in hold as read
    foreach i [$fh flagged seen 0] {
	$fh setFlag $i seen 1
    }
    $fh close
    
    # Get compose window to clean up
    DoComposeCleanup $mainW $handler noback
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
    upvar \#0 $id hd
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
		set n [string map {- _} $field]
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

    ::tkrat::winctl::SetGeometry composeChoose $w $w.l.canvas
    update idletasks
    set bbox [$w.l.canvas bbox $elemId]
    eval {$w.l.canvas configure -scrollregion $bbox}

    ::tkrat::winctl::ModalGrab $w
    tkwait variable ${id}(done)
    ::tkrat::winctl::RecordGeometry composeChoose $w $w.l.canvas
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
    upvar \#0 $textvariable textvar
    upvar \#0 $mhandler mh

    set handler compHE[incr idCnt]
    upvar \#0 $handler hd

    # Build windows
    frame $w
    text $w.t -relief sunken -yscroll "$w.s set" -width 40 -height 1 -wrap none
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
    set hd(autocomplete_list) {}
    set hd(autocomplete_start) {}

    # Do bindings
    bind $w <FocusIn> "focus $w.t"
    bind $w.t <Return> {focus [tk_focusNext %W]; break}
    bind $w.t <Tab> {focus [tk_focusNext %W]; break}
    bind $w.t <Shift-Tab> {focus [tk_focusPrev %W]; break}
    bind $w.t <$ISO_Left_Tab> {focus [tk_focusPrev %W]; break}
    bind $w.t <Shift-space> { }
    bind $w.t <KeyRelease-comma> "ComposeHandleHEComma %W $handler"
    bind $w.t <FocusOut> "after 1 ComposeHandleHEFocusOut %W $handler"
    bind $w.t <Destroy> "unset $handler"
    bind $w.t <<PasteSelection>> "ComposeHandleHEPaste %W $handler; break"
    bind $w.t <<Paste>> "ComposeHandleHEPaste %W $handler; break"
    bind $w.t <Control-l> "ComposeHandleHEAlias %W $handler"
    bind $w.t <Configure> "ComposeHandleHEConfigure %W $handler %w"

    AddrListInit $w.t

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
    upvar \#0 $handler hd

    if {![info exists hd(borders)]} {
	set hd(borders) [expr {2*([$w cget -borderwidth] \
		+[$w cget -highlightthickness])}]
    }
    set width [expr {($pixwidth-$hd(borders))/$defaultFontWidth}]
    if {$width == $hd(width) || $width < 1} {
	return
    }

    set hd(width) $width
    $w configure -tabs [expr {$defaultFontWidth*$hd(width)/2}]
    ComposeHandleHE $w $handler
}

# ComposeHandleHEFocusOut --
#
# Handle FocusOut events for the eheader entry. This routine checks if
# the actually still has focus and in that case does nothing.
#
# Arguments:
# w	  - The text widget
# handler - The handler which identifies this address widget

proc ComposeHandleHEFocusOut {w handler} {
    if {[winfo exists $w] && [focus] != $w && 0 == [AddrListClose $w 0]} {
        ComposeHandleHE $w $handler
    }
}

# ComposeHandleHEComma --
#
# Handle address separator events
#
# Arguments:
# w	  - The text widget
# handler - The handler which identifies this address widget

proc ComposeHandleHEComma {w handler} {
    AddrListClose $w 1
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
    upvar \#0 $handler hd
    upvar \#0 $hd(varname) var
    upvar \#0 $hd(mhandler) mh

    set sr [$w tag nextrange sel 1.0]
    if {[llength $sr]} {
	set sel [$w get [lindex $sr 0] [lindex $sr 1]]
    }
    set old [string trim [$w get 1.0 end]]

    $w delete 1.0 end
    set tempalist [RatSplitAdr $old]
    set alist {}
    set max 0
    set tot 0
    foreach adr $tempalist {
	if {[catch {RatAlias expand display $adr $mh(role)} adr2]} {
	    set tag($adr) error
	    lappend alist $adr
	} else {
	    set alist [concat $alist [RatSplitAdr $adr2]]
	}
    }

    # PGP actions
    foreach adr $tempalist {
        if {![catch {RatAlias expand pgpactions $adr $mh(role)} pgpa]} {
            if {[lindex $pgpa 0]} {
                set mh(pgp_sign) 1
                set mh(pgp_sign_explicit) 1
            }
            if {[lindex $pgpa 1]} {
                set mh(pgp_encrypt) 1
            }
        }
    }

    foreach adr $alist {
	if {![info exists tag($adr)]} {
	    set tag($adr) {}
	}
        set len [expr [string length $adr]+2]
	incr tot $len
	if {$len > $max} {
	    set max $len
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
    if {[string length $old] && ![regexp {,$} $old]} {
        set s [$w search -backwards , end]
        if {$s != ""} {
            $w delete $s end
        }
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

    set var [string trim [$w get 1.0 end]]
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
	set var [string map {mailto: {}} [selection get -displayof $w]]
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

# ComposeInsertFile --
#
# Insert a file into the message currently being composed
#
# Arguments:
# handler -	The handler for the active compose session

proc ComposeInsertFile {handler} {
    global t option
    upvar \#0 $handler mh

    set filename [rat_fbox::run \
                      -ok $t(open) \
                      -title $t(insert_file) \
                      -initialdir $option(initialdir) \
                      -parent [winfo toplevel $mh(composeBody)] \
                      -mode open]

    if {$filename != ""} {
        if {$option(initialdir) != [file dirname $filename]} {
            set option(initialdir) [file dirname $filename]
            SaveOptions
        }
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
    upvar \#0 $handler hd

    rat_edit::state $hd(composeBody) state

    $m entryconfigure [lindex $hd(undo_menu) 1] -state $state(undo)
    $m entryconfigure [lindex $hd(redo_menu) 1] -state $state(redo)
    $m entryconfigure [lindex $hd(cut_menu) 1] -state $state(selection)
    $m entryconfigure [lindex $hd(copy_menu) 1] -state $state(selection)
    $m entryconfigure [lindex $hd(paste_menu) 1] -state $state(paste)

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
    upvar \#0 $handler mh

    # Create identifier
    set id insert[incr idCnt]
    set w .$id
    upvar \#0 $id hd
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
    set b($w.t) command_to_run_through
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"

    pack $w.s -side top -anchor w -fill x
    pack $w.t -side top -expand 1 -fill both -padx 5
    pack $w.buttons -fill both -pady 5

    ::tkrat::winctl::SetGeometry giveCmd $w $w.t
    ::tkrat::winctl::ModalGrab $w $w.s.entry
    tkwait variable ${id}(done)
    ::tkrat::winctl::RecordGeometry giveCmd $w $w.t

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
    upvar \#0 $handler hd
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
	set cmd [string map [list %s $name.in] $cmd]
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
    upvar \#0 $handler hd

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
    upvar \#0 $id hd
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
    set b($hd(list)) saved_commands

    # The buttons
    frame $w.b
    button $hd(apply) -text $t(apply) -command "CmdApply $w $id" \
	    -state disabled
    button $hd(delete) -text $t(delete) -command "CmdDelete $w $id" \
	    -state disabled
    button $w.b.close -text $t(close) -command "destroy $w"
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
    pack $hd(text) -side bottom -expand 1 -fill x
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
    wm protocol $w WM_DELETE_WINDOW "destroy $w"
    bind $hd(list) <Destroy> "CmdClose $w $id"
    bind $w <Escape> "$w.b.close invoke"

    ::tkrat::winctl::SetGeometry cmdList $w $hd(list)
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
    upvar \#0 $handler hd

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
    upvar \#0 $handler hd

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
    upvar \#0 $handler hd

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
    upvar \#0 $handler hd

    ::tkrat::winctl::RecordGeometry cmdList $w $hd(list)
    if { 1 == $hd(changed)} {
	CmdWrite
    }

    catch {focus $hd(oldfocus)}
    unset hd
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
    button $w.b.dismiss -text $t(dismiss) -command "destroy $w"
    pack $w.b.update $w.b.dismiss -side left -padx 5 -expand 1
    text $w.text -yscroll "$w.scroll set" -relief sunken -bd 1 -wrap none
    scrollbar $w.scroll -relief raised -bd 1 \
	    -command "$w.text yview"
    pack $w.b -side bottom -pady 5 -fill x 
    pack $w.scroll -side right -fill y
    pack $w.text -expand 1 -fill both

    $w.text tag configure name -font $fixedBoldFont
    $w.text tag configure value -font $fixedNormFont -lmargin2 20

    UpdateGH $handler $w.text

    bind $w.text <Destroy> "::tkrat::winctl::RecordGeometry showGH $w $w.text"
    bind $w <Escape> "$w.b.dismiss invoke"
    ::tkrat::winctl::SetGeometry showGH $w $w.text
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
    foreach adr [RatSplitAdr $al] {
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
    upvar \#0 $handler hd

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
    set adr_smtp [RatAlias expand sending $al $hd(role)]
    set addresses [FixAddress $hd(role) $adr_smtp mail]
    foreach a [RatSplitAdr $addresses] {
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
		"[FixAddress $hd(role) [RatAlias expand sending $hd(reply_to) $hd(role)] rfc822]\n" \
		value
    }

    # To
    if { "" != $hd(to)} {
	$w insert end "      To: " name \
		"[FixAddress $hd(role) [RatAlias expand sending $hd(to) $hd(role)] rfc822]\n" \
		value
    }

    # CC
    if { "" != $hd(cc)} {
	$w insert end "      cc: " name \
		"[FixAddress $hd(role) [RatAlias expand sending $hd(cc) $hd(role)] rfc822]\n" \
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
    upvar \#0 $id hd
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

    bind $w.lname <Destroy> "EditorsEditClosed $id"
    ::tkrat::winctl::SetGeometry editorsEdit $w
}

# EditorsEditDone --
# 
# Called when EditorsEdit window is done
#
# Arguments:
# handler - Handler describing the windo
# ok	  - Boolean indicating if ok was pressed

proc EditorsEditDone {handler ok} {
    upvar \#0 $handler hd
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
    destroy $hd(w)
}

# EditorsEditClosed --
#
# Destroy handler
#
# Arguments:
# handler - Handler describing the windo

proc EditorsEditClosed {handler} {
    upvar \#0 $handler hd
    global b

    ::tkrat::winctl::RecordGeometry editorsEdit $hd(w)
    foreach bn [array names b $hd(w).*] {unset b($bn)}
    catch {focus $hd(oldfocus)}
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
    upvar \#0 $handler hd
    global editors t

    $hd(eeditm) delete 0 end
    foreach e $editors {
	$hd(eeditm) add command -label $e \
		-command "ComposeEEdit $handler [list $e] ; \
			        set ${handler}(eeditor)(eeditor) [list $e]"
    }
    if {[llength $editors] > 0} {
        set hd(eeditor) [lindex $editors 0]
        $hd(eeditb) configure -state normal
    } else {
        set hd(eeditor) $t(external_editor)
        $hd(eeditb) configure -state disabled
    }
}

# ComposeWrapCited --
#
# Wraps the cited message
#
# Arguments:
# handler - Handler identifying the compose window

proc ComposeWrapCited {handler} {
    upvar \#0 $handler hd

    set s 1.0

    rat_edit::storeSnapshot $hd(composeBody)
    while {[llength [set r [$hd(composeBody) tag nextrange Cited $s]]]} {
	set start [lindex $r 0]
	set end [lindex $r 1]
	set n [RatWrapCited [$hd(composeBody) get $start $end]]
	$hd(composeBody) delete $start $end
	$hd(composeBody) insert $start $n {noWrap Cited no_spell}
	set s [lindex $r 1]
    }
}

# CompareAddresses --
#
# Compares two list of addresses and returns 0 if they are equal
#
# Arguments:
# role - Role to operate under
# adr1, adr2 - List of addresses

proc CompareAddresses {role adr1 adr2} {
    set l1 [lsort [RatExtractAddresses $role $adr1]]
    set l2 [lsort [RatExtractAddresses $role $adr2]]
    return [string compare $l1 $l2]
}

# UpdateComposeRole
#
# Updates variables depeneding on role
#
# Arguments:
# handler - Handler identifying the compose window

proc UpdateComposeRole {handler} {
    global t option
    upvar \#0 $handler mh

    set role $mh(role)
    foreach v {from reply_to bcc} {
        if {![CompareAddresses $mh(orig,role) $mh($v) $mh(orig,$v)]} {
            set mh($v) $option($role,$v)
        }
        set mh(orig,$v) $option($role,$v)
    }
    set mh(orig,role) $role
    set gen [RatGenerateAddresses $handler]
    set mh(from) [lindex $gen 0]
    set mh(sender) [lindex $gen 1]
    if {0 == $mh(pgp_sign) || 0 == $mh(pgp_sign_explicit)} {
        set mh(pgp_sign) $option($role,sign_outgoing)
    }
    set mh(pgp_signer) $option($mh(role),sign_as)

    ComposeUpdateHeaderEntries $handler

    wm title $mh(toplevel) "$mh(title) ($option($mh(role),name))"

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
	$mh(composeBody) insert end "\n$sigtext" {noWrap no_spell sig}
	$mh(composeBody) mark set insert $insert
    }
}

# ComposeCreateMsg --
#
# Creates a message from a compose window
#
# Arguments:
# handler - Handler identifying the compose window

proc ComposeCreateMsg {handler {extra_envelope {}}} {
    global composeHeaderList composeAutoHeaderList option t charsetMapping \
        composeAdrHdrList rat_tmp
    upvar \#0 $handler mh

    # Envelope
    set envelope {}
    foreach h [concat $composeHeaderList $composeAutoHeaderList] {
	if {[info exists mh($h)] && [string length $mh($h)]} {
            lappend envelope [list $h $mh($h)]
            if {-1 != [lsearch $composeAdrHdrList $h]} {
                regsub -all "\n" $mh($h) "\n    " value
                lappend envelope [list X-TkRat-Original-$h $value]
            }
	}
    }

    # Text body
    if {[info exists mh(composeBody)]} {
        if [catch {$mh(composeBody) get 1.0 end-1c} bodydata] {
            set bodydata {}
        }
        set tags {}
        foreach tag {Cited noWrap no_spell sig} {
            lappend tags [list $tag [$mh(composeBody) tag ranges $tag]]
        }
        lappend envelope [list X-TkRat-Internal-Tags $tags]

        # Determine suitable charset
        set p(charset) $mh(charset)
        if {[info exists bh(parameter)]} {
            set params $bh(parameter)
            lappend params {a a}
            array get p $params
        }
        if {"auto" == $p(charset)} {
            set fallback $option(charset)
            set p(charset) [RatCheckEncodings bodydata \
                                $option(charset_candidates)]
        } else {
            set fallback $p(charset)
            set p(charset) [RatCheckEncodings bodydata $mh(charset)]
        }
        if {"" == $p(charset)} {
            if {0 != [RatDialog $mh(toplevel) $t(warning) $t(bad_charset) {} \
                          0 $t(continue) $t(abort)]} {
                return {}
            }
            set p(charset) $fallback
        }

        # Find encoding
        set fn [RatTildeSubst $rat_tmp/rat.[RatGenId]]
        set fh [open $fn w]
        if {[info exists charsetMapping($p(charset))]} {
            fconfigure $fh -encoding $charsetMapping($p(charset))
        } else {
            fconfigure $fh -encoding $p(charset)
        }
        puts -nonewline $fh $bodydata
        close $fh
        set encoding [RatGetCTE $fn]
        file delete $fn
    } else {
        set bodydata {}
        set encoding ""
    }

    lappend envelope [list message_id [RatGenerateMsgId $mh(role)]]
    lappend envelope [list X-TkRat-Internal-Role $mh(role)]
    lappend envelope [list X-TkRat-Internal-PGPActions \
                          [list $mh(pgp_sign) $mh(pgp_encrypt)]]
    if {[string length $mh(save_to)]} {
	lappend envelope [list X-TkRat-Internal-Save-To $mh(save_to)]
    }
    foreach e $extra_envelope {
        lappend envelope $e
    }

    # Prepare parameters array
    set params {}
    foreach name [array names p] {
	lappend params [list $name $p($name)]
    }


    # Collect into body entity
    set body [list text plain $params $encoding inline {} {} \
		  [list utfblob $bodydata]]

    # Handle attachments
    if { 0 < [llength $mh(attachmentList)]} {
	set attachments [list $body]
	foreach a $mh(attachmentList) {
	    upvar \#0 $a bh
	    set body_header {}
	    foreach h {content_description content_id} {
		if {[info exists bh($h)] && [string length $bh($h)]} {
		    lappend body_header [list $h $bh($h)]
		}
	    }
	    foreach v {parameter disp_parm} {
		if {![info exists bh($v)]} {
		    set bh($v) {}
		}
	    }
	    if {![info exists bh(encoding)]} {
		set bh(encoding) 7bit
	    }
	    set bp [list $bh(type) $bh(subtype) $bh(parameter) $bh(encoding) \
			attachment $bh(disp_parm) $body_header \
			[list file $bh(filename)]]
	    lappend attachments $bp
	}
	set body [list multipart mixed {} 7bit {} {} {} $attachments]
    }

    return [RatCreateMessage $mh(role) [list $envelope $body]]
}


# RatSendFailed --
#
# Handles a failed send. Does this bu first poping up a error dialog and
# then restart composing of the message.
#
# Arguments:
# name   - Handler of message
# reason - Text string desribing failure

proc RatSendFailed {name reason} {
    global t

    catch {
	Popup "$t(send_failed)\n$reason"
	ComposeContinue $name
    }
}

# RatSaveOutgoing --
#
# Save a copy of a sent message to the given folder
#
# Arguments:
# msg    - Handler of message
# folder - Folder to save message in

proc RatSaveOutgoing {msg folder} {
    global t

    set msg2 [$msg duplicate]
    $msg2 remove_internal
    if {![catch {$msg2 copy $folder} err]} {
	rename $msg2 ""
	return
    }

    # The save failed, present dialog which allows them to select a new folder
    set handler save_outgoing_failed
    set w .$handler
    toplevel $w -class TkRat

    set subject ""
    set recipients ""
    foreach h [$msg2 headers] {
	set hn [string tolower [lindex $h 0]]
	if {"subject" == $hn} {
	    set subject [lindex $h 1]
	} elseif {-1 != [lsearch -exact {to cc bcc} $hn]} {
	    if {"" != $recipients} {
		set recipients "$recipients, [lindex $h 1]"
	    } else {
		set recipients [lindex $h 1]
	    }
	}
    }
    if {[string length $subject] > 40} {
	set subject "[string range $subject 0 36]..."
    }
    if {[string length $recipients] > 40} {
	set recipients "[string range $recipients 0 36]..."
    }
    message $w.msg -justify left -text $t(save_outgoing_failed) -aspect 600
    label $w.subject_l -text "$t(subject):" -anchor e
    label $w.subject_v -text $subject -anchor w
    label $w.recipients_l -text "$t(recipients):" -anchor e
    label $w.recipients_v -text $recipients -anchor w
    frame $w.b -bd 5
    button $w.b.cancel -text " $t(cancel) " \
	-command "destroy $w; rename $msg2 {}"
    menubutton $w.b.save -text $t(save_to) -indicatoron 1 \
	-menu $w.b.save.m -relief raised -underline 0 \
	-padx 5 -pady 2
    menu $w.b.save.m -tearoff 0 -postcommand \
	    "RatSaveOutgoingPostMenu $w $w.b.save.m $msg2"
    pack $w.b.save $w.b.cancel -side left -expand 1

    grid $w.msg -
    grid $w.subject_l $w.subject_v -sticky ew
    grid $w.recipients_l $w.recipients_v -sticky ew
    grid $w.b - -sticky ew

    bind $w <Escape> "$w.b.cancel invoke"
    bind $w.msg <Destroy> "::tkrat::winctl::RecordGeometry saveOutgoing $w"
    wm title $w $t(save_outgoing_failed_title)
    ::tkrat::winctl::SetGeometry saveOutgoing $w
}

# RatSaveOutgoingPostMenu --
#
# Create the want to save to menu
#
# Arguments:
# w   -	Name of window
# m   - Name of menu
# msg - Message to save

proc RatSaveOutgoingPostMenu {w m msg} {
    global t

    $m delete 0 end
    VFolderBuildMenu $m 0 "RatSaveOutgoingDo $w $msg" 1
    $m add separator
    $m add command -label $t(to_file)... \
	    -command "RatSaveOutgoingDo $w $msg \
		      \[InsertIntoFile [winfo toplevel $w]\]"
    $m add command -label $t(to_dbase)... \
	    -command "RatSaveOutgoingDo $w $msg \
		      \[InsertIntoDBase [winfo toplevel $w]\]"
    FixMenu $m
}
proc RatSaveOutgoingDo {w msg save_to} {
    if {"" == $save_to} {
	return
    }
    if {1 == [llength $save_to]} {
	global vFolderDef
	set def $vFolderDef($save_to)
    } else {
	set def $save_to
    }
    if {![catch {$msg copy $def} err]} {
	rename $msg ""
	destroy $w
    }
}

# ComposeStoreBackup --
#
# Store a backup of the current message
#
# Arguments:
# handler   -	The handler for the active compose session
# schedule  -   True if another backup should be scheduled

proc ComposeStoreBackup {handler schedule} {
    upvar \#0 $handler mh
    global option vFolderDef vFolderHold

    if {![info exists mh(hold_fh)]} {
        set mh(hold_fh) [RatOpenFolder $vFolderDef($vFolderHold)]
    }

    set msg [ComposeCreateMsg $handler \
                 [list [list X-TkRat-Internal-AutoBackup [clock seconds]]]]
    foreach u [$mh(hold_fh) list "%u"] {
        set uids($u) 1
    }
    $mh(hold_fh) insert $msg
    rename $msg ""
    foreach u [$mh(hold_fh) list "%u"] {
        if {![info exists uids($u)]} {
            break
        }
    }
    if {[info exists mh(old_backup)]} {
        ComposeRemoveOldBackup $mh(hold_fh) $mh(old_backup)
    }
    set mh(old_backup) $u

    if {$option(compose_backup) > 0 && $schedule} {
        set mh(next_backup) [after [expr $option(compose_backup)*1000] \
                                 [list ComposeStoreBackup $handler 1]]
    }
}

proc ComposeRemoveOldBackup {hold_fh old_backup} {
    set uids [$hold_fh list "%u"]
    set index [lsearch -exact $uids $old_backup]
    if {-1 != $index} {
        $hold_fh setFlag $index deleted 1
        $hold_fh update sync
    }
}

# ComposeDoFinalBackup --
#
# Remove the backup of the current message
#
# Arguments:
# handler   -	The handler for the active compose session

proc ComposeDoFinalBackup {handler} {
    upvar \#0 $handler mh
    global option

    if {$mh(final_backup_done)} {
        return
    }
    set mh(final_backup_done) 1

    if {[info exists mh(next_backup)]} {
        catch {after cancel $mh(next_backup)}
    }

    ComposeStoreBackup $handler 0

    after [expr $option(compose_last_chance)*1000] \
        "ComposeRemoveOldBackup $mh(hold_fh) $mh(old_backup); $mh(hold_fh) close"
}

# ComposeStoreSnapshot --
#
# Store a snopshot of the message being composed
#
# Arguments:
# handler   -	The handler for the active compose session

proc ComposeStoreSnapshot {handler} {
    upvar \#0 $handler mh
    global option vFolderDef vFolderHold

    if {![info exists mh(hold_fh)]} {
        set mh(hold_fh) [RatOpenFolder $vFolderDef($vFolderHold)]
    }
    set msg [ComposeCreateMsg $handler]
    $mh(hold_fh) insert $msg
    rename $msg ""
}

# ComposeSetWrap --
#
# Set the wrap lines mode
#
# Arguments:
# handler   -	The handler for the active compose session

proc ComposeSetWrap {handler} {
    upvar \#0 $handler mh
    global option

    set option(do_wrap) $mh(do_wrap)
    SaveOptions
    rat_edit::setWrap $mh(composeBody) $mh(do_wrap)
}

# ComposeSetMarkWrap --
#
# Set the mark non-wrappable mode
#
# Arguments:
# handler   -	The handler for the active compose session

proc ComposeSetMarkWrap {handler} {
    upvar \#0 $handler mh
    global option

    set option(mark_nowrap) $mh(mark_nowrap)
    SaveOptions
    $mh(composeBody) tag configure noWrap -underline $mh(mark_nowrap)
}
