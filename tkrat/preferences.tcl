# preferences.tcl --
#
# This file controls the preferences window. There is just one preferences
# window, if the user requests a new preferences window when the window
# already exists, it is just unmapped and raised (if possible).
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Preferences --
#
# Make sure the user sees an preferences window.
#
# Arguments:

proc Preferences {} {
    if {![winfo exists .pref]} {
	BuildPreferences
    } else {
	wm deiconify .pref
    }
}


# BuildPreferences --
#
# Builds the preferences window.
#
# Arguments:

proc BuildPreferences {} {
    global t b pref

    # Initialize data table
    foreach l [GetLanguages] {
	lappend lang [lrange $l 0 1]
    }

    # The lists here can have the following elements
    # option var label_id value_list
    # entry var label_id
    # entry_unit var label_id unit
    # bool var label_id true_text false_text
    # special var label_id special_proc
    # label label_text font_options anchor
    set pref(appearance) "\
	{option language language [list $lang]} \
	{entry charset charset} \
	{bool useinputmethods useinputmethods \
	        {{$t(true)} {$t(false)}}} \
    "
    set pref(appearance,graphics) "\
	{special color_set color_scheme SetupColor} \
	{label $t(fonts) underline center} \
	{special prop_norm prop_norm {SelectFont prop_norm}} \
	{special prop_light prop_light {SelectFont prop_light}} \
	{special fixed_norm fixed_norm {SelectFont fixed_norm}} \
	{special fixed_bold fixed_bold {SelectFont fixed_bold}} \
	{special watcher_font watcher_font {SelectFont watcher_font}} \
    "
    set pref(appearance,msglist) "\
	{entry list_format list_format} \
	{option show_header show_headers \
		{{all {$t(show_all_headers)}} \
		 {selected {$t(show_selected_headers)}} \
		 {none {$t(show_no_headers)}}}} \
	{entry show_header_selection show_header_selection} \
	{option browse default_browse_mode \
		{{normal {$t(no_browse)}} \
		 {browse {$t(do_browse)}} \
		 {folder {$t(use_folder_default)}}}} \
	{option folder_sort sort_order \
		{{threaded {$t(sort_threaded)}} \
		 {subject {$t(sort_subject)}} \
		 {subjectonly {$t(sort_subjectonly)}} \
		 {sender {$t(sort_sender)}} \
		 {folder {$t(sort_folder)}} \
		 {reverseFolder {$t(sort_reverseFolder)}} \
		 {date {$t(sort_date)}} \
		 {reverseDate {$t(sort_reverseDate)}} \
		 {size {$t(sort_size)}}\
		 {reverseSize {$t(sort_reverseSize)}}}} \
	{option start_selection start_selection \
		{{first {$t(first_message)}} \
		 {last {$t(last_message)}} \
		 {first_new {$t(first_new_message)}} \
		 {before_new {$t(before_first_new_message)}}}} \
    "
    set pref(appearance,html) "\
	{special html_prop_font html_prop_font {SelectFont html_prop_font}} \
	{special html_fixed_font html_fixed_font \
	        {SelectFont html_fixed_font}} \
	{entry html_prop_font_sizes html_prop_font_sizes} \
	{entry html_fixed_font_sizes html_fixed_font_sizes} \
	{bool html_show_images html_show_images \
		{{$t(true)} {$t(false)}}} \
        "
    set pref(network) "\
	{message {} {$t(start_online_mode)}} \
	{option start_online_mode start_mode \
	    {{last {$t(som_last)}} \
	     {online {$t(som_online)}} \
             {offline {$t(som_offline)}}}} \
    "
    set pref(network,www) "\
	{option url_viewer url_viewer \
		{{RatUP {$t(userproc): RatUP_ShowURL}} \
		 {netscape Netscape} \
		 {opera Opera} \
		 {lynx Lynx} {other $t(other)}}} \
	{entry netscape netscape_cmd} \
	{entry opera opera_cmd} \
	{entry lynx lynx_cmd} \
	{entry other_browser other_browser_cmd} \
    "
    set pref(replies) "\
	{bool wrap_cited wrap_cited \
		{{$t(true)} {$t(false)}}} \
	{bool skip_sig on_reply \
		{{$t(skip_sig)} {$t(keep_sig)}}} \
        {bool append_sig append_sig \
                {{$t(true)} {$t(false)}}} \
        {bool reply_bottom reply_bottom \
                {{$t(at_bottom)} {$t(at_top)}}} \
    "
    set pref(roles) "\
	{entry name role_name} \
	{entry signature signature_file} \
	{special save_outgoing save_out SetupDefaultSave} \
	{custom SetupRole} \
    "
    set pref(roles,address) "\
	{entry from use_from_address} \
	{entry bcc default_bcc} \
	{entry reply_to default_reply_to} \
	{message $t(tip) {$t(test_by)}} \
	{entry pgp_keyid pgp_keyid} \
    "
    set pref(roles,sending) "\
	{option sendprot sendprot \
		{{smtp {$t(smtp)}} {prog {$t(user_program)}}}} \
	{entry smtp_hosts smtp_hosts} \
	{entry sendprog sendprog} \
	{bool sendprog_8bit progin \
		{{$t(eightbit)} {$t(sevenbit)}}} \
	{special dsn_request default_action SetupDSNRequest} \
    "
    set pref(roles,advanced) "\
        {label {$t(normally_ok)} {} center} \
        {label {$t(unqual_adr_domain)} {} w} \
        {entry uqa_domain domain} \
        {label {$t(smtp_from_long)} {} w} \
        {entry smtp_helo host}
    "
    set pref(advanced) "\
	{entry main_window_name window_name} \
	{entry icon_name icon_name} \
	{option icon icon_bitmap \
		{{normal {$t(normal_bitmap)}} \
		 {small {$t(small_bitmap)}} \
		 {none {$t(none)}}}} \
    "
    set pref(advanced,behaviour) "\
	{bool iconic startup_mode \
		{{$t(iconic)} {$t(normal)}}} \
	{bool info_changes show_changes \
		{{$t(show)} {$t(dont_show)}}} \
	{bool mail_steal check_stolen_mail \
		{{$t(check)} {$t(dont_check)}}} \
	{option print_header print_headers \
		{{all {$t(all)}} {selected {$t(selected)}} {none {$t(none)}}}} \
	{bool expunge_on_close expunge_on_close \
		{{$t(do)} {$t(do_not)}}} \
	{bool keep_pos remember_pos \
		{{$t(do_remember)} {$t(dont_remember)}}} \
	{bool checkpoint_on_unmap checkpoint_on_unmap
		{{$t(true)} {$t(false)}}} \
	{entry_unit checkpoint_interval checkpoint_interval {$t(seconds)}} \
    "
    set pref(advanced,appearance) "\
	{bool override_color override_color \
		{{$t(true)} {$t(false)}}} \
	{bool override_fonts override_fonts \
		{{$t(true)} {$t(false)}}} \
	{bool tearoff menu_tearoff \
		{{$t(true)} {$t(false)}}} \
	{entry_unit log_timeout log_timeout {$t(seconds)}} \
    "
    set pref(advanced,ssh) "\
	{entry ssh_path ssh_path} \
	{entry_unit ssh_timeout ssh_timeout {$t(seconds)}} \
    "
    set pref(advanced,files) "\
	{entry tmp tmp_dir} \
	{entry permissions file_permissions} \
	{entry userproc userproc_file} \
	{entry mailcap_path mailcap_path} \
	{entry print_command print_command} \
	{entry terminal terminal_command} \
	{entry ispell_path ispell_cmd} \
	{entry debug_file debug_file} \
	{entry mimeprog mimeprog} \
    "
    set pref(advanced,caching) "\
	{bool cache_passwd cache_passwd \
		{{$t(do_cache)} {$t(do_not_cache)}}} \
	{entry_unit cache_passwd_timeout cache_timeout {$t(seconds)}} \
	{bool cache_conn cache_conn \
		{{$t(do_cache)} {$t(do_not_cache)}}} \
	{entry_unit cache_conn_timeout cache_timeout {$t(seconds)}} \
    "
    set pref(advanced,folder_dynamic) "\
	{option dynamic_behaviour dynamic_behaviour \
		{{expanded {$t(dyn_expanded)}} \
		 {closed {$t(dyn_closed)}}}} \
    "
    set pref(advanced,network) "\
	{entry domain domain} \
	{entry remote_user remote_user} \
	{entry remote_host remote_host} \
	{entry imap_port imap_port} \
	{entry pop3_port pop3_port} \
	{entry urlprot url_protocols} \
	{entry url_color url_color} \
    "
    set pref(advanced,dbase) "\
	{option def_extype extype \
		{{none {$t(none)}} \
		 {remove {$t(remove)}} \
		 {incoming {$t(incoming)}} \
		 {backup {$t(backup)}}}} \
	{entry_unit def_exdate exdate {$t(days)}} \
	{entry dbase_backup dbase_backup} \
	{entry_unit chunksize chunksize {$t(messages)}} \
	{entry_unit expire_interval expire_interval {$t(days)}} \
    "
    set pref(advanced,watcher) "\
	{entry_unit watcher_time intervals {$t(seconds)}} \
	{entry watcher_name window_name} \
	{entry_unit watcher_max_height max_height {$t(lines)}} \
	{option watcher_show show_messages \
		{{new {$t(new)}} {all {$t(all)}}}} \
	{entry watcher_format list_format} \
	{entry watcher_bell bell_ringings} \
    "
    set pref(advanced,compose) "\
	{entry compose_headers headers} \
	{entry_unit wrap_length wrap_length {$t(characters)}} \
	{bool sigdelimit sigdelimit \
		{{$t(true)} {$t(false)}}} \
	{bool lookup_name lookup_name \
		{{$t(do_lookup)} {$t(dont_lookup)}}} \
	{bool copy_attached copy_attached_files \
		{{$t(true)} {$t(false)}}} \
	{option alias_expand alias_expansion \
		{{0 {$t(alias_0)}} {1 {$t(alias_1)}} {2 {$t(alias_2)}}}} \
	{bool always_editor always_use_external_editor \
		{{$t(true)} {$t(false)}}} \
    "
    set pref(advanced,replies) "\
	{entry re_regexp reply_regexp} \
	{entry attribution attribution} \
	{entry forwarded_message forwarded_label} \
	{entry no_subject no_subject} \
	{entry reply_lead reply_lead} \
	{entry citexp citexp} \
    "
    set pref(advanced,sending) "\
	{bool create_sender create_sender \
		{{$t(true)} {$t(false)}}} \
	{entry_unit smtp_timeout smtp_timeout {$t(seconds)}} \
	{bool smtp_reuse smtp_reuse \
		{{$t(true)} {$t(false)}}} \
	{option smtp_verbose smtpv \
		{{0 {$t(none)}} \
		 {1 {$t(terse)}} \
		 {2 {$t(normal)}} \
		 {3 {$t(verbose)}}}} \
	{bool force_send force_send \
		{{$t(force)} {$t(no_force)}}} \
    "
    set pref(advanced,notification) "\
	{entry dsn_directory dsn_directory} \
	{bool dsn_snarf_reports folderwindow \
		{{$t(snarf_dsn)} {$t(not_snarf_dsn)}}} \
	{entry_unit dsn_expiration dsn_expiration {$t(days)}} \
	{special dsn_verbose report_level SetupDSNVerbose} \
    "
    set pref(advanced,pgp) "\
	{option pgp_version pgp_version \
		{{0 $t(none)} {gpg-1 {GPG-1}} {2 {PGP-2}} \
		 {5 {PGP-5}} {6 {PGP-6}} {auto {$t(auto)}}}} \
	{entry pgp_path pgp_path} \
	{entry pgp_args pgp_extra_args} \
	{entry pgp_keyring pgp_keyring} \
	{bool cache_pgp cache_passwd \
		{{$t(do_cache)} {$t(do_not_cache)}}} \
	{entry_unit cache_pgp_timeout cache_timeout {$t(seconds)}} \
	{bool pgp_sign sign_outgoing \
		{{$t(true)} {$t(false)}}} \
	{bool pgp_encrypt encrypt_outgoing \
		{{$t(true)} {$t(false)}}} \
    "

    # Create top window structures
    set w .pref
    toplevel $w -class TkRat
    wm title $w $t(preferences)
    frame $w.pane -bd 2 -relief flat
    Size $w.pane prefPane
    set pref(pane) $w.pane
    set pref(lastPref) ""
    set pref(selected) ""

    # Setup tree
    set pref(tree) [rat_tree::create $w.tree \
	    -sizeid prefTree -selectcallback PrefSelectPane]
    $pref(tree) autoredraw 0
    set topnode [$pref(tree) gettopnode]
    set pref(rnode) \
	    [$topnode add folder -label $t(roles) -state open -id rnode]
    PrefPopulateRoles
    set onode [$topnode add folder -label $t(options) -state open]
    foreach n {{appearance {graphics msglist html}}
               {network {www}}
               replies
               {advanced {behaviour appearance ssh files caching
                          folder_dynamic network dbase watcher compose
	                  replies sending notification pgp}}} {
	set n0 [lindex $n 0]
	set name $t($n0)
	if {1 != [llength $n]} {
	    set node [$onode add folder -label $name -state closed \
		    -id [list option $n0]]
	    foreach n1 [lindex $n 1] {
		$node add leaf -label $t($n1) -id [list option $n0,$n1]
	    }
	} else {
	    $onode add leaf -label $name -id [list option $n0]
	}
    }
    $pref(tree) redraw

    # The buttons
    frame $w.buttons
    button $w.buttons.ok -text $t(apply) -command "PrefApply $w" \
	    -default active -state disabled
    button $w.buttons.reset -text $t(reset) -state disabled \
	    -command {
			global pref
			foreach v $pref(vars,$pref(lastPref)) {
			    set pref(opt,$v) $pref(old,$v)
			}
			foreach but $pref(traceButtons) {
			    $but configure -state disabled
			}
		     }
    button $w.buttons.close -text $t(close) \
	    -command "PrefCheck $w; \
		      RecordPos $w preferences; \
		      RecordSize $w.pane prefPane; \
		      wm withdraw $w"
    pack $w.buttons.ok \
	 $w.buttons.reset \
         $w.buttons.close -side left -expand 1
    bind $w <Return> "PrefApply $w"
    set b($w.buttons.ok) apply_prefs
    set b($w.buttons.reset) reset_changes
    set b($w.buttons.close) dismiss
    set pref(traceButtons) [list $w.buttons.ok $w.buttons.reset]

    grid $w.tree $w.pane -sticky nwse
    grid ^ $w.buttons  -sticky ew
    grid rowconfigure $w 0 -weight 1
    grid columnconfigure $w 0 -weight 1
    grid columnconfigure $w 1 -weight 5
    grid propagate $w.pane 0

    Place $w preferences

    # Initialize tracing function
    set pref(traceChanges) 0
    trace variable pref w PrefTraceProc
}

