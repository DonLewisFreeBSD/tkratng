# tkfbox.tcl --
#       This is a special version of the tk file selection box. It has been
#       subject to the following modifications:
#	- There is a new argument -mode (open, save, anyopen, dirok)
#       - There is a new argument -ok which sets the text of the ok button
#       - The file and cancel texts are fetched from the global variables
#         t(file) and t(cancel)
#	- The original tk icon list is used
#       - The whole thing is now contained in a separate namespace rat_fbox
#       The original comments follows:
#
#	Implements the "TK" standard file selection dialog box. This
#	dialog box is used on the Unix platforms whenever the tk_strictMotif
#	flag is not set.
#
#	The "TK" standard file selection dialog box is similar to the
#	file selection dialog box on Win95(TM). The user can navigate
#	the directories by clicking on the folder icons or by
#	selectinf the "Directory" option menu. The user can select
#	files by clicking on the file icons or by entering a filename
#	in the "Filename:" entry.
#
# RCS: @(#) $Id: rat_fbox.tcl,v 1.24 2002/01/01 14:19:34 maf Exp $
#
# Copyright (c) 1994-1998 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

package provide rat_fbox 1.1

namespace eval rat_fbox {
    namespace export run
    variable state
    variable previous {}
}

#----------------------------------------------------------------------
#
#		      I C O N   L I S T
#
# This is a pseudo-widget that implements the icon list inside the 
# file-dialog dialog box.
#
#----------------------------------------------------------------------

# ratIconList --
#
#	Creates an IconList widget.
#
proc ratIconList {w args} {
    upvar #0 $w data

    ratIconList_Config $w $args
    ratIconList_Create $w
}

# ratIconList_Config --
#
#	Configure the widget variables of IconList, according to the command
#	line arguments.
#
proc ratIconList_Config {w argList} {
    upvar #0 $w data

    # 1: the configuration specs
    #
    set specs {
	{-browsecmd "" "" ""}
	{-command "" "" ""}
    }

    # 2: parse the arguments
    #
    tclParseConfigSpec $w $specs "" $argList
}

# ratIconList_Create --
#
#	Creates an IconList widget by assembling a canvas widget and a
#	scrollbar widget. Sets all the bindings necessary for the IconList's
#	operations.
#
proc ratIconList_Create {w} {
    upvar #0 $w data
    global tklead

    frame $w
    set data(sbar)   [scrollbar $w.sbar -orient horizontal \
	-highlightthickness 0 -takefocus 0]
    set data(canvas) [canvas $w.canvas -bd 2 -relief sunken \
	-width 400 -height 120 -takefocus 1]
    pack $data(sbar) -side bottom -fill x -padx 2
    pack $data(canvas) -expand yes -fill both

    $data(sbar) config -command [list $data(canvas) xview]
    $data(canvas) config -xscrollcommand [list $data(sbar) set]

    # Initializes the max icon/text width and height and other variables
    #
    set data(maxIW) 1
    set data(maxIH) 1
    set data(maxTW) 1
    set data(maxTH) 1
    set data(numItems) 0
    set data(curItem)  {}
    set data(noScroll) 1

    # Creates the event bindings.
    #
    bind $data(canvas) <Configure>	[list ratIconList_Arrange $w]

    bind $data(canvas) <1>		[list ratIconList_Btn1 $w %x %y]
    bind $data(canvas) <B1-Motion>	[list ratIconList_Motion1 $w %x %y]
    bind $data(canvas) <B1-Leave>	[list ratIconList_Leave1 $w %x %y]
    bind $data(canvas) <B1-Enter>	[list ${tklead}CancelRepeat]
    bind $data(canvas) <ButtonRelease-1> [list ${tklead}CancelRepeat]
    bind $data(canvas) <Double-ButtonRelease-1> \
	    [list ratIconList_Double1 $w %x %y]

    bind $data(canvas) <Up>		[list ratIconList_UpDown $w -1]
    bind $data(canvas) <Down>		[list ratIconList_UpDown $w  1]
    bind $data(canvas) <Left>		[list ratIconList_LeftRight $w -1]
    bind $data(canvas) <Right>		[list ratIconList_LeftRight $w  1]
    bind $data(canvas) <Return>		[list ratIconList_ReturnKey $w]
    bind $data(canvas) <KeyPress>	[list ratIconList_KeyPress $w %A]
    bind $data(canvas) <Control-KeyPress> ";"
    bind $data(canvas) <Alt-KeyPress>	";"

    bind $data(canvas) <FocusIn>	[list ratIconList_FocusIn $w]

    return $w
}

# ratIconList_AutoScan --
#
# This procedure is invoked when the mouse leaves an entry window
# with button 1 down.  It scrolls the window up, down, left, or
# right, depending on where the mouse left the window, and reschedules
# itself as an "after" command so that the window continues to scroll until
# the mouse moves back into the window or the mouse button is released.
#
# Arguments:
# w -		The IconList window.
#
proc ratIconList_AutoScan {w} {
    upvar #0 $w data
    global ratPriv

    if {![winfo exists $w]} return
    set x $ratPriv(x)
    set y $ratPriv(y)

    if {$data(noScroll)} {
	return
    }
    if {$x >= [winfo width $data(canvas)]} {
	$data(canvas) xview scroll 1 units
    } elseif {$x < 0} {
	$data(canvas) xview scroll -1 units
    } elseif {$y >= [winfo height $data(canvas)]} {
	# do nothing
    } elseif {$y < 0} {
	# do nothing
    } else {
	return
    }

    ratIconList_Motion1 $w $x $y
    set ratPriv(afterId) [after 50 [list ratIconList_AutoScan $w]]
}

