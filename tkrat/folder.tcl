# folder.tcl --
#
# This file contains code which handles a folder window.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Variables used to update online/offline settings of tkrat menus
set tkrat_menus {}
set tkrat_online_index 0
set tkrat_online_imgs {}

# Images
set online_img [image create photo -data {
R0lGODlhIAAQAKUAAAAAAA8PDV1ZT2JeVGRgVmVgVmplW2xoXnBsYnJtY3h1a314bn56cIB9
dIF9c4qFe4uHfYuHfoyJgY2JgZOOhJeTipiTiZyXjZ+ck6CdlKaim6eimKeknqqnoLGtpLKw
q7Oxqrezqry5sr67tcTCvcXDvcbCusjFv87Kw9HQzOHf2+Lh3ePh3ejn5Oro4+vq5uzr6O7s
6vDu6/Dv7PLw7fPy8Pn5+P///////////////////////////////////yH5BAEAAD8ALAAA
AAAgABAAAAZ1wJ9wSCwaj8ikcslsOp9Qo6XxM1GjRkeI5KJ2L6IJVthYzTrdn6zxsZWuSZd8
Pqe9HjBqrIGptVh0c0QNhIUNCgojIAsqCj8pDRQnG4aGTAoSGgkojhwNDBkOB2M/BREIHqMV
DQMGAqREFgQ/EHCwt7i5urhBADs=}]
set offline_img [image create photo -data {
R0lGODlhIAAQAKUAAAAAAA8PDV1ZT2JeVGRgVmVgVmplW2xoXnBsYnJtY3h1a314bn56cIB9
dIF9c4qFe4uHfYuHfoyJgY2JgZOOhJeTipiTiZyXjZ+ck6CdlKaim6eimKeknqqnoLGtpLKw
q7Oxqrezqry5sr67tcTCvcXDvcbCusjFv87Kw9HQzN9CHuHf2+Lh3ePh3ejn5Oro4+vq5uzr
6O7s6vDu6/Dv7PLw7fPy8Pn5+P///////////////////////////////yH5BAEAAD8ALAAA
AAAgABAAAAaIwJ9wSCwaj8ikcslsOokqlTD6PFoaU+mP+jNhnY4Q6fXdRrU/8kU0WTZYtA55
eEbPGp9bqUx8+V81MA8xfFxCMg0YNi4tf0QNDQoKIyALKwpQaD8pDRQnG5CQRwoSGgkomFmG
HA0MGQ4HSwURCB6xqmgVDQMGAk8WBFnCPxB8VcOGx8rLzM1LQQA7}]
set icon_img [image create photo -data {
R0lGODlhQABAAIQeAAAAAD8DZksEek0EfVQEgFYFjHUGgHsGgIAIgIAJgIAKgK1vIsqBKM+F
KdSIKs+/keDPnezapfHfqf/3Tf3qsf//Wf//aP//e///gv//hP//kf//ov//sv//u///////
/yH5BAEKAB8ALAAAAABAAEAAAAX+4CeOZGmeaKqubOu+cCzPdG3feE4DIq//H4BQqBkGgTch
R8PcXCYaixBZA3A2GYd262hMGD7qS9jJcs+ORVjcApTR6Iaa3X7D0fPemt7jmO9neVZSJHtA
ABppCw2AW3NWGQ1gPRuGOgATC2oMjI2PiV2PG3lIAGo8mI1pqKCaqBekhz5CFp13jwyapwCV
sT97ABWes0MXGk4NU3xBfsN9x39ehHwAtYCuzH9nkpaHwrezG4qatqHdOW7agajNu5xcvr/N
16xau8Hw5zZC4s68WtzS5StlBYunU/O6TDA48FDCW7tAwdGF7ZBEiAAugLkI756+Gf+uqcmQ
Qc2Fcvb+1BAwIGDAgY8wIIkEQLJLsgYoVSYoMIQnDlT9Jo40o8sUOXsAFAwQEAAAgqY/BQql
SdSjUVcJAmgF4PPnw45UU2ZkwuTCEFMAmqaF6QKR0KFia27h5sHDWWXogjo6JXfVBXVdHAB4
QEEC2zFS92ZkiJTjXMGDD7dJHBewGsdzwdw1suOrq775TgLyQrb0NBluwYLmghMATlXjKOId
w9EkYNY3UXa0OrvtRVd677SmSNwVsCMxUiP9+5b45rsmwkgOErIxHuNn1dTdHuGc9JgJge8W
YoysWQASHkigQOGBoedn2/zO+Mf4BcYKMQiRsD79GiWlaTDBBeexoNwq82XugB9rE+jHX3v/
hedKBfFEN59E4qkiBwAeFObeLInIRkSFJRx4mSP0wbaKB+t92EOIaC1QAS8kFnIhipjBsWGH
7s0YBIxnkVTjCCZm1MmJKq4IoWBB1KLGEtBUpMKBG05gXZLaLYnKVcX1dkI15SCJpIraPeCe
IPB5iUIqKIo2JmxlomURa2pY+eZBHJ45nYHWtLmhaKq44oGeVBjVSZVVBqrdoAAwWSgA7yDo
l26K1WWma3vGlIsrm9aJkaVnDvkTb2DGVhQE7oWaaXLdXJWmZs8t86UQC5yRgTCaIIWPqGzA
Z5Ueq9KhpqzEFltsCAA7}]

# FolderWindowInit --
#
# Initializes a folder window, that is it populates it with widgets and
# installs all callbacks. It reaturns the handler that should be used for
# this folder window.
#
# Arguments:
# w -		Window which the folder window should be packed into
# toopen -	Folder which the caller is about to open so do not
#		open for monitoring

