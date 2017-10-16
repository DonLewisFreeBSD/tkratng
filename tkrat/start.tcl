#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

proc TkRatStart {} {
    global t tkrat_version tkrat_version_date idCnt inbox expAfter logAfter \
	   statusBacklog currentColor ratLogBottom ratLogTop vFolderDef \
	   option currentLanguage_t tk_patchLevel vFolderInbox \
	   folderWindowList openFolders folderChanged propNormFont \
	   propLightFont fixedNormFont fixedBoldFont watcherFont \
	   ISO_Left_Tab tk_version rat_tmp tklead folderUnseen

    # Base package requirements
    package require ratatosk 2.1

    # Function to let client know we have started
    proc RatPing {} {
	return pong
    }

    # Initialize variables
    set tkrat_version 2.1.5
    set tkrat_version_date 20050602
    set idCnt 0
    set inbox ""
    set expAfter {}
    set logAfter {}
    set statusBacklog {}
    set currentColor {}
    set ratLogBottom 0
    set ratLogTop 0

    # Bindings
    bind Entry <Return> {focus [tk_focusNext %W]}
    if {[catch {bind all <ISO_Left_Tab> {focus [tk_focusPrev %W]}}]} {
	set ISO_Left_Tab Shift-Tab
    } else {
	set ISO_Left_Tab ISO_Left_Tab
    }

    # Initialize
    RatGenId	# Force load of package
    OptionsInit
    InitMessages $option(language) t
    OptionsRead
    InitCharsetAliases
    InitPgp
    if {$tk_version >= 8.3} {
	tk useinputmethods $option(useinputmethods)
    }

    # Reinitialize language (if needed)
    if {[string compare $option(language) $currentLanguage_t]} {
	InitMessages $option(language) t
    }

    if {[info exists option(last_version_date)] 
	    && "$option(last_version_date)" != $tkrat_version_date} {
	NewVersionUpdate
    }

    # Update the default font
    set propNormFont [RatCreateFont $option(prop_norm)]
    set propLightFont [RatCreateFont $option(prop_light)]
    set fixedNormFont [RatCreateFont $option(fixed_norm)]
    set fixedBoldFont [RatCreateFont $option(fixed_bold)]
    set watcherFont [RatCreateFont $option(watcher_font)]
    if {$option(override_fonts)} {
	set pri interactive
    } else {
	set pri widgetDefault
    }
    option add *TkRat*font $propNormFont $pri
    option add *TkRat*Entry.font $fixedNormFont $pri
    option add *TkRat*Text.font $fixedNormFont $pri
    option add *TkRat*Listbox.font $fixedNormFont $pri
    option add *TkRat*RatList*Listbox.font $propNormFont $pri
    option add *TkRat*RatTree*font $propLightFont $pri

    option add *Menu.tearOff $option(tearoff) widgetDefault

    bind Menubutton <Up> {event generate %W <space>}
    bind Menubutton <Down> {event generate %W <space>}

    # Extra package requirements
    package require rat_list 1.0
    package require rat_fbox 1.1
    package require rat_balloon 1.0
    package require rat_edit 1.0
    package require rat_textlist 1.0
    package require blt_busy 1.0
    package require rat_ed 1.0
    package require rat_ispell 1.0
    package require rat_tree 1.0
    package require rat_enriched 1.0

    # Change the color
    if {$option(override_color)} {
	option add *TkRat*foreground black interactive
	option add *TkRat*background gray85 interactive
	eval "SetColor $option(color_set)"
    }

    # Make sure our config directory exists
    if {![file isdirectory $option(ratatosk_dir)]} {
	set but [RatDialog "" $t(need_tkrat_dir_title) \
	       "$t(need_tkrat_dir1) \"$option(ratatosk_dir)\". \
	       $t(need_tkrat_dir2)" {} 0 $t(create) $t(dont_create) $t(abort)]
	switch $but {
	    0 {
		catch "exec mkdir [RatTildeSubst $option(ratatosk_dir)]" result
		if {[string length $result]} {
		    Popup [concat \
			    "$t(failed_create) \"$option(ratatosk_dir)\":"\
			    "$result.\n$t(do_without_dir)"]
		}
	    }
	    1 {
		Popup $t(do_without_dir)
		set option(send_cache) $rat_tmp/send.$env(USER)
	    }
	    2 {exit 0}
	}
    }
    if {![string length $option(last_version)]
	    || "$option(last_version_date)" != $tkrat_version_date} {
	StartupInfo
    }

    # Create send cache
    if {![file isdirectory $option(send_cache)] &&
	    [catch {exec mkdir [RatTildeSubst $option(send_cache)]} result]} {
	Popup "$t(failed_to_create_send_cache) '$option(send_cache)': $result"
    }

    # Read misc files
    VFolderRead
    AliasRead
    if { 3 > $option(scan_aliases) } {
	ScanAliases
    }
    ReadUserproc
    ReadPos
    if {[file readable $option(ratatosk_dir)/expressions]} {
	ExpRead
    }

    # Initialize balloon help system
    InitMessages $option(language) balText
    rat_balloon::Init b balText

    # Setup trace of folderWindoList
    set openFolders {}
    trace variable folderWindowList wu RatTraceFolder

    # Setup online status
    switch $option(start_online_mode) {
	online { set option(online) 1 }
	offline { set option(online) 0 }
	default {}
    }

    # Redo bindings for entry and text to make the selection work more
    # intuitive
    if {$tk_version >= 8.4} {
	set tklead "tk::"
    } else {
	set tklead "tk"
    }
    bind Entry <1> "${tklead}EntryButton1 %W %x"
    bind Text <1> "${tklead}TextButton1 %W %x %y"

    if { 0 <= [expr {[RatDaysSinceExpire]-$option(expire_interval)}]} {
	catch {Expire} err
    } else {
	set expAfter \
		[after [expr {($option(expire_interval)- \
		[RatDaysSinceExpire])*24*60*60*1000}] Expire]
    }

    # Check for deferred mail
    RatSend init

    # Load watcher
    set folderChanged(no_such_folder) 0
    set folderUnseen(no_such_folder) 0
    trace variable folderChanged wu WatcherTrig
    trace variable folderUnseen w WatcherTrig
}