# PrefSelectPane --
#
# Called when the user selects an item in the tree
#
# Arguments:
# pane - pane to fill in

proc PrefSelectPane {pane} {
    global pref

    if {"" == $pane} {
	return fail
    }
    PrefCheck $pref(pane)
    set pref(selected) $pane
    foreach c [winfo children $pref(pane)] {
	destroy $c
    }
    switch [lindex $pane 0] {
	option {
	    PrefBuild [lindex $pane 1] $pref(pane) ""
	}
	role {
	    PrefBuild [lindex $pane 2] $pref(pane) "[lindex $pane 1],"
	}
	rnode {
	    PrefBuildRnode $pref(pane)
	}
    }
    return ok
}

# PrefBuild --
#
# Build a preferences pane
#
# Arguments:
# pane - Which pane of preferences to build
# w    - Name of frame to build it into
# rp   - Role prefix

proc PrefBuild {pane w rp} {
    global t b option pref

    set pref(traceChanges) 0
    set row 0
    set pref(lastPref) $pane
    set pref(rolePrefix) $rp
    set pref(vars,$pane) {}
    foreach p $pref($pane) {
	grid rowconfigure $w $row -weight 0

	if {![regexp {^(label|message|custom)$} [lindex $p 0]]} {
	    set var "${rp}[lindex $p 1]"
	    set bvar [lindex $p 1]
	    lappend pref(vars,$pane) $var
	    if {[info exists option($var)]} {
		set pref(opt,$var) $option($var)
	    } else {
		set pref(opt,$var) {}
	    }

	    label $w.r${row}_lab -text $t([lindex $p 2]):
	    grid $w.r${row}_lab -row $row -sticky ne -pady 1
	}
	switch [lindex $p 0] {
	    entry {
		    entry $w.r${row}_item -textvariable pref(opt,$var)
		    grid $w.r${row}_item - -row $row -column 1 -sticky we
		    set b($w.r${row}_item) pref_$bvar
		}
	    entry_unit {
		    entry $w.r${row}_item -textvariable pref(opt,$var)
		    label $w.r${row}_unit -text ([lindex $p 3])
		    grid $w.r${row}_item -row $row -column 1 -sticky we
		    grid $w.r${row}_unit -row $row -column 2 -sticky w
		    set b($w.r${row}_item) pref_$bvar
		    set b($w.r${row}_unit) unit_pref
		}
	    bool {
		    if {$pref(opt,$var)} {
			set pref(opt,$var) 1
		    } else {
			set pref(opt,$var) 0
		    }
		    set v [lindex $p 3]
		    set v [list [list 1 [lindex $v 0]] [list 0 [lindex $v 1]]]
    		    OptionMenu $w.r${row}_item pref(opt,$var) pref(text,$var) \
			    $v
		    grid $w.r${row}_item - -row $row -column 1 -sticky w
		    set b($w.r${row}_item) pref_$bvar
		}
	    option {
    		    OptionMenu $w.r${row}_item pref(opt,$var) \
			    pref(text,$var) [lindex $p 3]
		    grid $w.r${row}_item - -row $row -column 1 -sticky w
		    set b($w.r${row}_item) pref_$bvar
		}
	    special {
		    eval "[lindex $p 3] $w.r${row}_item"
		    grid $w.r${row}_item - -row $row -column 1 -sticky we
		}
	    label {
		label $w.r${row}_lab -text [lindex $p 1] -anchor [lindex $p 3]
		grid $w.r${row}_lab -row $row -columnspan 2 -sticky new -pady 1
		$w.r${row}_lab configure \
			-font "[$w.r${row}_lab cget -font] [lindex $p 2]"
	    }
	    message {
		label $w.r${row}_lab -text [lindex $p 1]
		message $w.r${row}_message -text [lindex $p 2] \
			-padx 0 -anchor w
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
		grid $w.r${row}_message -row $row -column 1 -sticky we
		bind $w.r${row}_message <Configure> \
			"$w.r${row}_message configure \
			 -width \[winfo width $w.r${row}_message\]"
	    }
	    custom {
		    eval "[lindex $p 1] $w.r${row}_item"
		    grid $w.r${row}_item - -row $row -column 1 -sticky we
		}
	    default {puts "Internal error <$p>"}
	}
	if {![regexp {^(label|message|custom)$} [lindex $p 0]]} {
	    set pref(old,$var) $pref(opt,$var)
	    if {[RatIsLocked option([lindex $p 1])]} {
		$w.r${row}_item configure -state disabled -relief flat
	    }
	}

	incr row
    }
    frame $w.space
    grid $w.space -row $row
    grid rowconfigure $w $row -weight 1
    grid columnconfigure $w 1 -weight 1
    foreach but $pref(traceButtons) {
	$but configure -state disabled
    }
    set pref(traceChanges) 1
}

