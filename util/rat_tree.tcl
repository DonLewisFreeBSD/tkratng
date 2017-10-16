# rat_tree --
#
# Create a tree

package provide rat_tree 1.0

namespace eval rat_tree {
    namespace export create
    variable idCnt 0
    variable dy 12
}


# rat_tree::create --
#
# Create a tree widget. Returns a new tree widget command. Valid invocations
# are:
#      rat_tree::create WIN ?-sizeid SIZEID? ?-selectcallback CMD?
#                           ?-movenotify CMD?
#
# Thsi will create a tree widget which uses a global variable 
# rat_tree::state_$id, hereafter known as ts. This state includes lots of
# things, some of which are documented here.
#
# Positions used for drag&drop
#  ts(pos,Y) dropzone parents {node_id index_in_node} {x y}
# Arguments:
# win    - Window to create
# args

proc rat_tree::create {win args} {
    variable idCnt
    variable top
    variable dy

    set id [incr idCnt]
    upvar \#0 rat_tree::state_$id ts

    # Handle arguments
    set a(-sizeid) {}
    set a(-selectcallback) {}
    set a(-movenotify) {}
    array set a $args
    set ts(selectcallback) $a(-selectcallback)
    set ts(movenotify) $a(-movenotify)
    set ts(selected) {}
    set ts(changes) 0
    set ts(move,dropin) {}
    set ts(autoredraw) 1
    set ts(marker) {}

    # Create window
    frame $win -class RatTree -bd 2 -relief ridge
    set ts(font) [option get $win font Font]
    set ts(c) $win.c
    canvas $win.c -yscrollcommand "$win.scrolly set" \
	    -xscrollcommand "$win.scrollx set" \
	    -highlightthickness 0 -selectborderwidth 0 -takefocus 0
    scrollbar $win.scrolly -relief sunken -command "$win.c yview" \
	    -highlightthickness 0 -bd 1 -takefocus 0
    scrollbar $win.scrollx -relief sunken -command "$win.c xview" \
	    -highlightthickness 0 -orient horizontal -bd 1 -takefocus 0
    if { 0 < [llength $a(-sizeid)]} {
	::tkrat::winctl::Size $a(-sizeid) $ts(c)
	bind $win.c <Delete> "::tkrat::winctl::RecordSize $a(-sizeid) $ts(c)"
    }
    set ts(scrollx) $win.scrollx
    set ts(scrolly) $win.scrolly
    grid $win.c -sticky nsew
    grid columnconfigure $win 0 -weight 1
    grid rowconfigure $win 0 -weight 1
    after idle "grid propagate $win 0"

    # Find height of text
    set i [$win.c create text 0 0 -anchor n -text F]
    set ts(dy) [lindex [$win.c bbox $i] 3]
    $win.c delete $i

    # Do bindings
    bind $win.c <ButtonPress-1> \
	    "rat_tree::preselect $id %W %x %y"
    bind $win.c <ButtonRelease-1> "rat_tree::postselect $id"
    bind $win.c <Motion> "rat_tree::move $id %W %x %y"

    set top($id) [incr idCnt]
    foreach v {image label id zone dropin state} {
	set ts(item,$top($id),$v) {}
    }

    upvar \#0 rat_tree::state_n$top($id)(contents) contents
    set contents {}

    proc tree$id {cmd args} "treecmd $id \$cmd \$args"
    proc tree${id}_$top($id) {cmd args} "nodecmd $id $top($id) \$cmd \$args"
    bind $ts(c) <Destroy> "rat_tree::destroy $id"
    bind $ts(c) <Configure> "rat_tree::configure_event $id"
    return rat_tree::tree$id
}

# rat_tree::destroy --
#
# Cleanup a rat_tree
#
# Arguments:
# tid - Tree id

proc rat_tree::destroy {tid} {
    upvar \#0 rat_tree::state_$tid ts
    variable top

    foreach cmd [info commands tree${tid}*] {
	rename $cmd {}
    }
    unset top($tid)
    unset ts
}

# rat_tree::treecmd --
#
# The command handling a rat_tree. The valid invocations of this command
# are:
#      CMD ID gettopnode
#      CMD ID autoredraw state
#      CMD ID redraw
#      CMD ID itemchange ITEMID args
#      CMD ID bind ITEMID KEY_EVENT BINDING
#      CMD ID getpos ITEMID 
#      CMD ID select ITEMID
#      CMD ID delete ITEMID
#      CMD ID getnumchanges

