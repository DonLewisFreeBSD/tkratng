# folder.tcl --
#
# This file contains code which handles a folder window.
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
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
	    propLightFont fixedNormFont tkrat_menus tkrat_online_index \
	    online_img tkrat_online_imgs numDeferred


    # Create the handler
    set handler f[incr idCnt]
    upvar #0 $handler fh


    # Initialize variables
    set fh(toplevel) [winfo toplevel $w]
    set fh(w) $w
    set fh(folder_name) {}
    set fh(folder_size) {}
    set fh(folder_messages) {}
    set fh(folder_new) {}
    set fh(num_messages) 0
    set fh(groupMessageLists) {}
    set fh(message_scroll) $w.t.messlist.scroll
    set fh(message_list) $w.t.messlist.list
    set fh(group) {}
    set fh(text) $w.b.text.text
    set fh(find_ignore_case) 1
    set fh(find_match) exact
    set fh(find_loc) body
    set fh(browse) 0
    set fh(setflag) ""
    set fh(syncing) 0
    set fh(menu_nokeep) {}
    set fh(role) $option(default_role)
    upvar #0 $fh(text) texth
    set texth(show_header) $option(show_header)
    set texth(struct_menu) $w.t.mbar.message.m.structmenu

    Size $w folderWindow
    frame $w.t
    frame $w.b

    # The menu and information line
    frame $w.t.mbar -relief raised -bd 1
    FindAccelerators a {tkrat folders message group show admin help}

    # Tkrat menu
    menubutton $w.t.mbar.tkrat -menu $w.t.mbar.tkrat.m -text $t(tkrat) \
	    -underline $a(tkrat)
    set m $w.t.mbar.tkrat.m
    menu $m -tearoff 1 -tearoffcommand "lappend tkrat_menus" \
	    -postcommand "PostTkRat $handler"
    $m add checkbutton -label $t(watcher) \
	    -variable option(watcher_enable) -onvalue 1 -offvalue 0 \
	    -command SaveOptions
    set b($m,[$m index end]) watcher_enable
    $m add command -label $t(setup_netsync)... -command SetupNetworkSync
    set b($m,[$m index end]) setup_netsync
    $m add command -label $t(netsync) -command "RatBusy NetworkSync"
    set b($m,[$m index end]) netsync
    set fh(netsync_all_menu) [list $m [$m index end]]
    $m add command
    lappend tkrat_menus $m
    set fh(online_menu) [list $m [$m index end]]
    set tkrat_online_index [$m index end]
    $m add separator
    $m add cascade -label $t(new_folder) -menu $m.new_folder
    set b($m,[$m index end]) new_folder
    menu $m.new_folder -postcommand "NewFolderMenu $m.new_folder"
    $m add separator
    $m add command -label $t(reread_aliases) -command AliasRead
    set b($m,[$m index end]) reread_aliases
    $m add command -label $t(reread_userproc) -command ReadUserproc
    set b($m,[$m index end]) reread_userproc
    $m add command -label $t(import_aliases) -command ScanAliases
    set b($m,[$m index end]) import_aliases
    $m add command -label $t(reread_mailcap) -command RatMailcapReload
    set b($m,[$m index end]) reread_mailcap
    $m add command -label $t(take_mail)... -command "MailSteal $handler 0"
    set b($m,[$m index end]) take_mail
    $m add separator
    $m add command -label $t(notifications)... -command ShowDSNList
    set b($m,[$m index end]) show_notifications
    $m add command -label $t(check_dbase)... \
	    -command "RatBusy \"DbaseCheck 0\""
    set b($m,[$m index end]) check_dbase
    $m add command -label $t(check_fix_dbase)... \
	    -command "RatBusy \"DbaseCheck 1\""
    set b($m,[$m index end]) check_fix_dbase
    $m add command -label $t(see_log)... -command SeeLog
    set b($m,[$m index end]) see_log
    $m add separator
    $m add command -label $t(close) -command "DestroyFolderWin $handler"
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
    menu $m -tearoff 1
    $m add command -label $t(find)... -command "FolderFind $handler"
    set b($m,[$m index end]) find
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set fh(find_menu) [list $m [$m index end]]
    $m add separator
    $m add command -label $t(compose)... -command "Compose \$${handler}(role)"
    set fh(compose_menu) [list $m [$m index end]]
    set b($m,[$m index end]) compose
    $m add command -label $t(reply_sender)... \
	    -command "FolderReply $handler sender"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) reply_sender
    set fh(replys_menu) [list $m [$m index end]]
    $m add command -label $t(reply_all)... -command "FolderReply $handler all"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) reply_all
    set fh(replya_menu) [list $m [$m index end]]
    $m add command -label $t(forward_inline)... \
	    -command "FolderSomeCompose $handler ComposeForwardInline"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) forward_inline
    set fh(forward_i_menu) [list $m [$m index end]]
    $m add command -label $t(forward_as_attachment)... \
	    -command "FolderSomeCompose $handler ComposeForwardAttachment"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) forward_attached
    set fh(forward_a_menu) [list $m [$m index end]]
    $m add command -label $t(bounce)... \
	    -command "FolderSomeCompose $handler ComposeBounce"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) bounce
    set fh(bounce_menu) [list $m [$m index end]]
    $m add command -label $t(getheld)... -command ComposeHeld
    set b($m,[$m index end]) compose_held
    $m add command -label $t(extract_adr)... -command "AliasExtract $handler"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) extract_adr
    $m add separator
    $m add cascade -label $t(move) -menu $m.move
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) move
    menu $m.move -postcommand "PostMove $handler current $m.move"
    $m add command -label $t(delete) \
	    -command "SetFlag $handler deleted 1; FolderNext $handler"
    set fh(delete_menu) [list $m [$m index end]]
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) delete
    $m add command -label $t(undelete) \
	    -command "SetFlag $handler deleted 0; FolderNext $handler"
    set fh(undelete_menu) [list $m [$m index end]]
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) undelete
    $m add command -label $t(mark_as_unread) \
	    -command "SetFlag $handler seen 0; FolderNext $handler"
    set fh(markunread_menu) [list $m [$m index end]]
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) mark_as_unread
    $m add command -label $t(print)... -command "Print $handler current"
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set fh(print_menu) [list $m [$m index end]]
    set b($m,[$m index end]) print
    $m add cascade -label $t(structure) -menu $m.structmenu
    lappend fh(menu_nokeep) [list $m [$m index end]]
    set b($m,[$m index end]) structure

    # Show menu
    menubutton $w.t.mbar.show -menu $w.t.mbar.show.m -text $t(show) \
			      -underline $a(show)
    set b($w.t.mbar.show) show_menu
    set m $w.t.mbar.show.m
    menu $m -tearoff 1
    $m add radiobutton -label $t(no_wrap) \
	    -variable option(wrap_mode) -value none \
	    -command "$fh(text) configure -wrap \$option(wrap_mode);SaveOptions"
    set b($m,[$m index end]) show_no_wrap
    $m add radiobutton -label $t(wrap_char) \
	    -variable option(wrap_mode) -value char \
	    -command "$fh(text) configure -wrap \$option(wrap_mode);SaveOptions"
    set b($m,[$m index end]) show_wrap_char
    $m add radiobutton -label $t(wrap_word) \
	    -variable option(wrap_mode) -value word \
	    -command "$fh(text) configure -wrap \$option(wrap_mode);SaveOptions"
    set b($m,[$m index end]) show_wrap_word
    $m add separator
    $m add radiobutton -label $t(show_all_headers) \
	    -variable $fh(text)(show_header) -value all \
	    -command "FolderSelect $fh(message_list) $handler \
		      \$${handler}(active) 0; SaveOptions"
    set b($m,[$m index end]) show_all_headers
    $m add radiobutton -label $t(show_selected_headers) \
	    -variable $fh(text)(show_header) -value selected \
	    -command "FolderSelect $fh(message_list) $handler \
		      \$${handler}(active) 0; SaveOptions"
    set b($m,[$m index end]) show_selected_headers
    $m add radiobutton -label $t(show_no_headers) \
	    -variable $fh(text)(show_header) -value no \
	    -command "FolderSelect $fh(message_list) $handler \
		      \$${handler}(active) 0; SaveOptions"
    set b($m,[$m index end]) show_no_headers

    # Group menu
    menubutton $w.t.mbar.group -menu $w.t.mbar.group.m -text $t(group) \
			      -underline $a(group)
    set b($w.t.mbar.group) group_menu
    set m $w.t.mbar.group.m
    menu $m -postcommand "SetupGroupMenu $m $handler" -tearoff 1
    $m add command -label $t(create_in_win)... \
	    -command "GroupMessageList $handler"
    set b($m,[$m index end]) create_in_win
    lappend fh(menu_nokeep) [list $m [$m index end]]
    $m add command -label $t(create_by_expr)... -command "ExpCreate $handler"
    set b($m,[$m index end]) create_by_expr
    lappend fh(menu_nokeep) [list $m [$m index end]]
    $m add cascade -label $t(use_saved_expr) -menu $m.saved
    set b($m,[$m index end]) use_saved_expr
    lappend fh(menu_nokeep) [list $m [$m index end]]
    menu $m.saved -postcommand "ExpBuildMenu $m.saved $handler"
    $m add command -label $t(clear_group) -command "GroupClear $handler"
    set b($m,[$m index end]) clear_group
    $m add separator
    $m add command -label $t(delete) -command "SetFlag \
	    $handler deleted 1 \[\$${handler}(folder_handler) flagged flagged\]"
    set b($m,[$m index end]) delete_group
    $m add command -label $t(undelete) -command "SetFlag \
	    $handler deleted 0 \[\$${handler}(folder_handler) flagged flagged\]"
    set b($m,[$m index end]) undelete_group
    $m add command -label $t(print) -command "Print $handler group"
    set b($m,[$m index end]) print_group
    $m add cascade -label $t(move) -menu $m.move
    set b($m,[$m index end]) move_group
    menu $m.move -postcommand "PostMove $handler group $m.move"

    # Admin menu
    menubutton $w.t.mbar.admin -menu $w.t.mbar.admin.m -text $t(admin) \
			     -underline $a(admin)
    set b($w.t.mbar.admin) admin_menu
    set m $w.t.mbar.admin.m
    menu $m -tearoff 1
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
    $m add separator
    $m add command -label $t(newedit_folder)... -command VFolderDef
    set b($m,[$m index end]) newedit_folder
    $m add command -label $t(aliases)... -command Aliases
    set b($m,[$m index end]) aliases
    $m add command -label $t(preferences)... -command Preferences
    set b($m,[$m index end]) preferences
    $m add command -label $t(define_keys)... -command "KeyDef folder"
    set b($m,[$m index end]) define_keys
    $m add command -label $t(saved_expr)... -command "ExpHandleSaved $handler"
    set b($m,[$m index end]) saved_expr
    $m add command -label $t(purge_pwcache) -command RatPurgePwChache
    set b($m,[$m index end]) purge_pwcache
    $m add command -label $t(reimport_all) -command VFolderReimportAll
    set b($m,[$m index end]) reimport_all
    $m add separator
    $m add cascade -label $t(role) -menu $m.role
    set b($m,[$m index end]) folder_select_role
    menu $m.role -postcommand \
	    [list PostRoles $handler $m.role [list UpdateFolderTitle $handler]]
    $m add cascade -label $t(sort_order) -menu $m.sort
    set b($m,[$m index end]) sort_order_folder
    lappend fh(menu_nokeep) [list $m [$m index end]]
    menu $m.sort
    foreach o {threaded subject subjectonly senderonly sender folder
	       reverseFolder date reverseDate size reverseSize} {
	$m.sort add radiobutton -label $t(sort_$o) \
		-variable ${handler}(sort) -value $o \
		-command "\$${handler}(folder_handler) setSortOrder $o; \
		RatBusy {Sync $handler update}"
	set b($m.sort,[$m.sort index end]) sort_$o
    }

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

    # The structure menu (constructed by the Show routine)
    menu $texth(struct_menu) -tearoff 0

    # Information
    button $w.t.mbar.netstatus -image $online_img -bd 0 -width 32
    lappend tkrat_online_imgs $w.t.mbar.netstatus
    label $w.t.mbar.lpar -text "(" -padx 0 -bd 0
    label $w.t.mbar.ndef -textvariable numDeferred -font $fixedNormFont \
	    -padx 0 -bd 0
    label $w.t.mbar.ndefl -text "$t(d) " -padx 0
    label $w.t.mbar.nhld -textvariable numHeld -font $fixedNormFont -padx 0 \
	    -bd 0
    label $w.t.mbar.nhldl -text $t(h) -padx 0
    label $w.t.mbar.rpar -text ")  " -padx 0 -bd 0
    set b($w.t.mbar.ndef) num_deferred
    set b($w.t.mbar.ndefl) num_deferred
    set b($w.t.mbar.nhld) num_held
    set b($w.t.mbar.nhldl) num_held

    # Pack the menus into the menu bar
    pack $w.t.mbar.tkrat \
	 $w.t.mbar.folder \
	 $w.t.mbar.message \
	 $w.t.mbar.show \
	 $w.t.mbar.group \
	 $w.t.mbar.admin -side left -padx 5
    pack $w.t.mbar.help -side right -padx 5
    pack $w.t.mbar.rpar \
         $w.t.mbar.nhldl \
         $w.t.mbar.nhld \
         $w.t.mbar.ndefl \
         $w.t.mbar.ndef \
         $w.t.mbar.lpar \
	 $w.t.mbar.netstatus -side right

    # The information part
    frame $w.t.info -relief raised -bd 1
    label $w.t.info.flabel -text $t(name):
    label $w.t.info.fname -textvariable ${handler}(folder_name) \
        -font $fixedNormFont -anchor w
    label $w.t.info.slabel -text $t(size):
    label $w.t.info.size -textvariable ${handler}(folder_size) -width 5 \
	-anchor w -font $fixedNormFont
    label $w.t.info.mlabel -text $t(messages):
    label $w.t.info.messages -textvariable ${handler}(folder_messages) \
	-width 4 -anchor w -font $fixedNormFont
    label $w.t.info.nlabel -text $t(new):
    label $w.t.info.new -textvariable ${handler}(folder_new) \
	-width 4 -anchor w -font $fixedNormFont
    pack $w.t.info.new \
         $w.t.info.nlabel \
         $w.t.info.messages \
         $w.t.info.mlabel \
         $w.t.info.size \
         $w.t.info.slabel -side right
    pack $w.t.info.flabel -side left
    pack $w.t.info.fname -side left -fill x -expand 1
    set b($w.t.info.fname) current_folder_name
    set b($w.t.info.size) current_folder_size
    set b($w.t.info.messages) current_folder_nummsg

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
	$fh(message_list) tag configure Found -background #ffff80
    } else {
	$fh(message_list) tag configure sel -underline 1
	$fh(message_list) tag configure Found -borderwidth 2 -relief raised
    }
    $fh(message_list) tag raise sel
    pack $fh(message_scroll) -side right -fill y
    pack $fh(message_list) -side left -expand 1 -fill both
    set b($fh(message_list)) list_of_messages

    # The status line
    label $w.statustext -textvariable statusText -relief raised \
	    -bd 1 -font $propLightFont -width 80
    set b($w.statustext) status_text
    
    # The command buttons
    frame $w.b.buttons
    menubutton $w.b.buttons.move -text $t(move) -bd 1 -relief raised \
	    -menu $w.b.buttons.move.m -indicatoron 1
    menu $w.b.buttons.move.m \
	    -postcommand "PostMove $handler current $w.b.buttons.move.m"
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
    set b($w.b.buttons.reply_sender) reply_to_sender
    set b($w.b.buttons.reply_all) reply_to_all

    # The actual text
    frame $w.b.text -relief raised -bd 1
    scrollbar $w.b.text.scroll \
        -relief sunken \
        -command "$w.b.text.text yview" \
	-highlightthickness 0
    text $fh(text) \
	-yscroll "RatScrollShow $w.b.text.text $w.b.text.scroll" \
        -relief raised \
        -wrap $option(wrap_mode) \
	-bd 0 \
	-highlightthickness 0 \
        -setgrid true
    $fh(text) mark set searched 1.0
    pack $w.b.text.scroll -side right -fill y
    pack $w.b.text.text -side left -expand yes -fill both
    set b($fh(text)) body_of_message

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
    FolderPane $handler [GetPane folderPane]

    place $w.t -relwidth 1
    place $w.b -relwidth 1 -rely 1 -anchor sw
    place $w.statustext -relwidth 1 -anchor w
    place $w.handle -anchor e
    after idle "set d \[expr \[winfo height $w.statustext\] / 2\]; \
		place configure $w.t -height -\$d; \
		place configure $w.b -height -\$d"

    # Do bindings
    focus $w
    bind $fh(message_list) <1> \
	    "FolderSelect $w.t.messlist.list $handler \
		    \[expr int(\[%W index @%x,%y\])-1\] 0"
    bind $fh(message_list) <Double-1> \
	    "FolderSelect $w.t.messlist.list $handler \
		    \[expr int(\[%W index @%x,%y\])-1\] 1; break"
    bind $fh(message_list) <Triple-1> break
    bind $fh(message_list) <B1-Motion> break
    bind $fh(message_list) <Shift-1> break
    bind $fh(message_list) <Double-Shift-1> break
    bind $fh(message_list) <Triple-Shift-1> break
    bind $fh(message_list) <B1-Leave> break
    bind $fh(message_list) <3> "FolderB3Event $handler \[%W index @%x,%y\]"
    bind $fh(message_list) <B3-Motion> \
	    "FolderB3Motion $handler \[%W index @%x,%y\]"
    bind $fh(message_list) <Shift-3> \
	    "FolderSB3Event $handler \[%W index @%x,%y\]"
    bind $fh(text) <Map> "FolderMap $handler"
    bind $fh(text) <Unmap> "FolderUnmap $handler"
    bind $fh(text) <Destroy> "DestroyFolderWin $handler"
    FolderBind $handler
    wm protocol $fh(toplevel) WM_DELETE_WINDOW "DestroyFolderWin $handler"

    # Calculate font width
    if {![info exists defaultFontWidth]} {
	CalculateFontWidth $fh(text)
    }

    SetOnlineStatus $option(online)

    # Do things which are done just when the first folder window is opened
    if {![info exists folderWindowList]} {
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

	    # Send deferred messages (if online)
	    if {$option(online) && 0 < $numDeferred} {
		SendDeferred
	    }
	}

    }

    set folderWindowList($handler) ""

    return $handler
}