# PrefBuildRnode --
#
# Build the roles pane
#
# Arguments:
# w    - Name of frame to build it into

proc PrefBuildRnode {w} {
    global t b

    grid rowconfigure $w 0 -weight 0
    grid rowconfigure $w 1 -weight 0
    message $w.rnmsg -text $t(roles_expl) -aspect 700
    button $w.nrole -text $t(create_new_role) -command PrefCreateRole
    grid $w.rnmsg - -row 0 -sticky ew -pady 5
    grid x $w.nrole -sticky w
}

# PrefApply --
#
# Applies any changes to the preferences made in the current window.
#
# Arguments:
# parent -	Parent window

proc PrefApply {parent} {
    global option pref t folderWindowList tk_version

    set hasChanged 0
    set needRestart 0
    set rp $pref(rolePrefix)
    switch $pref(lastPref) {
	appearance {
	    if {[string compare $option(useinputmethods) \
		    $pref(opt,useinputmethods)] && 8.3 <= $tk_version} {
		tk useinputmethods $pref(opt,useinputmethods)
	    }
	    if {[string compare $option(charset) $pref(opt,charset)]} {
		set option(charset_candidates) \
		    [linsert $option(charsets) 1 $pref(opt,charset)]
	    }
	}
	advanced,files {
	    if {![regexp %p $pref(opt,print_command)]} {
		Popup $t(no_pp_in_print_command) $parent
	    }
	    if {[string compare $option(mailcap_path) \
		    $pref(opt,mailcap_path)]} {
		set option(mailcap_path) $pref(opt,mailcap_path)
		RatMailcapReload
		set hasChanged 1
	    }
	}
	advanced,replies {
	    if {[catch {regexp -nocase $pref(opt,re_regexp) ""} e]} {
		Popup "$t(re_regexp_error): $e" $parent
		return
	    }
	    if {[catch {regexp -nocase $pref(opt,citexp) ""} e]} {
		Popup [format $t(illegal_regexp): $e] $parent
		return
	    }
	}
	roles {
	    if {[string compare $option(${rp}signature) \
		    $pref(opt,${rp}signature)]
	        && 1 == [llength [info commands RatUP_Signature]]} {
		Popup $t(sig_cmd_takes_precedence) $parent
	    }
	    if {[string compare $option(${rp}name) $pref(opt,${rp}name)]} {
		set option(${rp}name) $pref(opt,${rp}name)
		PrefSortRoles
		PrefPopulateRoles
		$pref(tree) redraw
		set hasChanged 1
	    }
	}
	roles,sending {
	    if {[string compare $option(${rp}sendprog) \
		    $pref(opt,${rp}sendprog)]
	    && ![file executable [lindex $pref(opt,${rp}sendprog) 0]]} {
		Popup $t(warning_sendprog) $parent
	    }
	}
	advanced {
	    if {[string compare $option(icon) $pref(opt,icon)]} {
		SetIcon . $pref(opt,icon)
	    }
	}
	advanced,network {
	    if {[string compare $option(url_color) $pref(opt,url_color)]} {
		foreach fw [array names folderWindowList] {
		    upvar #0 $fw fh
		    $fh(text) tag configure URL \
			    -foreground $pref(opt,url_color)
		}
	    }
	}
    }
    foreach prefs [array names pref opt,*] {
	set opt [string range $prefs 4 end]
	if {![info exists option($opt)]} {
	    continue
	}
	if {[string compare $option($opt) $pref(opt,$opt)]} {
	    set option($opt) $pref(opt,$opt)
	    set hasChanged 1
	    if { -1 != [lsearch -exact {language charset fontsize
		    			main_window_name icon_name
					default_folder watcher_name pgp_enable
					override_fonts prop_norm prop_light
					fixed_norm fixed_bold watcher_font
	                                charset} \
				$opt]} {
		set needRestart 1
	    }
	}
    }

    if {$hasChanged} {
	switch $pref(lastPref) {
	    appearance,msglist {
		foreach f [array names folderWindowList] {
		    Sync $f update
		}
	    }
	    advanced,pgp {
		InitPgp
	    }
	    roles,sending {
		RatSend kill
	    }
	}

	SaveOptions
    }
    if {$needRestart} {
	Popup $t(need_restart) $parent
    }

    foreach but $pref(traceButtons) {
	$but configure -state disabled
    }
}