# RatCreateFont --
#
# Create a font
#
# Arguments:
# s	- Specification, one of:
#	  {components FAMILY SIZE WEIGHT SLANT UNDERLINE OVERSTRIKE}
#	  {name FONT_NAME}

proc RatCreateFont {s} {
    if {"components" == [lindex $s 0]} {
	set res [list [lindex $s 1] -[lindex $s 2] [lindex $s 3] [lindex $s 4]]
	if {[lindex $s 5]} { lappend res underline }
	if {[lindex $s 6]} { lappend res overstrike }
	return $res
    } else {
	return [lindex $s 1]
    }
}

# RatLog:
# See ../doc/interface
proc RatLog {level message {mode time}} {
    global statusText option logAfter ratLogBottom ratLogTop ratLog \
	   statusBacklog option statusId

    switch $level {
    0		{set n BABBLE:}
    1		{set n PARSE:}
    2		{set n INFO:}
    3		{set n WARN:}
    4		{set n ERROR:}
    5		{set n FATAL:}
    default	{set n $level:}
    }
    set ratLog($ratLogTop) [format "%-8s %s" $n $message]
    incr ratLogTop
    if {$ratLogTop > [expr {$ratLogBottom+$option(num_messages)}]} {
	for {} {$ratLogTop > [expr {$ratLogBottom+$option(num_messages)}]} \
		{incr ratLogBottom} {
	    unset ratLog($ratLogBottom)
	}
    }
    if { 3 < $level} {
	# Fatal
	if {![string compare nowait $mode]} {
	    after 1 Popup [list $message]
	} else {
	    Popup $message
	}
    } else {
	if {$level} {
	    if {![string compare explicit $mode]} {
		if {[string length $logAfter]} {
		    after cancel $logAfter
		    set statusBacklog {}
		}
		set statusText $message
		set statusId $ratLogTop
	    } else {
		if {[string length $logAfter]} {
		    lappend statusBacklog $message
		} else {
		    set statusText $message
		    set logAfter [after [expr {$option(log_timeout)*1000}] \
					RatLogAfter]
		    set statusId ""
		}
	    }
	    update idletasks
	}
    }
    return $ratLogTop
}