proc FolderWindowInit {w toopen} {
    global b t idCnt statusText option folderWindowList defaultFontWidth\
	    fixedNormFont tkrat_menus tkrat_online_index \
	    online_img tkrat_online_imgs numDeferred icon_img accelerator

    # Create the handler
    set handler f[incr idCnt]
    upvar \#0 $handler fh

    # Initialize variables
    set fh(toplevel) [winfo toplevel $w]
    set fh(w) $w
    set fh(folder_name) {}
    set fh(folder_status) {}
    set fh(num_messages) 0
    set fh(groupMessageLists) {}
    set fh(uids) {}
    set fh(message_scroll) $w.t.messlist.scroll
    set fh(message_list) $w.t.messlist.list
    set fh(group) {}
    set fh(text) $w.b.text.text
    set fh(find_match_case) 0
    set fh(find_match) exact
    set fh(find_loc) body
    set fh(browse) 0
    set fh(setflag) ""
    set fh(syncing) 0
    set fh(menu_nokeep) {}
    set fh(role) $option(default_role)
    set fh(special_folder) none
    set fh(context_menu) $w.t.messlist.list.contextmenu
    set fh(struct_menu) $w.t.mbar.message.m.structmenu
    set fh(wrap_mode) $option(wrap_mode)
    set fh(last_filter) ""
    upvar \#0 $fh(text) texth
    set texth(show_header) $option(show_header)
    set texth(struct_menu) $fh(struct_menu)
    set texth(width_adjust) {}

    ::tkrat::winctl::Size folderWindow $w
    frame $w.t
    frame $w.b

    # Icon
    set i .icon[incr idCnt]
    toplevel $i -class TkRat
    pack [label $i.l -image $icon_img]
    wm iconwindow $fh(toplevel) $i

    # The menu and information line
    frame $w.t.mbar -relief raised -bd 1
    FindAccelerators a {tkrat folders message group show admin help filter}

    # Tkrat menu
    menubutton $w.t.mbar.tkrat -menu $w.t.mbar.tkrat.m -text $t(tkrat) \
	    -underline $a(tkrat)
    set m $w.t.mbar.tkrat.m
    menu $m -tearoff 1 -tearoffcommand "lappend tkrat_menus" \
	    -postcommand "PostTkRatMenu $handler"
    $m add cascade -label $t(role) -menu $m.role
    set b($m,[$m index end]) folder_select_role
    menu $m.role -postcommand \
	    [list PostRoles $handler $m.role [list UpdateFolderTitle $handler]]
    $m add cascade -label $t(new_folder) -menu $m.new_folder
    set b($m,[$m index end]) new_folder
    menu $m.new_folder -postcommand "NewFolderMenu $handler $m.new_folder"
    $m add separator
    $m add command -label $t(find)... -command "FolderFind $handler"
    set b($m,[$m index end]) find
    set fh(find_menu) [list $m [$m index end]]
    $m add command -label $t(compose)... \
        -command "Compose \$${handler}(role)"
    set fh(compose_menu) [list $m [$m index end]]
    set b($m,[$m index end]) compose
    $m add separator
    $m add checkbutton -label $t(watcher) \
	    -variable option(watcher_enable) -onvalue 1 -offvalue 0 \
	    -command SaveOptions
    set b($m,[$m index end]) watcher_enable
    $m add checkbutton -label $t(browse_mode) -variable ${handler}(browse)
    set b($m,[$m index end]) browse_mode
    $m add command -label $t(update_folder) \
	    -command "RatBusy \"Sync $handler update\""
    set fh(update_menu) [list $m [$m index end]]
    set b($m,[$m index end]) update
    $m add command -label $t(sync_folder)  \
	    -command "RatBusy \"Sync $handler sync\""
    set fh(sync_menu) [list $m [$m index end]]
    set b($m,[$m index end]) sync
    $m add command -label $t(netsync_folder) -state disabled \
	    -command "RatBusy {\$${handler}(folder_handler) netsync; \
	              Sync $handler update}"
    set b($m,[$m index end]) netsync_folder
    set fh(netsync_folder_menu) [list $m [$m index end]]
    $m add command -label $t(netsync) -command "RatBusy NetworkSync"
    set b($m,[$m index end]) netsync
    set fh(netsync_all_menu) [list $m [$m index end]]
    $m add command
    lappend tkrat_menus $m
    set fh(online_menu) [list $m [$m index end]]
    set tkrat_online_index [$m index end]
    $m add separator
    $m add command -label $t(see_log)... -command SeeLog
    set b($m,[$m index end]) see_log
    $m add command -label $t(close) -command "DestroyFolderWin $handler 0"
    set fh(close_menu) [list $m [$m index end]]
    set b($m,[$m index end]) close_folder
    $m add command -label $t(quit) -command "Quit $handler"
    set fh(quit_menu) [list $m [$m index end]]
    set b($m,[$m index end]) quit
    
    # Folder menu
    menubutton $w.t.mbar.folder -menu $w.t.mbar.folder.m -text $t(folders) \
			      -underline $a(folders)
    menu $w.t.mbar.folder.m \
	    -postcommand "PostFolder $handler $w.t.mbar.folder.m" -tearoff 1
    set b($w.t.mbar.folder) folder_menu

    # Message menu
    menubutton $w.t.mbar.message -menu $w.t.mbar.message.m -text $t(message) \
	-underline $a(message)
    set b($w.t.mbar.message) message_menu
    set m $w.t.mbar.message.m
    menu $m -tearoff 0 -postcommand "PostMessageMenu $handler $m"

    # Show menu
    menubutton $w.t.mbar.show -menu $w.t.mbar.show.m -text $t(show) \
			      -underline $a(show)
    set b($w.t.mbar.show) show_menu
    set m $w.t.mbar.show.m
    menu $m -tearoff 1
    $m add radiobutton -label $t(no_wrap) \
        -variable ${handler}(wrap_mode) -value none \
        -command [list SetWrapMode $handler]
    set b($m,[$m index end]) show_no_wrap
    $m add radiobutton -label $t(wrap_char) \
        -variable ${handler}(wrap_mode) -value char \
        -command [list SetWrapMode $handler]
    set b($m,[$m index end]) show_wrap_char
    $m add radiobutton -label $t(wrap_word) \
        -variable ${handler}(wrap_mode) -value word \
        -command [list SetWrapMode $handler]
    set b($m,[$m index end]) show_wrap_word
    $m add separator
    $m add radiobutton -label $t(show_all_headers) \
        -variable $fh(text)(show_header) -value all \
        -command [list SetShowHeaders $handler]
    set b($m,[$m index end]) show_all_headers
    $m add radiobutton -label $t(show_selected_headers) \
        -variable $fh(text)(show_header) -value selected \
        -command [list SetShowHeaders $handler]
    set b($m,[$m index end]) show_selected_headers
    $m add radiobutton -label $t(show_no_headers) \
        -variable $fh(text)(show_header) -value no \
        -command [list SetShowHeaders $handler]
    set b($m,[$m index end]) show_no_headers
    $m add separator
    $m add cascade -label $t(sort_order) -menu $m.sort
    set b($m,[$m index end]) sort_order_folder
    lappend fh(menu_nokeep) [list $m [$m index end]]
    menu $m.sort
    foreach o {threaded subject subjectonly senderonly sender folder
	       reverseFolder date reverseDate size reverseSize} {
	$m.sort add radiobutton -label $t(sort_$o) \
            -variable ${handler}(folder_sort) -value $o \
            -command [list SetSortOrder $handler]
	set b($m.sort,[$m.sort index end]) sort_$o
    }

    # Group menu
    menubutton $w.t.mbar.group -menu $w.t.mbar.group.m -text $t(group) \
			      -underline $a(group)
    set b($w.t.mbar.group) group_menu
    set m $w.t.mbar.group.m
    menu $m -postcommand "SetupGroupMenu $m $handler" -tearoff 1
    $m add command -label $t(create_in_win)... \
	    -command "GroupMessageList $handler"
    set b($m,[$m index end]) create_in_win
    $m add command -label $t(create_by_expr)... -command "ExpCreate $handler"
    set b($m,[$m index end]) create_by_expr
    $m add cascade -label $t(use_saved_expr) -menu $m.saved
    set b($m,[$m index end]) use_saved_expr
    menu $m.saved -postcommand "ExpBuildMenu $m.saved $handler"
    $m add command -label $t(clear_group) -command "GroupClear $handler"
    set b($m,[$m index end]) clear_group
    $m add separator
    $m add command -label $t(forward_separately) \
        -command "ForwardGroupSeparately \[GetMsgSet $handler group\] \
                  \$${handler}(role)"
    set b($m,[$m index end]) forward_separately
    $m add command -label $t(forward_in_one) \
        -command "ForwardGroupInOne \[GetMsgSet $handler group\] \
                  \$${handler}(role)"
    set b($m,[$m index end]) forward_in_one
    $m add command -label $t(bounce_messages) \
        -command "BounceMessages \[GetMsgSet $handler group\] \
                  \$${handler}(role)"
    set b($m,[$m index end]) bounce_messages
    $m add command -label $t(extract_adr)... \
        -command "AliasExtract $handler \[GetMsgSet $handler group\]"
    set b($m,[$m index end]) extract_adr
    $m add separator
    $m add command -label $t(print) -command "Print $handler group"
    set b($m,[$m index end]) print_group
    $m add cascade -label $t(move) -menu $m.move
    set b($m,[$m index end]) move_group
    menu $m.move -postcommand "PostMove $handler 1 group $m.move"
    $m add cascade -label $t(copy) -menu $m.copy
    set b($m,[$m index end]) copy_group
    menu $m.copy -postcommand "PostMove $handler 0 group $m.copy"
    $m add separator
    $m add command -label $t(delete) -command \
        "SetFlag $handler deleted 1 \
                         \[\$${handler}(folder_handler) flagged flagged 1\]"
    set b($m,[$m index end]) delete_group
    $m add command -label $t(undelete) -command \
        "SetFlag $handler deleted 0 \
                         \[\$${handler}(folder_handler) flagged flagged 1\]"
    set b($m,[$m index end]) undelete_group
    # Disable in drafts...
    $m add command -label $t(mark_as_unread) -command \
        "SetFlag $handler seem 0 \
                         \[\$${handler}(folder_handler) flagged flagged 1\]"
    set b($m,[$m index end]) mark_as_unread

    $m add command -label $t(mark_as_read) -command \
        "SetFlag $handler seen 1 \
                         \[\$${handler}(folder_handler) flagged flagged 1\]"
    set b($m,[$m index end]) mark_as_read

    $m add command -label $t(mark_as_answered) -command \
        "SetFlag $handler answered 0 \
                         \[\$${handler}(folder_handler) flagged flagged 1\]"
    set b($m,[$m index end]) mark_as_answered

    $m add command -label $t(mark_as_unanswered) -command \
        "SetFlag $handler answered 1 \
                         \[\$${handler}(folder_handler) flagged flagged 1\]"
    set b($m,[$m index end]) mark_as_unanswered

    # Admin menu
    menubutton $w.t.mbar.admin -menu $w.t.mbar.admin.m -text $t(admin) \
			     -underline $a(admin)
    set b($w.t.mbar.admin) admin_menu
    set m $w.t.mbar.admin.m
    menu $m -tearoff 1
    $m add command -label $t(newedit_folder)... -command VFolderDef
    set b($m,[$m index end]) newedit_folder
    $m add command -label $t(addressbook)... -command Aliases
    set b($m,[$m index end]) addressbook
    $m add command -label $t(preferences)... -command Preferences
    set b($m,[$m index end]) preferences
    $m add command -label $t(define_keys)... -command "KeyDef folder"
    set b($m,[$m index end]) define_keys
    $m add command -label $t(saved_expr)... -command "ExpHandleSaved $handler"
    set b($m,[$m index end]) saved_expr
    $m add command -label $t(setup_netsync)... -command SetupNetworkSync
    set b($m,[$m index end]) setup_netsync
    $m add command -label $t(purge_pwcache) -command RatPurgePwChache
    set b($m,[$m index end]) purge_pwcache
    $m add cascade -label $t(reread) -menu $m.reread
    $m add cascade -label $t(dbase) -menu $m.dbase

    set rm $m.reread
    menu $rm
    $rm add command -label $t(reimport_all) -command VFolderReimportAll
    set b($rm,[$rm index end]) reimport_all
    $rm add command -label $t(reread_userproc) -command ReadUserproc
    set b($rm,[$rm index end]) reread_userproc
    $rm add command -label $t(reread_mailcap) -command RatMailcapReload
    set b($rm,[$rm index end]) reread_mailcap

    menu $m.dbase
    $m.dbase add command -label $t(check_dbase)... \
	    -command "RatBusy \"DbaseCheck 0\""
    set b($m.dbase,[$m.dbase index end]) check_dbase
    $m.dbase add command -label $t(check_fix_dbase)... \
	    -command "RatBusy \"DbaseCheck 1\""
    set b($m.dbase,[$m.dbase index end]) check_fix_dbase

    # Help menu
    menubutton $w.t.mbar.help -menu $w.t.mbar.help.m -text $t(help) \
			    -underline $a(help)
    set b($w.t.mbar.help) help_menu
    set m $w.t.mbar.help.m
    menu $m -tearoff 1
    $m add checkbutton -label $t(balloon_help) \
	    -variable option(show_balhelp) \
	    -command {SaveOptions}
    set b($m,[$m index end]) balloon_help
    $m add separator
    $m add command -label $t(version)... -command Version
    set b($m,[$m index end]) show_version
    $m add command -label Ratatosk... -command Ratatosk
    set b($m,[$m index end]) explain_ratatosk
    $m add command -label $t(help_window)... -command Help
    set b($m,[$m index end]) help_window
    $m add command -label $t(send_bug)... -command SendBugReport
    set b($m,[$m index end]) send_bug

    # The structure menu (populated by the Show routine)
    menu $texth(struct_menu) -tearoff 0

    # Information
    button $w.t.mbar.netstatus -image $online_img -bd 0 -width 32
    lappend tkrat_online_imgs $w.t.mbar.netstatus
    label $w.t.mbar.status -textvariable ${handler}(folder_status)
    set b($w.t.mbar.status) folder_status

    # Pack the menus into the menu bar
    pack $w.t.mbar.tkrat \
	 $w.t.mbar.folder \
	 $w.t.mbar.message \
	 $w.t.mbar.show \
	 $w.t.mbar.group \
	 $w.t.mbar.admin -side left -padx 5
    pack $w.t.mbar.help -side right -padx 5
    pack $w.t.mbar.netstatus $w.t.mbar.status -side right

    # The information part
    frame $w.t.info -relief raised -bd 1
    label $w.t.info.flabel -text $t(name):
    label $w.t.info.fname -textvariable ${handler}(folder_name) \
	-anchor w -font $fixedNormFont
    set fh(filter_clear) $w.t.info.clear
    set fh(filter_apply) $w.t.info.apply
    button $fh(filter_clear) -text $t(clear) -bd 1 -highlightthickness 0 \
        -command [list FilterClear $handler] -state disabled
    button $fh(filter_apply) -text $t(apply) -bd 1 -highlightthickness 0 \
        -command [list FilterApply $handler] -state disabled
    entry $w.t.info.filter -width 25 -bd 1 -textvariable ${handler}(filter)
    set fh(filter_entry) $w.t.info.filter
    label $w.t.info.filterl -text $t(filter):
    ::tkrat::winctl::InstallMnemonic $w.t.info.filterl $a(filter) \
        $fh(filter_entry)

    # Override keyboard bindings...
    bind KbdBlock <KeyPress> {break}
    bind KbdBlock <KeyRelease> {break}
    bindtags $fh(filter_entry) [list $fh(filter_entry) Entry KbdBlock $w all]

    pack $w.t.info.flabel -side left
    pack $w.t.info.fname -side left -fill x -expand 1
    pack $fh(filter_clear) \
        $fh(filter_apply) \
        $fh(filter_entry) \
        $w.t.info.filterl -side right
    set b($w.t.info.fname) current_folder_name

    bind $fh(filter_entry) <Return> "$fh(filter_apply) invoke; break"

    # The message list
    frame $w.t.messlist
    scrollbar $fh(message_scroll) \
        -relief sunken \
        -command "$w.t.messlist.list yview" \
	-highlightthickness 0
    text $fh(message_list) \
        -yscroll "$fh(message_scroll) set" \
        -bd 0 \
	-highlightthickness 0 \
	-wrap none \
	-spacing1 1 \
	-spacing3 2 \
	-cursor {}
    $fh(message_list) tag configure Active \
	    -relief raised \
	    -borderwidth [$fh(message_list) cget -selectborderwidth] \
	    -foreground [$fh(message_list) cget -selectforeground] \
	    -background [$fh(message_list) cget -selectbackground]
    if { 4 < [winfo cells $fh(message_list)]} {
	$fh(message_list) tag configure sel -background #ffff80
	$fh(message_list) tag configure Found -background #ffff00
    } else {
	$fh(message_list) tag configure sel -underline 1
	$fh(message_list) tag configure Found -borderwidth 2 -relief raised
    }
    $fh(message_list) tag raise sel
    pack $fh(message_scroll) -side right -fill y
    pack $fh(message_list) -side left -expand 1 -fill both

    # The context menu (populated by FolderContextMenu)
    menu $fh(context_menu) -tearoff 0

    # The status line
    label $w.statustext -textvariable statusText -relief raised \
	    -bd 1 -width 80
    set b($w.statustext) status_text
    
    # The command buttons
    frame $w.b.buttons
    menubutton $w.b.buttons.move -text $t(move) -bd 1 -relief raised \
	    -menu $w.b.buttons.move.m -indicatoron 1
    menu $w.b.buttons.move.m \
	    -postcommand "PostMove $handler 1 current $w.b.buttons.move.m"
    button $w.b.buttons.delete -text $t(delete) -bd 1 -highlightthickness 0 \
	    -command "SetFlag $handler deleted 1; FolderNext $handler"
    button $w.b.buttons.compose -text $t(compose)... -highlightthickness 0 \
	    -command "Compose \$${handler}(role)" -bd 1
    button $w.b.buttons.reply_sender -text $t(reply_sender)... -bd 1 \
	    -highlightthickness 0 -command "FolderReply $handler sender"
    button $w.b.buttons.reply_all -text $t(reply_all)... -bd 1 \
	    -highlightthickness 0 -command "FolderReply $handler all"
    pack $w.b.buttons.move \
	 $w.b.buttons.delete \
	 $w.b.buttons.compose \
	 $w.b.buttons.reply_sender \
	 $w.b.buttons.reply_all -side left -expand 1 -fill x
    if { 0 < $option(pgp_version)} {
	set fh(sigbut) $w.b.buttons.signature
	button $fh(sigbut) -text $t(sig): -bd 1 -anchor w -state disabled \
		-highlightthickness 0 -width [expr {[string length $t(sig)]+5}]
	pack $fh(sigbut) -side right -expand 1 -fill x
	set b($fh(sigbut)) pgp_none
    }
    set b($w.b.buttons.move) move_msg
    set b($w.b.buttons.delete) delete_msg
    set b($w.b.buttons.compose) compose_msg
    set b($w.b.buttons.reply_sender) reply_sender
    set b($w.b.buttons.reply_all) reply_all

    # The actual text
    frame $w.b.text -relief raised -bd 1
    scrollbar $w.b.text.scroll \
        -relief sunken \
        -command "$w.b.text.text yview" \
	-highlightthickness 0
    text $fh(text) \
	-yscroll "RatScrollShow $w.b.text.text $w.b.text.scroll" \
        -relief raised \
        -wrap $fh(wrap_mode) \
	-bd 0 \
	-highlightthickness 0 \
	-setgrid 1
    $fh(text) mark set searched 1.0
    pack $w.b.text.scroll -side right -fill y
    pack $w.b.text.text -side left -expand yes -fill both

    # Pack all the parts into the window
    pack $w.t.mbar -side top -fill x
    pack $w.t.info -side top -fill x
    pack $w.t.messlist -fill both -expand 1
    pack $w.b.buttons -side top -fill x
    pack $w.b.text -side top -expand yes -fill both
    frame $w.handle -width 10 -height 10 \
	    -relief raised -borderwidth 2 \
	    -cursor sb_v_double_arrow
    set b($w.handle) pane_button

    # Do special packing
    bind $w <Configure> \
	    "set ${handler}(H) \[winfo height $w\]; \
	     set ${handler}(Y0) \[winfo rooty $w\]; \
	     set ${handler}(Y1) \[expr \[winfo width $w\] - 5\]; \
	     place configure $w.handle -x \$${handler}(Y1)"
    bind $w.handle <B1-Motion> \
	"FolderPane $handler \[expr (%Y-\$${handler}(Y0))/\$${handler}(H).0\]"
    FolderPane $handler [::tkrat::winctl::GetPane folderWindow]

    place $w.t -relwidth 1
    place $w.b -relwidth 1 -rely 1 -anchor sw
    place $w.statustext -relwidth 1 -anchor w
    place $w.handle -anchor e
    after idle "set d \[expr \[winfo height $w.statustext\] / 2\]; \
		place configure $w.t -height -\$d; \
		place configure $w.b -height -\$d"

    # Do bindings
    focus $w
    bind $w <1> "if {\"%W\" != \"$fh(filter_entry)\"} {focus $w}"
    bind $fh(message_list) <1> \
	    "FolderSelect $handler \[expr int(\[%W index @%x,%y\])-1\]"
    bind $fh(message_list) <Double-1> "FolderDouble $handler; break"
    bind $fh(message_list) <Triple-1> break
    bind $fh(message_list) <Double-Shift-1> break
    bind $fh(message_list) <Triple-Shift-1> break
    bind $fh(message_list) <B1-Leave> break
    bind $fh(message_list) <B1-Motion> break
    bind $fh(message_list) <Shift-1> \
	"FolderFlagEvent $handler \[%W index @%x,%y\]; break"
    bind $fh(message_list) <Shift-B1-Motion> \
	"FolderFlagMotion $handler \[%W index @%x,%y\]; break"
    bind $fh(message_list) <Control-1> \
	"FolderFlagEvent $handler \[%W index @%x,%y\]; break"
    bind $fh(message_list) <Control-B1-Motion> \
	"FolderFlagMotion $handler \[%W index @%x,%y\]; break"
    bind $fh(message_list) <2> "FolderFlagEvent $handler \[%W index @%x,%y\]"
    bind $fh(message_list) <Shift-2> \
	"FolderFlagRange $handler \[%W index @%x,%y\]"
    bind $fh(message_list) <B2-Motion> \
	"FolderFlagMotion $handler \[%W index @%x,%y\]"
    bind $fh(message_list) <3> "FolderContextMenu $handler %x %y %X %Y"
    bind $fh(text) <Map> "FolderMap $handler"
    bind $fh(text) <Unmap> "FolderUnmap $handler"
    bind $fh(text) <Destroy> "DestroyFolderWin $handler 1"
    bind $fh(text) <Configure> {ResizeBodyText %W %w}
    FolderBind $handler
    wm protocol $fh(toplevel) WM_DELETE_WINDOW "DestroyFolderWin $handler 1"

    # Calculate font width
    if {![info exists defaultFontWidth]} {
	CalculateFontWidth $fh(text)
    }

    trace variable fh(filter) w [list FilterChanged $handler]

    # Do things which are done just when the first folder window is opened
    if {![info exists folderWindowList]} {
        RatLibSetOnlineMode $option(online)
	update idletasks

	RatBusy {
	    # Reimport imported folders with the session setting
	    global vFolderDef
	    foreach id [array names vFolderDef] {
		if {"import" != [lindex $vFolderDef($id) 1]} {
		    continue
		}
		set a(reimport) unset
		array set a [lindex $vFolderDef($id) 2]
		if {"session" == $a(reimport)} {
		    RatImport $id
		}
	    }

	    # Open monitored folders
	    FolderStartMonitor $toopen
	}

    }

    SetOnlineStatus $option(online)

    set folderWindowList($handler) ""

    return $handler
}

