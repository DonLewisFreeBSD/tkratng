# rat_textlist --
#

package provide rat_textlist 1.0

namespace eval rat_textlist {
    variable tPriv

    namespace export create textwidget insert selection

    global tk_version fixedNormFont fixedBoldFont
    if {[info exists tk_version]} {
	set tPriv(titleFont) $fixedBoldFont
	set tPriv(listFont) $fixedNormFont
    }
}

proc rat_textlist::create {w title} {
    upvar rat_textlist::priv$w hd
    variable tPriv

    frame $w -class TkRat
    label $w.l -font $tPriv(titleFont) -text $title -bd 4 -anchor w
    pack $w.l -side top -fill x
    text $w.t \
	    -yscroll "$w.s set" \
	    -exportselection false \
	    -highlightthickness 0 \
	    -relief sunken \
	    -setgrid true \
	    -font $tPriv(listFont) \
	    -wrap none \
	    -cursor {} \
	    -bd 2
    scrollbar $w.s \
	    -relief sunken \
	    -command "$w.t yview" \
	    -highlightthickness 0 
    pack $w.s -side right -fill y
    pack $w.t -side left -expand 1 -fill both

    $w.t tag configure sel \
            -relief raised \
            -borderwidth [$w.t cget -selectborderwidth] \
            -foreground [$w.t cget -selectforeground] \
            -background [$w.t cget -selectbackground]
    $w.t tag configure topline -spacing1 4
    $w.t tag configure text -lmargin1 2
    bind $w.t <1> "rat_textlist::beginSelect $w \[%W index @%x,%y\]; break"
    bind $w.t <Double-1> {break}
    bind $w.t <Triple-1> {break}
    bind $w.t <B1-Motion> "rat_textlist::motion $w \[%W index @%x,%y\]; break"
    bind $w.t <B1-Leave> "rat_textlist::scan $w %x %y; break"
    bind $w.t <B1-Enter> "rat_textlist::cancelScan $w; break"
    bind $w.t <ButtonRelease-1> "rat_textlist::cancelScan $w; break"
    set hd(text) $w.t
    set hd(tags) {}
    set hd(nextid) 0
    set hd(afterId) {}

    bind $w.t <Destroy> "rat_textlist::destroy $w"
    return $w
}

proc rat_textlist::textwidget {w} {
    upvar rat_textlist::priv$w hd

    return $hd(text)
}

proc rat_textlist::insert {w index text} {
    upvar rat_textlist::priv$w hd

    if {"end" == $index || $index > [llength $hd(tags)]} {
	set i [$hd(text) index end-1c]
	set li end
    } else {
	set i [$hd(text) index [lindex $hd(tags) $index].first]
	set li $index
    }

    set tag item_[incr hd(nextid)]
    $hd(text) insert $i "$text\n" "$tag text"
    $hd(text) tag add topline $i
    set hd($tag-selected) 0

    set hd(tags) [linsert $hd(tags) $li $tag]
    return {}
}

proc rat_textlist::selection {w op args} {
    upvar rat_textlist::priv$w hd

    case $op {
    get {
	    set s {}
	    for {set i 0} {$i < [llength $hd(tags)]} {incr i} {
		if $hd([lindex $hd(tags) $i]-selected) {
		    lappend s $i
		}
	    }
	    return $s
	}
    clear {
	    for {set i 0} {$i < [llength $hd(tags)]} {incr i} {
		if $hd([lindex $hd(tags) $i]-selected) {
		    set hd([lindex $hd(tags) $i]-selected) 0
		    $hd(text) tag remove sel 1.0 end
		}
	    }
	}
    set {
	    for {set i 0} {$i < [llength $hd(tags)]} {incr i} {
		if $hd([lindex $hd(tags) $i]-selected) {
		    set hd([lindex $hd(tags) $i]-selected) 0
		    $hd(text) tag remove sel 1.0 end
		}
	    }
	    if {"end" == [lindex $args 1]} {
		set end [llength $hd(tags)]
	    } else {
		set end [lindex $args 1]
	    }
	    for {set i [lindex $args 0]} {$i < $end} {incr i} {
		set tag [lindex $hd(tags) $i]
		set hd($tag-selected) 1
		$hd(text) tag add sel $tag.first $tag.last
	    }
	}
    default {
	    error "Unkown argument $op"
	}
    }
}

proc rat_textlist::beginSelect {w index} {
    upvar rat_textlist::priv$w hd

    set tags [$hd(text) tag names $index]
    set current [lindex $tags [lsearch $tags item_*]]
    if {-1 != [lsearch $tags sel]} {
	set hd(selmode) unsel
	$hd(text) tag remove sel $current.first $current.last
	set hd($current-selected) 0
    } else {
	set hd(selmode) sel
	$hd(text) tag add sel $current.first $current.last
	set hd($current-selected) 1
    }
    set hd(lastsel) $current
    set hd(lastindex) [lsearch $hd(tags) $current]
}

proc rat_textlist::motion {w index} {
    upvar rat_textlist::priv$w hd

    set tags [$hd(text) tag names $index]
    if {{} == $tags} {
	return
    }
    set current [lindex $tags [lsearch $tags item_*]]
    if {$current == $hd(lastsel)} {
	return
    }

    set ci [lsearch $hd(tags) $current]
    if {$ci > $hd(lastindex)} {
	set d 1
    } else {
	set d -1
    }
    for {set i [expr {$hd(lastindex)}]} {$i != $ci} {} {
	incr i $d
	set c [lindex $hd(tags) $i]
	if {$hd(selmode) == "unsel"} {
	    $hd(text) tag remove sel $c.first $c.last
	    set hd($c-selected) 0
	} else {
	    $hd(text) tag add sel $c.first $c.last
	    set hd($c-selected) 1
	}
    }
    set hd(lastsel) $current
    set hd(lastindex) $ci
}

proc rat_textlist::scan {w x y} {
    upvar rat_textlist::priv$w hd

    if {![winfo exists $hd(text)]} return
    if {$y >= [winfo height $hd(text)]} {
        $hd(text) yview scroll 1 units
    } elseif {$y < 0} {
        $hd(text) yview scroll -1 units 
    } elseif {$x >= [winfo width $hd(text)]} {
        $hd(text) xview scroll 2 units
    } elseif {$x < 0} {
        $hd(text) xview scroll -2 units
    } else {
        return 
    }
    rat_textlist::motion $w [$hd(text) index @$x,$y]
    set hd(afterId) [after 50 rat_textlist::scan $w $x $y]
} 

proc rat_textlist::cancelScan {w} {
    upvar rat_textlist::priv$w hd
    after cancel $hd(afterId)
    set hd(afterId) {}
}

proc rat_textlist::destroy {w} {
    upvar rat_textlist::priv$w hd

    after cancel $hd(afterId)
    unset hd
}