# RatLogAfter --
#
# Show the next queued messagefor log display (if any).
#
# Arguments:

proc RatLogAfter {} {
    global statusText logAfter statusBacklog option statusId

    if {[llength $statusBacklog]} {
	set statusText [lindex $statusBacklog 0]
	set statusBacklog [lrange $statusBacklog 1 end]
	set logAfter [after [expr {$option(log_timeout)*1000}] RatLogAfter]
    } else {
	set statusText ""
	set logAfter {}
    }
    set statusId ""
}


# RatClearLog --
#
# Remove an explicit log message
#
# Arguments:
# id - the id of the message to remove

proc RatClearLog {id} {
    global statusText statusId

    if {![string compare $id $statusId]} {
	set statusId ""
	set statusText ""
    }
}

# GetRatLog --
#
# Return the saved log messages
#
# Arguments:

proc GetRatLog {} {
    global ratLogBottom ratLogTop ratLog

    set result {}
    for {set i $ratLogBottom} {$i < $ratLogTop} {incr i} {
	lappend result $ratLog($i)
    }

    return $result
}

# OkButtons
#
# Build two buttons and let the left one be surrounded by a frame. The
# buttons will be created inside a $w.buttons frame (the frame will
# also be created). The $w window will also be bound so that a press
# on the Return key also sets the ${id}(done) to 1.
#
# Arguments:
# w      -	Window in which to build the frame
# t1, t2 -	The text in the two buttons
# cmd	 -	Command which will be run when either button is pressed.
#		The command will get a '1' or a '0' as argument.

proc OkButtons {w t1 t2 cmd} {
    frame $w.buttons
    button $w.buttons.ok -text $t1 -command "$cmd 1" -default active
    button $w.buttons.cancel -text $t2 -command "$cmd 0"
    pack $w.buttons.ok \
	 $w.buttons.cancel -side left -expand 1
    bind $w <Return> "$cmd 1"
    wm protocol [winfo toplevel $w] WM_DELETE_WINDOW "$cmd 0"
}

# RatBind --
#
# Bind the specified keys to the specified function
#
# Arguments:
# w        - Window to bind in
# keylist  - Index into options array to get key combinations
# function - Function to bind the keys to
# menu     - The menu to configure (if any)
# eindex   - Index of the entry in the menu

proc RatBind {w keylist function {menu {}}} {
    global option

    foreach k $option($keylist) {
	if {[info exists a]} {
	    if {[string length $k] < [string length $a]} {
		set a $k
	    }
	} else {
	    set a $k
	}
	if {0 < [regsub < $k <Alt- altkey]} {
	    bind $w $altkey { }
	}
	bind $w $k $function
    }
    if {[string length $menu]} {
	if {[info exists a]} {
	    regsub Key- [string trim $a <>] {} key
	    if {0 != [regexp Shift- $key]} {
		set l [split $key -]
		set end [lindex $l end]
		if { 1 == [string length $end] && 
			[string compare [string tolower $end] \
					[string toupper $end]]} {
		    set l [lreplace $l end end [string toupper $end]]
		    set key [join $l -]
		    regsub Shift- $key {} key
		}
	    }
	    if {[regexp {^Control-([a-z])} $key {} l]} {
		set key "^[string toupper $l]"
	    }
	    regsub Meta- $key {M-} key
	} else {
	    set key ""
	}
	[lindex $menu 0] entryconfigure [lindex $menu 1] -accelerator $key
    }
}