# UpdateFolderStatus --
#
# Updates the show status of this folder window
#
# Arguments:
# handler - The handler which identifies the folder window
# new      - Number of new messages
# messages - Number of messages
# size     - Size of mailbox (-1 for unknown)

proc UpdateFolderStatus {handler new messages size} {
    upvar \#0 $handler fh

    set fh(old_messages) $messages
    set fh(old_size) $size

    if {-1 == $size} {
        set s "?"
    } else {
        set s [RatMangleNumber $size]
    }
    set fh(folder_status) "$new / $messages / $s"
}

# UpdateFolderStatusNew --
#
# Updates the new messages count in the show status of this folder window
#
# Arguments:
# handler - The handler which identifies the folder window
# new      - Number of new messages

proc UpdateFolderStatusNew {handler new} {
    upvar \#0 $handler fh

    UpdateFolderStatus $handler $new $fh(old_messages) $fh(old_size)
}

# ResizeBodyText --
#
# Handle resizing of the body text widget
#
# Arguments:
# w     - The handler which identifies the body text widget
# width - The new width

proc ResizeBodyText {w width} {
    upvar \#0 $w texth

    foreach c $texth(width_adjust) {
        $c configure -width $width
    }
}

# PostTkRat --
#
# Post-command for Tkrat menu
#
# Arguments:
# handler - The handler which identifies the folder window

proc PostTkRatMenu {handler} {
    upvar \#0 $handler fh
    global folderWindowList

    if {1 == [array size folderWindowList]} {
	set state disabled
    } else {
	set state normal
    }
    [lindex $fh(close_menu) 0] entryconfigure [lindex $fh(close_menu) 1] \
	    -state $state
}

# PostMessageMenu --
#
# Post-command for Message menu
#
# Arguments:
# handler - The handler which identifies the folder window
# m       - Menu to populat

proc PostMessageMenu {handler m} {
    upvar \#0 $handler fh

    if {![info exists fh(folder_handler)]} {
	return
    }

    if {"" == $fh(list_index)} {
        set msg ""
    } else {
        set msg $fh(current_msg)
    }

    BuildMessageMenu $handler $m $msg
}

# BuildMessageMenu --
#
# Populate the message menu
#
# Arguments:
# handler - The handler which identifies the folder window
# m       - Menu to populate
# msg     - The current message

