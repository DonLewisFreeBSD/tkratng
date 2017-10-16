# exp.tcl --
#
# This file contains code for handling message selection expressions
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of my legal notices is contained in the file called
#  COPYRIGHT, included with this distribution.

# ExpCreate --
#
# Create the expression creation window
#
# Arguments:
# handler	- The handler which identifies the folder window that
#		  the selection should be done in.

proc ExpCreate {handler {addproc {}}} {
    global t b option idCnt

    # Create identifier
    set id expWin[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(doSave) 0
    set hd(w) $w
    set hd(handler) $handler
    set hd(op) and
    set hd(exp) {}
    set hd(oldfocus) [focus]
    set hd(addproc) $addproc

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(create_exp)

    # Populate window
    frame $w.top
    menubutton $w.top.mode -indicatoron 1 -menu $w.top.mode.m \
	    -textvariable ${id}(modeName) -bd 1 -relief raised -width 18
    menu $w.top.mode.m -tearoff 0
    $w.top.mode.m add command -label $t(basic_mode) -command "ExpModeBasic $id"
    $w.top.mode.m add command -label $t(advanced_mode) -command "ExpModeAdv $id"
    frame $w.top.c
    checkbutton $w.top.c.but -text $t(save_as): -variable ${id}(doSave)
    entry $w.top.c.entry -width 20 -textvariable ${id}(saveAs)
    bind $w.top.c.entry <KeyRelease> \
	    "if {0 < \[string length ${id}(saveAs)\]} { \
		 set ${id}(doSave) 1 \
	     } else { \
		 set ${id}(doSave) 0 \
	     }"
    pack $w.top.c.but \
	 $w.top.c.entry -side left
    pack $w.top.mode \
	 $w.top.c -side left -padx 20
    set b($w.top.mode) switch_expression
    set b($w.top.c.but) save_expr_as
    set b($w.top.c.entry) save_expr_as

    frame $w.f -bd 2 -relief ridge
    set hd(frame) $w.f

    frame $w.buttons
    button $w.buttons.ok -text $t(ok) -default active -command "ExpDone $id 1"
    button $w.buttons.clear -text $t(clear) -command "ExpClear $id"
    button $w.buttons.cancel -text $t(cancel) -command "destroy $w"
    pack $w.buttons.ok \
	 $w.buttons.clear \
	 $w.buttons.cancel -side left -expand 1
    bind $w <Return> "ExpDone $id 1"
    pack $w.top -side top -fill x -pady 5
    pack $w.f -side top -fill both -pady 5 -expand 1
    pack $w.buttons -side top -fill x -pady 5
    set b($w.buttons.ok) exp_ok
    set b($w.buttons.clear) clear
    set b($w.buttons.cancel) dismiss

    set hd(mode) {}
    set hd(action) create
    if {[string compare advanced $option(expression_mode)]} {
	ExpModeBasic $id
    } else {
	ExpModeAdv $id
    }
    bind $w.buttons.ok <Destroy> "ExpClose $id"
    ::tkrat::winctl::SetGeometry ratExpression $w
}

# ExpEdit --
#
# Edit an expression
#
# Arguments:
# name	- Name of expression to edit

proc ExpEdit {name namechange} {
    global t b idCnt expExp

    # Create identifier
    set id expWin[incr idCnt]
    set w .$id
    upvar #0 $id hd
    set hd(w) $w
    set hd(handler) $id
    set hd(mode) {}
    set hd(frame) $w.f
    set hd(action) edit
    set hd(exp) "$expExp($name) "
    set hd(saveAs) $name
    set hd(oldName) $name

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(edit_exp)
    frame $w.f

    ExpModeAdv $id
    OkButtons $w $t(ok) $t(cancel) "ExpDone $id"

    pack $w.buttons -side bottom -pady 5 -fill x
    pack $w.f

    bind $w.buttons.ok <Destroy> "ExpClose $id"
    ::tkrat::winctl::SetGeometry ratExpEdit $w
}

# ExpModeBasic --
#
# Configure window for basic mode
#
# Arguments:
# handler -	The handler which identifies the expression window

proc ExpModeBasic {handler} {
    upvar #0 $handler hd
    global t b

    # Check that this is really neccessary
    if {![string compare $hd(mode) basic]} {
	return
    }

    # Setup variables
    set hd(modeName) $t(basic_mode)
    set hd(mode) basic
    set w $hd(frame)

    # Clear frame
    foreach s [grid slaves $w] {
	destroy $s
    }

    frame $w.f
    radiobutton $w.f.and -text $t(and) -variable ${handler}(op) -value and
    radiobutton $w.f.or -text $t(or) -variable ${handler}(op) -value or
    pack $w.f.and \
	 $w.f.or -side left -padx 5
    grid $w.f -columnspan 2
    set b($w.f.and) exp_basic_and
    set b($w.f.or) exp_basic_or

    foreach f {subject from reply-to sender to cc} {
	label $w.l$f -text $t($f):
	entry $w.e$f -textvariable ${handler}($f) -width 50
	grid $w.l$f $w.e$f -sticky e
	set b($w.l$f) exp_basic_field
	set b($w.e$f) exp_basic_field
    }
    focus $w.esubject
}

# ExpModeAdv --
#
# Configure window for advanced mode
#
# Arguments:
# handler -	The handler which identifies the expression window

proc ExpModeAdv {handler} {
    upvar #0 $handler hd
    global t b

    # Check that this is really neccessary
    if {![string compare $hd(mode) advanced]} {
	return
    }

    # Setup variables
    set hd(modeName) $t(advanced_mode)
    set hd(mode) advanced
    set w $hd(frame)

    # Clear frame
    foreach s [grid slaves $w] {
	destroy $s
    }

    # Pack windows
    frame $w.f
    text $w.f.text -relief sunken -bd 1 -width 80 -height 4 -wrap word \
	    -yscroll "$w.f.scroll set" -setgrid 1
    $w.f.text tag configure error -underline 1
    $w.f.text tag bind error <KeyPress> "$w.f.text tag remove error 1.0 end"
    scrollbar $w.f.scroll -relief sunken -bd 1 -command "$w.f.text yview"
    pack $w.f.scroll -side right -fill y
    pack $w.f.text -expand 1 -fill both
    grid $w.f -columnspan 4 -sticky nsew
    set b($w.f.text) exp_adv_exp

    frame $w.f1
    label $w.f1.l -text $t(fields)
    grid $w.f1.l -sticky we -columnspan 2
    set i 1
    foreach n {to from subject sender cc reply-to size} {
	button $w.f1.f$i -text $t($n) -width 10 \
		-command "$w.f.text insert insert \"$n \""
	set b($w.f1.f$i) exp_adv_fields
	incr i
    }
    grid $w.f1.f1 $w.f1.f5 -row 1
    grid $w.f1.f2 $w.f1.f6 -row 2
    grid $w.f1.f3 $w.f1.f7 -row 3
    grid $w.f1.f4 -row 4

    frame $w.o1
    label $w.o1.l -text $t(operators)
    grid $w.o1.l -sticky we
    set i 1
    foreach n [list "has $t(has) has" "is $t(is) is" "> > gt" "< < lt"] {
	button $w.o1.o$i -text [lindex $n 1] -width 10 \
		-command "$w.f.text insert insert \"[lindex $n 0] \""
	grid $w.o1.o$i -row $i
	set b($w.o1.o$i) exp_adv_[lindex $n 2]
	incr i
    }

    frame $w.b1
    label $w.b1.l -text $t(booleans)
    grid $w.b1.l -sticky we
    set i 1
    foreach n {not and or} {
	button $w.b1.b$i -text $t($n) -width 10 \
		-command "$w.f.text insert insert \"$n \""
	grid $w.b1.b$i -row $i
	set b($w.b1.b$i) exp_adv_bool
	incr i
    }

    frame $w.g1
    label $w.g1.l -text $t(grouping)
    grid $w.g1.l -sticky we
    set i 1
    foreach n {( )} {
	button $w.g1.g$i -text $n -width 10 \
		-command "$w.f.text insert insert \"$n \""
	grid $w.g1.g$i -row $i
	set b($w.g1.g$i) exp_adv_p
	incr i
    }

    grid $w.f1 \
	 $w.o1 \
	 $w.b1 \
	 $w.g1 -sticky n

    focus $w.f.text
    $w.f.text insert end $hd(exp)
    bind $w.f.text <Return> "ExpDone $handler 1; break"
    set hd(text) $w.f.text
}

# ExpClear --
#
# Clears the current expression window
#
# Arguments:
# handler	- the handler which identifies the expression window

proc ExpClear {handler} {
    upvar #0 $handler hd

    if {![string compare basic $hd(mode)]} {
	foreach f {subject from reply-to sender to cc} {
	    set hd($f) ""
	}
    } else {
	$hd(text) delete 1.0 end
    }
}

# ExpDone --
#
# Called when we are done with the expression window
#
# Arguments:
# handler	- the handler which identifies the expression window
# action	- what we should do

proc ExpDone {handler action} {
    upvar \#0 $handler hd
    upvar \#0 $hd(handler) fHd
    global t b option expList expExp

    if {![info exist expList]} {
	set expList {}
    }
    if {1 == $action} {
	# Build expression
	if {![string compare basic $hd(mode)]} {
	    set exp ""
	    foreach f {subject from reply-to sender to cc} {
		if {[string length $hd($f)]} {
		    if {[string length $exp]} {
			set exp "$exp $hd(op) "
		    }
                    regsub -all {\\} $hd($f) {\\\\} m
                    set exp "${exp}$f has [list $m]"
		}
	    }
	    if {[catch {RatParseExp $exp} expId]} {
		Popup $t(syntax_error_exp) $hd(w)
		return
	    }
	} else {
	    set exp [string trim [$hd(text) get 1.0 end]]
	    if {[catch {RatParseExp $exp} expId]} {
	        set i [lindex $expId 0]
	        $hd(text) tag add error "1.$i wordstart" "1.$i wordend"
		Popup "$t(error_underlined): [lindex $expId 1]" $hd(w)
		return
	   }
	}
	if {"create" == $hd(action)} {
	    if {$hd(doSave)} {
		if {[string length $hd(saveAs)]} {
		    if {-1 != [lsearch -exact $expList $hd(saveAs)]} {
			Popup $t(name_occupied) $hd(w)
			return
		    }
		    if {"" != $hd(addproc)} {
			eval "$hd(addproc) [list $hd(saveAs)]"
		    } else {
			lappend expList $hd(saveAs)
		    }
		    set expExp($hd(saveAs)) $exp
		    ExpWrite
		} else {
		    Popup $t(need_name) $hd(w)
		}
	    }
	    # Apply expression
	    set ids [$fHd(folder_handler) match $expId]
	    if {[string length $ids]} {
		SetFlag $hd(handler) flagged 1 $ids
	    }

	} else {
	    if {$hd(saveAs) != $hd(oldName)} {
		if {-1 != [lsearch -exact $expList $hd(saveAs)]} {
		    Popup $t(name_occupied) $hd(w)
		    return
		}
		ExpDelete $hd(oldName)
		lappend expList $hd(saveAs)
	    }
	    set expExp($hd(saveAs)) $exp
	    ExpWrite
	}
	RatFreeExp $expId
    }
    destroy $hd(w)
}

proc ExpClose {handler} {
    upvar \#0 $handler hd
    global b option
    
    if {"create" == $hd(action)} {
        ::tkrat::winctl::RecordGeometry ratExpression $hd(w)
    } else {
        ::tkrat::winctl::RecordGeometry ratExpEdit $hd(w)
    }
    catch {focus $hd(oldfocus)}
    foreach bn [array names b $hd(w).*] {unset b($bn)}
    if {[string compare $hd(mode) $option(expression_mode)]} {
	set option(expression_mode) $hd(mode)
	SaveOptions
    }
    unset hd
}

# ExpWrite --
#
# Write the saved expressions to disk
#
# Arguments:

proc ExpWrite {} {
    global option expList expExp

    set f [open $option(ratatosk_dir)/expressions w]
    puts $f "set expList [list $expList]"
    foreach e $expList {
	puts $f [list set expExp($e) $expExp($e)]
    }
    close $f
}

# ExpRead --
#
# Read the saved expressions
#
# Arguments:

proc ExpRead {} {
    global option expList expExp

    if {[file readable $option(ratatosk_dir)/expressions]} {
	source $option(ratatosk_dir)/expressions
    }
}

# ExpBuildMenu
#
# Build a menu of saved expressions
#
# Arguments:
# m		- The menu to populate
# handler	- The handler which identifies the folder window that
#		  the selection should be done in.

proc ExpBuildMenu {m handler} {
    global expList expExp

    $m delete 0 end

    if {![info exists expList]} {
	return
    }
    foreach i $expList {
	$m add command -label $i -command [list ExpMenuApply $i $handler]
    }
}

# ExpMenuApply
#
# Apply an expression from the menu
#
# Arguments:
# id		- The array index of the selected expression
# handler	- The handler which identifies the folder window that
#		  the selection should be done in.

proc ExpMenuApply {id handler} {
    upvar #0 $handler hd
    global expExp

    set expId [RatParseExp $expExp($id)]
    set ids [$hd(folder_handler) match $expId]
    if {[string length $ids]} {
	SetFlag $handler flagged 1 $ids
    }
    RatFreeExp $expId
}

# ExpHandleSaved
#
# Handle saved expressions
#
# Arguments:

proc ExpHandleSaved {handler} {
    global t

    rat_list::create expList expList "ExpCreate $handler" ExpEdit ExpDelete \
	    ExpWrite $t(saved_expr) $t(create) $t(edit) $t(delete) $t(dismiss)
}

# ExpDelete --
#
# Delete the selected expression
#
# Arguments:
# name - Name of expression to delete

proc ExpDelete {name} {
    global t expList expExp

    set i [lsearch -exact $expList $name]
    set expList [lreplace $expList $i $i]
    unset expExp($name)
}