# PrefCheck --
#
# Checks if there are any unapplied changes and if there is the user is
# queried if he wants to apply them.
#
# Arguments:
# parent - Parent of window

proc PrefCheck {parent} {
    global option pref t

    foreach prefs [array names pref opt,*] {
	set opt [string range $prefs 4 end]
	if {![info exists option($opt)]} {
	    continue
	}
	if {[string compare $option($opt) $pref(opt,$opt)]} {
	    set value [RatDialog $parent $t(unapplied_changes_title) \
		    $t(unapplied_changes) {} 0 $t(apply) $t(discard)]
	    if { 0 == $value } {
		PrefApply $parent
	    } else {
		foreach n [array names pref opt,*] {
		    unset pref($n)
		}
	    }
	    return
	}
    }
}


# OptionMenu --
#
# Generates an option menu. The generated menu will have window name "w"
# and will set the "varName" variable. The different options are
# controlled by the value arguments. Each value argument is a list of
# two elements. The first is the value to set "varName" to and the second
# is the text to show. The menubutton will use "textVar" as the textvariable.
#
# Arguments:
# w	  -	Name of menubutton to create
# varName -	Variable to set to value
# textVar -	Variable to use for the text we show
# values  -	A list of lists which describes the values of this button

proc OptionMenu {w varName textVar values} {
    upvar #0 $varName var
    upvar #0 $textVar text

    set width 10
    menubutton $w -textvariable $textVar -indicatoron 1 \
		  -relief raised -menu $w.m -pady 1
    menu $w.m -tearoff 0
    foreach elem $values {
	if {![string compare [lindex $elem 0] $var]} {
	    set text [lindex $elem 1]
	}
	$w.m add command -label [lindex $elem 1] \
		-command "set $varName [list [lindex $elem 0]]"
	if { $width < [string length [lindex $elem 1]]} {
	    set width [string length [lindex $elem 1]]
	}
    }
    $w configure -width $width

    trace variable var w "PrefTraceOptionProc $varName $textVar {$values}"
}