proc BuildMessageMenu {handler m msg} {
    upvar \#0 $handler fh
    global t b accelerator

    set msgsel_state normal
    set delete_state normal
    set undelete_state disabled
    set markunread_state disabled
    set markread_state normal
    set answered_state normal
    set unanswered_state disabled
    set dela_state disabled
    set fi [$fh(folder_handler) find $msg]
    if {-1 == $fi} {
        set delete_state disabled
	set msgsel_state disabled
        set markread_state disabled
        set answered_state disabled
    } else {
        if {1 == [$fh(folder_handler) getFlag $fi deleted]} {
            set delete_state disabled
            set undelete_state normal
        }
        if {1 == [$fh(folder_handler) getFlag $fi seen]} {
            set markunread_state normal
            set markread_state disabled
        }
        if {1 == [$fh(folder_handler) getFlag $fi answered]} {
            set unanswered_state normal
            set answered_state disabled
        }
        set body [$msg body]
        if {"MULTIPART" == [lindex [$body type] 0]} {
            set dela_state normal
        }
    }

    $m delete 0 end

    if {"drafts" == $fh(special_folder)} {
	$m add command -label $t(continue_composing)... -state $msgsel_state \
            -command "\
	    ComposeContinue $msg; \
            SetFlag $handler deleted 1 $fi"
    } else {
	$m add command -label $t(reply_sender)... -state $msgsel_state \
            -accelerator $accelerator(folder_key_replys) \
	    -command "ComposeReply $msg sender $fh(role) \
                      \"FolderReplySent $handler $msg\""
	set b($m,[$m index end]) reply_sender

	$m add command -label $t(reply_all)... -state $msgsel_state \
            -accelerator $accelerator(folder_key_replya) \
	    -command "ComposeReply $msg all $fh(role) \
                      \"FolderReplySent $handler $msg\""
	set b($m,[$m index end]) reply_all
    }

    $m add command -label $t(forward_inline)... -state $msgsel_state \
        -accelerator $accelerator(folder_key_forward_i) \
        -command "ComposeForwardInline $msg $fh(role)"
    set b($m,[$m index end]) forward_inline
    
    $m add command -label $t(forward_as_attachment)... \
        -state $msgsel_state \
        -accelerator $accelerator(folder_key_forward_a) \
        -command "ComposeForwardAttachment $msg $fh(role)"
    set b($m,[$m index end]) forward_attached

    if {"drafts" != $fh(special_folder)} {
	$m add command -label $t(bounce)... -state $msgsel_state \
            -accelerator $accelerator(folder_key_bounce) \
	    -command "ComposeBounce $msg $fh(role)"
	set b($m,[$m index end]) bounce

	$m add command -label $t(extract_adr)... -state $msgsel_state \
	    -command "AliasExtract $handler $msg"
	set b($m,[$m index end]) extract_adr
    }

    $m add command -label $t(delete_attachments)... -state $dela_state \
        -command "::tkrat::delattachments::delete $msg $handler"

    $m add separator

    $m add command -label $t(print)... -command "Print $handler $msg" \
        -state $msgsel_state -accelerator $accelerator(folder_key_print)
    set b($m,[$m index end]) print

    if {"drafts" != $fh(special_folder)} {
	$m add cascade -label $t(move) -menu $m.move -state $msgsel_state
	set b($m,[$m index end]) move
	if {![winfo exists $m.move]} {
	    menu $m.move
	}
        $m.move configure -postcommand "PostMove $handler 1 $msg $m.move"
    }

    $m add cascade -label $t(copy) -menu $m.copy -state $msgsel_state
    set b($m,[$m index end]) copy_msg
    if {![winfo exists $m.copy]} {
        menu $m.copy
    }
    $m.copy configure -postcommand "PostMove $handler 0 $msg $m.copy"
    $m add separator

    $m add command -label $t(delete) -state $delete_state \
	-command "SetFlag $handler deleted 1 $fi" \
        -accelerator $accelerator(folder_key_delete)
    set b($m,[$m index end]) delete

    $m add command -label $t(undelete) -state $undelete_state \
	-command "SetFlag $handler deleted 0 $fi" \
        -accelerator $accelerator(folder_key_undelete)
    set b($m,[$m index end]) undelete

    if {"drafts" != $fh(special_folder)} {
        $m add command -label $t(mark_as_unread) -state $markunread_state \
            -command "SetFlag $handler seen 0 $fi" \
            -accelerator $accelerator(folder_key_markunread)
        set b($m,[$m index end]) mark_as_unread

        $m add command -label $t(mark_as_read) -state $markread_state \
            -command "SetFlag $handler seen 1 $fi"
        set b($m,[$m index end]) mark_as_read

        $m add command -label $t(mark_as_answered) -state $answered_state \
            -command "SetFlag $handler answered 1 $fi"
        set b($m,[$m index end]) mark_as_answered

        $m add command -label $t(mark_as_unanswered) -state $unanswered_state \
            -command "SetFlag $handler answered 0 $fi"
        set b($m,[$m index end]) mark_as_unanswered
    }
}

# FolderStartMonitor --
#
# Start monitoring of folders
#
# Arguments:
# toopen - Folder which will be opened anyway so do not bother to open here

proc FolderStartMonitor {toopen} {
    global vFolderDef vFolderMonitorFH vFolderMonitorID ratNetOpenFailures

    set netopenFailures 0
    foreach id [array names vFolderDef] {
	catch {unset f}
	array set f [lindex $vFolderDef($id) 2]
	if {[info exists f(monitor)] && $f(monitor)
	    && ![FolderFailedOpen check $vFolderDef($id)]} {
	    if {![catch {VFolderDoOpen $id $vFolderDef($id)} hd]} {
		WatcherInit $hd
	    } else {
		FolderFailedOpen set $vFolderDef($id)
	    }
	}
    }
    if {0  < $ratNetOpenFailures} {
	SetOnlineStatus 0
    }
}

# FolderFailedOpen --
#
# Check if we previously failed to open a folder on the same host/prot/port
# Returns true if we previously failed to open.
#
# Arguments:
# op  - Operation to perform (check, set or clearall)
# def - Folder definition to check

proc FolderFailedOpen {op def} {
    global failedConnections

    if {-1 == [lsearch -exact {imap pop3} [lindex $def 1]]} {
	return 0
    }
    set spec [lindex $def 3]

    if {"check" == $op} {
	return [info exists failedConnections($spec)]
    } elseif {"set" == $op} {
	set failedConnections($spec) 1
    } else {
	unset failedConnections
    }
    return 0
}

# FolderPane --
#
# Pane the folder window
#
# Arguments:
# handler - The handler which identifies the folder window
# y       - Y position of dividing line

proc FolderPane {handler y} {
    upvar \#0 $handler fh

    if {$y < 0.01 || 0.99 < $y}  return
        # Prevents placing into inaccessibility (off the window).

    set w $fh(w)
    set fh(pane) $y

    place $w.t -relheight $y
    place $w.b -relheight [expr {1.0 - $y}]
    place $w.statustext -rely $y
    place $w.handle -rely $y
}

# FolderBind --
#
# Bind the key definitions for a folder window
#
# Arguments:
# handler - The handler which identifies the fodler window

proc FolderBind {handler} {
    upvar \#0 $handler fh

    RatBindMenu $fh(w) folder_key_close $fh(close_menu)
    RatBindMenu $fh(w) folder_key_online $fh(online_menu)
    RatBindMenu $fh(w) folder_key_quit $fh(quit_menu)
    RatBindMenu $fh(w) folder_key_sync $fh(sync_menu)
    RatBindMenu $fh(w) folder_key_netsync $fh(netsync_all_menu)
    RatBindMenu $fh(w) folder_key_update $fh(update_menu)
    RatBindMenu $fh(w) folder_key_compose $fh(compose_menu)
    RatBindMenu $fh(w) folder_key_find $fh(find_menu)
    RatBind $fh(w) folder_key_delete \
        "SetFlag $handler deleted 1; FolderNext $handler"
    RatBind $fh(w) folder_key_undelete \
        "SetFlag $handler deleted 0; FolderNext $handler"
    RatBind $fh(w) folder_key_replya "FolderReply $handler all"
    RatBind $fh(w) folder_key_replys "FolderReply $handler sender"
    RatBind $fh(w) folder_key_forward_i \
        "FolderSomeCompose $handler ComposeForwardInline"
    RatBind $fh(w) folder_key_forward_a \
        "FolderSomeCompose $handler ComposeForwardAttachment"
    RatBind $fh(w) folder_key_bounce "FolderSomeCompose $handler ComposeBounce"
    RatBind $fh(w) folder_key_markunread \
        "SetFlag $handler seen 0; FolderNext $handler"
    RatBind $fh(w) folder_key_print "Print $handler current"

    RatBind $fh(w) folder_key_openfile \
	"PostFolderOpen $handler \[SelectFileFolder $fh(toplevel)\]"
    RatBind $fh(w) folder_key_nextu "FolderSelectUnread $handler; break"
    RatBind $fh(w) folder_key_flag \
	"SetFlag $handler flagged toggle; FolderNext $handler"
    RatBind $fh(w) folder_key_next	"FolderNext $handler"
    RatBind $fh(w) folder_key_prev	"FolderPrev $handler"
    RatBind $fh(w) folder_key_home	"ShowHome $fh(text)"
    RatBind $fh(w) folder_key_bottom	"ShowBottom $fh(text)"
    RatBind $fh(w) folder_key_pagedown  "ShowPageDown $fh(text)"
    RatBind $fh(w) folder_key_pageup    "ShowPageUp $fh(text)"
    RatBind $fh(w) folder_key_linedown  "ShowLineDown $fh(text)"
    RatBind $fh(w) folder_key_lineup    "ShowLineUp $fh(text)"
    RatBind $fh(w) folder_key_cycle_header "CycleShowHeader $handler"
    RatBind $fh(w) folder_key_mvdb \
	"PostFolderOpen $handler \[SelectDbaseFolder $fh(toplevel)\]"
}


# FolderWindowClear --
#
# Clears a folder window. This is the state we get into when no folder is
# currently open.
#
# Arguments:
# handler   -	The handler which identifies the folder window

proc FolderWindowClear {handler} {
    global folderWindowList option
    upvar \#0 $handler fh

    set fh(folder_name) ""
    UpdateFolderTitle $handler
    UpdateFolderStatus $handler 0 0 0
    ShowNothing $fh(text)
    catch {unset fh(current_msg); unset fh(folder_handler)}
    $fh(message_list) configure -state normal
    $fh(message_list) delete 1.0 end
    $fh(message_list) configure -state disabled
    set fh(list_index) ""
    FolderButtons $handler 0
    set folderWindowList($handler) ""
}


# FolderRead --
#
# Reads and the given folder and calls draw_folder_list to show the content.
# If there already is an active folder it is closed. As arguments it expects
# a folderwindow handler and a command which when run opens the folder.
#
# Arguments:
# handler   -	The handler which identifies the folder window
# foldercmd -	Command which creates the new folder
# name	    -	Human readable name of folder (proposed)

proc FolderRead {handler foldercmd name} {
    return [RatBusy [list FolderReadDo $handler $foldercmd $name]]
}

proc FolderReadDo {handler foldercmd name} {
    global option inbox t folderWindowList folderExists folderUnseen
    upvar \#0 $handler fh

    # First we expunge the old folder
    if {[info exists fh(folder_handler)]} {
	if {![string compare $inbox $fh(folder_handler)]} {
	    set inbox ""
	}
	set folderWindowList($handler) ""
	if {[info exists fh(folder_handler)]} {
	    CloseFolder $fh(folder_handler)
	    unset fh(folder_handler)
	}
    }

    # Open the new folder
    set id [RatLog 2 $t(opening_folder)... explicit]
    if {[catch $foldercmd folder_handler]} {
	RatClearLog $id
	FolderWindowClear $handler
	return {}
    }
    RatClearLog $id

    # Set name (if needed)
    if {[string length $name]} {
	$folder_handler setName $name
    }

    # Update our information
    set fh(folder_handler) $folder_handler
    set folderWindowList($handler) $folder_handler
    set fh(folder_sort) [$folder_handler getSortOrder]
    set i [$fh(folder_handler) info]
    set fh(folder_name) [lindex $i 0]
    if {"" != [$fh(folder_handler) role]} {
        set fh(role) [$fh(folder_handler) role]
    }
    UpdateFolderTitle $handler
    FolderDrawList $handler
    set i [$fh(folder_handler) info]
    UpdateFolderStatus $handler $folderUnseen($fh(folder_handler)) \
        $folderExists($fh(folder_handler)) [lindex $i 2]
    switch $option(folder_sort) {
	reverseFolder {
	    set dir -1
	    set start 0
	}
	reverseDate {
	    set dir -1
	    set start 0
	}
	reverseSize {
	    set dir -1
	    set start 0
	}
	default {
	    set dir 1
	    set start [expr {$fh(num_messages)-1}]
	}
    }
    if { 0 != $fh(num_messages)} {
	switch $option(start_selection) {
	    last {
		set index [expr {$fh(num_messages)-1}]
	    }
	    first_new {
		set index [FolderGetNextUnread $handler $start $dir]
	    }
	    before_new {
		set index [FolderGetNextUnread $handler $start $dir]
                set fi $fh(mapping,$index)
		if { 0 == [$fh(folder_handler) getFlag $fi seen]} {
		    incr index [expr -1 * $dir]
		    if {$index < 0 || $index >= $fh(num_messages)} {
			incr index $dir
		    }
		}
	    }
	    default {	# And first
		set index 0
	    }
	}
    } else {
	set index ""
    }

    FolderSelect $handler $index

    set type [$folder_handler type]
    if {"dis" == $type} {
	set state normal
    } else {
	set state disabled
    }
    [lindex $fh(netsync_folder_menu) 0] entryconfigure \
	    [lindex $fh(netsync_folder_menu) 1] -state $state

    # Initialize watcher
    set fh(exists) $folderExists($folder_handler)
    WatcherInit $folder_handler
    return $folder_handler
}

