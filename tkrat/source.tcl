# source.tcl --
#
# This file contains code which handles shows the source of a bodypart
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


# ShowSource
#
# Shows the source of a bodypart
#
# Arguments:
# body   - The bodypart to show

proc ShowSource {body} {
    global idCnt t showParams option fixedNormFont

    # Create identifier
    set id sourceWin[incr idCnt]
    set w .$id

    # Create toplevel
    toplevel $w -class TkRat
    wm title $w $t(source)

    # Type
    set type [$body type]
    set tstring [string tolower [lindex $type 0]/[lindex $type 1]]
    frame $w.type
    label $w.type.label -width 15 -anchor e -text $t(type):
    label $w.type.value -text $tstring -font $fixedNormFont
    pack $w.type.label \
	 $w.type.value -side left
    pack $w.type -side top -anchor w
    # Description
    if {[string length [$body description]]} {
	frame $w.desc
	label $w.desc.label -width 15 -anchor e -text $t(description):
	label $w.desc.value -text [$body description] -font $fixedNormFont
	pack $w.desc.label \
	     $w.desc.value -side left
        pack $w.desc -side top -anchor w
    }
    # Size
    frame $w.size
    label $w.size.label -width 15 -anchor e -text $t(size):
    label $w.size.value -text [RatMangleNumber [$body size]] \
	    -font $fixedNormFont
    pack $w.size.label \
	 $w.size.value -side left
    pack $w.size -side top -anchor w
    # Parameters
    set typepair [$body type]
    set type [string tolower [lindex $typepair 0]/[lindex $typepair 1]]
    if {[info exists showParams($type)]} {
	foreach p $showParams($type) {
	    frame $w.p_$p
	    label $w.p_$p.label -width 15 -anchor e -text $t($p):
	    label $w.p_$p.value -text [$body parameter $p] -font $fixedNormFont
	    pack $w.p_$p.label \
		 $w.p_$p.value -side left
	    pack $w.p_$p -side top -anchor w
	}
    }
    # Encodings
    frame $w.enc
    label $w.enc.label -width 15 -anchor e -text $t(encoding):
    label $w.enc.value -text [$body encoding] -font $fixedNormFont
    pack $w.enc.label \
         $w.enc.value -side left
    pack $w.enc -side top -anchor w

    # The data
    frame $w.text -relief sunken -bd 1
    scrollbar $w.text.scroll \
              -relief sunken \
              -command "$w.text.text yview" \
              -highlightthickness 0
    text $w.text.text \
         -yscroll "$w.text.scroll set" \
         -setgrid true \
         -wrap char \
         -bd 0 \
         -highlightthickness 0
    pack $w.text.scroll -side right -fill y
    pack $w.text.text -expand yes -fill both
    pack $w.text -side top -expand yes -fill both

    # The buttons
    button $w.dismiss -text $t(dismiss) -command "destroy $w"
    button $w.save -text $t(save_to_file)... -command "SaveBody $body $w"
    pack $w.save \
         $w.dismiss -side left -expand 1 -pady 5

    # Insert the source into the text widget
    set data [string map [list "\r\n" "\n"] [$body data true]]
    $w.text.text insert end $data
    $w.text.text configure -state disabled

    bind $w.text.text <Destroy> \
        "::tkrat::winctl::RecordGeometry showSource $w $w.text.text"
    ::tkrat::winctl::SetGeometry showSource $w $w.text.text
}