proc PrefTraceOptionProc {varName textVar values args} {
    upvar #0 $varName var
    upvar #0 $textVar text

    foreach v $values {
	if {![string compare $var [lindex $v 0]]} {
	    set text [lindex $v 1]
	    return
	}
    }
}

proc SetupColor {w} {
    global t option pref tk_version b

    frame $w
    menubutton $w.mb -textvariable pref(text,color_set) \
	    -indicatoron 1 -relief raised -menu $w.mb.m -pady 1
    set b($w) pref_color_scheme
    set b($w.mb) pref_color_scheme
    menu $w.mb.m -tearoff 0
    set width 20
    foreach c { {gray85 black} {PeachPuff2 black} {bisque black}
                {SlateBlue1 black} {SteelBlue4 white} {SkyBlue1 black}
                {aquamarine2 black} {SpringGreen4 black}} {
	set name $t([lindex $c 0])
	if {![string compare $c $option(color_set)]} {
	    set pref(text,color_set) $name
	}
	$w.mb.m add command -label $name \
		-command "set pref(opt,color_set) [list $c]; \
		set pref(text,color_set) [list $name]; \
		SetColor $c" \
		-background [lindex $c 0] -foreground [lindex $c 1]
	if { $width < [string length $name]} {
	    set width [string length $name]
	}
    }
    $w.mb configure -width $width
    pack $w.mb -side left
}

proc SetupDSNRequest {w} {
    global option t pref b
    frame $w

    set var "$pref(rolePrefix)dsn_request"
    OptionMenu $w.menu pref(opt,$var) pref(text,$var) \
	     [list [list 0 $t(not_request_dsn)]\
		   [list 1 $t(request_dsn)]]
    button $w.but -text $t(probe)... \
	    -command [list PrefProbeDSN $pref(rolePrefix)] -pady 0
    pack $w.menu -side top
    pack $w.but -side left
    set b($w) pref_dsn_request
    set b($w.menu) pref_dsn_request
    set b($w.but) pref_dsn_probe
}

proc SetupDSNVerbose {w} {
    global option t pref b

    foreach elem $option(dsn_verbose) {
	set pref(opt,[lindex $elem 0]) [lindex $elem 1]
    }
    frame $w
    set irow 0
    foreach cat {failed delayed delivered relayed expanded} {
	set sf $w.$cat
	label ${sf}_l -text $t($cat): -anchor e
	OptionMenu ${sf}_mbut pref(opt,$cat) pref(text,$cat) \
		[list [list none $t(rl_none)] \
		      [list status $t(rl_status)] \
		      [list notify $t(rl_notify)]]
	if {[RatIsLocked option(dsn_verbose)]} {
	    ${sf}_mbut configure -state disabled
	}
	grid ${sf}_l -row $irow -column 0 -sticky e
	grid ${sf}_mbut -row $irow -column 1 -sticky w
	incr irow
	set b(${sf}_mbut) pref_dsn_verbose
    }
    set b($w) pref_dsn_verbose
    grid columnconfigure $w 2 -weight 1
}

# PrefProbeDSN --
#
# Probe the current SMTP servers for DSN support.
#
# Arguments:
# rp - Role prefix