# FolderDrawList --
#
# Constructs the list of messages in the folder and shows this.
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderDrawList {handler} {
    upvar \#0 $handler fh
    global option

    set old_num_messages $fh(num_messages)
    set fh(num_messages) 0
    set folder_index 0
    $fh(message_list) configure -state normal
    $fh(message_list) delete 1.0 end
    set entries [$fh(folder_handler) list "%u $option(list_format)"]
    set fh(uids) {}
    array unset fh mapping,*
    array unset fh rmapping,*
    foreach e $entries {
	regexp {^([^ ]*) (.*)} $e unused uid l
        if {"" == $fh(filter) || [string match -nocase "*$fh(filter)*" $l]} {
            $fh(message_list) insert end "$l\n"
            set fh(mapping,$fh(num_messages)) $folder_index
            set fh(rmapping,$folder_index) $fh(num_messages)
            lappend fh(uids) $uid
            incr fh(num_messages)
        }
        incr folder_index
    }
    $fh(message_list) delete end-1c
    foreach w $fh(groupMessageLists) {
	GroupMessageListUpdate $w $handler
    }
    $fh(message_list) xview moveto 0
    $fh(message_list) configure -state disabled
}

# FolderListRefreshEntry --
#
# Refresh an entry in the message list
#
# Arguments:
# handler -	The handler which identifies the folder window
# index   -	Index of the entry to refresh (list index)

proc FolderListRefreshEntry {handler index} {
    upvar \#0 $handler fh
    global option

    set fi $fh(mapping,$index)
    set line [expr {$index+1}]
    $fh(message_list) configure -state normal
    set tags [$fh(message_list) tag names $line.0]
    $fh(message_list) delete $line.0 "$line.0 lineend"
    $fh(message_list) insert $line.0 [format %-256s \
	    [[$fh(folder_handler) get $fi] list $option(list_format)]] $tags
    $fh(message_list) configure -state disabled
}

# FolderSelect --
#
# Handle the selection of a message
#
# Arguments:
# handler -	The handler which identifies the folder window
# index   -	Index to select

proc FolderSelect {handler index} {
    upvar \#0 $handler fh
    global option t b folderUnseen

    if {![info exists fh(folder_handler)]} {
	return
    }
    set fh(list_index) $index
    if {0 == [string length $index] || $index >= $fh(num_messages)} {
	catch {unset fh(current_msg)}
	FolderButtons $handler 0
	ShowNothing $fh(text)
	set fh(list_index) ""
	return
    }
    set fh(folder_index) $fh(mapping,$index)
    if {![info exists fh(current_msg)]} {
	FolderButtons $handler 1
    }
    set line [expr {$index+1}]
    $fh(message_list) tag remove Active 1.0 end
    $fh(message_list) tag add Active $line.0 "$line.0 lineend+1c"
    $fh(message_list) see $line.0
    update idletasks
    if {![info exists fh(message_list)]} return
    set fh(current_msg) [$fh(folder_handler) get $fh(folder_index)]
    set seen [$fh(folder_handler) getFlag $fh(folder_index) seen]
    $fh(folder_handler) setFlag $fh(folder_index) seen 1
    set result [Show $fh(text) $fh(current_msg) $fh(browse)]
    set sigstatus [lindex $result 0]
    set pgpOutput [lindex $result 1]
    if { 0 == $seen } {
	FolderListRefreshEntry $handler $index
	set fh(folder_new) [RatMangleNumber $folderUnseen($fh(folder_handler))]
        UpdateFolderStatusNew $handler $folderUnseen($fh(folder_handler))
    }

    if {0 < $option(pgp_version) && [info exists fh(sigbut)]} {
	set state normal
	set command {}
	set b($fh(sigbut)) $sigstatus
	switch $sigstatus {
	pgp_none {
		set state disabled
	    }
	pgp_unchecked {
		set command "FolderCheckSignature $handler"
	    }
	pgp_good {
		set command [list RatText $t(pgp_output) $pgpOutput]
	    }
	pgp_bad {
		set command [list RatText $t(pgp_output) $pgpOutput]
	    }
	}
	$fh(sigbut) configure -state $state \
			      -text "$t(sig): $t($sigstatus)" \
			      -command $command
    }
}

# FolderDouble --
#
# Handle doubleclicks on messages
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderDouble {handler} {
    upvar \#0 $handler fh

    if {![info exists fh(current_msg)]} {
        return
    }
    if {"drafts" == $fh(special_folder)} {
        ComposeContinue $fh(current_msg)
        $fh(folder_handler) setFlag $fh(folder_index) deleted 1
        FolderListRefreshEntry $handler $fh(list_index)
    } else {
        FolderReply $handler all
    }
}

# FolderContextMenu --
#
# Popup the context menu on the indicated message
#
# Arguments:
# handler -	The handler which identifies the folder window
# x, y    -     Location in listbox
# X, Y    -     Location on screen

proc FolderContextMenu {handler x y X Y} {
    upvar \#0 $handler fh
    global t

    if {![info exists fh(folder_handler)]} {
	return
    }
    set index [expr int([$fh(message_list) index @$x,$y])-1]
    if {![info exists fh(mapping,$index)]} {
	return
    }
    set fi $fh(mapping,$index)
    if {[catch {$fh(folder_handler) get $fi} msg]} {
	return
    }

    BuildMessageMenu $handler $fh(context_menu) $msg

    tk_popup $fh(context_menu) $X $Y
}

# FolderNext --
#
# Advance to the next message in the folder
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderNext {handler} {
    upvar \#0 $handler fh
    set ml $fh(message_list)

    if {![string length $fh(list_index)]} { return }

    set index [expr {1+$fh(list_index)}]
    if { $index >= $fh(num_messages) } {
	return
    }

    if {$index >= [expr {round([lindex [$ml yview] 1]*$fh(num_messages))}]} {
	$ml yview $index
    }
    FolderSelect $handler $index
}

# FolderPrev --
#
# Retreat to the previous message in the folder
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderPrev {handler} {
    upvar \#0 $handler fh
    set ml $fh(message_list)

    if {![string length $fh(list_index)]} { return }

    set index [expr {$fh(list_index)-1}]
    if {$index < 0} {
	return
    }

    if {$index < [expr {round([lindex [$ml yview] 0]*$fh(num_messages))}]} {
	$ml yview scroll -1 pages
    }
    FolderSelect $handler $index
}

# SetFlag --
#
# Set a flag to a specified value for the current message or a set of
# messages given as argument.
#
# Arguments:
# handler  -	The handler which identifies the folder window
# flag	   -	The flag to set
# value    -	The new value of the deletion flag ('1' or '0')
# messages -	An optional list of messages to do this to
#               The list is of folder indexes.

proc SetFlag {handler flag value {messages {}}} {
    upvar \#0 $handler fh
    global option folderUnseen

    if {![string length $fh(list_index)]} { return }
    if {$fh(list_index) >= $fh(num_messages)} { return }

    if {![string length $messages]} {
	set messages $fh(mapping,$fh(list_index))
    }
    set onlist {}
    set offlist {}
    foreach i $messages {
	if {[string compare toggle $value]} {
	    if {$value} {
		lappend onlist $i
	    } else {
		lappend offlist $i
	    }
	} else {
	    if {[$fh(folder_handler) getFlag $i $flag]} {
		lappend offlist $i
	    } else {
		lappend onlist $i
	    }
	}
    }
    if {0 < [llength $onlist]} {
	$fh(folder_handler) setFlag $onlist $flag 1
    }
    if {0 < [llength $offlist]} {
	$fh(folder_handler) setFlag $offlist $flag 0
    }
    foreach i $messages {
	FolderListRefreshEntry $handler $fh(rmapping,$i)
    }
    UpdateFolderStatusNew $handler $folderUnseen($fh(folder_handler))
}

# Quit --
#
# Closes the given folder window and quits tkrat
#
# Arguments:
# handler -	The handler which identifies the folder window

proc Quit {handler} {
    global folderWindowList
    upvar \#0 $handler fh

    if {0 == [PrepareQuit]} {
	return
    }

    foreach fw [array names folderWindowList] {
	CloseFolderWin $fw
    }

    RatCleanup
    destroy .
}

# Quit --
#
# Closes the given folder window and quits tkrat
#
# Arguments:
# handler -	The handler which identifies the folder window

proc PrepareQuit {} {
    global expAfter logAfter ratSenderSending t composeWindowList \
	    alreadyQuitting folderWindowList

    if {[info exists alreadyQuitting]} {
	return 0
    } 
    set alreadyQuitting 1

    if {[info exists composeWindowList]} {
	if {0 < [llength $composeWindowList]} {
	    if {1 == [RatDialog "" $t(really_quit) $t(compose_sessions) \
                          {} 1 $t(quit_anyway) $t(dont_quit)]} {
		unset alreadyQuitting
		return 0
	    }
	}
    }
    if {1 < [array size folderWindowList]} {
        set msg [format $t(n_folder_windows) [array size folderWindowList]]
	if {1 == [RatDialog "" $t(really_quit) $msg \
                      {} 1 $t(quit_anyway) $t(dont_quit)]} {
	    unset alreadyQuitting
	    return 0
	}
    }

    AliasWeAreQuitting
    if {$ratSenderSending} {
	RatLog 2 $t(waiting_on_sender) explicit
	while {1 == $ratSenderSending} {
	    tkwait variable ratSenderSending
	}
	RatLog 2 "" explicit
    }
    if {[string length $expAfter]} {
	after cancel $expAfter
    }
    if {[string length $logAfter]} {
	after cancel $logAfter
    }
    return 1
}

# FindAdjacent --
#
# Find elemnts in new list which were adjacent to an element in an old list
#
# Arguments:
# handler   -	The handler which identifies the folder window
# old_index -   Index of old message
# old_list  -   Old list of messages

proc FindAdjacent {handler old_index old_list} {
    upvar \#0 $handler fh
    global option

    set listAfterFull [$fh(folder_handler) list %u]
    set listAfter {}
    for {set i 0} {$i <$fh(num_messages)} {incr i} {
        lappend listAfter [lindex $listAfterFull $fh(mapping,$i)]
    }
    set list [lrange $old_list [expr {$old_index+1}] end]
    for {set i $old_index} {$i >= 0} {incr i -1} {
	lappend list [lindex $old_list $i]
    }
    foreach element $list {
	if {-1 != [set index [lsearch -exact $listAfter $element]]} {
	    return $index
	}
    }
    return 0
}

# Sync --
#
# Does an update on the current folder.
#
# Arguments:
# handler   -	The handler which identifies the folder window
# mode   -	Mode of sync