# Deletes all the items inside the canvas subwidget and reset the IconList's
# state.
#
proc ratIconList_DeleteAll {w} {
    upvar #0 $w data
    upvar #0 ${w}_itemList itemList

    $data(canvas) delete all
    catch {unset data(selected)}
    catch {unset data(rect)}
    catch {unset data(list)}
    catch {unset itemList}
    set data(maxIW) 1
    set data(maxIH) 1
    set data(maxTW) 1
    set data(maxTH) 1
    set data(numItems) 0
    set data(curItem)  {}
    set data(noScroll) 1
    $data(sbar) set 0.0 1.0
    $data(canvas) xview moveto 0
}

# Adds an icon into the IconList with the designated image and text
#
proc ratIconList_Add {w image text} {
    upvar #0 $w data
    upvar #0 ${w}_itemList itemList
    upvar #0 ${w}_textList textList

    set iTag [$data(canvas) create image 0 0 -image $image -anchor nw]
    set tTag [$data(canvas) create text  0 0 -text  $text  -anchor nw \
	-font $data(font)]
    set rTag [$data(canvas) create rect  0 0 0 0 -fill "" -outline ""]
    
    set b [$data(canvas) bbox $iTag]
    set iW [expr {[lindex $b 2]-[lindex $b 0]}]
    set iH [expr {[lindex $b 3]-[lindex $b 1]}]
    if {$data(maxIW) < $iW} {
	set data(maxIW) $iW
    }
    if {$data(maxIH) < $iH} {
	set data(maxIH) $iH
    }
    
    set b [$data(canvas) bbox $tTag]
    set tW [expr {[lindex $b 2]-[lindex $b 0]}]
    set tH [expr {[lindex $b 3]-[lindex $b 1]}]
    if {$data(maxTW) < $tW} {
	set data(maxTW) $tW
    }
    if {$data(maxTH) < $tH} {
	set data(maxTH) $tH
    }
    
    lappend data(list) [list $iTag $tTag $rTag $iW $iH $tW $tH $data(numItems)]
    set itemList($rTag) [list $iTag $tTag $text $data(numItems)]
    set textList($data(numItems)) [string tolower $text]
    incr data(numItems)
}

# Places the icons in a column-major arrangement.
#
proc ratIconList_Arrange {w} {
    upvar #0 $w data

    if {![info exists data(list)]} {
	if {[info exists data(canvas)] && [winfo exists $data(canvas)]} {
	    set data(noScroll) 1
	    $data(sbar) config -command ""
	}
	return
    }

    set W [winfo width  $data(canvas)]
    set H [winfo height $data(canvas)]
    set pad [expr {[$data(canvas) cget -highlightthickness] + \
	    [$data(canvas) cget -bd]}]
    if {$pad < 2} {
	set pad 2
    }

    incr W -[expr {$pad*2}]
    incr H -[expr {$pad*2}]

    set dx [expr {$data(maxIW) + $data(maxTW) + 8}]
    if {$data(maxTH) > $data(maxIH)} {
	set dy $data(maxTH)
    } else {
	set dy $data(maxIH)
    }
    incr dy 2
    set shift [expr {$data(maxIW) + 4}]

    set x [expr {$pad * 2}]
    set y [expr {$pad * 1}] ; # Why * 1 ?
    set usedColumn 0
    foreach sublist $data(list) {
	set usedColumn 1
	set iTag [lindex $sublist 0]
	set tTag [lindex $sublist 1]
	set rTag [lindex $sublist 2]
	set iW   [lindex $sublist 3]
	set iH   [lindex $sublist 4]
	set tW   [lindex $sublist 5]
	set tH   [lindex $sublist 6]

	set i_dy [expr {($dy - $iH)/2}]
	set t_dy [expr {($dy - $tH)/2}]

	$data(canvas) coords $iTag $x                    [expr {$y + $i_dy}]
	$data(canvas) coords $tTag [expr {$x + $shift}]  [expr {$y + $t_dy}]
	$data(canvas) coords $tTag [expr {$x + $shift}]  [expr {$y + $t_dy}]
	$data(canvas) coords $rTag $x $y [expr {$x+$dx}] [expr {$y+$dy}]

	incr y $dy
	if {($y + $dy) > $H} {
	    set y [expr {$pad * 1}] ; # *1 ?
	    incr x $dx
	    set usedColumn 0
	}
    }

    if {$usedColumn} {
	set sW [expr {$x + $dx}]
    } else {
	set sW $x
    }

    if {$sW < $W} {
	$data(canvas) config -scrollregion [list $pad $pad $sW $H]
	$data(sbar) config -command ""
	$data(canvas) xview moveto 0
	set data(noScroll) 1
    } else {
	$data(canvas) config -scrollregion [list $pad $pad $sW $H]
	$data(sbar) config -command [list $data(canvas) xview]
	set data(noScroll) 0
    }

    set data(itemsPerColumn) [expr {($H-$pad)/$dy}]
    if {$data(itemsPerColumn) < 1} {
	set data(itemsPerColumn) 1
    }

    if {$data(curItem) != ""} {
	ratIconList_Select $w [lindex [lindex $data(list) $data(curItem)] 2] 0
    }
}

# Gets called when the user invokes the IconList (usually by double-clicking
# or pressing the Return key).
#
proc ratIconList_Invoke {w} {
    upvar #0 $w data

    if {$data(-command) != "" && [info exists data(selected)]} {
	uplevel #0 $data(-command)
    }
}