# MailSteal --
#
# Steal back mail that has been kidnapped by other programs.
#
# Arguments:
# handler - The handler of the folder window which has the inbox
# ask     - A boolean which says if we should ask the user for confirmation

proc MailSteal {handler ask} {
    global option t inbox

    if { 0 == [file readable $option(ms_netscape_pref_file)] 
	    || 0 == [string length $inbox]} {
	return
    }
    set dir ""
    set fh [open $option(ms_netscape_pref_file) r]
    while {0 == [eof $fh]} {
	gets $fh line
	if {![string compare MAIL_DIR: [lindex $line 0]]} {
	    set dir [RatTildeSubst [lindex $line 1]]
	    break
	}
    }
    close $fh
    if { ![string length $dir] || ![file readable $dir/Inbox]} {
	return
    }

    if {$option(ms_netscape_mtime) != [file mtime $dir/Inbox]} {
	if { 1 == $ask } {
	    set ask [RatDialog "" ! $t(netscape_steal) {} \
				0 $t(steal_back) $t(nothing)] } {
	}
	if {0 == $ask} {
	    set f [RatOpenFolder [list Netscape file {} $dir/Inbox]]
	    set max [lindex [$f info] 1]
	    for {set i 0} {$i < $max} {incr i} {
		$inbox insert [$f get $i]
		$f setFlag $i deleted 1
	    }
	    $f close
	    RatBusy {Sync $handler update}
	}
	set option(ms_netscape_mtime) [file mtime $dir/Inbox]
	SaveOptions
    }
}

# CalculateFontWidth --
#
# Calculate the default font width
#
# Arguments:
# w - The text widget to use

proc CalculateFontWidth {w} {
    global defaultFontWidth fixedNormFont

    set defaultFontWidth [font measure $fixedNormFont -displayof $w m]
}

# SetColor --
#
# Set the color scheme
#
# Arguments:
# baseColor  - The base color for the new scheme.
# foreground - The new foreground color

proc SetColor {baseColor {foreground black}} {
    global currentColor

    # Do nothing on monochorme displays
    if {2 == [winfo cells .]} {
	return
    }

    # Do nothing if no change
    if {[list $baseColor $foreground] == $currentColor} {
	return
    }

    # Remember new settings
    set currentColor [list $baseColor $foreground]

    # Apply new settings
    switch $baseColor {
    bisque {tk_bisque}
    default {tk_setPalette background $baseColor foreground $foreground}
    }

    # Make'em stick
    foreach p {background foreground activeForeground insertBackground
	       selectForeground highlightColor disabledForeground
	       highlightBackground activeBackground selectBackground
	       troughColor selectColor} {
	option add *TkRat*$p [option get . $p Color] interactive
    }
}

# SetIcon --
#
# Set the icon bitmap
#
# Arguments:
# w    - window to set the icon for
# icon - the name of the icon

proc SetIcon {w icon} {
    global env

    switch $icon {
	normal {
	    if {[file readable $env(LIBDIR)/tkrat.xbm]} {
		wm iconbitmap $w @$env(LIBDIR)/tkrat.xbm
		wm iconmask $w @$env(LIBDIR)/tkratmask.xbm
	    }
	}
	small {
	    if {[file readable $env(LIBDIR)/tkrat_small.xbm]} {
		wm iconbitmap $w @$env(LIBDIR)/tkrat_small.xbm
		wm iconmask $w @$env(LIBDIR)/tkrat_smallmask.xbm
	    }
	}
	none {
	    wm iconbitmap $w ""
	    wm iconmask $w ""
	}
    }
}

# FixMenu --
#
# Fixes a menu if it is to big to fit on the screen. This should be called
# as a postcommand and it will only check one menu, no cascades etc.
#
# Arguments:
# m -	The menu to fix

