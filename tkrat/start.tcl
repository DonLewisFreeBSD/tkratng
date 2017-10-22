#
#  TkRat software and its included text is Copyright 1996-2005 by
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
	   ISO_Left_Tab tk_version rat_tmp tklead folderUnseen propBigFont \
	   fixedItalicFont

    # Base package requirements
    package require -exact ratatosk 2.3

    # Function to let client know we have started
    proc RatPing {} {
	return pong
    }

    # Initialize variables
    set tkrat_version 2.3.0
    set tkrat_version_date 20050717
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
    OptionsInitText
    OptionsRead
    InitCharsetAliases
    InitPgp
    if {$tk_version >= 8.3} {
	tk useinputmethods $option(useinputmethods)
    }
    if {$tk_version == 8.3} {
        package require rat_compat 1.0
        rat_compat::init8_3
    }

    # Reinitialize language (if needed)
    if {[string compare $option(language) $currentLanguage_t]} {
	InitMessages $option(language) t
    }

    if {0 != $option(last_version_date)
	&& "$option(last_version_date)" != $tkrat_version_date} {
	NewVersionUpdate
    }

    # Update the default fonts
    if {$option(font_size) > 10} {
        set big_size [expr 10+($option(font_size)-10)*2]
    } else {
        set big_size [expr $option(font_size)+2]
    }
    set propBigFont [RatCreateFont \
                         [list components $option(font_family_prop) \
                              $big_size bold roman 1 0]]
    set propNormFont [RatCreateFont \
                         [list components $option(font_family_prop) \
                              $option(font_size) bold roman 0 0]]
    set propLightFont [RatCreateFont \
                         [list components $option(font_family_prop) \
                              $option(font_size) normal roman 0 0]]
    set fixedNormFont [RatCreateFont \
                         [list components $option(font_family_fixed) \
                              $option(font_size) normal roman 0 0]]
    set fixedBoldFont [RatCreateFont \
                         [list components $option(font_family_fixed) \
                              $option(font_size) bold roman 0 0]]
    set fixedItalicFont [RatCreateFont \
                         [list components $option(font_family_fixed) \
                              $option(font_size) normal italic 0 0]]
    set watcherFont [RatCreateFont $option(watcher_font)]
    if {$option(override_fonts)} {
	set pri interactive
    } else {
	set pri widgetDefault
    }
    option add *TkRat*font $propLightFont $pri
    option add *TkRat*Entry.font $fixedNormFont $pri
    option add *TkRat*Text.font $fixedNormFont $pri
    option add *TkRat*Listbox.font $fixedNormFont $pri
    option add *TkRat*RatList*Listbox.font $propNormFont $pri
    option add *TkRat*RatTree*font $propLightFont $pri

    option add *Menu.tearOff $option(tearoff) widgetDefault

    option add *TkRat*Button.padY 1 widgetDefault
    option add *TkRat*Button.padX 2 widgetDefault
    option add *TkRat*Menubutton.padY 1 widgetDefault
    option add *TkRat*Menubutton.padX 2 widgetDefault
    option add *TkRat*Menu.activeBorderWidth 0 widgetDefault

    bind Menubutton <Up> {event generate %W <space>}
    bind Menubutton <Down> {event generate %W <space>}

    # Extra package requirements
    package require rat_list 1.0
    package require rat_fbox 1.1
    package require rat_balloon 1.0
    package require rat_edit 1.1
    package require rat_textlist 1.0
    package require blt_busy 1.0
    package require rat_ed 1.0
    package require rat_ispell 1.1
    package require rat_tree 1.0
    package require rat_enriched 1.0
    package require rat_flowmsg 1.0
    package require rat_scrollframe 1.0
    package require rat_textspell 1.0
    package require rat_find 1.0
    package require rat_table 1.0

    # Change the color
    if {$option(override_color)} {
	option add *TkRat*foreground black interactive
	option add *TkRat*background \#dde3eb interactive
	eval "SetColor $option(color_set)"
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

    # Bind global window close
    bind TkRat <Control-w> {destroy %W}

    # Make sure our config directory exists
    if {![file isdirectory $option(ratatosk_dir)]} {
	catch {file mkdir [RatTildeSubst $option(ratatosk_dir)]} result
	if {[string length $result]} {
	    Popup [concat \
		       "$t(failed_create) \"$option(ratatosk_dir)\":"\
		       "$result.\n$t(do_without_dir)"]
	}
    }

    # Initialize balloon help
    InitMessages $option(language) balText
    rat_balloon::Init b balText

    # Give info about new features (or run first use wizard)
    if {"$option(last_version_date)" != $tkrat_version_date} {
	set isFirstUse [StartupInfo]
    } else {
        set isFirstUse 0
    }

    # Read misc files
    VFolderRead
    AliasRead
    if { 3 > $option(scan_aliases) } {
	ScanAliases
    }
    ReadUserproc
    ::tkrat::winctl::ReadPos
    if {[file readable $option(ratatosk_dir)/expressions]} {
	ExpRead
    }

    if {$isFirstUse} {
        FirstUseWizard
        SaveOptions
    }

    # Setup trace of folderWindoList
    set openFolders {}
    trace variable folderWindowList wu RatTraceFolder

    # Setup online status
    switch $option(start_online_mode) {
	online { set option(online) 1 }
	offline { set option(online) 0 }
	default {}
    }

    if { 0 <= [expr {[RatDaysSinceExpire]-$option(expire_interval)}]} {
	catch {Expire} err
    } else {
	set expAfter \
		[after [expr {($option(expire_interval)- \
		[RatDaysSinceExpire])*24*60*60*1000}] Expire]
    }

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
	if {$level > 0} {
	    if {![string compare explicit $mode]} {
		if {[string length $logAfter]} {
		    after cancel $logAfter
		    set statusBacklog {}
		    set logAfter {}
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
    bind $w <Escape> "$cmd 0"
    wm protocol [winfo toplevel $w] WM_DELETE_WINDOW "$cmd 0"
}

# RatMkAccelerator --
#
# Creates the accelerator entry for a key-binding
#
# Arguments:
# keylist  - Index into options array to get key combinations

proc RatMkAccelerator {keylist} {
    global option accelerator

    foreach k $option($keylist) {
	if {[info exists a]} {
	    if {[string length $k] < [string length $a]} {
		set a $k
	    }
	} else {
	    set a $k
	}
    }

    if {[info exists a]} {
        set n {}
        foreach k [split [string trim $a "<>"] -] {
            switch $k {
                "Key"		{}
                "Control"	{lappend n "Ctrl"}
                "Meta"		{lappend n "M"}
                "exclam"       	{lappend n "!"}
                "quotedbl"    	{lappend n "\""}
                "numbersign"    {lappend n "#"}
                "dollar"       	{lappend n "\$"}
                "percent"       {lappend n "%"}
                "ampersand"     {lappend n "&"}
                "parenleft"     {lappend n "("}
                "parenright"    {lappend n ")"}
                "asterisk"      {lappend n "*"}
                "plus"       	{lappend n "+"}
                "comma"       	{lappend n ","}
                "minus"       	{lappend n "-"}
                "period"       	{lappend n "."}
                "slash"       	{lappend n "/"}
                "colon"		{lappend n ":"}
                "semicolon"	{lappend n ";"}
                "less"		{lappend n "<"}
                "equal"		{lappend n "="}
                "greater"	{lappend n ">"}
                "question"	{lappend n "?"}
                "at"		{lappend n "@"}
                "bracketleft"	{lappend n "["}
                "backslash"	{lappend n "\\"}
                "bracketright"	{lappend n "]"}
                "asciicircum"	{lappend n "^"}
                "underscore"	{lappend n "_"}
                "braceleft"	{lappend n "{"}
                "bar"		{lappend n "|"}
                "braceright"	{lappend n "}"}
                default		{lappend n $k}
            }
        }
        set key [join $n -]
    } else {
        set key ""
    }
    set accelerator($keylist) $key
}

# RatBind --
#
# Bind the specified keys to the specified function
#
# Arguments:
# w        - Window to bind in
# keylist  - Index into options array to get key combinations
# function - Function to bind the keys to

proc RatBind {w keylist function} {
    global option accelerator

    if {![info exists accelarator($keylist)]} {
        RatMkAccelerator $keylist
    }

    foreach k $option($keylist) {
	if {0 < [regsub < $k <Alt- altkey]} {
	    bind $w $altkey { }
	}
	bind $w $k $function
    }
}

# RatBindMenu --
#
# Bind the specified keys to the specified menu function
#
# Arguments:
# w        - Window to bind in
# keylist  - Index in options array to get key combinations
# menu     - The menu to configure (if any)

proc RatBindMenu {w keylist menu} {
    global option accelerator

    set cmd [list [lindex $menu 0] invoke [lindex $menu 1]]
    RatBind $w $keylist "$cmd ; break"
    [lindex $menu 0] entryconfigure [lindex $menu 1] \
        -accelerator $accelerator($keylist)
}

# RatBindMenus --
#
# Apply bindings for a number of menus.
#
# Arguments:
# w      - Window to bind in
# aname  - Name of array holding info
# prefix - Prefix of option keys
# keys   - Keys to bind for

proc RatBindMenus {w aname prefix keys} {
    upvar \#0 $aname hd
    global option

    foreach k $keys {
	RatBindMenu $w ${prefix}_key_${k} $hd($k)
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

proc SetColor {baseColor baseFg editColor editFg} {
    global currentColor

    # Do nothing on monochrome displays
    if {2 == [winfo cells .]} {
	return
    }

    # Do nothing if no change
    if {[list $baseColor $baseFg $editColor $editFg] == $currentColor} {
	return
    }

    # Remember new settings
    set currentColor [list $baseColor $baseFg $editColor $editFg]

    array set base [CalculateColors $baseColor $baseFg]
    array set edit [CalculateColors $editColor $editFg]

    set edit(highlightBackground) $base(background)
    set edit(selectBackground) $base(selectBackground)

    foreach c [array names base] {
	option add *TkRat*$c $base($c) interactive
    }
    foreach c [array names edit] {
	option add *TkRat*Text.$c $edit($c) interactive
	option add *TkRat*Entry.$c $edit($c) interactive
	option add *TkRat*Listbox.$c $edit($c) interactive
	option add *TkRat*Canvas.$c $edit($c) interactive
	option add *TkRat*Spinbox.$c $edit($c) interactive
    }
}

# CalculateColors --
#
# Calculate a set of colors
#
# Arguments:
# background - The background color
# foreground - The foreground color

proc CalculateColors {background foreground} {
    set new(background) $background
    set new(foreground) $foreground

    set bg [winfo rgb . $new(background)]
    set fg [winfo rgb . $new(foreground)]
    set darkerBg [format #%02x%02x%02x [expr {(9*[lindex $bg 0])/2560}] \
	    [expr {(9*[lindex $bg 1])/2560}] [expr {(9*[lindex $bg 2])/2560}]]
    foreach i {activeForeground insertBackground selectForeground \
	    highlightColor} {
        set new($i) $new(foreground)
    }
    set new(disabledForeground) \
        [format #%02x%02x%02x \
             [expr {(3*[lindex $bg 0] + [lindex $fg 0])/1024}] \
             [expr {(3*[lindex $bg 1] + [lindex $fg 1])/1024}] \
             [expr {(3*[lindex $bg 2] + [lindex $fg 2])/1024}]]
    set new(highlightBackground) $new(background)

    foreach i {0 1 2} {
        set light($i) [expr {[lindex $bg $i]/256}]
        set inc1 [expr {($light($i)*15)/100}]
        set inc2 [expr {(255-$light($i))/3}]
        if {$inc1 > $inc2} {
            incr light($i) $inc1
        } else {
            incr light($i) $inc2
        }
        if {$light($i) > 255} {
            set light($i) 255
        }
    }
    set new(activeBackground) [format #%02x%02x%02x $light(0) \
                                   $light(1) $light(2)]
    set new(selectBackground) $darkerBg
    set new(troughColor) $darkerBg
    set new(selectColor) #b03060

    return [array get new]
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
	    $m.m delete 0 end
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
		    catch {RatAlias read $book $f}
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
# used  - List of already use accelerators

proc FindAccelerators {var ids {used {}}} {
    upvar $var result
    global t

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

# SetupShortcuts --
#
# Setup suitable keyboard shortcuts for a number of buttons. This includes:
# - Figuring out sutable accelerators
# - Marking them by underlining
# - Adding keybindings
#
# Arguments:
# buttons - List of buttons to work on

proc SetupShortcuts {buttons} {
    set top [winfo toplevel [lindex $buttons 0]]

    # Remove old bindings
    foreach b [bind $top] {
	if {[string match "<Alt-Key-?>" $b]} {
	    bind $top $b {}
	}
    }

    set u " "
    foreach but $buttons {
	set text [string tolower [$but cget -text]]	
	regsub -all {[^\w ]} $text " " text
	if {![regexp -indices "(^| )(\[^$u\])\\w" $text unused unused loc]} {
	    if {![regexp -indices "\[^$u\]" $text loc]} {
		$but configure -underline -1
		continue
	    }
	}
	set c [string index $text [lindex $loc 0]]
	bind $top <Alt-Key-$c> "$but invoke ; break"
	bind $top <Alt-Key-[string toupper $c]> "$but invoke ; break"
	$but configure -underline [lindex $loc 0]
	set u "$u$c"
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
		    set title [string map {%f . %r ?} \
				   $option(main_window_name)]
		    wm title $w $title
		    set ititle [string map {%f .} $option(icon_name)]
		    wm iconname $w $ititle
		    SetIcon $w $option(icon)
		    ::tkrat::winctl::Place folderWindow $w
		    set handler [FolderWindowInit $w $arg]
		    ::tkrat::winctl::Place folderWindow $w
		    if {$option(iconic)} {
			wm iconify $w
		    } else {
			wm deiconify $w
		    }
		}
		return [RatExecOpen $handler $arg]
	    }
	    blank {
		global idCnt option vFolderDef vFolderInbox
		
		set w .f[incr idCnt]
		toplevel $w -class TkRat
		set title [string map {%f . %r ?} $option(main_window_name)]
		wm title $w $title
		set ititle [string map {%f .} $option(icon_name)]
		wm iconname $w $ititle
		SetIcon $w $option(icon)
		if {"" == $arg} {
		    set arg [lindex $vFolderDef($vFolderInbox) 0]
		}
                ::tkrat::winctl::Place folderWindow $w
		set handler [FolderWindowInit $w $arg]
                ::tkrat::winctl::Place folderWindow $w
		FolderWindowClear $handler
		if {$option(iconic)} {
		    wm iconify $w
		} else {
		    wm deiconify $w
		}
	    }
	    compose {
		return [ComposeClient $arg]
	    }
	    mailto {
		return [MailtoClient $arg]
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
    global env t option
    upvar \#0 $var filein

    if {"" != $filein} {
	if {[file isdirectory $filein]} {
	    set dir $filein
	    set file ""
	} else {
	    set dir [file dirname $filein]
	    set file [file tail $filein]
	}
    } else {
	set dir $option(initialdir)
	set file ""
    }

    set r [rat_fbox::run \
               -parent $w \
               -initialdir $dir \
               -initialfile $file \
               -title $t(select_file) \
               -ok $t(ok) \
               -mode $mode]
    if {"" != $r} {
        if {$option(initialdir) != [file dirname $r]} {
            set option(initialdir) [file dirname $r]
            SaveOptions
        }
	uplevel \#0 [list set $var $r]
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


# DumpStack --
#
# Dumps the tcl calling stack
#
# Arguments:

proc DumpStack {} {
    for {set i 1} {$i < [info level]} {incr i} {
        puts "$i: [info level $i]"
    }
}