# ratIconList_See --
#
#	If the item is not (completely) visible, scroll the canvas so that
#	it becomes visible.
proc ratIconList_See {w rTag} {
    upvar #0 $w data
    upvar #0 ${w}_itemList itemList

    if {$data(noScroll)} {
	return
    }
    set sRegion [$data(canvas) cget -scrollregion]
    if {[string equal $sRegion {}]} {
	return
    }

    if {![info exists itemList($rTag)]} {
	return
    }


    set bbox [$data(canvas) bbox $rTag]
    set pad [expr {[$data(canvas) cget -highlightthickness] + \
	    [$data(canvas) cget -bd]}]

    set x1 [lindex $bbox 0]
    set x2 [lindex $bbox 2]
    incr x1 -[expr {$pad * 2}]
    incr x2 -[expr {$pad * 1}] ; # *1 ?

    set cW [expr {[winfo width $data(canvas)] - $pad*2}]

    set scrollW [expr {[lindex $sRegion 2]-[lindex $sRegion 0]+1}]
    set dispX [expr {int([lindex [$data(canvas) xview] 0]*$scrollW)}]
    set oldDispX $dispX

    # check if out of the right edge
    #
    if {($x2 - $dispX) >= $cW} {
	set dispX [expr {$x2 - $cW}]
    }
    # check if out of the left edge
    #
    if {($x1 - $dispX) < 0} {
	set dispX $x1
    }

    if {$oldDispX != $dispX} {
	set fraction [expr {double($dispX)/double($scrollW)}]
	$data(canvas) xview moveto $fraction
    }
}

proc ratIconList_SelectAtXY {w x y} {
    upvar #0 $w data

    ratIconList_Select $w [$data(canvas) find closest \
	    [$data(canvas) canvasx $x] [$data(canvas) canvasy $y]]
}