proc PrefProbeDSN {rp} {
    global idCnt option t fixedNormFont

    # Create identifier
    set id probeWin[incr idCnt]
    upvar #0 $id hd
    set w .$id
    set hd(w) $w

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(probe)

    if {[string compare $option(${rp}sendprot) smtp]} {
	message $w.message -aspect 600 -text $t(dsn_need_smtp)
	button $w.button -text $t(dismiss) \
		-command "RecordPos $w prefProbeDSN; destroy $w; unset $id"
	pack $w.message \
	     $w.button -side top -padx 5 -pady 5
	return
    }

    set row 0
    foreach h $option(${rp}smtp_hosts) {
	label $w.l$row -text $h -width 32 -anchor e
	label $w.a$row -textvariable ${id}($h) -font $fixedNormFont -width 32 \
		-anchor w
	grid $w.l$row -row $row -column 0 -sticky e
	grid $w.a$row -row $row -column 1 -sticky w
	incr row
    }
    button $w.button -text $t(dismiss) -command "destroy $w; unset $id" \
	    -state disabled
    grid $w.button -row $row -column 0 -columnspan 2
    Place $w prefProbeDSN
    wm protocol $w WM_DELETE_WINDOW "destroy $w; unset $id"

    foreach h $option(${rp}smtp_hosts) {
        set hd($h) $t(probing)...
	update idletasks
	if {[RatSMTPSupportDSN $h]} {
	    set hd($h) $t(supports_dsn)
	} else {
	    set hd($h) $t(no_dsn_support)
	}
    }
    $w.button configure -state normal
}

# SetupNetworkSync --
#
# Setup the network synchronization
#
# Arguments:

proc SetupNetworkSync {} {
    global t option setupNS

    # Check if window already exists
    set w .setns
    if {[winfo exists $w]} {
	wm deiconify $w
	raise $w
	return
    }

    # Initialize variables
    set setupNS(w) $w
    set setupNS(deferred) [lindex $option(network_sync) 0]
    set setupNS(disconnected) [lindex $option(network_sync) 1]
    set setupNS(runcmd) [lindex $option(network_sync) 2]
    set setupNS(cmd) [lindex $option(network_sync) 3]

    # Create window
    toplevel $w -bd 5 -class TkRat
    wm title $w $t(setup_netsync)

    checkbutton $w.cmd -variable setupNS(runcmd) -text $t(run_command)
    grid $w.cmd - -sticky w
    entry $w.cmdentry -textvariable setupNS(cmd)
    grid x $w.cmdentry -sticky ew

    checkbutton $w.def -variable setupNS(deferred) -text $t(send_deferred)
    grid $w.def - -sticky w

    checkbutton $w.dis -variable setupNS(disconnected) -text $t(sync_dis)
    grid $w.dis -  -sticky w

    frame $w.f
    button $w.f.ok -text $t(ok) -default active -command {
	destroy $setupNS(w)
	set option(network_sync) [list $setupNS(deferred) \
				       $setupNS(disconnected) \
				       $setupNS(runcmd) \
				       $setupNS(cmd)]
	unset setupNS
	SaveOptions
    }
    button $w.f.cancel -text $t(cancel) \
	    -command {destroy $setupNS(w); unset setupNS}
    pack $w.f.ok $w.f.cancel -side left -expand 1
    grid $w.f - -sticky nsew -pady 5
}

# SelectFont --
#
# Show font selection
#
# Arguments:
# f - font to select
# w - window to build

proc SelectFont {f w} {
    global pref fixedNormFont t b

    set d [ConvertFontToText $pref(opt,$f)]

    frame $w
    label $w.l -text $d -font [RatCreateFont $pref(opt,$f)] -anchor w
    pack $w.l -side left
    set b($w) pref_$f
    set b($w.l) pref_$f

    button $w.e -text $t(edit)... -padx 2 -pady 1 \
	    -command "DoEditFont $f $w.l $w"
    pack $w.e -side right -fill x
}

# ConvertFontToText --
#
# Convert a font specification to text
#
# Arguments:
# s - A font specification

proc ConvertFontToText {s} {
    global t

    if {"components" == [lindex $s 0]} {
	set d [concat [lindex $s 1] [lindex $s 2]]
	if {"bold" == [lindex $s 3]} {
	    set d "$d $t(bold)"
	}
	if {"roman" != [lindex $s 4]} {
	    set d "$d $t(italic)"
	}
	if {[lindex $s 5]} {
	    set d "$d $t(underline)"
	}
	if {[lindex $s 6]} {
	    set d "$d $t(overstrike)"
	}
	return $d
    } else {
	return [lindex $s 1]
    }

}

# DoEditFont --
#
# Show the edit font window
#
# Arguments:
# font	 - Font setting to edit
# l	 - Label to update afterwards
# parent - Parent window

