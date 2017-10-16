# preferences.tcl --
#
# This file controls the preferences window. There is just one preferences
# window, if the user requests a new preferences window when the window
# already exists, it is just unmapped and raised (if possible).
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
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

# Images
set checkmark_img [image create photo -data {
    R0lGODlhDwAPAIQcADSNHjWOHzeQIjmRIzuTJj6VKT+WKkGXLEKYLUicM0ue
    Nk6gOV+sTGqzWG61XHy+bH2/boHBconFeovGfJzQkKTUmafVm6nWnqvXoLPc
    qbzgs8Xkvf///////////////yH5BAEKAB8ALAAAAAAPAA8AAAUv4CeOZGme
    aKp+E7R+EfYuzHs0r5Og2VB9EsIGpUEEPoKH6gIoGF4KgOU1obyu2BAAOw==}]
set error_img [image create photo -data {
    R0lGODlhDwAPAMZAALgEALoGCcUBDMUFAM8ABNEAAKcUFtEBFNsAAtsADd4B
    AL4NF6UaItwAGNsAKekAA84QJcMoI8MnMtEgPr5BULxMU7xOXLNXUs5MStdF
    WOBFTeJDU+VDWvBBSPg9VrhhcNBdYOhSYNpbb/xidsaAhvFyj99/feh+ktmJ
    jf55gr+XjfCChbqenNqSrt2WosOrtdmps/+kpPatoPmvtfq7t/jQy9fv09z7
    6uP/+fH/6v/1/O3/+/758f/76/z+6v7+/v//////////////////////////
    ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////yH5BAEKAEAA
    LAAAAAAPAA8AAAfCgECCgh0BAwcYg4otFgwJCwILDRAiixkLAw0yMTQeCA0U
    gxaYBQs5PzgmAAMCFUAqEAUICAGnO6oFCg0nGAmzBQc8Oj8jCAUEAiEQBwUD
    AAc1OzkbCM4BEQerCAIJM9IbCc8FBRG+AAQHKTk+sQAPCREhBQ8AshwlJAIE
    A7QXQAYPBhAIYGMHjxQAzi1wAeRDtQcFfKBKMS+BKyAvJgQgkKDHjRsrCBCQ
    oAhGBQgKPITI4ODARUVAWIAwMACCBhSKAgEAOw==}]


# BuildPreferences --
#
# Builds the preferences window.
#
# Arguments:

proc BuildPreferences {} {
    global t b pref option

    # Initialize data table
    foreach l [GetLanguages] {
	lappend lang [lrange $l 0 1]
    }

    # Give initial values
    set spell_dictionaries [list [list auto $t(auto)]]
    set pref(dictionaries) {}

    # Font families
    if {![info exists pref(families)]} {
        foreach f [lsort -dictionary [font families]] {
            lappend pref(families) [list $f [string totitle $f]]
        }
    }

    # The lists here can have the following elements
    # option var label_id value_list
    # checkbutton var label_id onvalue offvalue
    # bool var label_id
    # entry var label_id
    # entry_unit var label_id unit
    # spinbox var label_id from increment to
    # spinbox_unit var label_id unit from increment to
    # special var label_id special_proc
    # label label_text anchor
    # custom custom_proc
    set pref(roles) \
        [list \
             [list entry name role_name] \
             [list entry signature signature_file] \
             [list special save_outgoing save_out SetupDefaultSave] \
             [list custom SetupRole]]
    set pref(roles,address) \
        [list \
             [list entry from use_from_address] \
             [list entry bcc default_bcc] \
             [list entry reply_to default_reply_to] \
             [list message $t(tip) $t(test_by)] \
             [list entry pgp_keyid pgp_keyid]]
    set pref(roles,sending) \
        [list \
             [list option sendprot sendprot \
                  [list [list smtp $t(smtp)] \
                       [list prog $t(user_program)]]] \
             [list entry smtp_hosts smtp_hosts] \
             [list entry smtp_user smtp_user] \
             [list entry smtp_passwd passwd] \
             [list bool validate_cert ssl_check_cert] \
             [list entry sendprog sendprog] \
             [list bool sendprog_8bit sendprog_8bit] \
             [list bool same_sending_prefs same_sending_prefs]]
    set pref(roles,advanced) \
        [list \
             [list label $t(normally_ok) center] \
             [list label $t(unqual_adr_domain) w] \
             [list entry uqa_domain domain] \
             [list label $t(smtp_from_long) w] \
             [list entry smtp_helo host]]
    set pref(roles,pgp) \
        [list \
             [list bool sign_outgoing sign_outgoing] \
             [list special sign_as sign_as PrefSetupSignAs]]
    set pref(appearance) \
        [list \
             [list option language language $lang] \
             [list special color_set color_scheme SetupColor] \
             [list option font_family_prop prop_font_family $pref(families)] \
             [list option font_family_fixed fixed_font_family $pref(families)]\
             [list spinbox font_size font_size 1 1 120] \
             [list special watcher_font watcher_font\
                  [list SelectFont watcher_font]] \
             [list bool useinputmethods useinputmethods] \
            ]
    set pref(general) \
        [list \
             [list bool iconic startup_iconic] \
             [list option start_online_mode start_mode \
                  [list [list last $t(som_last)] \
                       [list online $t(som_online)] \
                       [list offline $t(som_offline)]]] \
             [list option start_selection start_selection \
                  [list [list first $t(first_message)] \
                       [list last $t(last_message)] \
                       [list first_new $t(first_new_message)] \
                       [list before_new $t(before_first_new_message)]]] \
             [list entry list_format list_format] \
             [list entry date_format date_format] \
             [list entry show_header_selection show_header_selection] \
             [list spinbox_unit checkpoint_interval \
                  checkpoint_interval $t(seconds) 0 30 1000000] \
             [list bool expunge_on_close expunge_on_close] \
            ]
    set pref(html) \
        [list \
             [list special url_viewer url_viewer SetupURLViewer] \
             [list label "" center] \
             [list label $t(html_messages) center] \
             [list bool prefer_other_over_html avoid_html] \
             [list bool html_show_images html_show_images] \
             [list spinbox html_min_image_size html_min_image_size \
                  0 1 1000000] \
             [list entry html_proxy_host html_proxy_host] \
             [list spinbox html_proxy_port html_proxy_port 1 1 65535] \
             [list spinbox_unit html_timeout html_timeout $t(ms) \
                  0 1000 10000000] \
            ]
    set pref(new_messages) \
        [list \
             [list spinbox_unit watcher_time watcher_intervals $t(seconds) \
                 0 1 1000000] \
             [list spinbox_unit watcher_max_height max_height $t(lines) \
                 1 1 1000] \
             [list option watcher_show show_messages \
                  [list [list new $t(new)] [list all $t(all)]]] \
             [list entry watcher_format list_format] \
             [list spinbox watcher_bell bell_ringings 0 1 1000] \
            ]
    set pref(composing) \
        [list \
             [list bool always_editor always_use_external_editor] \
             [list entry compose_headers headers] \
             [list spinbox_unit compose_backup compose_backup $t(seconds) \
                 0 10 1000000] \
             [list spinbox_unit compose_last_chance compose_last_chance \
                  $t(seconds) 0 30 1000000] \
             [list entry re_regexp reply_regexp] \
             [list entry attribution attribution] \
             [list bool wrap_cited wrap_cited] \
             [list bool skip_sig skip_sig] \
             [list bool reply_bottom reply_bottom] \
            ]
    set pref(spell_checking) \
        [list \
             [list option def_spell_dict dictionary $spell_dictionaries] \
             [list special auto_dicts auto_dicts SetupCheckDicts] \
             [list special spell_path spell_cmd SetupSpellPath] \
            ]
    set pref(dbase) \
        [list \
             [list option def_extype extype \
                  [list [list none $t(none)] \
                       [list remove $t(remove)] \
                       [list incoming $t(incoming)] \
                       [list backup $t(backup)]]] \
             [list entry_unit def_exdate exdate $t(days)] \
             [list entry dbase_backup dbase_backup] \
            ]
    set pref(paths) \
        [list \
             [list entry tmp tmp_dir] \
             [list entry print_command print_command] \
             [list entry terminal terminal_command] \
             [list entry ssh_path ssh_path] \
             [list entry tnef tnef_path] \
            ]
    set pref(pgp) \
        [list \
             [list option pgp_version pgp_version \
                  [list [list 0 $t(none)] [list gpg-1 GPG-1] [list 2 PGP-2] \
                       [list 5 PGP-5] [list 6 PGP-6] [list auto $t(auto)]]] \
             [list entry pgp_path pgp_path] \
             [list entry pgp_args pgp_extra_args] \
             [list entry pgp_keyring pgp_keyring] \
             [list bool cache_pgp cache_passwd] \
             [list spinbox_unit cache_pgp_timeout cache_timeout $t(seconds)\
                 0 30 1000000] \
             [list bool pgp_encrypt encrypt_outgoing] \
            ]

    # Create top window structures
    set w .pref
    toplevel $w -class TkRat
    wm title $w $t(preferences)
    set pref(scrollpane) $w.pane
    set pref(pane) [rat_scrollframe::create $w.pane -bd 2 -relief flat]
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

    # Preferences
    foreach n {appearance general html new_messages composing spell_checking
        dbase paths pgp} {
	set n0 [lindex $n 0]
	set name $t($n0)
	if {1 != [llength $n]} {
	    set node [$topnode add folder -label $name -state closed \
		    -id [list option $n0]]
	    foreach n1 [lindex $n 1] {
		$node add leaf -label $t($n1) -id [list option $n0,$n1]
	    }
	} else {
	    $topnode add leaf -label $name -id [list option $n0]
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
                      ::tkrat::winctl::RecordGeometry preferences $w $w.pane; \
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

    ::tkrat::winctl::SetGeometry preferences $w $w.pane
    
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
    rat_scrollframe::recalc $pref(scrollpane)
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
    global t b option pref propNormFont tk_version

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
	}
	switch [lindex $p 0] {
	    entry {
		label $w.r${row}_lab -text $t([lindex $p 2]):
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
		entry $w.r${row}_item -textvariable pref(opt,$var)
		grid $w.r${row}_item - -row $row -column 1 -sticky we
		set b($w.r${row}_item) pref_$bvar
	    }
	    entry_unit {
		label $w.r${row}_lab -text $t([lindex $p 2]):
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
		entry $w.r${row}_item -textvariable pref(opt,$var)
		label $w.r${row}_unit -text ([lindex $p 3])
		grid $w.r${row}_item -row $row -column 1 -sticky we
		grid $w.r${row}_unit -row $row -column 2 -sticky w
		set b($w.r${row}_item) pref_$bvar
	    }
	    spinbox {
		label $w.r${row}_lab -text $t([lindex $p 2]):
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
                if {$tk_version >= 8.4} {
                    spinbox $w.r${row}_item -textvariable pref(opt,$var) \
                        -from [lindex $p 3] -increment [lindex $p 4] \
                        -to [lindex $p 5] -validate all \
                        -vcmd [list ValidateInt %P [lindex $p 3] [lindex $p 5]]
                } else {
                    entry $w.r${row}_item -textvariable pref(opt,$var) \
                        -validate all \
                        -vcmd [list ValidateInt %P [lindex $p 3] [lindex $p 5]]
                }
		grid $w.r${row}_item - -row $row -column 1 -sticky we
		set b($w.r${row}_item) pref_$bvar
	    }
	    spinbox_unit {
		label $w.r${row}_lab -text $t([lindex $p 2]):
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
                if {$tk_version >= 8.4} {
                    spinbox $w.r${row}_item -textvariable pref(opt,$var) \
                        -from [lindex $p 4] -increment [lindex $p 5] \
                        -to [lindex $p 6] -validate all \
                        -vcmd [list ValidateInt %P [lindex $p 4] [lindex $p 6]]
                } else {
                    entry $w.r${row}_item -textvariable pref(opt,$var) \
                        -validate all \
                        -vcmd [list ValidateInt %P [lindex $p 4] [lindex $p 6]]
                }
		label $w.r${row}_unit -text ([lindex $p 3])
		grid $w.r${row}_item -row $row -column 1 -sticky we
		grid $w.r${row}_unit -row $row -column 2 -sticky w
		set b($w.r${row}_item) pref_$bvar
	    }
	    option {
		label $w.r${row}_lab -text $t([lindex $p 2]):
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
		OptionMenu $w.r${row}_item $var [lindex $p 3]
		grid $w.r${row}_item - -row $row -column 1 -sticky w
		set b($w.r${row}_item) pref_$bvar
	    }
	    checkbutton {
		checkbutton $w.r${row}_cb -text $t([lindex $p 2]) \
		    -variable pref(opt,$var) -onvalue [lindex $p 3] \
		    -offvalue [lindex $p 4]
		grid $w.r${row}_cb - -row $row -column 1 -sticky w
		set b($w.r${row}_cb) pref_$bvar
	    }
	    bool {
		checkbutton $w.r${row}_cb -text $t([lindex $p 2]) \
		    -variable pref(opt,$var)
		if {$pref(opt,$var)} {
		    set pref(opt,$var) 1
		} else {
                    set pref(opt,$var) 0
		}
		grid $w.r${row}_cb - -row $row -column 1 -sticky w
		set b($w.r${row}_cb) pref_$bvar
	    }
	    special {
		label $w.r${row}_lab -text $t([lindex $p 2]):
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
		eval "[lindex $p 3] $w.r${row}_item"
		grid $w.r${row}_item - -row $row -column 1 -sticky we
	    }
	    label {
		frame $w.r${row}_lab
		grid $w.r${row}_lab -row $row -columnspan 2 -sticky new -pady 1
		rat_flowmsg::create $w.r${row}_lab.l -text [lindex $p 1] \
		    -anchor [lindex $p 2] -padx 0 -font $propNormFont
		pack $w.r${row}_lab.l -fill both -expand 1
	    }
	    message {
		label $w.r${row}_lab -text [lindex $p 1]
		frame $w.r${row}_message
		grid $w.r${row}_lab -row $row -sticky ne -pady 1
		grid $w.r${row}_message -row $row -column 1 -sticky we
		rat_flowmsg::create $w.r${row}_message.m \
		    -text [lindex $p 2] -padx 0 -anchor w
		pack $w.r${row}_message.m -fill both -expand 1
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

# PrefValidate --
#
# Check for errors in new preferences
#
# Arguments:
# parent -	Parent window

proc PrefValidate {parent} {
    global option pref t folderWindowList tk_version

    set rp $pref(rolePrefix)
    switch $pref(lastPref) {
        general {
            set r [RatCheckListFormat $pref(opt,list_format)]
            if {"ok" != $r} {
                Popup $r $parent
                return "fail"
            }
        }
	paths {
	    if {![regexp %p $pref(opt,print_command)]} {
		Popup $t(no_pp_in_print_command) $parent
                return "fail"
	    }
	}
	composing {
	    if {[catch {regexp -nocase $pref(opt,re_regexp) ""} e]} {
		Popup "$t(re_regexp_error): $e" $parent
		return "fail"
	    }
	    if {[catch {regexp -nocase $pref(opt,citexp) ""} e]} {
		Popup [format $t(illegal_regexp): $e] $parent
		return "fail"
	    }
	}
	roles,sending {
	    if {[string compare $option(${rp}sendprog) \
		     $pref(opt,${rp}sendprog)]
		&& ![file executable [lindex $pref(opt,${rp}sendprog) 0]]} {
		Popup $t(warning_sendprog) $parent
		return "fail"
	    }
	}
        watcher_format {
            set r [RatCheckListFormat $pref(opt,watcher_format)]
            if {"ok" != $r} {
                Popup $r $parent
                return "fail"
            }
        }
    }
    return "ok"
}

# PrefApply --
#
# Applies any changes to the preferences made in the current window.
#
# Arguments:
# parent -	Parent window

proc PrefApply {parent} {
    global option pref t folderWindowList tk_version

    if {"ok" != [PrefValidate $parent]} {
        return
    }

    set hasChanged 0
    set needRestart 0
    set rp $pref(rolePrefix)
    switch $pref(lastPref) {
	appearance {
	    if {[string compare $option(useinputmethods) \
		    $pref(opt,useinputmethods)] && 8.3 <= $tk_version} {
		tk useinputmethods $pref(opt,useinputmethods)
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
	    if {$pref(opt,${rp}same_sending_prefs) != 
		$option(${rp}same_sending_prefs)} {
		if {1 == $pref(opt,${rp}same_sending_prefs)} {
		    if [CheckSameSendingPrefs $rp] {
			set pref(opt,${rp}same_sending_prefs) 0
			return
		    }
		}
		set hasChanged 1
		foreach r $option(roles) {
		    set pref(opt,${r},same_sending_prefs) \
			$pref(opt,${rp}same_sending_prefs)
		}
	    }
	}
        html {
            if {[string compare $option(html_proxy_host) \
                     $pref(opt,html_proxy_host)]} {
                ::http::config -proxyhost $pref(opt,html_proxy_host)
                if {![string length $pref(opt,html_proxy_host)]} {
                    set pref(opt,html_proxy_port) ""
                }
            }
            if {[string compare $option(html_proxy_port) \
                     $pref(opt,html_proxy_port)]} {
                ::http::config -proxyport $pref(opt,html_proxy_port)
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
					default_folder pgp_enable
					override_fonts prop_norm prop_light
					fixed_norm fixed_bold watcher_font
	                                charset prop_big fixed_italic
                                        color_set} \
				$opt]} {
		set needRestart 1
	    }
	}
    }

    if {$hasChanged} {
	switch $pref(lastPref) {
	    general {
		foreach f [array names folderWindowList] {
		    Sync $f update
		}
	    }
	    pgp {
		InitPgp
		foreach v {pgp_version pgp_path pgp_keyring} {
		    set pref(opt,$v) $option($v)
		}
	    }
	    roles,sending {
		if {1 == $option(${rp}same_sending_prefs)} {
		    foreach r $option(roles) {
			foreach v {sendprot smtp_hosts validate_cert
			    sendprog sendprog_8bit smtp_user smtp_passwd} {
			set option(${r},$v) $option(${rp}$v)
			set pref(opt,${r},$v) $option(${rp}$v)
		    }
		}
	    }
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
                    set opt [string range $n 4 end]
                    set pref(opt,$opt) $option($opt)
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
# varid -	Variable to set to value
# values  -	A list of lists which describes the values of this button

proc OptionMenu {w varid values} {
    upvar \#0 pref(opt,$varid) var
    global pref

    menubutton $w -textvariable pref(text,$varid) -indicatoron 1 \
		  -relief raised -menu $w.m -pady 1
    set pref(w,$varid) $w
    menu $w.m -tearoff 0
    PrefPopulateOptionsMenu $varid $values

    trace variable var w "PrefTraceOptionProc $varid"
}

proc PrefPopulateOptionsMenu {varid values} {
    upvar \#0 pref(opt,$varid) var
    upvar \#0 pref(text,$varid) text
    global pref

    set pref(values,$varid) $values
    $pref(w,$varid).m delete 0 end
    set width 10
    set text ""
    foreach elem $values {
	if {![string compare [lindex $elem 0] $var]} {
	    set text [lindex $elem 1]
	}
	$pref(w,$varid).m add command -label [lindex $elem 1] \
            -command "set pref(opt,$varid) [list [lindex $elem 0]]"
	if { $width < [string length [lindex $elem 1]]} {
	    set width [string length [lindex $elem 1]]
	}
    }
    if {"" == $text && 0 < [llength $values]} {
        set text [lindex [lindex $values 0] 1]
    }
    $pref(w,$varid) configure -width $width
}

proc PrefTraceOptionProc {varid args} {
    upvar \#0 pref(opt,$varid) var
    upvar \#0 pref(text,$varid) text
    upvar \#0 pref(values,$varid) values

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
    foreach c {
        {\#dde3eb black white black}
        {PeachPuff2 black white black}
        {SlateBlue1 black white black}
        {SteelBlue4 white LightBlue black}
        {SkyBlue1 black white black}
        {aquamarine2 black white black}
        {SpringGreen4 black PaleGreen black}
        {gray85 black gray85 black}} {
	set name $t([lindex $c 0])
	if {![string compare $c $option(color_set)]} {
	    set pref(text,color_set) $name
	}
	$w.mb.m add command -label $name \
		-command "set pref(opt,color_set) [list $c]; \
		set pref(text,color_set) [list $name]" \
		-background [lindex $c 0] -foreground [lindex $c 1]
	if { $width < [string length $name]} {
	    set width [string length $name]
	}
    }
    $w.mb configure -width $width
    pack $w.mb -side left
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
    entry $w.cmdentry -textvariable setupNS(cmd)
    grid $w.cmd $w.cmdentry -sticky w

    checkbutton $w.def -variable setupNS(deferred) -text $t(send_deferred)
    grid $w.def - -sticky w

    checkbutton $w.dis -variable setupNS(disconnected) -text $t(sync_folders)
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
    grid $w.f - -sticky nsew
}

# SelectFont --
#
# Show font selection
#
# Arguments:
# f - font to select
# w - window to build

proc SelectFont {f w} {
    global pref fixedNormFont t b prefFontLabel

    set d [ConvertFontToText $pref(opt,$f)]

    set prefFontLabel($f) $w.l
    frame $w
    label $w.l -text $d -font [RatCreateFont $pref(opt,$f)] -anchor w
    pack $w.l -side left
    set b($w) pref_$f
    set b($w.l) pref_$f

    button $w.e -text $t(edit)... -command "::tkrat::fontedit::edit $f $w.l $w"
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
	$pref(tree) redraw; \
	SaveOptions \
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
	foreach p {address sending pgp advanced} {
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

    $m delete 0 end
    VFolderBuildMenu $m 0 "SelectDefaultSave" 1
    $m add command -label "-- $t(none) --" -command {SelectDefaultSave ""}
    FixMenu $m
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

# CheckSameSendingPrefs --
#
# Checks if all roles uses the same sending preferences as the ones
# currently being shown. If not show a dialog and give the user the
# option to abort
#
# Arguments:
# rp - role we are currently editing

proc CheckSameSendingPrefs {rp} {
    global option pref t
    
    set differs 0
    foreach r $option(roles) {
	foreach v {sendprot smtp_hosts validate_cert sendprog sendprog_8bit} {
	    if {$option(${r},$v) != $pref(opt,${rp}$v)} {
		set differs 1
		break
	    }
	}
    }

    if {1 == $differs} {
	return [RatDialog .pref $t(send_settings_differs) \
		    $t(roles_use_other_send_settings) {} \
		    1 $t(continue) $t(cancel)]
    } else {
	return 0
    }
}

# PrefSetupSignAs --
#
# Draw the sign-as parts of the preference dialog.
#
# Arguments:
# w - Window to build

proc PrefSetupSignAs {w} {
    global pref

    SetupSignAsWidget $w pref(opt,$pref(rolePrefix)sign_as)
}

# SetupCheckDicts --
#
# Setup the check dictionaries stuff
#
# Arguments:
# w - Window to build

proc SetupCheckDicts {w} {
    global pref b t

    frame $w -bd 2
    scrollbar $w.scroll \
        -bd 1 \
        -highlightthickness 0 \
        -command "$w.list yview"
    listbox $w.list \
        -yscroll "$w.scroll set" \
        -bd 1 \
        -exportselection false \
        -highlightthickness 0 \
        -selectmode multiple
    button $w.mark_all -text $t(mark_all) -command "CheckDictsMarkAll $w"
    button $w.clear_all -text $t(clear_all) -command "CheckDictsClearAll $w"
    grid $w.list - - $w.scroll -sticky nsew
    grid x $w.mark_all $w.clear_all x
    grid rowconfigure $w 0 -weight 1
    grid columnconfigure $w 0 -weight 1

    set pref(list,auto_dicts) $w.list
    set pref(dictionaries_state) normal
    PrefPopulateCheckDicts
    $w.list configure -height 10

    bind $w.list <<ListboxSelect>> {CheckDictsSelect %W}
    set b($w.list) pref_auto_dicts
}

proc PrefPopulateCheckDicts {} {
    global pref tk_version

    if {$tk_version >= 8.4} {
        $pref(list,auto_dicts) configure -state normal
    }
    $pref(list,auto_dicts) delete 0 end
    foreach l $pref(dictionaries) {
        $pref(list,auto_dicts) insert end [string totitle $l]
        if {-1 != [lsearch -exact $pref(opt,auto_dicts) $l]
            || 0 == [llength $pref(opt,auto_dicts)]} {
            $pref(list,auto_dicts) selection set end
        }
    }
    if {$tk_version >= 8.4} {
        $pref(list,auto_dicts) configure -state $pref(dictionaries_state)
    }
}

proc CheckDictsSelect {w} {
    global pref

    set n {}

    if {$pref(dictionaries_state) == "normal"} {
        foreach s [$w curselection] {
            lappend n [lindex $pref(dictionaries) $s]
        }
    }
    set pref(opt,auto_dicts) $n
}

proc CheckDictsMarkAll {w} {
    global pref
    set pref(opt,auto_dicts) $pref(dictionaries)
    $pref(list,auto_dicts) selection set 0 end
}

proc CheckDictsClearAll {w} {
    global pref
    set pref(opt,auto_dicts) {}
    $pref(list,auto_dicts) selection clear 0 end
}

# SetupSpellPath --
#
# Setup the spell checker path entry
#
# Arguments:
# w - Window to build

proc SetupSpellPath {w} {
    global pref b checkmark_img

    frame $w

    entry $w.entry -textvariable pref(opt,spell_path)
    label $w.status -image $checkmark_img

    pack $w.status -side right -fill y
    pack $w.entry -fill x -pady 2

    set b($w.entry) pref_spell_path
    set pref(last,spell_path) ""
    trace variable pref(opt,spell_path) w [list SpellPathChanged $w.status]
    bind $w.entry <Map> [list UpdateSpellPath $w.status 0]
    bind $w.entry <FocusOut> [list UpdateSpellPath $w.status 0]
    UpdateSpellPath $w.status 1
}

# SpellPathChanged --
#
# Called when the pref(opt,spell_path) variable has been updated
#
# Arguments:
# lab - label argument to UpdateSpellPath
# normal trace function arguments

proc SpellPathChanged {lab args} {
    global pref

    if {[info exists pref(spell_path_afterid)]} {
        after cancel $pref(spell_path_afterid)
    }
    set pref(spell_path_afterid) [after 1000 UpdateSpellPath $lab 0]
}

# UpdateSpellPath --
#
# Check which dictionaries the spelling-checker provides and update
# the display accordingly
#
# Arguments:
# lab   - label to configure icon of
# force - if true update everything even if the value has not changed

proc UpdateSpellPath {lab force} {
    global t pref option b checkmark_img error_img

    # Cancel any outstanding calls
    if {[info exists pref(spell_path_afterid)]} {
        after cancel $pref(spell_path_afterid)
    }

    # Ignore if unchanged
    if {$pref(last,spell_path) == $pref(opt,spell_path) && !$force} {
        return
    }

    set old_spell_path $option(spell_path)
    set option(spell_path) $pref(opt,spell_path)
    set spell_dictionaries [list [list auto $t(auto)]]
    set pref(dictionaries) [lsort [rat_spellutil::get_dicts 1]]
    set option(spell_path) $old_spell_path
    set pref(last,spell_path) $pref(opt,spell_path)
    foreach l $pref(dictionaries) {
        lappend spell_dictionaries [list $l [string totitle $l]]
    }
    PrefPopulateOptionsMenu def_spell_dict $spell_dictionaries

    if {0 == [llength $pref(dictionaries)]} {
        if {"" == [rat_spellutil::get_cmd]} {
            set err no_spell
        } else {
            set err no_dictionaries
        }
        lappend pref(dictionaries) $t($err)
        set pref(dictionaries_state) disabled
        set b($lab) $err
        $lab configure -image $error_img
    } else {
        set pref(dictionaries_state) normal
        $lab configure -image $checkmark_img
        catch {unset b($lab)}
    }
    PrefPopulateCheckDicts
}

# SetupURLViewer --
#
# Setup the url viewer entries
#
# Arguments:
# w - Window to populate

proc SetupURLViewer {w} {
    global pref t option b

    foreach var {url_behavior browser_cmd} {
        set pref(opt,$var) $option($var)
        set pref(old,$var) $pref(opt,$var)
    }

    frame $w
    OptionMenu $w.viewer url_viewer \
        [list [list RatUP "$t(userproc): RatUP_ShowURL"] \
             [list mozilla "Mozilla"] \
             [list firefox "Firefox"] \
             [list galeon "Galeon"] \
             [list netscape "Netscape"] \
             [list opera "Opera"] \
             [list lynx Lynx] \
             [list other $t(other)]]
    set b($w.viewer) pref_url_viewer

    trace variable pref(opt,url_viewer) w PrefUpdateURLCmd
    lappend pref(vars,$pref(lastPref)) browser_cmd
    lappend pref(vars,$pref(lastPref)) url_behavior

    label $w.mode_label -text $t(open_in):
    OptionMenu $w.mode url_behavior \
        [list [list old_window $t(reuse_old_window)] \
             [list new_window $t(new_window)] \
             [list new_tab $t(new_tab)]]
    set b($w.mode) pref_url_behavior

    label $w.cmd_label -text $t(cmd): -anchor w
    entry $w.cmd -textvariable pref(opt,browser_cmd)
    set b($w.cmd) pref_url_viewer_cmd

    grid $w.viewer - -sticky w
    grid $w.mode_label $w.mode -sticky w
    grid $w.cmd_label $w.cmd -sticky we
    grid columnconfigure $w 1 -weight 1
}

# PrefUpdateURLCmd --
#
# Trace function for url_viewer
#
# Arguments:
# Standard trace function arguments

proc PrefUpdateURLCmd {args} {
    global pref

    switch $pref(opt,url_viewer) {
        "RatUP" {set pref(opt,browser_cmd) ""}
        "mozilla" {set pref(opt,browser_cmd) "mozilla"}
        "firefox" {set pref(opt,browser_cmd) "firefox"}
        "galeon" {set pref(opt,browser_cmd) "galeon"}
        "netscape" {set pref(opt,browser_cmd) "netscape"}
        "opera" {set pref(opt,browser_cmd) "opera"}
        "lynx" {set pref(opt,browser_cmd) \
                    {xterm -T "Lynx:%u" +sb -e lynx "%u"}}
        "other" {set pref(opt,browser_cmd) "other_browser %u"}
    }
}

# ValidateInt --
#
# Check that the goiven command is an integer in the given interval
#
# Arguments:
# value - Value to check
# min   - The smallest valid value
# max   - The largest valid value

proc ValidateInt {value min max} {
    if {![string is integer $value]
        || $value < $min || $value > $max} {
        return 0
    } else {
        return 1
    }
}