proc ratIconList_Select {w rTag {callBrowse 1}} {
    upvar #0 $w data
    upvar #0 ${w}_itemList itemList

    if {![info exists itemList($rTag)]} {
	return
    }
    set iTag   [lindex $itemList($rTag) 0]
    set tTag   [lindex $itemList($rTag) 1]
    set text   [lindex $itemList($rTag) 2]
    set serial [lindex $itemList($rTag) 3]

    if {![info exists data(rect)]} {
        set data(rect) [$data(canvas) create rect 0 0 0 0 \
		-fill #a0a0ff -outline #a0a0ff]
    }
    $data(canvas) lower $data(rect)
    set bbox [$data(canvas) bbox $tTag]
    eval [list $data(canvas) coords $data(rect)] $bbox

    set data(curItem) $serial
    set data(selected) $text

    if {$callBrowse && $data(-browsecmd) != ""} {
	eval $data(-browsecmd) [list $text]
    }
}

proc ratIconList_Unselect {w} {
    upvar #0 $w data

    if {[info exists data(rect)]} {
	$data(canvas) delete $data(rect)
	unset data(rect)
    }
    if {[info exists data(selected)]} {
	unset data(selected)
    }
    #set data(curItem)  {}
}

# Returns the selected item
#
proc ratIconList_Get {w} {
    upvar #0 $w data

    if {[info exists data(selected)]} {
	return $data(selected)
    } else {
	return ""
    }
}


proc ratIconList_Btn1 {w x y} {
    upvar #0 $w data

    focus $data(canvas)
    ratIconList_SelectAtXY $w $x $y
}

# Gets called on button-1 motions
#
proc ratIconList_Motion1 {w x y} {
    global ratPriv
    set ratPriv(x) $x
    set ratPriv(y) $y

    ratIconList_SelectAtXY $w $x $y
}

proc ratIconList_Double1 {w x y} {
    upvar #0 $w data

    if {[string compare $data(curItem) {}]} {
	ratIconList_Invoke $w
    }
}

proc ratIconList_ReturnKey {w} {
    ratIconList_Invoke $w
}

proc ratIconList_Leave1 {w x y} {
    global ratPriv

    set ratPriv(x) $x
    set ratPriv(y) $y
    ratIconList_AutoScan $w
}

proc ratIconList_FocusIn {w} {
    upvar #0 $w data

    if {![info exists data(list)]} {
	return
    }

    if {[string compare $data(curItem) {}]} {
	ratIconList_Select $w [lindex [lindex $data(list) $data(curItem)] 2] 1
    }
}

# ratIconList_UpDown --
#
# Moves the active element up or down by one element
#
# Arguments:
# w -		The IconList widget.
# amount -	+1 to move down one item, -1 to move back one item.
#
proc ratIconList_UpDown {w amount} {
    upvar #0 $w data

    if {![info exists data(list)]} {
	return
    }

    if {[string equal $data(curItem) {}]} {
	set rTag [lindex [lindex $data(list) 0] 2]
    } else {
	set oldRTag [lindex [lindex $data(list) $data(curItem)] 2]
	set rTag [lindex [lindex $data(list) [expr {$data(curItem)+$amount}]] 2]
	if {[string equal $rTag ""]} {
	    set rTag $oldRTag
	}
    }

    if {[string compare $rTag ""]} {
	ratIconList_Select $w $rTag
	ratIconList_See $w $rTag
    }
}

# ratIconList_LeftRight --
#
# Moves the active element left or right by one column
#
# Arguments:
# w -		The IconList widget.
# amount -	+1 to move right one column, -1 to move left one column.
#
proc ratIconList_LeftRight {w amount} {
    upvar #0 $w data

    if {![info exists data(list)]} {
	return
    }
    if {[string equal $data(curItem) {}]} {
	set rTag [lindex [lindex $data(list) 0] 2]
    } else {
	set oldRTag [lindex [lindex $data(list) $data(curItem)] 2]
	set newItem [expr {$data(curItem)+($amount*$data(itemsPerColumn))}]
	set rTag [lindex [lindex $data(list) $newItem] 2]
	if {[string equal $rTag ""]} {
	    set rTag $oldRTag
	}
    }

    if {[string compare $rTag ""]} {
	ratIconList_Select $w $rTag
	ratIconList_See $w $rTag
    }
}

#----------------------------------------------------------------------
#		Accelerator key bindings
#----------------------------------------------------------------------

# ratIconList_KeyPress --
#
#	Gets called when user enters an arbitrary key in the listbox.
#
proc ratIconList_KeyPress {w key} {
    global ratPriv

    append ratPriv(ILAccel,$w) $key
    ratIconList_Goto $w $ratPriv(ILAccel,$w)
    catch {
	after cancel $ratPriv(ILAccel,$w,afterId)
    }
    set ratPriv(ILAccel,$w,afterId) [after 500 [list ratIconList_Reset $w]]
}

proc ratIconList_Goto {w text} {
    upvar #0 $w data
    upvar #0 ${w}_textList textList
    global ratPriv
    
    if {![info exists data(list)]} {
	return
    }

    if {[string equal {} $text]} {
	return
    }

    if {$data(curItem) == "" || $data(curItem) == 0} {
	set start  0
    } else {
	set start  $data(curItem)
    }

    set text [string tolower $text]
    set theIndex -1
    set less 0
    set len [string length $text]
    set len0 [expr {$len-1}]
    set i $start

    # Search forward until we find a filename whose prefix is an exact match
    # with $text
    while {1} {
	set sub [string range $textList($i) 0 $len0]
	if {[string equal $text $sub]} {
	    set theIndex $i
	    break
	}
	incr i
	if {$i == $data(numItems)} {
	    set i 0
	}
	if {$i == $start} {
	    break
	}
    }

    if {$theIndex > -1} {
	set rTag [lindex [lindex $data(list) $theIndex] 2]
	ratIconList_Select $w $rTag
	ratIconList_See $w $rTag
    }
}

proc ratIconList_Reset {w} {
    global ratPriv

    catch {unset ratPriv(ILAccel,$w)}
}

#----------------------------------------------------------------------
#
#		      F I L E   D I A L O G
#
#----------------------------------------------------------------------

# rat_fbox::run --
#
#	Implements the TK file selection dialog. This dialog is used when
#	the tk_strictMotif flag is set to false. This procedure shouldn't
#	be called directly. Call tk_getOpenFile or tk_getSaveFile instead.
#
# Arguments:
#	args		Options parsed by the procedure.
#

proc rat_fbox::run {args} {
    variable state
    set dataName __rat_filedialog
    upvar #0 $dataName data

    rat_fbox::config $dataName $args

    if {![string compare $data(-parent) .]} {
        set w .$dataName
    } else {
        set w $data(-parent).$dataName
    }

    # (re)create the dialog box if necessary
    #
    if {![winfo exists $w]} {
	set data(showDotfiles) 0
	rat_fbox::create $w
    } else {
	set data(dirMenuBtn) $w.f1.menu
	set data(dirMenu) $w.f1.menu.menu
	set data(upBtn) $w.f1.up
	set data(icons) $w.icons
	set data(ent) $w.f2.ent
	set data(rclBtn) $w.f2.rcl
	set data(prevMenuLab) $w.f3.lab
	set data(prevMenuBtn) $w.f3.menu
	set data(prevMenu) $w.f3.menu.menu
	set data(okBtn) $w.f4.ok
	set data(cancelBtn) $w.f4.cancel
    }

    # Initialize recall button
    #
    set data(storedFile) $data(selectFile)
    if {"" == $data(storedFile)} {
	$data(rclBtn) config -state disabled
    } else {
	$data(rclBtn) config -state normal
    }

    # Update previous directories menu
    #
    variable previous
    $data(prevMenu) delete 0 end
    if {[llength $previous]} {
	set var [format %s(selectPath) $dataName]
	foreach path $previous {
	    $data(prevMenu) add command -label $path \
		    -command [list set $var $path]
	}
	set data(prevPath) [lindex $previous 0]
	$data(prevMenuBtn) config -state normal
	$data(prevMenuLab) config -state normal
    } else {
	set data(prevPath) ""
	$data(prevMenuBtn) config -state disabled
	$data(prevMenuLab) config -state disabled
    }

    rat_fbox::updateWhenIdle $w

    # Withdraw the window, then update all the geometry information
    # so we know how big it wants to be, then center the window in the
    # display and de-iconify it.

    wm withdraw $w
    update idletasks
    set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
	    - [winfo vrootx [winfo parent $w]]}]
    set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
	    - [winfo vrooty [winfo parent $w]]}]
    wm geom $w [winfo reqwidth $w]x[winfo reqheight $w]+$x+$y
    wm title $w $data(-title)
    wm transient $w $data(-parent)
    wm deiconify $w

    # Set a grab and claim the focus too.

    ModalGrab $w $data(ent)
    $data(ent) delete 0 end
    $data(ent) insert 0 $data(selectFile)
    $data(ent) select from 0
    $data(ent) select to   end
    $data(ent) icursor end

    trace variable data(selectPath) w "rat_fbox::setPath $w"

    # Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    tkwait variable rat_fbox::state(selectFilePath)
    if { "" != $state(selectFilePath)} {
        set dir [file dirname $state(selectFilePath)]
        set i [lsearch -exact $previous $dir]
        if {-1 == $i} {
	    set previous [lrange [linsert $previous 0 $dir] 0 10]
        } else {
	    set previous [lrange \
		    [linsert [lreplace $previous $i $i] 0 $dir] 0 10]
	}
    }
    destroy $w

    return $state(selectFilePath)
}