proc DoEditFont {font l parent} {
    global t idCnt pref
    
    set id doEditFont[incr idCnt]
    upvar #0 $id hd

    # Initialization
    set hd(done) 0
    set hd(new_spec) $pref(opt,$font)
    set hd(old_spec) ""
    set hd(font_name) ""

    if {"components" == [lindex $hd(new_spec) 0]} {
	set hd(family) [lindex $hd(new_spec) 1]
	set hd(size) [lindex $hd(new_spec) 2]
	set hd(weight) [lindex $hd(new_spec) 3]
	set hd(slant) [lindex $hd(new_spec) 4]
	set hd(underline) [lindex $hd(new_spec) 5]
	set hd(overstrike) [lindex $hd(new_spec) 6]
	set hd(method) components
    } else {
	set hd(name) [lindex $hd(new_spec) 1]
	set hd(method) name
	set hd(family) Helvetica
	set hd(size) 12
    }

    # Create toplevel
    set w .fontedit
    toplevel $w -class TkRat
    wm title $w $t(edit_font)
    wm transient $w $parent

    # Top label
    label $w.topl -text $t(use_one_method)

    # Specification method frame
    frame $w.s -bd 1 -relief raised
    radiobutton $w.s.select -variable ${id}(method) -value components \
	    -command "UpdateFontSpec $id components"
    label $w.s.fl -text $t(family):
    set m $w.s.family.m
    menubutton $w.s.family -bd 1 -relief raised -indicatoron 1 -menu $m \
	    -textvariable ${id}(family) -width 15
    menu $m -tearoff 0
    if {![info exists pref(families)]} {
	set pref(families) [lsort -dictionary [font families]]
    }
    foreach f $pref(families) {
	$m add command -label $f -command \
		"set ${id}(family) [list $f]; UpdateFontSpec $id components"
    }
    FixMenu $m
    label $w.s.sl -text "  $t(size):"
    set m $w.s.size.m
    menubutton $w.s.size -bd 1 -relief raised -indicatoron 1 -menu $m \
	    -textvariable ${id}(size) -width 3
    menu $m -tearoff 0
    foreach s {4 5 6 7 8 9 10 11 12 13 14 15 16 18 20 22 24 26 30 36} {
	$m add command -label $s -command \
		"set ${id}(size) $s; UpdateFontSpec $id components"
    }
    checkbutton $w.s.weight -text "$t(bold) " -onvalue bold -offvalue normal \
	    -variable ${id}(weight) -command "UpdateFontSpec $id components"
    checkbutton $w.s.italic -text "$t(italic) " -onvalue italic -offvalue roman\
	    -variable ${id}(slant) -command "UpdateFontSpec $id components"
    checkbutton $w.s.underline -text "$t(underline) " \
	    -variable ${id}(underline) -command "UpdateFontSpec $id components"
    checkbutton $w.s.overstrike -text $t(overstrike) \
	    -variable ${id}(overstrike) -command "UpdateFontSpec $id components"

    pack $w.s.select \
	 $w.s.fl $w.s.family \
	 $w.s.sl $w.s.size \
	 $w.s.weight \
	 $w.s.italic \
	 $w.s.underline \
	 $w.s.overstrike -side left -pady 2

    # Name method frame
    frame $w.n -bd 1 -relief raised 
    radiobutton $w.n.select -variable ${id}(method) -value name \
	    -command "UpdateFontSpec $id name"
    label $w.n.l -text $t(name):
    entry $w.n.e -width 20 -textvariable ${id}(name)
    set hd(updateButton) $w.n.set
    button $w.n.set -text $t(update) -command "UpdateFontSpec $id name" -bd 1
    pack $w.n.select \
	 $w.n.l \
	 $w.n.e \
	 $w.n.set -side left -pady 2
    trace variable hd(name) w "UpdateFontUpdateButton $id"
    UpdateFontUpdateButton $id

    # Sample text
    message $w.sample -text $t(ratatosk) -aspect 200 -justify left
    set hd(sample) $w.sample

    # Buttons
    OkButtons $w $t(ok) $t(cancel) "set ${id}(done)"
    set hd(okbutton) $w.buttons.ok

    # Pack things
    pack $w.topl \
	 $w.s \
	 $w.n -side top -fill x -pady 2 -padx 2
    pack $w.buttons -side bottom -fill x -pady 2 -padx 2
    pack $w.sample -fill x -pady 2 -padx 2

    # Bindings
    bind $w.n.e <Tab> "UpdateFontSpec $id name"
    bind $w.n.e <Return> "UpdateFontSpec $id name; break"

    # Update sample font
    UpdateFont $id

    # Show window and wait for completion
    Place $w editFont
    ModalGrab $w
    pack propagate $w 0
    tkwait variable ${id}(done)

    # Finalization
    RecordPos $w editFont
    destroy $w
    set pref(opt,$font) $hd(old_spec)
    if {"" != $hd(font_name)} {
	font delete $hd(font_name)
    }
    unset hd
    $l configure -text [ConvertFontToText $pref(opt,$font)] \
	    -font [RatCreateFont $pref(opt,$font)]
}

# UpdateFontUpdateButton --
#
# Set state of the update button
#
# Arguments:
# handler - Handler of font window
# args    - Possibly standard trace args

proc UpdateFontUpdateButton {handler args} {
    upvar #0 $handler hd

    if {"" != $hd(name) && "name" == $hd(method)} {
	set state normal
    } else {
	set state disabled
    }
    $hd(updateButton) configure -state $state
}

# UpdateFontSpec --
#
# Update the shown font
#
# Arguments:
# handler - Handler of font window
# method  - which method to use

proc UpdateFontSpec {handler method} {
    upvar #0 $handler hd

    if {"components" == $method} {
	set hd(new_spec) [list components $hd(family) $hd(size) $hd(weight) \
				    $hd(slant) $hd(underline) $hd(overstrike)]
	set hd(method) components
    } else {
	set hd(new_spec) [list name $hd(name)]
	set hd(method) name
    }
    UpdateFont $handler
}

# UpdateFont --
#
# Update the sample text
#
# Arguments:
# handler - Handler of font window

proc UpdateFont {handler} {
    upvar #0 $handler hd
    global t

    if {"$hd(new_spec)" == "$hd(old_spec)"} {
	return
    }
    if {[lindex $hd(new_spec) 0] == "components"} {
	if {"" == $hd(font_name)} {
	    set op create
	    set hd(font_name) fontedit
	} else {
	    set op configure
	}
	font $op $hd(font_name) \
		-family [lindex $hd(new_spec) 1] \
		-size -[lindex $hd(new_spec) 2] \
		-weight [lindex $hd(new_spec) 3] \
		-slant [lindex $hd(new_spec) 4] \
		-underline [lindex $hd(new_spec) 5] \
		-overstrike [lindex $hd(new_spec) 6]
	set fn $hd(font_name)
    } else {
	set fn [lindex $hd(new_spec) 1]
    }

    set hd(old_spec) $hd(new_spec)

    if {[catch {$hd(sample) configure -font $fn} err]} {
	set okstatus disabled
	set msg $t(invalid_font)
	set aspect 1000
	$hd(sample) configure -font fixed
    } else {
	set okstatus normal
	set msg $t(ratatosk)
	set aspect 200
    }
    $hd(sample) configure -text $msg -aspect $aspect
    $hd(okbutton) configure -state $okstatus
}