proc Sync {handler mode} {
    upvar \#0 $handler fh
    global option folderExists folderUnseen

    if {![info exists fh(folder_handler)]} {return}

    if {$fh(syncing)} {
        return
    }
    set fh(syncing) 1
    set oldActive $fh(list_index)
    set listBefore $fh(uids)
    if {[string length $oldActive]} {
	set subject [lindex $listBefore $oldActive]
	set msg $fh(current_msg)
    }
    if {[llength $listBefore]} {
	# Get data about what is visible at the moment. We want...
	#  ...name & index of the top message
	#  ...to know if the active message is visible
	#  ...to know if the last message is visible
        if {[catch {
            set oldTopIndex [lindex [split [$fh(message_list) index @0,0] .] 0]
            set fi $fh(mapping,[expr $oldTopIndex-1])
            set oldTopMsg [$fh(folder_handler) get $fi]
            if {"" != $oldActive} {
                set sawActive [$fh(message_list) bbox $oldActive.0+1l]
            } else {
                set sawActive 0
            }
            set sawEnd [$fh(message_list) bbox end-1c]
        } err]} {
            unset oldTopIndex
        }
    }

    if {[catch {$fh(folder_handler) update $mode}]} {
	FolderWindowClear $handler
	set fh(syncing) 0
	return
    }
    set xview [lindex [$fh(message_list) xview] 0]
    FolderDrawList $handler

    # Update information
    set i [$fh(folder_handler) info]
    UpdateFolderStatus $handler $folderUnseen($fh(folder_handler)) \
        $folderExists($fh(folder_handler)) [lindex $i 2]
    if { 0 == [lindex $i 1] } {
	FolderSelect $handler ""
	set fh(syncing) 0
	return
    }

    # Check if our element is still in there
    set fh(list_index) ""
    if {[string length $oldActive]} {
	set findex [$fh(folder_handler) find $msg]
	if { -1 != $findex && [info exists fh(rmapping,$findex)]} {
            set fh(folder_index) $findex
            set fh(list_index) $fh(rmapping,$findex)
	    set line [expr {$fh(list_index)+1}]
	    $fh(message_list) tag add Active $line.0 "$line.0 lineend+1c"
	    $fh(message_list) see $line.0
	} else {
	    set index [FindAdjacent $handler $oldActive $listBefore]
	    FolderSelect $handler $index
	}
	if {![string length $fh(list_index)]} {
	    FolderSelect $handler 0
	}
    }
    
    # Fix scroll position in text widget
    if {[info exists oldTopIndex]} {
	# Set topmost visible message
	set findex [$fh(folder_handler) find $oldTopMsg]
	if { -1 == $findex || ![info exists fh(mapping,$findex)]} {
	    set index [FindAdjacent $handler $oldTopIndex $listBefore]
	} else {
            set index $fh(mapping,$findex)
        }
	$fh(message_list) yview $index

	# If the last message used to be visible, make sure it still is
	if {4 == [llength $sawEnd]} {
	    $fh(message_list) see {end linestart-1l}
	}

	# If the active message was visible make it visible again
	# this will override the last-message visibility
	if {4 == [llength $sawActive]} {
	    $fh(message_list) see [expr $fh(list_index)+1].0
	}
    }
    $fh(message_list) xview moveto $xview
    set fh(syncing) 0
}

# FolderReply --
#
# Construct a reply to a message and update the messages status if the
# reply was sent.
#
# Arguments:
# handler   -	The handler which identifies the folder window
# recipient -	Who the reply should be sent to 'sender' or 'all'

proc FolderReply {handler recipient} {
    upvar \#0 $handler fh

    if {![string length $fh(list_index)]} { return }

    set hd [ComposeReply $fh(current_msg) $recipient $fh(role) \
		"FolderReplySent $handler $fh(current_msg)"]
}
proc FolderReplySent {handler current} {
    upvar \#0 $handler fh

    if {[info exists fh]} {
	set findex [$fh(folder_handler) find $current]
	if { -1 != $findex } {
	    if {![$fh(folder_handler) getFlag $findex answered]} {
                if {[info exists fh(mapping,$findex)]} {
                    SetFlag $handler answered 1 $fh(mapping,$findex)
                } else {
                    $fh(folder_handler) setFlag $findex answered 1
                }
	    }
	}
    }
}

# FolderSomeCompose --
#
# Run a compose function on a message and update the message status if the
# message actually was sent.
#
# Arguments:
# handler   -	The handler which identifies the folder window

proc FolderSomeCompose {handler composeFunc} {
    upvar \#0 $handler fh
    global option

    if {![string length $fh(list_index)]} { return }

    $composeFunc $fh(current_msg) $fh(role)
}

# PostFolder --
#
# Populate the folder menu
#
# Arguments:
# handler -	The handler which identifies the folder window
# m       -	The menu which we should populate

proc PostFolder {handler m} {
    global vFolderLastUsedList vFolderDef
    upvar \#0 $handler fh
    global t option vFolderSpecials idmap$m

    if {[info exists idmap$m]} {
	unset idmap$m
    }
    $m delete 1 end
    VFolderBuildMenu $m 0 "VFolderOpen $handler" 0
    $m add separator
    foreach id $vFolderLastUsedList {
        if {[info exists vFolderDef($id)]} {
	    VFolderAddItem $m $id $id "VFolderOpen $handler" 0
	}
    }
    $m add separator
    VFolderBuildMenu $m $vFolderSpecials "VFolderOpen $handler" 0
    $m add separator
    $m add command -label $t(open_file)... \
	    -command "PostFolderOpen $handler \[SelectFileFolder $fh(toplevel)\]"
    $m add command -label $t(open_dbase)... \
	    -command "PostFolderOpen $handler \
		    \[SelectDbaseFolder $fh(toplevel)\]"
    FixMenu $m
}

# PostFolderOpen --
#
# Opens a folder choosen interactively from the PostFolder menu
#
# Arguments:
# handler -	The handler which identifies the folder window
# cmd	  -	The command which opens the new folder (if any)

proc PostFolderOpen {handler cmd} {
    if {"" != $cmd} {
	FolderRead $handler $cmd ""
    }
}


# PostMove --
#
# Populate the move menu
#
# Arguments:
# handler -	The handler which identifies the folder window
# move    -     1 for move mode (original is deleted)
# which	  -	Which set of messages we should move (current or group)
# m       -	The menu which we should populate

proc PostMove {handler move which m} {
    upvar \#0 $handler fh
    global t vFolderLastUsedList vFolderDef

    if {"current" == $which && 1 == $move} {
	set a 1
    } else {
	set a 0
    }
    $m delete 0 end
    VFolderBuildMenu $m 0 \
	    "VFolderInsert $handler $a $move \[GetMsgSet $handler $which\]" 1
    $m add separator
    foreach id $vFolderLastUsedList {
        if {[info exists vFolderDef($id)]} {
	    VFolderAddItem $m $id $id \
                "VFolderInsert $handler $a $move \[GetMsgSet $handler $which\]" 1
	}
    }
    $m add separator
    $m add command -label $t(to_file)... -command \
        "VFolderInsert $handler $a $move \[GetMsgSet $handler $which\] \
	               \[InsertIntoFile $fh(toplevel)]"
    $m add command -label $t(to_dbase)... -command \
        "VFolderInsert $handler $a $move \[GetMsgSet $handler $which\] \
		       \[InsertIntoDBase $fh(toplevel)\]"
    FixMenu $m
}

# GetMsgSet --
#
# Get the messages that the current operation should be performed on
#
# Arguments:
# handler -	The handler which identifies the folder window
# which	  -	Which set of messages we should move (current or group)

proc GetMsgSet {handler which} {
    upvar \#0 $handler fh

    switch $which {
	current {
	    if {[info exists fh(current_msg)]} {
		return $fh(current_msg)
	    } else {
		return {}
	    }
	}
	group {
	    set msgs {}
	    foreach i [$fh(folder_handler) flagged flagged 1] {
		lappend msgs [$fh(folder_handler) get $i]
	    }
	    return $msgs
	}
	default {
	    return $which
	}
    }
}

# FolderButtons --
#
# Enable or disable the buttons in the folder window which depends on
# an active message.
#
# Arguments:
# handler -	The handler which identifies the folder window
# onoff   -	The new state of the buttons

proc FolderButtons {handler onoff} {
    upvar \#0 $handler fh
    global option t b
    set w $fh(w)

    if {$onoff} {
	set state normal
    } else {
	set state disabled
	if {0 < $option(pgp_version) && [info exists fh(sigbut)]} {
	    $fh(sigbut) configure -state disabled -text "$t(sig): $t(none)"
	    set b($fh(sigbut)) pgp_none
	}
    }
    foreach but [list $w.b.buttons.move \
		      $w.b.buttons.delete \
		      $w.b.buttons.reply_sender \
		      $w.b.buttons.reply_all] {
	$but configure -state $state
    }
    foreach m $fh(menu_nokeep) {
	[lindex $m 0] entryconfigure [lindex $m 1] -state $state
    }
}

# FolderGetNextUnread --
#
# Return the index of the next unread message in folder after index,
# or index if none is found.
#
# Arguments:
# handler -	The handler which identifies the folder window
# index   -	Where in the list to start looking

proc FolderGetNextUnread {handler index dir} {
    upvar \#0 $handler fh

    if {0 == $fh(num_messages)} {
	return 0
    }
    for {set i [expr {$index+$dir}]} {$i != $index} {incr i $dir} {
	if {$i >= $fh(num_messages)} {
	    set i 0
	} elseif {$i < 0} {
	    set i [expr {$fh(num_messages)-1}]
	}
	if {$i == $index} {
	    return 0
	}
	if { 0 == [$fh(folder_handler) getFlag $fh(mapping,$i) seen]} {
	    return $i
	}
    }
    return $index
}


# FolderSelectUnread --
#
# Selects the next unread message in the folder.
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderSelectUnread {handler} {
    upvar \#0 $handler fh

    set index $fh(list_index)
    if {0 == [llength $index]} {
	return
    }
    set i [FolderGetNextUnread $handler $index 1]
    FolderSelect $handler $i
}

# GroupMessageList --
#
# Pops a message list that lets the user select messages for a group
#
# Arguments:
# handler -	The handler which identifies the folder window