# rat_fbox::config --
#
#	Configures the TK filedialog according to the argument list
#
proc rat_fbox::config {dataName argList} {
    upvar #0 $dataName data
    global env tklead

    # 0: Delete all variable that were set on data(selectPath) the
    # last time the file dialog is used. The traces may cause troubles
    # if the dialog is now used with a different -parent option.

    foreach trace [trace vinfo data(selectPath)] {
	trace vdelete data(selectPath) [lindex $trace 0] [lindex $trace 1]
    }

    # 0.1: Find a good starting directory
    if {[catch {pwd} start_dir]} {
	foreach d [list $env(HOME) /] {
	    if {![catch "cd $d; pwd" start_dir]} {
		break
	    }
	}
	
    }

    # 1: the configuration specs
    #
    set specs {
	{-defaultextension "" "" ""}
	{-filetypes "" "" ""}
	{-initialdir "" "" ""}
	{-initialfile "" "" ""}
	{-parent "" "" "."}
	{-title "" "" ""}
	{-ok "" "" ""}
	{-mode "" "" ""}
    }

    # 2: default values depending on the type of the dialog
    #
    if {![info exists data(selectPath)]} {
	# first time the dialog has been popped up
	set data(selectPath) $start_dir
	set data(selectFile) ""
    }

    # 3: parse the arguments
    #
    tclParseConfigSpec $dataName $specs "" $argList

    if {![string compare $data(-title) ""]} {
	if {![string compare $data(-mode) "open"]} {
	    set data(-title) "Open"
	} else {
	    set data(-title) "Save As"
	}
    }

    # 4: set the default directory and selection according to the -initial
    #    settings
    #
    if {[string compare $data(-initialdir) ""]} {
	if {[file isdirectory $data(-initialdir)]} {
	    set data(selectPath) [lindex [glob $data(-initialdir)] 0]
	} else {
	    set data(selectPath) $start_dir
	}

	# Convert the initialdir to an absolute path name.

	set old $start_dir
	cd $data(selectPath)
	set data(selectPath) $start_dir
	cd $old
    }
    set data(selectFile) $data(-initialfile)

    # 5. Parse the -filetypes option
    #
    set data(-filetypes) [${tklead}FDGetFileTypes $data(-filetypes)]

    if {![winfo exists $data(-parent)]} {
	error "bad window path name \"$data(-parent)\""
    }
}

proc rat_fbox::create {w} {
    set dataName [lindex [split $w .] end]
    upvar #0 $dataName data
    global tk_library t tklead
    variable state

    toplevel $w -class TkRat

    # f1: the frame with the directory option menu
    #
    set f1 [frame $w.f1]
    label $f1.lab -text $t(directory): -under 0
    set data(dirMenuBtn) $f1.menu
    set data(dirMenu) [tk_optionMenu $f1.menu [format %s(selectPath) $dataName] ""]
    $f1.menu configure -width 25
    checkbutton $f1.dot -text . -variable ${dataName}(showDotfiles) \
		-command "rat_fbox::updateWhenIdle $w"
    set data(upBtn) [button $f1.up]
    if {![info exists state(updirImage)]} {
	set state(updirImage) [image create bitmap -data {
#define updir_width 28
#define updir_height 16
static char updir_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x80, 0x1f, 0x00, 0x00, 0x40, 0x20, 0x00, 0x00,
   0x20, 0x40, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x01, 0x10, 0x00, 0x00, 0x01,
   0x10, 0x02, 0x00, 0x01, 0x10, 0x07, 0x00, 0x01, 0x90, 0x0f, 0x00, 0x01,
   0x10, 0x02, 0x00, 0x01, 0x10, 0x02, 0x00, 0x01, 0x10, 0x02, 0x00, 0x01,
   0x10, 0xfe, 0x07, 0x01, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x01,
   0xf0, 0xff, 0xff, 0x01};}]
    }
    $data(upBtn) config -image $state(updirImage)

    $f1.menu config -takefocus 1 -highlightthickness 2
 
    pack $data(upBtn) -side right -padx 4 -fill both
    pack $f1.dot -side right -padx 4 -fill both
    pack $f1.lab -side left -padx 4 -fill both
    pack $f1.menu -expand yes -fill both -padx 4

    # data(icons): the IconList that list the files and directories.
    #
    set data(icons) [ratIconList $w.icons \
	-browsecmd "rat_fbox::listBrowse $w" \
	-command   "rat_fbox::okCmd $w iconList"]

    # f2: the frame with the recall button and the "file name" field
    #
    set f2 [frame $w.f2 -bd 0]
    label $f2.lab -text $t(filename): -anchor e -width 14 -pady 0
    set data(ent) [entry $f2.ent]
    set data(rclBtn) [button $f2.rcl -text $t(recall) -under 0 -width 8 ]
    pack $f2.rcl -side right
    pack $f2.lab -side left
    pack $f2.ent -expand yes -fill x -pady 0

    # The font to use for the icons. The default Canvas font on Unix
    # is just deviant.
    upvar #0 $w.icons(font) font
    set font [$data(ent) cget -font]

    # f3: the frame with the Previous directory field
    #
    set f3 [frame $w.f3 -bd 0]

    set data(prevMenuLab) [button $f3.lab -text $t(previous): \
        -anchor e -width 14 \
        -bd [$f2.lab cget -bd] \
        -highlightthickness [$f2.lab cget -highlightthickness] \
        -relief [$f2.lab cget -relief] \
        -padx 0 ]
    bindtags $data(prevMenuLab) [list $data(prevMenuLab) Label \
            [winfo toplevel $data(prevMenuLab)] all]
    set data(prevMenuBtn) $f3.menu
    set data(prevMenu) \
	    [tk_optionMenu $f3.menu [format %s(prevPath) $dataName] ""]
    $f3.menu config -width 25 -takefocus 1 -highlightthickness 2

    # The "File of types:" label needs to be grayed-out when
    # -filetypes are not specified. The label widget does not support
    # grayed-out text on monochrome displays. Therefore, we have to
    # use a button widget to emulate a label widget (by setting its
    # bindtags)

    pack $data(prevMenuLab) -side left
    pack $data(prevMenuBtn) -expand yes -fill x -side right

    # f4: the frame with the buttons
    set f4 [frame $w.f4 -bd 0]
    set data(okBtn)     [button $f4.ok     -text $t(ok)     -under 0 -width 6 \
	-default active -pady 3]
    set data(cancelBtn) [button $f4.cancel -text $t(cancel) -under 0 -width 6\
	-default normal -pady 3]
    pack $data(okBtn) -side left -padx 20
    pack $data(cancelBtn) -side right -padx 20

    # Pack all the frames together. We are done with widget construction.
    #
    pack $f1 -side top -fill x -pady 4
    pack $f4 -side bottom -fill x -padx 40
    pack $f3 -side bottom -fill x -padx 3
    pack $f2 -side bottom -fill x -padx 3
    pack $data(icons) -expand yes -fill both -padx 4 -pady 1

    # Set up the event handlers
    #
    bind $data(ent) <Return>  "rat_fbox::activateEnt $w return; break"
    
    $data(upBtn)     config -command "rat_fbox::upDirCmd $w"
    $data(okBtn)     config -command "rat_fbox::okCmd $w okBtn"
    $data(cancelBtn) config -command "rat_fbox::cancelCmd $w"
    $data(rclBtn)    config -command "rat_fbox::rclCmd $w"

    bind $w <Alt-d> "focus $data(dirMenuBtn)"
    bind $w <Alt-n> "focus $data(ent)"
    bind $w <KeyPress-Escape> "${tklead}ButtonInvoke $data(cancelBtn)"
    bind $w <Alt-c> "${tklead}ButtonInvoke $data(cancelBtn)"
    bind $w <Alt-o> "rat_fbox::invokeBtn $w Open"
    bind $w <Alt-s> "rat_fbox::invokeBtn $w Save"

    wm protocol $w WM_DELETE_WINDOW "rat_fbox::cancelCmd $w"

    # Build the focus group for all the entries
    #
    ${tklead}FocusGroup_Create $w
    ${tklead}FocusGroup_BindIn $w  $data(ent) "rat_fbox::entFocusIn $w"
    ${tklead}FocusGroup_BindOut $w $data(ent) "rat_fbox::entFocusOut $w"
}

