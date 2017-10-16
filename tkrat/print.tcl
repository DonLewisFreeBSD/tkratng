# print.tcl --
#
# Handles printing of a message
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

# Print --
#
# Shows the print dialog
#
# Arguments:
# handler -	Handler of the corresponding folder command
# which	  -	Which messages that should be printed "group" or "selected"

proc Print {handler which} {
    global option idCnt t b propLightFont

    set msgs [GetMsgSet $handler $which]
    if {0 == [llength $msgs]} {
	return
    }

    # Create identifier
    set id p[incr idCnt]
    upvar #0 $id hd
    set w .$id
    set hd(w) $w
    set hd(oldfocus) [focus]

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(print_setup)

    # Print setup button
    button $w.ps -text $t(print_setup)... -command PrintSetup -pady 1
    grid x $w.ps -sticky e -pady 5 -padx 5
    set b($w.ps) print_setup

    # What to print
    label $w.label -text $t(what_to_print): -width 25 -anchor w
    grid $w.label - -sticky w -padx 5
    frame $w.f -relief ridge -bd 2
    grid $w.f - -sticky nsew -padx 5

    # Header set
    label $w.f.hs_label -text $t(headers): -anchor e
    set m $w.f.hs_menu.m
    menubutton $w.f.hs_menu -indicatoron 1 -relief raised -menu $m -pady 1 \
	    -textvariable ${id}(header_set) -width 12 -font $propLightFont
    menu $m -tearoff 0
    foreach i {none selected all} {
	$m add command -label $t($i) \
		-command "set ${id}(header_set) $t($i); set ${id}(hs) $i"
    }
    set hd(hs) $option(show_header)
    set hd(header_set) $t($hd(hs))
    grid $w.f.hs_label $w.f.hs_menu -sticky w
    set b($w.f.hs_menu) print_headers

    # Parts of message (if any)
    if {1 < [llength $msgs]} {
	checkbutton $w.f.attachments \
		-variable ${id}(attachments) \
		-text $t(print_attachments)
	grid $w.f.attachments - -sticky w
	set b($w.f.attachments) print_attachments
    } else {
	set hd(state) body
	set hd(bodys) [PrintAddBodyparts $id $w.f [$msgs body] ""]
    }


    # Print and cancel buttons
    OkButtons $w $t(print) $t(cancel) "Print2 $id"
    grid $w.buttons - -sticky nsew -pady 5

    # Bindings
    wm protocol $w WM_DELETE_WINDOW "Print2 $id 0"
    bind $w <Return> "Print2 $id 1"

    # Place and focus
    Place $w print
    focus $w

    set hd(msgs) $msgs
}

# PrintAddBodyparts --
#
# Add bodyparts to list
#
# Arguments:
# handler - Handler identifying the print window
# w	  - Window to add them to
# body    - Bodypart to add
# indent  - Indention to add

proc PrintAddBodyparts {handler w body indent} {
    upvar #0 $handler hd
    global t b propLightFont

    set type [string tolower [$body type]]
    if {"multipart" == [lindex $type 0]} {
	set bd {}
	foreach c [$body children] {
	    set b2 [PrintAddBodyparts $handler $w $c "  $indent"]
	    if {[llength $bd]} {
		set bd [concat $bd $b2]
	    } else {
		set bd $b2
	    }
	}
	return $bd
    }

    if {"body" == $hd(state)} {
	set indent ""
	set hd($body) 1
    }
    if {"" == [set desc [string range [$body description] 0 40]]} {
	if {"body" == $hd(state)} {
	    if {"text" == [lindex $type 0]} {
		set desc "$t(body): [lindex $type 0]/[lindex $type 1]"
	    } else {
		set desc "[lindex $type 0]/[lindex $type 1]"
	    }
	    set hd(state) attachment
	} else {
	    set desc "$t(attachment): [lindex $type 0]/[lindex $type 1]"
	}
    }
    set hd($body) 1
    checkbutton $w.b$body -variable ${handler}($body) -text "$indent$desc" \
	    -font $propLightFont
    grid $w.b$body - -sticky w
    set b($w.b$body) print_bodypart
    return $body
}