proc GroupMessageList {handler} {
    global b idCnt t option
    upvar \#0 $handler fh

    # Create identifier
    set id f[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(edit_group)
    frame $w.f
    scrollbar $w.f.scroll \
        -relief sunken \
        -command "$w.f.list yview" \
	-highlightthickness 0
    listbox $w.f.list \
        -yscroll "$w.f.scroll set" \
        -exportselection false \
	-highlightthickness 0 \
	-selectmode multiple \
	-setgrid true
    set b($w.f.list) group_list_editor
    pack $w.f.scroll -side right -fill y
    pack $w.f.list -side left -expand 1 -fill both
    frame $w.buttons
    button $w.buttons.ok -text $t(ok) \
	    -command "GroupMessageListDone $w $handler 1"
    set b($w.buttons.ok) group_window_ok
    button $w.buttons.sel -text $t(select_all) \
	    -command "$w.f.list selection set 0 end"
    set b($w.buttons.sel) group_window_selall
    button $w.buttons.unsel -text $t(deselect_all) \
	    -command "$w.f.list selection clear 0 end"
    set b($w.buttons.unsel) group_window_unselall
    button $w.buttons.cancel -text $t(cancel) \
	    -command "GroupMessageListDone $w $handler 0"
    set b($w.buttons.cancel) cancel
    pack $w.buttons.ok \
	 $w.buttons.sel \
	 $w.buttons.unsel \
	 $w.buttons.cancel -side left -expand 1
    pack $w.buttons -side bottom -fill x -pady 5
    pack $w.f -expand 1 -fill both

    set fi 0
    set li 0
    foreach e [$fh(folder_handler) list "%u $option(list_format)"] {
	regexp {^([^ ]*) (.*)} $e unused uid list_entry
        if {"" == $fh(filter)
            || [string match -nocase "*$fh(filter)*" $list_entry]} {
            lappend fh($w.uids) $uid
            $w.f.list insert end $list_entry
            set rmapping($fi) $li
            incr li
        }
        incr fi
    }
    foreach i [$fh(folder_handler) flagged flagged 1] {
        if {[info exists rmapping($i)]} {
            $w.f.list selection set $rmapping($i)
        }
    }
    lappend fh(groupMessageLists) $w
    bind $w.f.list <Destroy> "GroupMessageListDone $w $handler 0"

    ::tkrat::winctl::SetGeometry groupMessages $w $w.f.list
}

# GroupMessageListUpdate --
#
# Update the message list since the underlying folder was updated
#
# Arguments:
# w	  -	The group selection window
# handler -	The handler which identifies the folder window

proc GroupMessageListUpdate {w handler} {
    upvar \#0 $handler fh
    global option

    foreach c [$w.f.list curselection] {
	set selected([lindex $fh($w.uids) $c]) 1
    }
    set top [lindex [$w.f.list yview] 0]
    $w.f.list delete 0 end
    set fh($w.uids) {}
    foreach e [$fh(folder_handler) list "%u $option(list_format)"] {
	regexp {^([^ ]*) (.*)} $e unused uid list_entry
        if {"" != $fh(filter)
            && ![string match -nocase "*$fh(filter)*" $list_entry]} {
            continue
        }
	lappend fh($w.uids) $uid
	$w.f.list insert end $list_entry
	if {[info exists selected($uid)]} {
	    $w.f.list selection set end
	}
    }
    $w.f.list yview moveto $top
}

# GroupMessageListDone --
#
# Calls when the grouping is done
#
# Arguments:
# w	  -	The group selection window
# handler -	The handler which identifies the folder window
# done    -	The users selection (1=ok, 0=cancel)

proc GroupMessageListDone {w handler done} {
    upvar \#0 $handler fh
    global b option

    bind $w.f.list <Destroy> {}
    if {$done} {
	set candidates [$w.f.list curselection]
	set isset [$fh(folder_handler) flagged flagged 1]
	set toset {}
	set toclear {}
        set torefresh {}
	for {set i 0} {$i < [$w.f.list size]} {incr i} {
	    set nv [expr {-1 != [lsearch $candidates $i]}]
            set ov [expr {-1 != [lsearch $isset $i]}]
            if {$nv != $ov} {
                if {$nv} {
                    lappend toset $fh(mapping,$i)
                } else {
                    lappend toclear $fh(mapping,$i)
                }
                lappend torefresh $i
            }
	}
	$fh(folder_handler) setFlag $toset flagged 1
	$fh(folder_handler) setFlag $toclear flagged 0
	foreach i $torefresh {
	    FolderListRefreshEntry $handler $i
	}
    }
    ::tkrat::winctl::RecordGeometry groupMessages $w $w.f.list
    set index [lsearch $w $fh(groupMessageLists)]
    set fh(groupMessageLists) [lreplace $fh(groupMessageLists) $index $index]
    destroy $w
    unset fh($w.uids)
    foreach a [array names b $w*] {
	unset b($a)
    }
}

# GroupClear --
#
# Removes the flag from every message
#
# Arguments:
# handler -	The handler which identifies the folder window

proc GroupClear {handler} {
    upvar \#0 $handler fh
    global option

    foreach i [$fh(folder_handler) flagged flagged 1] {
	$fh(folder_handler) setFlag $i flagged 0
	FolderListRefreshEntry $handler $fh(rmapping,$i)
    }
}

# SetupGroupMenu --
#
# Setup the entries in the group menu
#
# Arguments:
# m	  -	The menu command name
# handler -	The handler which identifies the folder window

proc SetupGroupMenu {m handler} {
    upvar \#0 $handler fh
    global t

    # Create groups
    if {$fh(num_messages) > 0} {
        set s normal
    } else {
        set s disabled
    }
    foreach i {1 2 3} {
        $m entryconfigure $i -state $s
    }

    # Group operations
    if {![info exists fh(folder_handler)]} {
	set s disabled
    } elseif {[set num [llength [$fh(folder_handler) flagged flagged 1]]]} {
	set s normal
    } else {
	set s disabled
    }
    foreach i {4 6 7 8 9 11 12 13 15 16 17 18 19 20} {
        $m entryconfigure $i -state $s
    }
    # Number of grouped messages
    $m entryconfigure 4 -label "$t(clear_group) ($num)"

    # Dsiable some ops in drafts folder
    if {$s == "normal"} {
        if {"drafts" == $fh(special_folder)} {
            set s disabled
        } else {
            set s normal
        }
        foreach i {17 18 19 20} {
            $m entryconfigure $i -state $s
        }
    }
}

# CycleShowHeader --
#
# Cycle through the values of the show_header option
#
# Arguments:
# handler -	The handler which identifies the folder window

proc CycleShowHeader {handler} {
    global option
    upvar \#0 $handler fh
    upvar \#0 $fh(text) texth

    switch $texth(show_header) {
    all		{ set texth(show_header) selected }
    selected	{ set texth(show_header) no }
    no		{ set texth(show_header) all }
    }
    FolderSelect $handler $fh(list_index)
    SaveOptions
}

# FolderCheckSignature --
#
# Check the signature(s) of the current message
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderCheckSignature {handler} {
    upvar \#0 $handler fh
    upvar \#0 msgInfo_$fh(current_msg) msgInfo
    global t b

    set tot pgp_none
    set b($fh(sigbut)) pgp_none
    set first 1
    set result {}
    foreach bodypart $msgInfo(pgp,signed_parts) {
	set part [string map [list "\a" ""] [$bodypart checksig]]
	if {[string length $result]} {
	    set result "$totresult\n\n$part"
	} else {
	    set result $part
	}
	set status [$bodypart sigstatus]
	set tot $status
	set b($fh(sigbut)) $status
	if {![string compare pgp_good $status] && $first} {
	    set first 0
	    if {[string compare $bodypart [$fh(current_msg) body]]} {
		set tot pgp_part
		set b($fh(sigbut)) part_sig
	    }
	}
    }
    if {[string length $result]} {
	RatText $t(pgp_output) $result
    }
    $fh(sigbut) configure -state normal \
			  -text "$t(sig): $t($tot)" \
			  -command [list RatText $t(pgp_output) $result]
}

# FolderFind --
#
# Find text in a message or the message list
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderFind {handler} {
    global t b idCnt
    upvar \#0 $handler fh

    # Create identifier
    set id f[incr idCnt]
    upvar \#0 $id hd
    set w .$id

    # Initialize variables
    set hd(match_case) $fh(find_match_case)
    set hd(match) $fh(find_match)
    if {"list" == $fh(find_loc)} {
	set hd(loc) $fh(message_list)
    } else {
	set hd(loc) $fh(text)
    }
    set hd(w) $w
    set hd(handler) $handler
    set hd(def_start) 0
    set hd(oldfocus) [focus]
    set hd(text) ""

    # Create toplevel
    toplevel $w -class TkRat -bd 5
    wm title $w $t(find)

    # Create window
    frame $w.l
    label $w.l.label -text $t(find_in):
    radiobutton $w.l.list -text $t(message_list) -variable ${id}(loc) \
	    -value $fh(message_list)
    radiobutton $w.l.body -text $t(message_body) -variable ${id}(loc) \
	    -value $fh(text)
    pack $w.l.label \
	 $w.l.list \
	 $w.l.body -side left -anchor w
    set b($w.l.list) find_in_mlist
    set b($w.l.body) find_in_body

    entry $w.e -textvariable ${id}(text)
    set b($w.e) enter_search_text_here

    checkbutton $w.c -text $t(match_case) -variable ${id}(match_case)
    set b($w.c) toggle_ignore_case

    frame $w.b
    frame $w.b.find_next -relief sunken -bd 2
    button $w.b.find_next.b -text $t(find_next) -state disabled
    pack $w.b.find_next.b -padx 1 -pady 1
    frame $w.b.find_prev -relief flat -bd 2
    button $w.b.find_prev.b -text $t(find_prev) -state disabled
    pack $w.b.find_prev.b -padx 1 -pady 1
    button $w.b.dismiss -text $t(dismiss) -command "destroy $w"
    pack $w.b.find_next \
	 $w.b.find_prev \
	 $w.b.dismiss -side left -expand 1
    set b($w.b.find.b) find_first
    set b($w.b.find_next.b) find_next
    set b($w.b.find_prev.b) find_prev
    set b($w.b.dismiss) dismiss

    pack $w.l \
	 $w.e \
	 $w.c \
	 $w.b -side top -fill x -expand 1

    ::tkrat::winctl::SetGeometry find $w
    focus $w.e
    bind $w.e <Return> [list $w.b.find_next invoke]
    bind $w.e <Destroy> "FolderFindDone $id"

    set hd(find_args) \
        [list $w.e $w.c $w.b.find_next.b $w.b.find_prev.b]
    trace variable hd(loc) w "FinderFindTrace $id"
    FinderFindTrace $id
}

# FinderFindTrace --
#
# Trace the find text variable and change the button state accordingly
#
# Arguments:
# id      -	The handler which identifies the find window

proc FinderFindTrace {id args} {
    upvar \#0 $id hd

    if {[info exists hd(find)]} {
        rat_find::uninit $hd(find)
    }
    set hd(find) [eval rat_find::init $hd(loc) $hd(find_args)]
}

# FolderFindDone --
#
# Close find window
#
# Arguments:
# id      -	The handler which identifies the find window

proc FolderFindDone {id} {
    upvar \#0 $id hd
    global b

    if {[info exists hd(find)]} {
        rat_find::uninit $hd(find)
    }
    ::tkrat::winctl::RecordGeometry find $hd(w)
    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    foreach a [array names b $hd(w)*] {
	unset b($a)
    }
    unset hd
}

# FolderMap --
#
# Called when the folder window is mapped
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderMap {handler} {
    upvar \#0 $handler fh
    global option

    if {[info exists fh(folder_handler)]} {
	WatcherSleepFH $fh(folder_handler)
    }
    if {0 != $option(checkpoint_interval)} {
	set fh(checkpointaid) \
		[after [expr {$option(checkpoint_interval)*1000}] \
		"FolderCheckpoint $handler"]
    }
}


# FolderUnmap --
#
# Called when the folder window is unmapped
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderUnmap {handler} {
    upvar \#0 $handler fh
    global option

    if {$option(checkpoint_on_unmap)} {
	#RatBusy {Sync $handler checkpoint}
	Sync $handler checkpoint
    }
    if {[info exists fh(checkpointaid)]} {
	after cancel $fh(checkpointaid)
	unset fh(checkpointaid)
    }
}

# FolderCheckpoint --
#
# Called when the folder window should be periodically checkpointed
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderCheckpoint {handler} {
    upvar \#0 $handler fh
    global option

    RatBusy {Sync $handler checkpoint}
    if {0 != $option(checkpoint_interval)} {
	set fh(checkpointaid) \
		[after [expr {$option(checkpoint_interval)*1000}] \
		"FolderCheckpoint $handler"]
    }
}

# NewFolderMenu --
#
# Populate the new folder menu
#
# Arguments:
# handler -	The handler which identifies the folder window
# m       -	The menu which we should populate

proc NewFolderMenu {handler m} {
    upvar \#0 $handler fh
    global t vFolderSpecials vFolderLastUsedList vFolderDef

    $m delete 0 end
    VFolderBuildMenu $m 0 NewFolderWin 0
    $m add separator
    foreach id $vFolderLastUsedList {
        if {[info exists vFolderDef($id)]} {
	    VFolderAddItem $m $id $id NewFolderWin 0
	}
    }
    $m add separator
    VFolderBuildMenu $m $vFolderSpecials NewFolderWin 0
    $m add separator
    $m add command -label $t(open_file)... -command "NewFolderWin openfile"
    $m add command -label $t(open_dbase)... -command "NewFolderWin opendbase"

    $m add command -label $t(dbase_same_subject)
    set sub_idx [$m index end]
    $m add command -label $t(dbase_to_from_sender)
    set send_idx [$m index end]
    $m add command -label $t(empty) -command "NewFolderWin empty"

    if {![info exists fh(current_msg)]} {
        $m entryconfigure $sub_idx -state disabled
        $m entryconfigure $send_idx -state disabled
    } else {
        set subject [$fh(current_msg) list "%c"]
        set exp [list and subject \"$subject\"]
        set vf_subject [list $t(dbase_same_subject) dbase {} {} {} $exp]
        $m entryconfigure $sub_idx -state normal \
            -command [list NewFolderWin $vf_subject]

        set from [$fh(current_msg) get from]
        if {"" == $from} {
            set from [$fh(current_msg) get sender]
        }
        if {"" == $from} {
            set from [$fh(current_msg) get reply_to]
        }
        if {"" == $from} {
            $m entryconfigure $send_idx -state disabled
        } else {
            set exp [list and all_addresses [$from get mail]]
            set vf_sender [list $t(dbase_to_from_sender) dbase {} {} {} $exp]
            $m entryconfigure $send_idx -state normal \
                -command [list NewFolderWin $vf_sender]
        }
    }

    FixMenu $m
}

# NewFolderWin --
#
# Create a new folder window and populate it
#
# Arguments:
# vf	- VFolder definition of folder to open

proc NewFolderWin {vf} {
    global idCnt option folderWindowList

    # Complement folder information (if needed)
    set manual 0
    if {"openfile" == $vf} {
	upvar \#0 [lindex [array names folderWindowList] 0] fh
	set vf [SelectFileFolder $fh(toplevel)]
	if {"" == $vf} return
	set manual 1
    } elseif {"opendbase" == $vf} {
	upvar \#0 [lindex [array names folderWindowList] 0] fh
	set vf [SelectDbaseFolder $fh(toplevel)]
	if {"" == $vf} return
	set manual 1
    }

    # Create folder window
    set w .f[incr idCnt]
    toplevel $w -class TkRat
    SetIcon $w $option(icon)
    set handler [FolderWindowInit $w ""]
    UpdateFolderTitle $handler
    ::tkrat::winctl::Place folderWindow $w
    if {"empty" != $vf} {
	if {$manual} {
	    FolderRead $handler $vf ""
	} else {
	    VFolderOpen $handler $vf
	}
    }
}

# CloseFolder --
#
# Closes the given folder
#
# Arguments:
# handler -	The handler which identifies the folder

proc CloseFolder {handler} {
    catch {$handler close}
}

# CloseFolderWin --
#
# Closes the given folder window
#
# Arguments:
# handler -	The handler which identifies the folder window

proc CloseFolderWin {handler} {
    global folderWindowList
    upvar \#0 $handler fh

    if {[catch {unset folderWindowList($handler)}]} {
	return
    }
    if {[info exists fh(folder_handler)]} {
	CloseFolder $fh(folder_handler) 
    }
    if {[winfo exists $fh(w)]} {
        ::tkrat::winctl::RecordGeometry folderWindow \
            [winfo toplevel $fh(w)] [winfo toplevel $fh(w)] $fh(pane)
	bind $fh(text) <Destroy> { }
	destroy $fh(w)
    }
    unset fh
}

# DestroyFolderWin --
#
# Destroys the given folder window, if it was the last then we also
# quit tkrat.
#
# Arguments:
# handler -	The handler which identifies the folder window
# force   -	True if the window really will be closed

proc DestroyFolderWin {handler force} {
    global folderWindowList

    if {1 < [array size folderWindowList] || ($force && 1 == [PrepareQuit])} {
        CloseFolderWin $handler
        if {0 == [array size folderWindowList]} {
            RatCleanup
            destroy .
        }
    }
}

# FolderFlagEvent --
#
# Toggle flag status of current message
#
# Arguments:
# handler -	The handler which identifies the folder window
# index -	Message under pointer

proc FolderFlagEvent {handler index} {
    upvar \#0 $handler fh

    set fh(setflag) [expr {int($index-1)}]
    set fi $fh(mapping,$fh(setflag))
    SetFlag $handler flagged toggle $fi
    set fh(lastFlagResult) [$fh(folder_handler) getFlag $fi flagged]
}

# FolderFlagMotion --
#
# Toggle flag status of current message
#
# Arguments:
# handler -	The handler which identifies the folder window
# index -	Message under pointer

proc FolderFlagMotion {handler index} {
    upvar \#0 $handler fh

    set i [expr {int($index-1)}]
    if {$i != $fh(setflag)} {
	if {$i > $fh(setflag)} {
	    for {set ui $i} {$ui > $fh(setflag)} {incr ui -1} {
		SetFlag $handler flagged $fh(lastFlagResult) $fh(mapping,$ui)
	    }
	} else {
	    for {set ui $i} {$ui < $fh(setflag)} {incr ui} {
		SetFlag $handler flagged $fh(lastFlagResult) $fh(mapping,$ui)
	    }
	}
	set fh(setflag) $i
    }
}

# FolderFlagRange --
#
# Toggle flag status of messages between current and last
#
# Arguments:
# handler -	The handler which identifies the folder window
# index -	Message under pointer

proc FolderFlagRange {handler index} {
    upvar \#0 $handler fh

    if {"" != $fh(setflag)} {
	FolderFlagMotion $handler $index
    }
}


# NetworkSyncs --
#
# Do network synchronization
#
# Arguments:

proc NetworkSync {} {
    global option t numDeferred folderWindowList

    if {[lindex $option(network_sync) 2]} {
	RatLog 2 $t(running_cmd) explicit
	if {[catch "exec [lindex $option(network_sync) 3]" error]} {
	    Popup $error
	}
	RatLog 2 "" explicit
    }

    if {[lindex $option(network_sync) 0]} {
	RatNudgeSender
    }

    if {[lindex $option(network_sync) 1]} {
	RatSyncDisconnected
	foreach f [array names folderWindowList] {
	    upvar \#0 $f fh
	    if {[info exists fh(folder_handler)] 
		    && "" != $fh(folder_handler)
		    && "dis" == [$fh(folder_handler) type]} {
		Sync $f update
	    }
	}
    }
}

# SetOnlineStatus --
#
# Update all instanses of tkrat to reflect the current online status
#
# Arguments:
# online - true if new mode is online

proc SetOnlineStatus {online} {
    global tkrat_menus tkrat_online_index option t b \
	    tkrat_online_imgs online_img offline_img

    if {$option(online) != $online} {
	catch {RatLibSetOnlineMode $online}
    }
    if {$option(online)} {
	set label $t(go_offline)
	set bal go_offline
	set ibal icon_online
	set cmd "RatBusy {SetOnlineStatus 0}; SaveOptions"
	set img $online_img
    } else {
	set label $t(go_online)
	set bal go_online
	set ibal icon_offline
	set cmd "RatBusy {SetOnlineStatus 1}; SaveOptions"
	set img $offline_img
    }

    set imgs {}
    foreach i $tkrat_online_imgs {
	if {[winfo exists $i]} {
	    $i configure -image $img -command $cmd
	    lappend imgs $i
	    set b($i) $ibal
	}
    }
    set tkrat_online_imgs $imgs

    foreach m $tkrat_menus {
	if {[winfo exists $m]} {
	    $m entryconfigure $tkrat_online_index -label $label -command $cmd
	    set b($m,$tkrat_online_index) $bal
	}
    }
}

# PostRoles --
#
# Post command for the roles menu. Populates the menu with the list
# of roles.
#
# Arguments:
# handler -	The handler which identifies the folder window
# m       -     Menu to populate

proc PostRoles {handler m cmd} {
    global option

    $m delete 0 end
    foreach r $option(roles) {
	$m add radiobutton -variable ${handler}(role) -value $r \
		-label $option($r,name) -command $cmd
    }
}

# UpdateFolderTitle --
#
# Updates the title of a folder window
#
# Arguments:
# handler -	The handler which identifies the folder window

proc UpdateFolderTitle {handler} {
    global option
    upvar \#0 $handler fh

    set title [string map [list %f $fh(folder_name) \
			       %r $option($fh(role),name)] \
		   $option(main_window_name)]
    wm title $fh(toplevel) $title
    set ititle [string map [list %f $fh(folder_name)] $option(icon_name)]
    wm iconname $fh(toplevel) $ititle
}

# FilterClear --
#
# Clears the filter
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FilterClear {handler} {
    upvar \#0 $handler fh

    set fh(filter) ""
    set fh(last_filter) $fh(filter)
    Sync $handler update
    FilterChanged $handler

    if {[focus] == $fh(filter_entry)} {
        focus $fh(w)
    }
}

# FilterApply --
#
# Applies the filter
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FilterApply {handler} {
    upvar \#0 $handler fh

    set fh(last_filter) $fh(filter)
    Sync $handler update
    $fh(filter_apply) configure -state disabled

    focus $fh(w)
}

# FilterChanged --
#
# Callback which is called when the filter definition changes
#
# Arguments:
# handler -	The handler which identifies the folder window
# args    -     Standard trace callback arguments

proc FilterChanged {handler args} {
    upvar \#0 $handler fh

    if {"" == $fh(filter)} {
        $fh(filter_clear) configure -state disabled
    } else {
        $fh(filter_clear) configure -state normal
    }
    if {$fh(last_filter) == $fh(filter)} {
        $fh(filter_apply) configure -state disabled
    } else {
        $fh(filter_apply) configure -state normal
    }
}

# SetSortOrder --
#
# Callback when the user has selected a new sort order in the menu.
# Should update the current sort order and set the default sort order
# to the selected one.
#
# Arguments:
# handler -	The handler which identifies the folder window

proc SetSortOrder {handler} {
    upvar \#0 $handler fh
    global option

    set option(folder_sort) $fh(folder_sort)
    $fh(folder_handler) setSortOrder $fh(folder_sort)
    RatBusy {Sync $handler update}
    SaveOptions
}

# SetShowHeaders --
#
# Updates the show_header setting. Both for this folder and the
# default.
#
# Arguments:
# handler -	The handler which identifies the folder window

proc SetShowHeaders {handler} {
    upvar \#0 $handler fh
    upvar \#0 $fh(text) texth
    global option

    set option(show_header) $texth(show_header)
    FolderSelect $handler $fh(list_index)
    SaveOptions
}

# SetWrapMode --
#
# Updates the wrap mode settings. Both for this folder and the
# default.
#
# Arguments:
# handler -	The handler which identifies the folder window

proc SetWrapMode {handler} {
    upvar \#0 $handler fh
    global option

    set option(wrap_mode) $fh(wrap_mode)
    $fh(text) configure -wrap $fh(wrap_mode)
    SaveOptions
}