# rat_fbox::updateWhenIdle --
#
#	Creates an idle event handler which updates the dialog in idle
#	time. This is important because loading the directory may take a long
#	time and we don't want to load the same directory for multiple times
#	due to multiple concurrent events.
#
proc rat_fbox::updateWhenIdle {w} {
    upvar #0 [winfo name $w] data

    if {[info exists data(updateId)]} {
	return
    } else {
	set data(updateId) [after idle rat_fbox::doUpdate $w]
    }
}

# rat_fbox::doUpdate --
#
#	Loads the files and directories into the IconList widget. Also
#	sets up the directory option menu for quick access to parent
#	directories.
#
proc rat_fbox::doUpdate {w} {

    # This proc may be called within an idle handler. Make sure that the
    # window has not been destroyed before this proc is called
    if {![winfo exists $w]} {
	return
    }

    set dataName [winfo name $w]
    upvar #0 $dataName data
    global tk_library 
    variable state
    catch {unset data(updateId)}

    if {![info exists state(folderImage)]} {
	set state(folderImage) [image create photo -data {
R0lGODlhEAAMAKEAAAD//wAAAPD/gAAAACH5BAEAAAAALAAAAAAQAAwAAAIghINhyycvVFsB
QtmS3rjaH1Hg141WaT5ouprt2HHcUgAAOw==}]
	set state(fileImage)   [image create photo -data {
R0lGODlhDAAMAKEAALLA3AAAAP//8wAAACH5BAEAAAAALAAAAAAMAAwAAAIgRI4Ha+IfWHsO
rSASvJTGhnhcV3EJlo3kh53ltF5nAhQAOw==}]
    }
    set folder $state(folderImage)
    set file   $state(fileImage)

    if {[catch {
	set appPWD [pwd]
	cd $data(selectPath)
    } msg]} {
	# We cannot change directory to $data(selectPath). $data(selectPath)
	# should have been checked before rat_fbox::doUpdate is called, so
	# we normally won't come to here. Anyways, give an error and abort
	# action.
	tk_messageBox -type ok -parent $data(-parent) -message $msg \
	    -icon warning
	cd $appPWD
	return
    }

    # Turn on the busy cursor. BUG?? We haven't disabled X events, though,
    # so the user may still click and cause havoc ...
    #
    set entCursor [$data(ent) cget -cursor]
    set dlgCursor [$w         cget -cursor]
    $data(ent) config -cursor watch
    $w         config -cursor watch
    update idletasks
    
    ratIconList_DeleteAll $data(icons)

    # Make the dir list
    #
    if {$data(showDotfiles)} {
	set fl [glob -nocomplain .* *]
    } else {
	set fl [glob -nocomplain *]
    }
    foreach f [lsort -dictionary $fl] {
	if {![string compare $f .]} {
	    continue
	}
	if {![string compare $f ..]} {
	    continue
	}
	if {[file isdirectory ./$f]} {
	    if {![info exists hasDoneDir($f)]} {
		ratIconList_Add $data(icons) $folder $f
		set hasDoneDir($f) 1
	    }
	}
    }
    # Make the file list
    #
    set files [lsort -dictionary $fl]

    set top 0
    foreach f $files {
	if {![file isdirectory ./$f]} {
	    if {![info exists hasDoneFile($f)]} {
		ratIconList_Add $data(icons) $file $f
		set hasDoneFile($f) 1
	    }
	}
    }

    ratIconList_Arrange $data(icons)

    # Update the Directory: option menu
    #
    set list ""
    set dir ""
    foreach subdir [file split $data(selectPath)] {
	set dir [file join $dir $subdir]
	lappend list $dir
    }

    $data(dirMenu) delete 0 end
    set var [format %s(selectPath) $dataName]
    foreach path $list {
	$data(dirMenu) add command -label $path -command [list set $var $path]
    }

    # Restore the PWD to the application's PWD
    #
    cd $appPWD

    # Restore the Open/Save Button
    #
    $data(okBtn) config -text $data(-ok)

    # turn off the busy cursor.
    #
    $data(ent) config -cursor $entCursor
    $w         config -cursor $dlgCursor
}