# Print2 --
#
# Second stage of print functions, the user has decided...
#
# Arguments:
# handler - Handler identifying the print window
# print   - True if we should print

proc Print2 {handler print} {
    upvar #0 $handler hd
    global option b

    if {$print} {
	set children [winfo children .]
	foreach c $children {
	    blt_busy hold $c
	}
	update idletasks

	if {1 < [llength $hd(msgs)]} {
	    foreach m $hd(msgs) {
		set hd(state) body
		set hd(bodys) {}
		PrintGetWhich $handler [$m body]
		DoPrintMsg $hd(hs) $m $hd(bodys)
	    }
	} else {
	    set bodys {}
	    foreach bd $hd(bodys) {
		if {$hd($bd)} {
		    lappend bodys $bd
		}
	    }
	    DoPrintMsg $hd(hs) $hd(msgs) $bodys
	}

	foreach c $children {
	    blt_busy release $c
	}
    }
    foreach bn [array names b $hd(w).*] {unset b($bn)}
    catch {focus $hd(oldfocus)}
    destroy $hd(w)
    unset hd
}

# PrintGetWhich --
#
# Gets which boyparts to print, either just the first text-bodypart or
# all non-multiparts.
#
# Arguments:
# handler - Handler identifying the print window
# body	  - Current bodypart to maybe include

proc PrintGetWhich {handler body} {
    upvar #0 $handler hd

    set type [string tolower [lindex [$body type] 0]]
    if {"multipart" == $type} {
	foreach c [$body children] {
	    PrintGetWhich $handler $c
	}
    } else {
	if {"text" == $type && "body" == $hd(state)} {
	    lappend hd(bodys) $body
	    set hd(state) attachment
	} elseif {$hd(attachments)} {
	    lappend hd(bodys) $body
	}
    }
}

# DoPrintMsg --
#
# Actually do the printing of a message
#
# Arguments:
# hs	- Set of headers to print
# msg	- Message to print
# bodys	- Bodyparts to print

proc DoPrintMsg {hs msg bodys} {
    global option rat_tmp

    if {"printer" == $option(print_dest)} {
	set fileName $rat_tmp/rat.[RatGenId]
    } else {
	set fileName $option(print_file)
    }
    set tmpFH [open $fileName w]
    if {$option(print_pretty)} {
	RatPrettyPrintMsg $tmpFH $hs $msg $bodys
    } else {
	PlainPrintMsg $tmpFH $hs $msg $bodys
    }
    close $tmpFH
    if {"printer" == $option(print_dest)} {
	ExecPrintCommand $fileName
    }
}

# PlainPrintMsg --
#
# Do the simple printing of a message
#
# Arguments:
# channel - Channel to print to
# hs	  - Set of headers to print
# msg	  - Message to print
# bodys	  - Bodyparts to print

proc PlainPrintMsg {channel hs msg bodys} {
    global option

    switch $hs {
	all	{
		foreach h [$msg headers] {
		    puts $channel "[lindex $h 0]: [lindex $h 1]"
		}
		puts $channel ""
	    }
	selected {
		foreach h [$msg headers] {
		    set header([string tolower [lindex $h 0]]) [lindex $h 1]
		}
		foreach f $option(show_header_selection) {
		    set n [string tolower $f]
		    if {[info exists header($n)]} {
			puts $channel "$f: $header($n)"
		    }
		}
		unset header
		puts $channel ""
	    }
	default	{ }
    }
    set first 1
    foreach b $bodys {
	if {$first} {
	    set first 0
	} else {
	    puts $channel "_______________________________________________________________________________"
	}
	$b saveData $channel 0 1
    }
}