proc FixMenu {m} {
    set height [winfo screenheight $m]

    if { [$m yposition last] > $height} {
	global t

	# Calculate breakpoint. We assue all entries are of uniform height
	set i [expr {([$m index last]*$height)/[$m yposition last]-1}]
	$m insert $i cascade -label $t(more) -menu $m.m
	if {![winfo exists $m.m]} {
	    menu $m.m -postcommand "FixMenu $m.m"
	} else {
	    $m.m delete 1 end
	}
	incr i
	while {$i <= [$m index last]} {
	    switch [$m type $i] {
	    separator {
		    $m.m add separator
		}
	    command {
		    $m.m add command \
			    -label [$m entrycget $i -label] \
			    -command [$m entrycget $i -command]
		}
	    cascade {
		    $m.m add cascade \
			    -label [$m entrycget $i -label] \
			    -menu [$m entrycget $i -menu]
		}
	    }
	    $m delete $i
	}
    }
}


# AliasRead --
#
# Read aliases from default file.
#
# Arguments:

proc AliasRead {} {
    global option aliasBook

    set as $option(addrbooks)
    if {$option(use_system_aliases)} {
	lappend as $option(system_aliases)
    }
    foreach a $as {
	set book [lindex $a 0]
	set aliasBook(changed,$book) 0
	switch [lindex $a 1] {
	    tkrat {
		set f [lindex $a 2]
		if {[file readable $f]} {
		    catch {RatAlias read $f}
		}
		set dir [file dirname $f]
		if {([file isfile $f] && [file writable $f])
		|| (![file exists $f] && [file isdirectory $dir]
		&& [file writable $dir])} {
		    set aliasBook(writable,$book) 1
		} else {
		    set aliasBook(writable,$book) 0
		}
	    }
	    mail {
		set f [lindex $a 2]
		if {[file readable $f]} {
		    ReadMailAliases $f $book
		}
		set aliasBook(writable,$book) 0
	    }
	    elm  {
		set f [lindex $a 2]
		if {[file readable $f]} {
		    ReadElmAliases $f $book
		}
		set aliasBook(writable,$book) 0
	    }
	    pine {
		set f [lindex $a 2]
		if {[file readable $f]} {
		    ReadPineAliases $f $book
		}
		set aliasBook(writable,$book) 0
	    }
	}
    }
}


# FindAccelerators --
#
# Finds suitable accelerator keys for a bunch of strings. The result is
# an array where the keys are the different ids and the contents are
# the index of the character to use as accelerator.
#
# Arguments:
# var	- Name of array (in callers context) to place result in
# ids	- List of ids of strings to search

proc FindAccelerators {var ids} {
    upvar $var result
    global t

    set used ""
    foreach id $ids {
	set tot [string length $t($id)]
	set sub [string length [string trimleft $t($id) $used]]
	if {$sub > 0} {
	    set result($id) [expr {$tot - $sub}]
	    set used ${used}[string index $t($id) $result($id)]
	} else {
	    set result($id) -1
	}
    }
}

# RatTraceFolder --
#
# Traces the folder window list
#
# Arguments:
# as provided by trace

proc RatTraceFolder {args} {
    global openFolders folderWindowList

    set openFolders {}
    foreach h [array names folderWindowList] {
	lappend openFolders $folderWindowList($h)
    }
}

# RatExec --
#
# Used by the client to send commands to us
#
# Arguments:
# cmds	- Commands to execute