# rat_fbox::setPathSilently --
#
# 	Sets data(selectPath) without invoking the trace procedure
#
proc rat_fbox::setPathSilently {w path} {
    upvar #0 [winfo name $w] data
    
    trace vdelete  data(selectPath) w "rat_fbox::setPath $w"
    set data(selectPath) $path
    trace variable data(selectPath) w "rat_fbox::setPath $w"
}


# This proc gets called whenever data(selectPath) is set
#
proc rat_fbox::setPath {w name1 name2 op} {
    if {[winfo exists $w]} {
	upvar #0 [winfo name $w] data
	set data(selectPath) $data($name2)
	rat_fbox::updateWhenIdle $w
    }
}


# rat_fbox::resolveFile --
#
#	Interpret the user's text input in a file selection dialog.
#	Performs:
#
#	(1) ~ substitution
#	(2) resolve all instances of . and ..
#	(3) check for non-existent files/directories
#	(4) check for chdir permissions
#
# Arguments:
#	context:  the current directory you are in
#	text:	  the text entered by the user
#	defaultext: the default extension to add to files with no extension
#
# Return vaue:
#	[list $flag $directory $file]
#
#	 flag = OK	: valid input
#	      = PATH	: the directory does not exist
#	      = FILE	: the directory exists by the file doesn't
#			  exist
#	      = CHDIR	: Cannot change to the directory
#	      = ERROR	: Invalid entry
#
#	 directory      : valid only if flag = OK or FILE
#	 file           : valid only if flag = OK
#
#	directory may not be the same as context, because text may contain
#	a subdirectory name
#
proc rat_fbox::resolveFile {context text defaultext} {

    set appPWD [pwd]

    set path [rat_fbox::joinFile $context $text]

    if {[file extension $path] == ""} {
	set path "$path$defaultext"
    }


    if {[catch {file exists $path}]} {
	# This "if" block can be safely removed if the following code
	# stop generating errors.
	#
	#	file exists ~nonsuchuser
	#
	return [list ERROR $path ""]
    }

    if {[file exists $path]} {
	if {[file isdirectory $path]} {
	    if {[catch {
		cd $path
	    }]} {
		return [list CHDIR $path ""]
	    }
	    set directory [pwd]
	    set file ""
	    set flag OK
	    cd $appPWD
	} else {
	    if {[catch {
		cd [file dirname $path]
	    }]} {
		return [list CHDIR [file dirname $path] ""]
	    }
	    set directory [pwd]
	    set file [file tail $path]
	    set flag OK
	    cd $appPWD
	}
    } else {
	set dirname [file dirname $path]
	if {[file exists $dirname]} {
	    if {[catch {
		cd $dirname
	    }]} {
		return [list CHDIR $dirname ""]
	    }
	    set directory [pwd]
	    set file [file tail $path]
	    set flag FILE
	    cd $appPWD
	} else {
	    set directory $dirname
	    set file [file tail $path]
	    set flag PATH
	}
    }

    return [list $flag $directory $file]
}


# Gets called when the entry box gets keyboard focus. We clear the selection
# from the icon list . This way the user can be certain that the input in the 
# entry box is the selection.
#
proc rat_fbox::entFocusIn {w} {
    upvar #0 [winfo name $w] data

    if {[string compare [$data(ent) get] ""]} {
	$data(ent) selection from 0
	$data(ent) selection to   end
	$data(ent) icursor end
    } else {
	$data(ent) selection clear
    }

    ratIconList_Unselect $data(icons)

    $data(okBtn) config -text $data(-ok)
}

proc rat_fbox::entFocusOut {w} {
    upvar #0 [winfo name $w] data

    $data(ent) selection clear
}