# SetupRole --
#
# Setup the roles buttons
#
# Arguments:
# w - Window to build

proc SetupRole {w} {
    global t pref option

    frame $w

    # Find max length of labels
    set l1 [string length $t(set_as_default)]
    set l2 [string length $t(delete)]
    if {$l1 > $l2} {
	set l $l1
    } else {
	set l $l2
    }

    # Get role id and check for enablement
    set id [lindex [split $pref(rolePrefix) ,] 0]
    if {$id == $option(default_role)} {
	set s disabled
    } else {
	set s normal
    }
    button $w.default -text $t(set_as_default) -pady 0 -width $l -state $s \
	    -command "\
	set option(default_role) $id; \
	$w.default configure -state disabled; \
	$w.delete configure -state disabled; \
	PrefPopulateRoles; \
	$pref(tree) redraw \
    "
    button $w.delete -text $t(delete) -pady 0 -width $l -state $s \
	    -command "PrefDeleteRole $id"
    pack $w.default $w.delete -side top -anchor w -pady 5
}

# PrefDeleteRole --
#
# Delete a role
#
# Arguments:
# id - role id

proc PrefDeleteRole {id} {
    global option pref vFolderDef t

    set fdls [VFoldersUsesRole $id]
    if {"" != $fdls} {
	Popup "$t(cant_delete_role_used): $fdls"
	return
    }
    set value [RatDialog $pref(pane) $t(delete) $t(are_you_sure_delete_role) \
	    {} 1 $t(delete) $t(cancel)]
    if {1 == $value} {
	return
    }
    foreach v [array names option $id,*] {
	unset option($v)
    }
    set i [lsearch -exact $option(roles) $id]
    set option(roles) [lreplace $option(roles) $i $i]
    set pref(selected) ""
    foreach c [winfo children $pref(pane)] {
	destroy $c
    }
    PrefPopulateRoles
    $pref(tree) redraw
    SaveOptions
}

# PrefTraceProc --
#
# Preferences tracing procedure
#
# Arguments:
# name1, name2 - Name of variable changed
# op           - Operation

proc PrefTraceProc {name1 name2 op} {
    global pref option

    if {!$pref(traceChanges)} {return}

    if {[regexp {^opt,} $name2]} {
	if {[info exists pref(opt,failed)]} {
	    set pref(opt,dsn_verbose) [list [list failed $pref(opt,failed)] \
		    [list delayed $pref(opt,delayed)] \
		    [list delivered $pref(opt,delivered)] \
		    [list relayed $pref(opt,relayed)] \
		    [list expanded $pref(opt,expanded)]]
	}

	set state disabled
	foreach v $pref(vars,$pref(lastPref)) {
	    if {$option($v) != $pref(opt,$v)} {
		set state normal
		break
	    }
	}
	foreach but $pref(traceButtons) {
	    $but configure -state $state
	}
    }
}

# PrefPopulateRoles --
#
# Populate the roles branch in the preferences tree
#
# Arguments:

proc PrefPopulateRoles {} {
    global pref option t
    
    $pref(rnode) clear
    foreach r $option(roles) {
	set name $option($r,name)
	if {$r == $option(default_role)} {
	    set name "$name ($t(default))"
	}
	set node [$pref(rnode) add folder -label $name -state closed \
		-id [list role $r roles]]
	foreach p {address sending advanced} {
	    $node add leaf -label $t($p) -id [list role $r roles,$p]
	}
    }
    if {"role" == [lindex $pref(selected) 0]} {
	$pref(tree) select $pref(selected)
    }
}

# PrefCreateRole --
#
# Create a new role
#
# Arguments:

proc PrefCreateRole {} {
    global pref option t

    foreach rid $option(roles) {
	lappend ids [string range $rid 1 end]
    }
    set id "r[expr {[lindex [lsort -integer $ids] end]+1}]"
    foreach vn [array names option $option(default_role),*] {
	set v [lindex [split $vn ,] 1]
	set option($id,$v) $option($vn)
    }
    set option($id,name) $t(new_role)
    lappend option(roles) $id
    PrefSortRoles
    SaveOptions

    PrefPopulateRoles
    $pref(tree) select [list role $id roles]
    $pref(tree) redraw
    PrefSelectPane [list role $id roles]
}

# PrefSortRoles --
#
# Sort the list of roles. This function updates the option(roles) list
#
# Arguments:

proc PrefSortRoles {} {
    global option

    set option(roles) [lsort -command PrefCmpRoles $option(roles)]
}
proc PrefCmpRoles {a b} {
    global option

    return [string compare $option($a,name) $option($b,name)]
}

# SetupDefaultSave --
#
# Setup the default save folder
#
# Arguments:
# w - Window to build

proc SetupDefaultSave {w} {
    global t pref option b

    menubutton $w -textvariable pref(text,save_outgoing) -indicatoron 1 \
	    -relief raised -menu $w.m -pady 1
    menu $w.m -postcommand "PopulateDefaultSave $w.m"
    if {"" == $pref(opt,$pref(rolePrefix)save_outgoing)} {
	SelectDefaultSave {}
    } else {
	SelectDefaultSave $pref(opt,$pref(rolePrefix)save_outgoing)
    }
    set b($w) pref_save_outgoing
}

proc PopulateDefaultSave {m} {
    global t

    $m delete 1 end
    VFolderBuildMenu $m 0 "SelectDefaultSave" 1
    $m add command -label "-- $t(none) --" -command {SelectDefaultSave ""}
}

proc SelectDefaultSave {id} {
    global t pref vFolderDef

    set pref(opt,$pref(rolePrefix)save_outgoing) $id
    if {"" == $id} {
	set pref(text,save_outgoing) "-- $t(none) --"
    } else {
	set pref(text,save_outgoing) [lindex $vFolderDef($id) 0]
    }
}