# PostTkRat --
#
# Post-command for Tkrat menu
#
# Arguments:
# handler - The handler which identifies the folder window

proc PostTkRat {handler} {
    upvar #0 $handler fh
    global folderWindowList

    if {1 == [array size folderWindowList]} {
	set state disabled
    } else {
	set state normal
    }
    [lindex $fh(close_menu) 0] entryconfigure [lindex $fh(close_menu) 1] \
	    -state $state
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
    upvar #0 $handler fh
    set w $fh(w)
    set fh(pane) $y

    if {$y < 0.1 || 0.9 < $y}  return
        # Prevents placing into inaccessibility (off the window).
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
    upvar #0 $handler fh

    RatBind $fh(w) folder_key_compose   "Compose \$${handler}(role)" \
	    $fh(compose_menu)
    RatBind $fh(w) folder_key_close	"\
	    if {1 == \[array size folderWindowList]} {\
	        bell \
	    } else { \
	        CloseFolderWin $handler \
	    }" $fh(close_menu)
    RatBind $fh(w) folder_key_online \
	    "[lindex $fh(online_menu) 0] invoke [lindex $fh(online_menu) 1]" \
	    $fh(online_menu)
    RatBind $fh(w) folder_key_openfile \
	    "PostFolderOpen $handler \[SelectFileFolder $fh(toplevel)\]"
    RatBind $fh(w) folder_key_quit	"Quit $handler" $fh(quit_menu)
    RatBind $fh(w) folder_key_nextu	"FolderSelectUnread $handler; break"
    RatBind $fh(w) folder_key_sync	"RatBusy {Sync $handler sync}" \
	    $fh(sync_menu)
    RatBind $fh(w) folder_key_netsync	"RatBusy NetworkSync" \
	    $fh(netsync_all_menu)
    RatBind $fh(w) folder_key_update    "RatBusy {Sync $handler update}" \
	    $fh(update_menu)
    RatBind $fh(w) folder_key_delete    "SetFlag $handler deleted 1; \
	    FolderNext $handler" $fh(delete_menu)
    RatBind $fh(w) folder_key_undelete  "SetFlag $handler deleted 0; \
	    FolderNext $handler" $fh(undelete_menu)
    RatBind $fh(w) folder_key_flag	"SetFlag $handler flagged toggle; \
	    FolderNext $handler"
    RatBind $fh(w) folder_key_next	"FolderNext $handler"
    RatBind $fh(w) folder_key_prev	"FolderPrev $handler"
    RatBind $fh(w) folder_key_home	"ShowHome $fh(text)"
    RatBind $fh(w) folder_key_bottom	"ShowBottom $fh(text)"
    RatBind $fh(w) folder_key_pagedown  "ShowPageDown $fh(text)"
    RatBind $fh(w) folder_key_pageup    "ShowPageUp $fh(text)"
    RatBind $fh(w) folder_key_linedown  "ShowLineDown $fh(text)"
    RatBind $fh(w) folder_key_lineup    "ShowLineUp $fh(text)"
    RatBind $fh(w) folder_key_replya    "FolderReply $handler all" \
	    $fh(replya_menu)
    RatBind $fh(w) folder_key_replys    "FolderReply $handler sender" \
	    $fh(replys_menu)
    RatBind $fh(w) folder_key_forward_i \
	    "FolderSomeCompose $handler ComposeForwardInline" \
	    $fh(forward_i_menu)
    RatBind $fh(w) folder_key_forward_a \
	    "FolderSomeCompose $handler ComposeForwardAttachment" \
	    $fh(forward_a_menu)
    RatBind $fh(w) folder_key_bounce \
	    "FolderSomeCompose $handler ComposeBounce" $fh(bounce_menu)
    RatBind $fh(w) folder_key_cycle_header "CycleShowHeader $handler"
    RatBind $fh(w) folder_key_find  "FolderFind $handler" $fh(find_menu)
    RatBind $fh(w) folder_key_markunread "SetFlag $handler seen 0; \
	    FolderNext $handler" $fh(markunread_menu)
    RatBind $fh(w) folder_key_print "Print $handler current" $fh(print_menu)
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
    upvar #0 $handler fh

    set fh(folder_name) ""
    UpdateFolderTitle $handler
    set fh(folder_messages) ""
    set fh(folder_size) ""
    ShowNothing $fh(text)
    catch {unset fh(current); unset fh(folder_handler)}
    $fh(message_list) configure -state normal
    $fh(message_list) delete 1.0 end
    $fh(message_list) configure -state disabled
    set fh(active) ""
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
    upvar #0 $handler fh

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
    set fh(sort) [$folder_handler getSortOrder]
    set i [$fh(folder_handler) info]
    set fh(folder_name) [lindex $i 0]    
    set fh(role) [$fh(folder_handler) role]
    UpdateFolderTitle $handler
    set fh(folder_messages) [RatMangleNumber $folderExists($folder_handler)]
    set fh(folder_new) [RatMangleNumber $folderUnseen($folder_handler)]
    set fh(num_messages) [lindex $i 1]
    FolderDrawList $handler
    set i [$fh(folder_handler) info]
    if { -1 == [lindex $i 2]} {
	set fh(folder_size) 0
    } else {
	set fh(folder_size) [RatMangleNumber [lindex $i 2]]
    }
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
	    set start [expr {$fh(size)-1}]
	}
    }
    if { 0 != $fh(size)} {
	switch $option(start_selection) {
	    last {
		set index [expr {$fh(size)-1}]
	    }
	    first_new {
		set index [FolderGetNextUnread $handler $start $dir]
	    }
	    before_new {
		set index [FolderGetNextUnread $handler $start $dir]
		if { 0 == [$fh(folder_handler) getFlag $index seen]} {
		    incr index [expr -1 * $dir]
		    if {$index < 0 || $index >= $fh(size)} {
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
    FolderSelect $fh(message_list) $handler $index 0

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
    upvar #0 $handler fh
    global option

    $fh(message_list) configure -state normal
    $fh(message_list) delete 1.0 end
    set lines [$fh(folder_handler) list $option(list_format)]
    set fh(size) [llength $lines]
    foreach l $lines {
	$fh(message_list) insert end "$l\n"
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
# index   -	Index of the entry to refresh

proc FolderListRefreshEntry {handler index} {
    upvar #0 $handler fh
    global option

    set line [expr {$index+1}]
    $fh(message_list) configure -state normal
    set tags [$fh(message_list) tag names $line.0]
    $fh(message_list) delete $line.0 "$line.0 lineend"
    $fh(message_list) insert $line.0 [format %-256s \
	    [[$fh(folder_handler) get $index] list $option(list_format)]] $tags
    $fh(message_list) configure -state disabled
}

# FolderSelect --
#
# Handle the selection of a message
#
# Arguments:
# l       -	The listbox the message was selected from
# handler -	The handler which identifies the folder window
# index   -	Index to select
# force   -	Force showing regarding of browse mode

proc FolderSelect {l handler index force} {
    upvar #0 $handler fh
    global option t b folderUnseen

    if {![info exists fh(folder_handler)]} {
	return
    }
    set fh(active) $index
    if {0 == [string length $index] || $index >= $fh(num_messages)} {
	catch {unset fh(current)}
	FolderButtons $handler 0
	ShowNothing $fh(text)
	set fh(active) ""
	return
    }
    if {![info exists fh(current)]} {
	FolderButtons $handler 1
    }
    set line [expr {$index+1}]
    $fh(message_list) tag remove Active 1.0 end
    $fh(message_list) tag add Active $line.0 "$line.0 lineend+1c"
    $fh(message_list) see $line.0
    update idletasks
    if {![info exists fh(message_list)]} return
    set fh(current) [$fh(folder_handler) get $index]
    set seen [$fh(folder_handler) getFlag $index seen]
    $fh(folder_handler) setFlag $index seen 1
    if {$force} {
	set mode 0
    } else {
	set mode $fh(browse)
    }
    set result [Show $fh(text) $fh(current) $mode]
    set sigstatus [lindex $result 0]
    set pgpOutput [lindex $result 1]
    if { 0 == $seen } {
	FolderListRefreshEntry $handler $index
	set fh(folder_new) [RatMangleNumber $folderUnseen($fh(folder_handler))]
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

# FolderNext --
#
# Advance to the next message in the folder
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderNext {handler} {
    upvar #0 $handler fh
    set ml $fh(message_list)

    if {![string length $fh(active)]} { return }

    set index [expr {1+$fh(active)}]
    if { $index >= $fh(size) } {
	return
    }

    if {$index >= [expr {round([lindex [$ml yview] 1]*$fh(size))}]} {
	$ml yview $index
    }
    FolderSelect $ml $handler $index 0
}

# FolderPrev --
#
# Retreat to the previous message in the folder
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderPrev {handler} {
    upvar #0 $handler fh
    set ml $fh(message_list)

    if {![string length $fh(active)]} { return }

    set index [expr {$fh(active)-1}]
    if {$index < 0} {
	return
    }

    if {$index < [expr {round([lindex [$ml yview] 0]*$fh(size))}]} {
	$ml yview scroll -1 pages
    }
    FolderSelect $ml $handler $index 0
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
# messages -	Ano optional list of messages to do this to

proc SetFlag {handler flag value {messages {}}} {
    upvar #0 $handler fh
    global option

    if {![string length $fh(active)]} { return }
    if {$fh(active) >= $fh(num_messages)} { return }

    set sel $fh(active)
    if {![string length $messages]} {
	set messages $sel
    }
    foreach i $messages {
	if {[string compare toggle $value]} {
	    $fh(folder_handler) setFlag $i $flag $value
	} else {
	    if {[$fh(folder_handler) getFlag $i $flag]} {
		set v 0 
	    } else {
		set v 1
	    }
	    $fh(folder_handler) setFlag $i $flag $v
	}
	FolderListRefreshEntry $handler $i
    }
}

# Quit --
#
# Closes the given folder window and quits tkrat
#
# Arguments:
# handler -	The handler which identifies the folder window

proc Quit {handler} {
    global folderWindowList
    upvar #0 $handler fh

    if {0 == [PrepareQuit]} {
	return
    }

    foreach fw [array names folderWindowList] {
	CloseFolderWin $fw
    }

    RatCleanup
    SavePos
    destroy .
}

# Quit --
#
# Closes the given folder window and quits tkrat
#
# Arguments:
# handler -	The handler which identifies the folder window

proc PrepareQuit {} {
    global expAfter logAfter numDeferred ratSenderSending t composeWindowList \
	    alreadyQuitting folderWindowList

    if {[info exists alreadyQuitting]} {
	return 0
    } 
    set alreadyQuitting 1

    if {[info exists composeWindowList]} {
	if {0 < [llength $composeWindowList]} {
	    if {0 == [RatDialog "" $t(really_quit) $t(compose_sessions) \
		    {} 0 $t(dont_quit) $t(do_quit)]} {
		unset alreadyQuitting
		return 0
	    }
	}
    }
    if {1 < [array size folderWindowList]} {
	if {0 == [RatDialog "" $t(really_quit) $t(folder_windows) \
		{} 0 $t(dont_quit) $t(do_quit)]} {
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
    } elseif {0 < $numDeferred} {
	if {1 < $numDeferred} {
	    set text "$t(you_have) $numDeferred $t(deferred_messages)"
	} else {
	    set text "$t(you_have) 1 $t(deferred_message)"
	}
	if {0 == [RatDialog "" $t(send_deferred) $text {} 0 \
		$t(send_now) $t(no)]} {
	    set win [SendDeferred]
	    if {"" != $win} {
		catch {tkwait window $win}
	    }
	}
    }
    RatSend kill
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
    upvar #0 $handler fh
    global option

    set listAfter [$fh(folder_handler) list $option(msgfind_format)]
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
    upvar #0 $handler fh
    global option folderExists folderUnseen

    if {![info exists fh(folder_handler)]} {return}

    if {$fh(syncing)} {
        return
    }
    set fh(syncing) 1
    set oldActive $fh(active)
    catch {$fh(folder_handler) list $option(msgfind_format)} listBefore
    if {[string length $oldActive]} {
	set subject [lindex $listBefore $oldActive]
	set msg $fh(current)
    }
    if {[llength $listBefore]} {
	# Get data about what is visible at the moment. We want...
	#  ...name & index of the top message
	#  ...to know if the active message is visible
	#  ...to know if the last message is visible
	set oldTopIndex [lindex [split [$fh(message_list) index @0,0] .] 0]
	set oldTopMsg [$fh(folder_handler) get [expr {$oldTopIndex-1}]]
	if {"" != $oldActive} {
	    set sawActive [$fh(message_list) bbox $oldActive.0+1l]
	} else {
	    set sawActive 0
	}
	set sawEnd [$fh(message_list) bbox end-1c]
    }

    if {[catch {$fh(folder_handler) update $mode}]} {
	FolderWindowClear $handler
	set fh(syncing) 0
	return
    }
    FolderDrawList $handler

    # Update information
    set i [$fh(folder_handler) info]
    set fh(folder_messages) \
	    [RatMangleNumber $folderExists($fh(folder_handler))]
    set fh(folder_new) [RatMangleNumber $folderUnseen($fh(folder_handler))]
    set fh(num_messages) [lindex $i 1]
    if { -1 == [lindex $i 2]} {
	set fh(folder_size) ???
    } else {
	set fh(folder_size) [RatMangleNumber [lindex $i 2]]
    }
    if { 0 == [lindex $i 1] } {
	FolderSelect $fh(message_list) $handler "" 0
	set fh(syncing) 0
	return
    }

    # Check if our element is still in there
    set fh(active) ""
    if {[string length $oldActive]} {
	set index [$fh(folder_handler) find $msg]
	if { -1 != $index } {
	    set line [expr {$index+1}]
	    $fh(message_list) tag add Active $line.0 "$line.0 lineend+1c"
	    set fh(active) $index
	    $fh(message_list) see $line.0
	} else {
	    set index [FindAdjacent $handler $oldActive $listBefore]
	    FolderSelect $fh(message_list) $handler $index 0
	}
    }
    if {![string length $fh(active)]} {
	FolderSelect $fh(message_list) $handler 0 0
    }
    
    # Fix scroll position in text widget
    if {[info exists oldTopIndex]} {
	$fh(message_list) xview moveto 0

	# Set topmost visible message
	set index [$fh(folder_handler) find $oldTopMsg]
	if { -1 == $index } {
	    set index [FindAdjacent $handler $oldTopIndex $listBefore]
	}
	$fh(message_list) yview $index

	# If the last message used to be visible, make sure it still is
	if {4 == [llength $sawEnd]} {
	    $fh(message_list) see {end linestart-1l}
	}

	# If the active message was visible make it visible again
	# this will override the last-message visibility
	if {4 == [llength $sawActive]} {
	    $fh(message_list) see $fh(active).0+1l
	}
    }
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
    upvar #0 $handler fh

    if {![string length $fh(active)]} { return }

    set current $fh(current)
    set hd [ComposeReply $current $recipient $fh(role)]
    upvar #0 $hd sendHandler
    trace variable sendHandler w "FolderReplySent $hd $handler $current"
}
proc FolderReplySent {hd handler current name1 name2 op} {
    upvar #0 $handler fh
    upvar #0 $hd sendHandler
    global option

    trace vdelete $name1 $op "FolderReplySent $hd $handler $current"

    if {[info exists sendHandler(do)] && [info exists fh(folder_handler)]} {
	if {![string compare send $sendHandler(do)]} {
	    set index [$fh(folder_handler) find $current]
	    if { -1 != $index } {
		if {![$fh(folder_handler) getFlag $index answered]} {
		    SetFlag $handler answered 1 $index
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
    upvar #0 $handler fh
    global option

    if {![string length $fh(active)]} { return }

    $composeFunc $fh(current) $fh(role)
}

# PostFolder --
#
# Populate the folder menu
#
# Arguments:
# handler -	The handler which identifies the folder window
# m       -	The menu which we should populate

proc PostFolder {handler m} {
    upvar #0 $handler fh
    global t option

    $m delete 1 end
    VFolderBuildMenu $m 0 "VFolderOpen $handler" 0
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
# which	  -	Which set of messages we should move (current or group)
# m       -	The menu which we should populate

proc PostMove {handler which m} {
    upvar #0 $handler fh
    global t

    if {[string compare "group" $which]} {
	set a 1
    } else {
	set a 0
    }
    $m delete 1 end
    VFolderBuildMenu $m 0 \
	    "VFolderInsert $handler $a \[GetMsgSet $handler $which\]" 1
    $m add separator
    $m add command -label $t(to_file)... \
	    -command "VFolderInsert $handler $a \[GetMsgSet $handler $which\] \
		      \[InsertIntoFile $fh(toplevel)]"
    $m add command -label $t(to_dbase)... \
	    -command "VFolderInsert $handler $a \[GetMsgSet $handler $which\] \
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
    upvar #0 $handler fh

    if {[string compare group $which]} {
	if {[info exists fh(current)]} {
	    return $fh(current)
	} else {
	    return {}
	}
    } else {
	set msgs {}
	foreach i [$fh(folder_handler) flagged flagged] {
	    lappend msgs [$fh(folder_handler) get $i]
	}
	return $msgs
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
    upvar #0 $handler fh
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
    upvar #0 $handler fh

    if {0 == $fh(size)} {
	return 0
    }
    for {set i [expr {$index+$dir}]} {$i != $index} {incr i $dir} {
	if {$i >= $fh(size)} {
	    set i 0
	} elseif {$i < 0} {
	    set i [expr {$fh(size)-1}]
	}
	if {$i == $index} {
	    return 0
	}
	if { 0 == [$fh(folder_handler) getFlag $i seen]} {
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
    upvar #0 $handler fh

    set index $fh(active)
    if {0 == [llength $index]} {
	return
    }
    set i [FolderGetNextUnread $handler $index 1]
    FolderSelect $fh(message_list) $handler $i 0
}

# GroupMessageList --
#
# Pops a message list that lets the user select messages for a group
#
# Arguments:
# handler -	The handler which identifies the folder window

proc GroupMessageList {handler} {
    global b idCnt t option
    upvar #0 $handler fh

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
    Size $w.f.list gFolderL
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

    eval "$w.f.list insert 0 [$fh(folder_handler) list $option(list_format)]"
    foreach i [$fh(folder_handler) flagged flagged] {
	$w.f.list selection set $i
    }
    lappend fh(groupMessageLists) $w
    wm protocol $w WM_DELETE_WINDOW "GroupMessageListDone $w $handler 0"

    Place $w groupMessageList
}

# GroupMessageListUpdate --
#
# Update the message list since the underlying folder was updated
#
# Arguments:
# w	  -	The group selection window
# handler -	The handler which identifies the folder window

proc GroupMessageListUpdate {w handler} {
    upvar #0 $handler fh
    global option

    $w.f.list delete 0 end
    eval "$w.f.list insert 0 [$fh(folder_handler) list $option(list_format)]"
    foreach i [$fh(folder_handler) flagged flagged] {
	$w.f.list selection set $i
    }
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
    upvar #0 $handler fh
    global b option

    if {$done} {
	set toset [$w.f.list curselection]
	set isset [$fh(folder_handler) flagged flagged]
	for {set i 0} {$i < [$w.f.list size]} {incr i} {
	    set s [expr {-1 != [lsearch $toset $i]}]
	    if {$s != [expr {-1 != [lsearch $isset $i]}]} {
		$fh(folder_handler) setFlag $i flagged $s
		FolderListRefreshEntry $handler $i
	    }
	}
    }
    RecordPos $w groupMessageList
    RecordSize $w.f.list gFolderL
    set index [lsearch $w $fh(groupMessageLists)]
    set fh(groupMessageLists) [lreplace $fh(groupMessageLists) $index $index]
    destroy $w
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
    upvar #0 $handler fh
    global option

    foreach i [$fh(folder_handler) flagged flagged] {
	$fh(folder_handler) setFlag $i flagged 0
	FolderListRefreshEntry $handler $i
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
    upvar #0 $handler fh

    if {![info exists fh(folder_handler)]} {
	set s disabled
    } elseif {[llength [$fh(folder_handler) flagged flagged]]} {
	set s normal
    } else {
	set s disabled
    }
    $m entryconfigure 4 -state $s
    $m entryconfigure 6 -state $s
    $m entryconfigure 7 -state $s
    $m entryconfigure 8 -state $s
    $m entryconfigure 9 -state $s
}

# CycleShowHeader --
#
# Cycle through the values of the show_header option
#
# Arguments:
# handler -	The handler which identifies the folder window

proc CycleShowHeader {handler} {
    global option
    upvar #0 $handler fh
    upvar #0 $fh(text) texth

    switch $texth(show_header) {
    all		{ set texth(show_header) selected }
    selected	{ set texth(show_header) no }
    no		{ set texth(show_header) all }
    }
    FolderSelect $fh(message_list) $handler $fh(active) 0
    SaveOptions
}

# FolderCheckSignature --
#
# Check the signature(s) of the current message
#
# Arguments:
# handler -	The handler which identifies the folder window

proc FolderCheckSignature {handler} {
    upvar #0 $handler fh
    upvar #0 msgInfo_$fh(current) msgInfo
    global t b

    set tot pgp_none
    set b($fh(sigbut)) pgp_none
    set first 1
    set result {}
    foreach bodypart $msgInfo(pgp,signed_parts) {
	regsub -all "\a" [$bodypart checksig] {} part
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
	    if {[string compare $bodypart [$fh(current) body]]} {
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
    upvar #0 $handler fh

    # Create identifier
    set id f[incr idCnt]
    upvar #0 $id hd
    set w .$id

    # Initialize variables
    set hd(ignore_case) $fh(find_ignore_case)
    set hd(match) $fh(find_match)
    set hd(loc) $fh(find_loc)
    set hd(w) $w
    set hd(handler) $handler
    set hd(def_start) 0
    set hd(oldfocus) [focus]

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(find)

    # Create window
    entry $w.e -textvariable ${id}(text)
    set b($w.e) enter_search_exp_here

    checkbutton $w.c -text $t(ignore_case) -variable ${id}(ignore_case)
    set b($w.c) toggle_ignore_case

    frame $w.m
    label $w.m.label -text $t(match): -anchor e -width 10
    radiobutton $w.m.exact -text $t(exact) -variable ${id}(match) \
	    -value exact
    radiobutton $w.m.regexp -text $t(regexp) -variable ${id}(match) \
	    -value regexp
    pack $w.m.label \
	 $w.m.exact \
	 $w.m.regexp -side left
    set b($w.m.exact) find_exact
    set b($w.m.regexp) find_regexp

    frame $w.l
    label $w.l.label -text $t(find_in): -anchor e -width 10
    radiobutton $w.l.list -text $t(message_list) -variable ${id}(loc) \
	    -value list
    radiobutton $w.l.body -text $t(message_body) -variable ${id}(loc) \
	    -value body
    pack $w.l.label \
	 $w.l.list \
	 $w.l.body -side left
    set b($w.l.list) find_in_mlist
    set b($w.l.body) find_in_body

    frame $w.b
    frame $w.b.find -relief sunken -bd 2
    button $w.b.find.b -text $t(find) -state disabled \
	    -command "FolderFindDo $id 0"
    pack $w.b.find.b -padx 1 -pady 1
    frame $w.b.find_next -relief flat -bd 2
    button $w.b.find_next.b -text $t(find_next) -state disabled \
	    -command "FolderFindDo $id 1"
    pack $w.b.find_next.b -padx 1 -pady 1
    button $w.b.dismiss -text $t(dismiss) \
	    -command "FolderFindDone $id"
    pack $w.b.find \
	 $w.b.find_next \
	 $w.b.dismiss -side left -padx 5 -pady 5 -expand 1
    set b($w.b.find.b) find_first
    set b($w.b.find_next.b) find_next
    set b($w.b.dismiss) dismiss

    pack $w.e \
	 $w.c \
	 $w.m \
	 $w.l \
	 $w.b -side top -pady 5 -padx 5 -anchor w -fill x -expand 1

    Place $w find
    focus $w.e
    bind $w.e <Return> "FolderFindDo $id \$${id}(def_start); break"
    wm protocol $w WM_DELETE_WINDOW "FolderFindDone $id"

    trace variable hd(text) w "FinderFindTrace $id"
}

# FinderFindTrace --
#
# Trace the find text variable and change the button state accordingly
#
# Arguments:
# id      -	The handler which identifies the find window

proc FinderFindTrace {id args} {
    upvar #0 $id hd

    if {[string length $hd(text)]} {
	set state normal
    } else {
	set state disabled
	set hd(def_start) 0
    }
    $hd(w).b.find.b configure -state $state
    $hd(w).b.find_next.b configure -state $state
    if {$hd(def_start)} {
	$hd(w).b.find configure -relief flat
	$hd(w).b.find_next configure -relief sunken
    } else {
	$hd(w).b.find configure -relief sunken
	$hd(w).b.find_next configure -relief flat
    }
}

# FolderFindDo --
#
# Actually do the finding
#
# Arguments:
# id      -	The handler which identifies the find window
# current  -	Start at current location

proc FolderFindDo {id current} {
    upvar #0 $id hd
    upvar #0 $hd(handler) fh

    if {"list" == $hd(loc)} {
	set w $fh(message_list)
    } else {
	set w $fh(text)
    }
    if {$current} {
	set r [$w tag nextrange Found 1.0]
	if {[llength $r] > 0} {
	    set start [lindex $r 1]
	} else {
	    set start @0,0
	}
    } else {
	set start 1.0
    }
    if {$hd(ignore_case)} {
	set found [$w search -$hd(match) -nocase -count len \
		$hd(text) $start end]
    } else {
	set found [$w search -$hd(match) -count len $hd(text) $start end]
    }
    if {[string length $found]} {
	$w tag remove Found 1.0 end
	$w tag add Found $found $found+${len}c
	$w see $found
	set hd(def_start) 1
    } else {
	bell
	set hd(def_start) 0
    }
    FinderFindTrace $id
    set fh(find_ignore_case) $hd(ignore_case)
    set fh(find_match) $hd(match)
    set fh(find_loc) $hd(loc)
}

# FolderFindDone --
#
# Close find window
#
# Arguments:
# id      -	The handler which identifies the find window

proc FolderFindDone {id} {
    upvar #0 $id hd
    global b

    RecordPos $hd(w) find
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
    upvar #0 $handler fh
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
    upvar #0 $handler fh
    global option

    if {$option(checkpoint_on_unmap)} {
	RatBusy {Sync $handler checkpoint}
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
    upvar #0 $handler fh
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
# m       -	The menu which we should populate

proc NewFolderMenu {m} {
    global t

    $m delete 1 end
    VFolderBuildMenu $m 0 NewFolderWin 0
    $m add separator
    $m add command -label $t(open_file)... -command "NewFolderWin openfile"
    $m add command -label $t(open_dbase)... -command "NewFolderWin opendbase"
    $m add command -label $t(empty) -command "NewFolderWin empty"
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
	upvar #0 [lindex [array names folderWindowList] 0] fh
	set vf [SelectFileFolder $fh(toplevel)]
	if {"" == $vf} return
	set manual 1
    } elseif {"opendbase" == $vf} {
	upvar #0 [lindex [array names folderWindowList] 0] fh
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
    Place $w folder
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
    upvar #0 $handler fh

    if {[catch {unset folderWindowList($handler)}]} {
	return
    }
    if {[info exists fh(folder_handler)]} {
	CloseFolder $fh(folder_handler) 
    }
    if {[winfo exists $fh(w)]} {
	RecordPos [winfo toplevel $fh(w)] folder
	RecordSize [winfo toplevel $fh(w)] folderWindow
	RecordPane $fh(pane) folderPane
	if {[info exists fh(watcher_w)]} {
	    if {[info exists fh(watcher_geom)]} {
		wm geom $fh(watcher_w) $fh(watcher_geom)
		RecordPos $fh(watcher_w) watcher
	    }
	    RecordSize $fh(watcher_list) watcher
	}
	SavePos
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

proc DestroyFolderWin {handler} {
    global folderWindowList

    if {1 == [array size folderWindowList] && 0 == [PrepareQuit]} {
	return
    }
    CloseFolderWin $handler
    if {0 == [array size folderWindowList]} {
	RatCleanup
	destroy .
    }
}

# FolderB3Event --
#
# Toggle flag status of current message
#
# Arguments:
# handler -	The handler which identifies the folder window
# index -	Message under pointer

proc FolderB3Event {handler index} {
    upvar #0 $handler fh

    set fh(setflag) [expr {int($index-1)}]
    SetFlag $handler flagged toggle $fh(setflag)
}

# FolderB3Motion --
#
# Toggle flag status of current message
#
# Arguments:
# handler -	The handler which identifies the folder window
# index -	Message under pointer

proc FolderB3Motion {handler index} {
    upvar #0 $handler fh

    set i [expr {int($index-1)}]
    if {$i != $fh(setflag)} {
	if {$i > $fh(setflag)} {
	    for {set ui $i} {$ui > $fh(setflag)} {incr ui -1} {
		SetFlag $handler flagged toggle $ui
	    }
	} else {
	    for {set ui $i} {$ui < $fh(setflag)} {incr ui} {
		SetFlag $handler flagged toggle $ui
	    }
	}
	set fh(setflag) $i
    }
}

# FolderSB3Event --
#
# Toggle flag status of messages between current and last
#
# Arguments:
# handler -	The handler which identifies the folder window
# index -	Message under pointer

proc FolderSB3Event {handler index} {
    upvar #0 $handler fh

    if {"" != $fh(setflag)} {
	FolderB3Motion $handler $index
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

    if {[lindex $option(network_sync) 0] && $numDeferred > 0} {
	SendDeferred
    }

    if {[lindex $option(network_sync) 1]} {
	RatSyncDisconnected
	foreach f [array names folderWindowList] {
	    upvar #0 $f fh
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
    upvar #0 $handler fh

    regsub -all -- %f $option(main_window_name) $fh(folder_name) title
    regsub -all -- %r $title $option($fh(role),name) title
    wm title $fh(toplevel) $title
    regsub -all -- %f $option(icon_name) $fh(folder_name) ititle
    wm iconname $fh(toplevel) $ititle
}