proc rat_tree::treecmd {tid cmd alist} {
    upvar \#0 rat_tree::state_$tid ts
    variable top

    # Possibly locate item
    if {[regexp {itemchange|bind|getpos|select|delete} $cmd]} {
	set iid {}
	foreach i [array names ts item,*,id] {
	    if {$ts($i) == [lindex $alist 0]} {
		regexp {item,([^,]+),id} $i unused iid
		break
	    }
	}
	if {"" == $iid} {
	    error "Unkown item"
	}
    }
    set need_redraw 0

    switch -- $cmd {
	gettopnode {
	    return rat_tree::tree${tid}_$top($tid)
	}
	autoredraw {
	    set ts(autoredraw) [lindex $alist 0]
	}
	getnumchanges {
	    set n $ts(changes)
	    set ts(changes) 0
	    return $n
	}
	redraw {
	    redraw $tid
	}
	itemchange {
	    array set a [lrange $alist 1 end]
	    foreach v {image label id zone dropin state} {
		if {[info exists a(-$v)]} {
		    set ts(item,$iid,$v) $a(-$v)
		}
	    }
	    set need_redraw 1
	}
	bind {
	    set ts(item,$iid,bind,[lindex $alist 1]) [lindex $alist 2]
	}
	getpos {
	    return [getpos $iid $tid $top($tid)]
	}
	select {
	    set ts(selected) $iid
	    set need_redraw 1
	}
	delete {
	    set nodes [uplevel #0 "info vars rat_tree::state_n*"]
	    foreach n $nodes {
		upvar \#0 $n sn
		if {-1 != [set i [lsearch -exact $sn(contents) $iid]]} {
		    set sn(contents) [lreplace $sn(contents) $i $i]
		    break
		}
	    }
	    foreach v [array names ts item,$iid,*] {
		unset ts($v)
	    }
	    if {$ts(selected) == $iid} {
		set ts(selected) ""
	    }
	    set need_redraw 1
	    incr ts(changes)
	}
	default {
	    error "Illegal arguments to tree command"
	}
    }
    if {$need_redraw && $ts(autoredraw)} {
	redraw $tid
    }
    return {}
}

# rat_tree::nodecmd --
#
# Command for a tree node. The valid invocations of this command are:
#    CMD add TYPE ?-position POS? ?-image IMAGE? ?-label LABEL? ?-id ID? \
#                 ?-zone ID? ?-dropin IDLIST? ?-state STATE? ?-redraw do?
#    CMD list
#    CMD clear
#    CMD configure INDEX flag value
#
# Arguments:

proc rat_tree::nodecmd {tid nid cmd alist} {
    variable idCnt
    upvar \#0 rat_tree::state_n$nid s
    upvar \#0 rat_tree::state_$tid ts

    switch -- $cmd {
	add {
	    set type [lindex $alist 0]
	    set a(-position) [llength $s(contents)]
	    set a(-image) {}
	    set a(-label) {}
	    set a(-id) {}
	    set a(-zone) {}
	    set a(-dropin) {}
	    set a(-state) closed
	    array set a [lrange $alist 1 end]
	    set id [incr idCnt]
	    if {"folder" == $type} {
		upvar \#0 rat_tree::state_n${id}(contents) contents
		set contents {}
		proc tree${tid}_$id {cmd args} \
			"nodecmd $tid $id \$cmd \$args"
		set ret rat_tree::tree${tid}_$id
		set type node
	    } else {
		set ret {}
		set type leaf
	    }
	    set s(contents) [linsert $s(contents) $a(-position) $id]
	    set ts(item,$id,type) $type
	    foreach v {image label id zone dropin state} {
		set ts(item,$id,$v) $a(-$v)
	    }
	    set ts(item,$id,node) $nid
	    if {$ts(autoredraw)} {
		redraw $tid
	    }
	    incr ts(changes)
	    return $ret
	}
	list {
	    # Returns list of entries {type name id}
	    set l {}
	    foreach id $s(contents) {
		set e [list $ts(item,$id,type) $ts(item,$id,label) \
			$ts(item,$id,id)]
		if {"node" == $ts(item,$id,type)} {
		    lappend e rat_tree::tree${tid}_$id
		}
		lappend l $e
	    }
	    return $l
	}
	clear {
	    foreach id $s(contents) {
		if {$ts(selected) == $id} {
		    set ts(selected) ""
		}
		if {"node" == $ts(item,$id,type)} {
		    nodecmd $tid $id clear {}
		}
		foreach v [array names ts item,$id,*] {
		    unset ts($v)
		}
	    }
	    set s(contents) {}
	}
	default {
	    error "rat_tree::nodecmd $tid $nid Illegal arguments"
	}
    }
}

# rat_tree::getpos --
#
# Return the position of a certain item. Returns a list of {node pos}
# where item is the node containing it and pos is the position within
# that node.
#
# Arguments:
# id  - Item id
# tid - Tree id
# nid - Node to look in

proc rat_tree::getpos {id tid nid} {
    upvar \#0 rat_tree::state_$tid ts
    upvar \#0 rat_tree::state_n$nid s

    set pos 0
    foreach i $s(contents) {
	if {$id == $i} {
	    return [list rat_tree::tree${tid}_$nid $pos]
	}
	if {"node" == $ts(item,$i,type)} {
	    set r [getpos $id $tid $i]
	    if {"" != $r} {
		return $r
	    }
	}
	incr pos
    }
    return {}
}

# rat_tree::redraw --
#
# Redraws the entire tree
#
# Arguments:
# tid - Tree-id

proc rat_tree::redraw {tid} {
    variable top
    upvar \#0 rat_tree::state_$tid ts
    upvar \#0 rat_tree::state_n$top($tid) ns

    $ts(c) delete all
    set ts(y) 0
    set ts(dx0) 8
    set ts(dx1) 16
    foreach n [array names ts pos,*] {
	unset ts($n)
    }
    draw_node $tid $top($tid) 10 {}
    $ts(c) raise sel
    set bbox [$ts(c) bbox all]
    $ts(c) configure \
	    -scrollregion [list 0 0 \
	    [expr {[lindex $bbox 2]+5}] \
	    [expr {[lindex $bbox 3]+5}]]
    draw_selected $tid
    rat_tree::configure_event $tid
}

# rat_tree:draw_node --
#
# Redraws a node
#
# Arguments:
# tid     - Tree id
# nid     - Node id
# x       - X offset
# parents - List of parents

proc rat_tree::draw_node {tid nid x parents} {
    upvar \#0 rat_tree::state_n$nid ns
    upvar \#0 rat_tree::state_$tid ts

    set index 1
    set c $ts(c)
    set y0 $ts(y)
    set x1 [expr {$x+$ts(dx0)}]
    set x2 [expr {$x1+$ts(dx1)}]
    set yb [expr {$ts(y)+$ts(dy)/2}]
    set ts(pos,$yb) [list $ts(item,$nid,zone) $parents [list $nid 0] \
	    [list $x $yb]]
    foreach i $ns(contents) {
	set y [incr ts(y) $ts(dy)]
	set yl $y
	if {"node" == $ts(item,$i,type)} {
	    set i0 [$c create rectangle  [expr {$x-4}] [expr {$y-4}] \
		    [expr {$x+4}] [expr {$y+4}] -fill white -tags sel]
	    if {"open" == $ts(item,$i,state)} {
		set i1 [$c create line [expr {$x-2}] $y \
			[expr {$x+2}] $y -tags sel]
		draw_node $tid $i [expr {($x+$x2)/2}] [concat $parents $nid]
		incr ts(y)
	    } else {
		set i1 [$c create line [expr {$x-2}] $y \
			[expr {$x+2}] $y \
			$x $y \
			$x [expr {$y-2}] \
			$x [expr {$y+3}] \
			-tags sel]
	    }
	    $c bind $i0 <1> "rat_tree::node_state_toggle $tid $nid $i"
	    $c bind $i1 <1> "rat_tree::node_state_toggle $tid $nid $i"
	}
	if {"" != $ts(item,$i,image)} {
	    $c create line $x $y [expr {($x1+$x2)/2}] $y
	    set id [$c create image $x1 $y -anchor w -image $ts(item,$i,image)]
	    set xt $x2
	} else {
	    $c create line $x $y [expr {$x1-1}] $y
	    set xt $x1
	}
	set tag [$c create text $xt $y -anchor w -text $ts(item,$i,label) \
		-font $ts(font)]
	set ts(tag_to_id,$tag) $i
	set ts(item,$i,tag) $tag
	set bbox [$c bbox $tag] 
	set yb [expr {$ts(y)+$ts(dy)/2}]
        set ts(pos,$yb) [list $ts(item,$nid,zone) $parents [list $nid $index] \
		[list $x $yb]]
	if {"node" == $ts(item,$i,type) && "open" != $ts(item,$i,state)} {
	    set ts(pos,$y) [list $ts(item,$i,zone) [concat $parents $nid] \
		    [list $i 0] [list [expr {$x+$ts(dx1)}] [expr {$yb-1}]]]
	}
	foreach bd [array names ts item,$i,bind,*] {
	    regexp "item,$i,bind,(.*)" $bd unused key
	    $c bind $tag $key $ts(item,$i,bind,$key)
	}
	incr index
    }
    if {[info exists yl]} {
	$c create line $x [expr {$y0+$ts(dy)/2}] $x $yl 
    }
}

# rat_tree::draw_selected --
#
# Mark the selected item
#
# Arguments:
# tid - Tree id

proc rat_tree::draw_selected {tid} {
    upvar \#0 rat_tree::state_$tid ts

    if {"" == $ts(selected)} {
	return
    }
    if {[catch {$ts(c) bbox $ts(item,$ts(selected),tag)} bb]} {
	return
    }
    if {0 == [llength $bb]} {
	return
    }
    $ts(c) delete selectbox
    $ts(c) create rectangle [lindex $bb 0] [lindex $bb 1] \
	    [lindex $bb 2] [lindex $bb 3] -fill lightblue -tags selectbox
    $ts(c) lower selectbox
}

# rat_tree::node_state_toggle --
#
# Toggle open/close state for a node
#
# Arguments:
# tid - Tree id
# nid - Node id
# i   - Index

proc rat_tree::node_state_toggle {tid nid i} {
    upvar \#0 rat_tree::state_$tid ts

    if {"open" == $ts(item,$i,state)} {
	set ts(item,$i,state) closed
    } else {
	set ts(item,$i,state) open
    }
    redraw $tid
}

# rat_tree::preselect --
#
# Prepares to select or drag an item
#
# Arguments:
# tid  - Tree id
# w    - Widget name
# x, y - Select-coordinates

proc rat_tree::preselect {tid w x y} {
    upvar \#0 rat_tree::state_$tid ts

    # Find coordinates in canvas
    set cx [$w canvasx $x]
    set cy [$w canvasy $y]

    # Figure out what was selected and if it is interesting
    set ts(seltag) ""
    foreach st [$ts(c) find overlapping $cx $cy $cx $cy] {
	if {"" != $st && [info exists ts(tag_to_id,$st)]} {
	    set ts(seltag) $st
	    break
	}
    }
    if {"" != $ts(seltag)} {
	set ts(selid) $ts(tag_to_id,$ts(seltag))
	set ts(move,dropin) $ts(item,$ts(selid),dropin)
	set ts(move,origin_x) $cx
	set ts(move,origin_y) $cy
	set ts(move,status) start
    } else {
	set ts(selid) ""
	set ts(move,dropin) {}
    }
    set ts(release_action) select
}

# rat_tree::postselect --
#
# Maybe try select an item
#
# Arguments:
# tid  - Tree id

proc rat_tree::postselect {tid} {
    upvar \#0 rat_tree::state_$tid ts

    if {"" == $ts(selid)} { return}

    set ts(move,dropin) {}
    # What should we do with the object?
    switch $ts(release_action) {
	"select" {
	    set client_id $ts(item,$ts(selid),id)
	    
	    # Let master approve/disapprove
	    if {"" != $ts(selectcallback)} {
		if {"ok" != [$ts(selectcallback) $client_id]} {
		    return
		}
	    }
	    set ts(selected) $ts(selid)
	    draw_selected $tid
	}
	"place" {
	    set old_node_id $ts(item,$ts(selid),node)
	    set new_node_id [lindex $ts(move,dest) 0]
	    if {"" != $ts(marker)} {
		$ts(c) delete $ts(marker)
	    }
	    if {$ts(selid) == $new_node_id} {
		redraw $tid
		return
	    }
	    upvar \#0 rat_tree::state_n$old_node_id old_node
	    upvar \#0 rat_tree::state_n$new_node_id new_node
	    set p [lsearch $old_node(contents) $ts(selid)]
	    if {$old_node_id == $new_node_id \
		    && $p > [lindex $ts(move,dest) 1]} {
		incr p
	    }
	    set new_node(contents) [linsert $new_node(contents) \
		    [lindex $ts(move,dest) 1] $ts(selid)]
	    set ts(item,$ts(selid),node) $new_node_id
	    set old_node(contents) [lreplace $old_node(contents) $p $p]
	    redraw $tid
	    incr ts(changes)
	    # Tell master
	    if {"" != $ts(movenotify)} {
		$ts(movenotify)
	    }
	}
	"redraw" {
	    redraw $tid
	}
    }
}

# rat_tree::move --
#
# Move an item
#
# Arguments:
# tid  - Tree id
# w    - Widget name
# x, y - Coordinates

proc rat_tree::move {tid w x y} {
    upvar \#0 rat_tree::state_$tid ts

    # Should we do this?
    if {0 == [llength $ts(move,dropin)]} {return}

    # Find coordinates in canvas
    set cx [$w canvasx $x]
    set cy [$w canvasy $y]

    # Have we started to actually move it?
    if {"start" == $ts(move,status)} {
	if {3 > [expr {abs($ts(move,origin_x)-$cx)}]
	&& 3 > [expr {abs($ts(move,origin_y)-$cy)}]} {
	    return
	}
	set ts(move,status) move
	set ts(move,last_x) $ts(move,origin_x)
	set ts(move,last_y) $ts(move,origin_y)
    }

    # Move it
    $w move $ts(seltag) \
	    [expr {$cx-$ts(move,last_x)}] [expr {$cy-$ts(move,last_y)}]
    set ts(move,last_x) $cx
    set ts(move,last_y) $cy

    # Find where to put placement marker
    set found 0
    set diff 0
    while {!$found && $diff < 100} {
	set ay [expr {int($cy-$diff)}]
	if {[info exists ts(pos,$ay)]
	        && -1 == [lsearch -exact [lindex $ts(pos,$ay) 1] $ts(selid)]
	        && -1 != [lsearch -exact \
		   $ts(move,dropin) [lindex $ts(pos,$ay) 0]]} {
	    set found 1
	} else {
	    set ay [expr {int($cy+$diff)}]
	    if {[info exists ts(pos,$ay)]
	            && -1 == 
	               [lsearch -exact [lindex $ts(pos,$ay) 1] $ts(selid)]
	            && -1 != [lsearch -exact \
		       $ts(move,dropin) [lindex $ts(pos,$ay) 0]]} {
		set found 1
	    }
	}
	incr diff
    }

    # Draw placement marker
    if {"" != $ts(marker)} {
	$ts(c) delete $ts(marker)
    }
    if {$found} {
	set pos [lindex $ts(pos,$ay) 3]
	set lx [lindex $pos 0]
	set ly [lindex $pos 1]
	set ts(marker) [$w create line $lx $ly 1000 $ly]
	set ts(move,dest) [lindex $ts(pos,$ay) 2]
	set ts(release_action) place
    } else {
	set ts(move,dest) {}
	set ts(marker) {}
	set ts(release_action) redraw
    }
}

# rat_tree::configure_event --
#
# This proc is run when the tree canvas receives an configure event.
# It adds or removes the scrollbars. One tricky detail here is that if
# this route adds or removes scrollbars then that act will generate a
# new Configure event. Therefore this routine will be called again.
# This actually helps us since adding one scrollbar may force the need to
# add the other.
#
# Arguments:
# tid  - Tree id

proc rat_tree::configure_event {tid} {
    upvar \#0 rat_tree::state_$tid ts

    set bbox [$ts(c) bbox all]
    # Check if we need vertical scrollbar
    if {[winfo height $ts(c)] > [lindex $bbox 3]} {
	grid forget $ts(scrolly)
    } else {
	grid $ts(scrolly) -column 1 -row 0 -sticky ns
    }
    # Check if we need horizontal scrollbar
    if {[winfo width $ts(c)] > [lindex $bbox 2]} {
	grid forget $ts(scrollx)
    } else {
	grid $ts(scrollx) -column 0 -row 1 -sticky ew
    }
}