# ExecPrintCommand --
#
# Actually does the printing
#
# Arguments:
# fileName  - The name of the file to print

proc ExecPrintCommand {fileName} {
    global option idCnt env

    # Create identifier
    set id print[incr idCnt]
    upvar ${id}(fileName) ifn
    set ifn $fileName

    set cmd $option(print_command)
    regsub "%p" $cmd $option(print_printer) cmd
    if { 0 == [regsub "%s" $cmd $fileName cmd]} {
	set cmd "cat $fileName | $cmd"
    }
    regsub {^~/} $cmd $env(HOME)/ cmd
    regsub -all {[ 	]~/} $cmd " $env(HOME)/" cmd
    RatBgExec ${id}(exitStatus) $cmd
    uplevel #0 "trace variable ${id}(exitStatus) w PrintDone"
}

# PrintDone --
#
# Is called by the trace callback when the print command is done. It is
# meant to do any cleaning up.
#
# Arguments:
# name1, name2 -	Variable specifiers
# op	       -	Operation

proc PrintDone {name1 name2 op} {
    upvar #0 $name1 pr

    file delete -force -- $pr(fileName) &
    unset pr
}

# PrintSetup --
#
# Shows the print setup dialog
#
# Arguments:

proc PrintSetup {} {
    global t option b propLightFont

    set w .ps

    if {[winfo exists $w]} {
	wm deiconify $w
	raise $w
	return
    }
    set id ps
    upvar #0 $id hd
    set hd(w) $w
    set hd(pretty) $option(print_pretty)
    set hd(dest) $option(print_dest)
    set hd(printer) $option(print_printer)
    set hd(file) $option(print_file)
    set hd(papersize) $option(print_papersize)
    set hd(orientation) $option(print_orientation)
    set hd(fontfamily) $option(print_fontfamily)
    set hd(ori) $t($hd(orientation))
    set hd(fontsize) $option(print_fontsize)
    set hd(resolution) $option(print_resolution)

    toplevel $w -class TkRat
    wm title $w $t(print_setup)

    # Destination
    label $w.dest -text $t(destination): -anchor w
    frame $w.d -relief ridge -bd 2
    radiobutton $w.d.printer -text $t(printer) -variable ${id}(dest) \
	    -value printer -anchor w -command "PrintSetDest $id"
    entry $w.d.pentry -width 15 -textvariable ${id}(printer)
    radiobutton $w.d.file -text $t(file) -variable ${id}(dest) \
	    -value file -anchor w -command "PrintSetDest $id"
    entry $w.d.fentry -width 15 -textvariable ${id}(file)
    button $w.d.browse -text $t(browse)... -pady 1 \
	    -command "Browse $w ${id}(file) any"
    grid $w.d.printer $w.d.pentry -sticky ew
    grid $w.d.file $w.d.fentry $w.d.browse -sticky ew
    set b($w.d.printer) dest_printer
    set b($w.d.pentry) dest_printer
    set b($w.d.file) dest_file
    set b($w.d.fentry) dest_file
    set b($w.d.browse) file_browse
    set hd(dest_printer) $w.d.pentry
    set hd(dest_file) $w.d.fentry
    PrintSetDest $id

    # Mode
    frame $w.m
    label $w.m.l -text $t(mode): -anchor e
    radiobutton $w.m.plain -text $t(plain_text) -variable ${id}(pretty) \
	    -value 0 -command "rat_ed::enabledisable 0 $w.p" -anchor w
    radiobutton $w.m.pretty -text $t(pretty_ps) -variable ${id}(pretty) \
	    -value 1 -command "rat_ed::enabledisable 1 $w.p" -anchor w
    grid $w.m.l $w.m.plain -sticky we
    grid x $w.m.pretty -sticky we
    grid columnconfigure $w.m 1 -weight 1
    set b($w.m.plain) plain_text
    set b($w.m.pretty) pretty_ps

    frame $w.p -relief ridge -bd 2 
    label $w.p.size -text $t(paper_size): -anchor e
    set m $w.p.msize.m
    menubutton $w.p.msize -textvariable ${id}(papersize) -menu $m -pady 1 \
	    -relief raised -width 10
    menu $m
    foreach s $option(print_papersizes) {
	set n [lindex $s 0]
        $m add command -label $n -command "set ${id}(papersize) $n"
    }
    grid $w.p.size $w.p.msize - -sticky ew
    set b($w.p.msize) paper_size
    label $w.p.ori -text $t(orientation): -anchor e
    set m $w.p.mori.m
    menubutton $w.p.mori -textvariable ${id}(ori) -menu $m -pady 1 \
	    -relief raised -width 10
    menu $m
    foreach o {portrait landscape} {
        $m add command -label $t($o) \
		-command "set ${id}(ori) $t($o); set ${id}(orientation) $o"
    }
    grid $w.p.ori $w.p.mori - -sticky ew
    set b($w.p.mori) orientation
    label $w.p.font -text $t(font): -anchor e
    set m $w.p.mfont.m
    menubutton $w.p.mfont -textvariable ${id}(fontfamily) -menu $m -pady 1 \
	    -relief raised -width 15
    menu $m
    foreach f {Times Helvetica Courier} {
        $m add command -label $f -command "set ${id}(fontfamily) $f"
    }
    grid $w.p.font $w.p.mfont - -sticky ew
    set b($w.p.mfont) print_font
    label $w.p.fontsize -text $t(font_size): -anchor e
    entry $w.p.efsize -width 5 -textvariable ${id}(fontsize)
    label $w.p.funit -text $t(points) -font $propLightFont -anchor w
    grid $w.p.fontsize $w.p.efsize $w.p.funit -sticky ew
    set b($w.p.efsize) print_fontsize
    label $w.p.res -text $t(pic_res): -anchor e
    entry $w.p.eres -width 5 -textvariable ${id}(resolution)
    label $w.p.runit -text $t(dpi) -font $propLightFont -anchor w
    grid $w.p.res $w.p.eres $w.p.runit -sticky ew
    set b($w.p.eres) print_res
    grid columnconfigure $w.p 3 -weight 1

    # Buttons
    OkButtons $w $t(apply) $t(cancel) "PrintSetup2 $id"

    pack $w.dest \
	 $w.d \
	 $w.m \
	 $w.p \
	 $w.buttons -side top -expand 1 -fill both 

    Place $w printSetup

    bind $w <Return> "PrintSetup2 $id 1"
    rat_ed::enabledisable $hd(pretty) $w.p
    wm protocol $w WM_DELETE_WINDOW "PrintSetup2 $id 0"
}

# PrintSetDest --
#
# Enable/disable widgets according to destination radiobutton value
#
# Arguments:
# handler	- Handler for the print setup window

proc PrintSetDest {handler} {
    upvar #0 $handler hd

    if {"printer" == $hd(dest)} {
	rat_ed::enable $hd(dest_printer)
	rat_ed::disable $hd(dest_file)
    } else {
	rat_ed::disable $hd(dest_printer)
	rat_ed::enable $hd(dest_file)
    }
}


# PrintSetup2 --
#
# Called when the print setup window is done
#
# Arguments:
# handler - Handler identifying the print setup window
# apply   - Boolean stating if we should apply the settings or not

proc PrintSetup2 {handler apply} {
    upvar #0 $handler hd
    global option

    if {$apply} {
	set changed 0
	foreach v {dest printer file pretty papersize fontfamily
		   fontsize resolution orientation} {
	    if {$hd($v) != $option(print_$v)} {
		set option(print_$v) $hd($v)
		incr changed
	    }
	}
	if {$changed} {
	    SaveOptions
	}
    }
    wm withdraw $hd(w)
}