# Gets called when user presses Return in the "File name" entry.
#
proc rat_fbox::activateEnt {w from} {
    upvar #0 [winfo name $w] data

    set text [string trim [$data(ent) get]]
    set list [rat_fbox::resolveFile $data(selectPath) $text \
		  $data(-defaultextension)]
    set flag [lindex $list 0]
    set path [lindex $list 1]
    set file [lindex $list 2]

    switch -- $flag {
	OK {
	    if {![string compare $file ""] && "$data(-mode)" != "dirok"} {
		# user has entered an existing (sub)directory
		set data(selectPath) $path
		$data(ent) delete 0 end
	    } else {
		rat_fbox::setPathSilently $w $path
		set data(selectFile) $file
		rat_fbox::done $w
	    }
	}
	FILE {
	    if {![string compare $data(-mode) open]} {
		tk_messageBox -icon warning -type ok -parent $data(-parent) \
		    -message "File \"[file join $path $file]\" does not exist."
		$data(ent) select from 0
		$data(ent) select to   end
		$data(ent) icursor end
	    } else {
		rat_fbox::setPathSilently $w $path
		set data(selectFile) $file
		rat_fbox::done $w
	    }
	}
	PATH {
	    tk_messageBox -icon warning -type ok -parent $data(-parent) \
		-message "Directory \"$path\" does not exist."
	    $data(ent) select from 0
	    $data(ent) select to   end
	    $data(ent) icursor end
	}
	CHDIR {
	    tk_messageBox -type ok -parent $data(-parent) -message \
	       "Cannot change to the directory \"$path\".\nPermission denied."\
		-icon warning
	    $data(ent) select from 0
	    $data(ent) select to   end
	    $data(ent) icursor end
	}
	ERROR {
	    tk_messageBox -type ok -parent $data(-parent) -message \
	       "Invalid file name \"$path\"."\
		-icon warning
	    $data(ent) select from 0
	    $data(ent) select to   end
	    $data(ent) icursor end
	}
    }
}

# Gets called when user presses the Alt-s or Alt-o keys.
#
proc rat_fbox::invokeBtn {w key} {
    upvar #0 [winfo name $w] data
    global tklead

    if {![string compare [$data(okBtn) cget -text] $key]} {
	${tklead}ButtonInvoke $data(okBtn)
    }
}

# Gets called when user presses the "parent directory" button
#
proc rat_fbox::upDirCmd {w} {
    upvar #0 [winfo name $w] data

    if {[string compare $data(selectPath) "/"]} {
	set data(selectPath) [file dirname $data(selectPath)]
    }
}

# Gets called when user presses the "recall" button
#
proc rat_fbox::rclCmd {w} {
    upvar #0 [winfo name $w] data

    $data(ent) delete 0 end
    $data(ent) insert 0 $data(storedFile)
}

# Join a file name to a path name. The "file join" command will break
# if the filename begins with ~
#
proc rat_fbox::joinFile {path file} {
    if {[string match {~*} $file] && [file exists $path/$file]} {
	return [file join $path ./$file]
    } else {
	return [file join $path $file]
    }
}



# Gets called when user presses the "OK" button
#
proc rat_fbox::okCmd {w from} {
    upvar #0 [winfo name $w] data

    set text [ratIconList_Get $data(icons)]
    if {[string compare $text ""]} {
	set file [rat_fbox::joinFile $data(selectPath) $text]
	if {[file isdirectory $file] && ([string compare $data(-mode) dirok] ||
		![string compare $from iconList])} {
	    rat_fbox::listInvoke $w $text
	    return
	}
    }

    rat_fbox::activateEnt $w $from
}

# Gets called when user presses the "Cancel" button
#
proc rat_fbox::cancelCmd {w} {
    upvar #0 [winfo name $w] data
    variable state

    set state(selectFilePath) ""
}

# Gets called when user browses the IconList widget (dragging mouse, arrow
# keys, etc)
#
proc rat_fbox::listBrowse {w text} {
    upvar #0 [winfo name $w] data
    global t

    if {$text == ""} {
	return
    }

    set file [rat_fbox::joinFile $data(selectPath) $text]
    if {![file isdirectory $file]} {
	$data(ent) delete 0 end
	$data(ent) insert 0 $text

	$data(okBtn) config -text $data(-ok)
    } else {
	$data(okBtn) config -text $t(open)
    }
}

# Gets called when user invokes the IconList widget (double-click, 
# Return key, etc)
#
proc rat_fbox::listInvoke {w text} {
    upvar #0 [winfo name $w] data

    if {$text == ""} {
	return
    }

    set file [rat_fbox::joinFile $data(selectPath) $text]

    if {[file isdirectory $file]} {
	if {[catch {set appPWD [pwd]; cd $file} msg]} {
	    tk_messageBox -type ok -parent $data(-parent) -message $msg \
		-icon warning
	} else {
	    cd $appPWD
	    set data(selectPath) $file
	}
    } else {
	set data(selectFile) $file
	rat_fbox::done $w
    }
}

# rat_fbox::done --
#
#	Gets called when user has input a valid filename.  Pops up a
#	dialog box to confirm selection when necessary. Sets the
#	state(selectFilePath) variable, which will break the "tkwait"
#	loop in the dialog and return the selected filename to the
#	script that calls tk_getOpenFile or tk_getSaveFile
#
proc rat_fbox::done {w {selectFilePath ""}} {
    upvar #0 [winfo name $w] data
    variable state

    if {![string compare $selectFilePath ""]} {
	set selectFilePath [rat_fbox::joinFile $data(selectPath) \
		$data(selectFile)]
	set state(selectFile)     $data(selectFile)
	set state(selectPath)     $data(selectPath)

	if {[file exists $selectFilePath] && 
	    ![string compare $data(-mode) save]} {

		set reply [tk_messageBox -icon warning -type yesno\
			-parent $data(-parent) -message "File\
			\"$selectFilePath\" already exists.\nDo\
			you want to overwrite it?"]
		if {![string compare $reply "no"]} {
		    return
		}
	}
    }
    set state(selectFilePath) $selectFilePath
}