proc RatExec {cmds} {
    foreach cmd $cmds {
	if {2 == [llength $cmd]} {
	    set arg [lindex $cmd 1]
	} else {
	    set arg ""
	}
	switch -glob -- [lindex $cmd 0] {
	    open* {
		    global folderWindowList idCnt

		    if {"open" == [lindex $cmd 0]
			    && [array size folderWindowList]} {
			set handler [lindex [array names folderWindowList] 0]
		    } else {
			global idCnt option vFolderDef vFolderInbox

			set w .f[incr idCnt]
			toplevel $w -class TkRat
			regsub -all -- %f $option(main_window_name) . title
			wm title $w $title
			regsub -all -- %f $option(icon_name) . ititle
			wm iconname $w $ititle
			SetIcon $w $option(icon)
			Place $w folder
			if {"" == $arg} {
			    set arg [lindex $vFolderDef($vFolderInbox) 0]
			}
			if {$option(iconic)} {
			    wm iconify $w
			} else {
			    wm deiconify $w
			}
			set handler [FolderWindowInit $w $arg]
		    }
		    return [RatExecOpen $handler $arg]
		}
	    blank {
		    global idCnt option vFolderDef vFolderInbox

		    set w .f[incr idCnt]
		    toplevel $w -class TkRat
		    regsub -all -- %f $option(main_window_name) . title
		    wm title $w $title
		    regsub -all -- %f $option(icon_name) . ititle
		    wm iconname $w $ititle
		    SetIcon $w $option(icon)
		    Place $w folder
		    if {"" == $arg} {
			set arg [lindex $vFolderDef($vFolderInbox) 0]
		    }
		    if {$option(iconic)} {
			wm iconify $w
		    } else {
			wm deiconify $w
		    }
		    set handler [FolderWindowInit $w $arg]
		    FolderWindowClear $handler
		}
	    compose {
		    return [ComposeClient $arg]
		}
	    netsync {
		    return [RatExecNetsync $arg]
		}
	}
    }
}

# RatExecOpen --
#
# Executes the open command from the client
#
# Arguments:
# handler - Handler of window to use
# name	  - name or spec of folder to open

proc RatExecOpen {handler name} {
    global vFolderDef vFolderInbox option

    if {"" == $name} {
	set spec $vFolderInbox
	set check 1
    } elseif {[llength $name] >= 4 &&
	    [regexp {file|mh|dbase|imap|pop3|dynamic|dis} [lindex $name 1]]} {
	set spec $name
	set check 0
    } else {
	foreach i [array names vFolderDef] {
	    if {$name == [lindex $vFolderDef($i) 0]} {
		set spec $i
		break
	    }
	}
	if {![info exists spec]} {
	    error "No such folder '$name'"
	}
	set check 0
    }
    if {![FolderFailedOpen check $vFolderDef($spec)]} {
	VFolderOpen $handler $spec
    }

    # Check for stolen mail
    if {$check && $option(mail_steal)} {
	MailSteal $handler 1
    }
}

# RatExecNetsync --
#
# Executes the netsync command from the client
#
# Arguments:
# what - Which parts of the network sync to perform

proc RatExecNetsync {what} {
    global option

    set old $option(network_sync)
    if {[llength $what]} {
	set ns_send 0
	set ns_fetch 0
	set ns_cmd 0
	foreach w $what {
	    switch $w {
		send	{set ns_send 1}
		fetch	{set ns_fetch 1}
		cmd	{set ns_cmd 1}
		default	{error "Illegal netsync arg '$w'"}
	    }
	}
	set option(network_sync) [list $ns_send $ns_fetch $ns_cmd]
    }
    RatBusy {NetworkSync}
    set option(network_sync) $old
}

# Browse --
#
# Browse for a file, fetches the default from the give var and returns the
# result in the same.
#
# Arguments:
# w	- Parent window of browser
# var	- Name of variable containing name of file
# mode	- Mode of file dialog

proc Browse {w var mode} {
    global env t
    upvar #0 $var filein

    if {"" != $filein} {
	set dir [file dirname $filein]
	set file [file tail $filein]
    } else {
	set dir $env(HOME)
	set file ""
    }

    set r [rat_fbox::run -parent $w -initialdir $dir -initialfile $file \
	    -title $t(select_file) -ok $t(ok) -mode $mode]
    if {"" != $r} {
	uplevel #0 [list set $var $r]
    }
}


# IsExecutable --
#
# Checks if a given command matches any executable file (by searching path
# if needed).
#
# Arguments:
# cmd	- Command to check

proc IsExecutable {cmd} {
    global env

    if {[regexp / $cmd]} {
	return [file executable $cmd]
    }

    foreach p [split $env(PATH) :] {
	if {[file executable $p/$cmd]} {
	    return 1
	}
    }
    return 0
}
