# show.tcl --
#
# This file contains code which handles the actual displaying of a message
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


# A list of types we can show
set GoodTypes [list text/plain message/rfc822 image/gif image/ppm image/pgm]
set ImageTypes [list gif ppm pgm]

# A list of parameters to show for different types
set showParams(text/plain) {charset}
set showParams(message/external-body) {access-type site server name directory}

# A list of address headers
set showAddrHdr {from to cc bcc reply_to Return_Path}

# handlers for different bodyparts
set mimeHandler(text/plain)		{ ShowTextPlain $handler $body $msg}
set mimeHandler(text/enriched)		{ ShowTextEnriched $handler $body $msg}
set mimeHandler(text)			{ ShowTextOther $handler $body $msg}
set mimeHandler(multipart/alternative)	{ ShowMultiAlt $handler $body $msg }
set mimeHandler(multipart/encrypted)	{ ShowMultiEnc $handler $body $msg }
set mimeHandler(multipart)		{ ShowMultiMixed $handler $body $msg }
set mimeHandler(message/rfc822)		{ ShowMessage 0 $handler \
	                                              [$body message] }
set mimeHandler(message/delivery-status) {ShowDSN $handler $body }
set mimeHandler(image)			{ ShowImage $handler $body \
                                                      [lindex $type 1]}
set mimeHandler(application/pgp-keys)	{ ShowPGPKeys $handler $body }
set mimeHandler(application/pgp)	{ ShowTextPlain $handler $body $msg}

if {![catch {package require Img} err]} {
    set extensions(Img) 1
    lappend GoodTypes image/jpeg image/tiff image/bmp image/xpm image/png \
        image/pjpeg image/x-portable-bitmap image/x-portable-graymap \
	image/x-portable-pixmap image/x-png image/x-bmp
    lappend ImageTypes jpeg tiff bmp xpm png pjpeg x-png
}
if {![catch {package require Tkhtml} err]} {
    set extensions(Tkhtml) 1
    lappend GoodTypes text/html
    set mimeHandler(text/html) {RatBusy {ShowTextHtml $handler $body $msg}}
}


# ShowNothing --
#
# Clear the show window
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowNothing handler {
    $handler configure -state normal
    $handler delete 0.0 end
    $handler configure -state disabled
}

# Show --
#
# Shows the given message in the show portion of a folder window
#
# Arguments:
# handler -	The handler which identifies the show text widget
# msg     -	Message to show
# browse  -	True if we should use browse mode showing

proc Show {handler msg browse} {
    global option b fixedBoldFont
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    set fh(id) 0
    set fh(current) $msg
    set fh(sigstatus) pgp_none
    set fh(pgpOutput) ""
    set fh(browse) $browse
    set fh(toplevel) [winfo toplevel $handler]

    # Enable updates & clear data
    $handler configure -state normal
    $handler delete 0.0 end
    eval "$handler tag delete [$handler tag names]"

    # Reset the tags to a known status
    $handler tag configure HeaderName -font $fixedBoldFont
    $handler tag configure Center -justify center -spacing1 5m -spacing3 5m
    $handler tag configure CenterNoSpacing -justify center
    if { 4 < [winfo cells $handler]} {
	$handler tag configure URL -foreground $option(url_color) -underline 1
	$handler tag configure Found -background #ffff80
    } else {
	$handler tag configure URL -underline 1
	$handler tag configure Found -borderwidth 2 -relief raised
    }
    $handler tag bind URL <Enter> "$handler config -cursor hand2"
    $handler tag bind URL <Leave> "$handler config -cursor xterm"

    # Delete old subwindows
    foreach slave [winfo children $handler] {
	destroy $slave
    }
    # Do other misc cleanup
    if {[info exists fh(show_cleanup_cmds)]} {
	foreach clear $fh(show_cleanup_cmds) {
	    eval $clear
	}
    }
    set fh(show_cleanup_cmds) {}
    foreach n [array names fh charset_*] {
	unset fh($n)
    }
    set fh(lastType) ""
    foreach bn [array names b $handler.*] {unset b($bn)}
    foreach bn [array names b $fh(struct_menu)*] {unset b($bn)}

    # Show the message
    if {[catch "ShowMessage 1 $handler $msg" errmsg]} {
	Popup $errmsg $fh(toplevel)
    }
    BuildStructMenu $handler $msg

    # Prepare for URL searching
    $handler mark set searched 1.0

    # Don't allow the user to change this
    $handler configure -state disabled

    # Return the signature status
    set msgInfo(pgp,signed_parts) $fh(signed_parts)
    return [list $fh(sigstatus) $fh(pgpOutput)]
}

# InsertHeader --
#
# Inserts a header row into the text widget.
#
# Arguments:
# gtag	 - Tag to add to text elements
# w      - Text widget to insert into
# header - List of header rows

proc InsertHeader {gtag w header {width 1}} {
    global t showAddrHdr
    upvar #0 $w fh

    set hn [lindex $header 0]
    regsub -all -- - [string tolower $hn] _ hni
    if {[info exists t($hni)]} {
	set hn $t($hni)
    }
    $w insert insert [format "%${width}s: " $hn] "HeaderName $gtag"
    if {-1 == [lsearch -exact $showAddrHdr $hni]} {
	$w insert insert "[lindex $header 1]\n" $gtag
    } else {
	set first 1
	foreach a [RatSplitAdr [lindex $header 1]] {
	    if {0 == $first} {
		$w insert insert ",\n[format "%${width}s  " ""]" $gtag
	    }
	    $w insert insert "[string trim $a]" $gtag
	    set first 0
	}
	$w insert insert "\n" $gtag
    }
}

# ShowMessage --
#
# Inserts a message entity at the end. This is done by first inserting the
# selected headers and then the body via ShowBody.
#
# Arguments:
# first	  -	Indicates if this is the top message or an embedded message
# handler -	The handler which identifies the show text widget
# msg     -	Message to show

proc ShowMessage {first handler msg} {
    global option t
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    set tag t[incr fh(id)]
    if {![info exists msgInfo(show,$msg)]} {
        set msgInfo(show,$msg) 1
    }
    if {0 == $first} {
	$handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
			        \[lsearch \[set ${handler}(struct_list)\] \
			        $msg\]"
    } else {
	set fh(signed_parts) {}
    }

    if {![string length $msg]} {
	$handler insert insert "\[$t(empty_message)\]" $tag
	return
    }

    # Add a newline if this isn't the first message
    if {0 == $first} {
	$handler insert insert "\n" $tag
    }

    switch $fh(show_header) {
    all	{
	    foreach h [$msg headers] {
		InsertHeader $tag $handler $h
	    }
	}
    selected {
	    foreach h [$msg headers] {
		lappend header([string tolower [lindex $h 0]]) [lindex $h 1]
	    }
	    set length 5
	    foreach f $option(show_header_selection) {
		if { $length < [string length $f]} {
		    set length [string length $f]
		}
	    }
	    foreach f $option(show_header_selection) {
		set n [string tolower $f]
		if {[info exists header($n)]} {
		    foreach h $header($n) {
			InsertHeader $tag $handler [list $f $h] $length
		    }
		}
	    }
	}
    default	{ }
    }

    if {$fh(browse)} {
	if {![info exists msgInfo(browse)]} {
	    if {1 == $first} {
		set msgInfo(browse) $fh(browse)
	    } else {
		set msgInfo(browse) 0
	    }
	}
    } else {
	set msgInfo(browse) 0
    }

    if {$msgInfo(browse)} {
	button $handler.download -text $t(show_body) \
		-command "Show $handler $msg 0" -cursor top_left_arrow
	$handler window create end -window $handler.download -padx 20 -pady 20
	set fh(sigstatus) pgp_none
	return
    }

    # Insert the body
    set body [$msg body]
    if {![info exists msgInfo(show,$body)]} {
	set msgInfo(show,$body) 1
    }
    if {$msgInfo(show,$msg)} {
	$handler insert insert "\n"
	if {![info exists msgInfo(show,$body)]} {
	    set msgInfo(show,$body) 1
	}
    } else {
        set msgInfo(show,$body) 0
    }
    ShowBody $handler $body $msg

    if {1 == $first && ![string compare pgp_good $fh(sigstatus)] &&
	    [string compare pgp_good [$body sigstatus]]} {
	set fh(sigstatus) pgp_part
    }
}

# ShowBody --
#
# Inserts a bodypart entity at the end, this is really just a switch which
# calls upon the commands which does the actual work.
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowBody {handler body msg} {
    global option mimeHandler
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    if {!$msgInfo(show,$body)} {
        return
    }

    # Update signature status
    if {[string compare pgp_none [set sigstatus [$body sigstatus]]]} {
	set fh(sigstatus) $sigstatus
	lappend fh(signed_parts) $body
	if {[string length $fh(pgpOutput)]} {
	    set fh(pgpOutput) "$fh(pgpOutput)\n\n[$body getPGPOutput]"
	} else {
	    set fh(pgpOutput) [$body getPGPOutput]
	}
    }

    # We will need this later on.
    set type [string tolower [$body type]]

    # Draw separator between items only if the previous entity was of
    # the same type and not multipart.
    set mtype [lindex $type 0]
    if {[string compare $mtype multipart]} {
        if {![string compare $mtype $fh(lastType)]} {
	    frame $handler.f[incr fh(id)] -width 12c -height 2 \
		    -relief sunken -bd 2
	    $handler window create insert -window $handler.f$fh(id) -pady 10
	    $handler insert insert "\n"
        }
        set fh(lastType) $mtype
    }

    # Switch for subroutines which does the actual drawing
    set tp [join $type /]
    if {[string equal $tp application/octet-stream]} {
		set type [GetHandlerFromExtension [$body filename]]
		set tp [join $type /]
	}
    if {[info exists mimeHandler($tp)]} {
	set mh $mimeHandler($tp)
    } elseif {[info exists mimeHandler([lindex $type 0])]} {
	set mh $mimeHandler([lindex $type 0])
    } else {
	set mh { ShowDefault $handler $body }
    }
    eval $mh
}

# ShowTextPlain --
#
# Show text/plain entities, should handle different fonts...
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowTextPlain {handler body msg} {
    global option b charsetName propNormFont
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    set tag t[incr fh(id)]
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
			        \[lsearch \[set ${handler}(struct_list)\] \
			        $body\]"

    if {[$body isGoodCharset]} {
	$handler insert insert [$body data false] $tag
    } else {
	global t
	set w $handler.w[incr fh(id)]
	frame $w -relief raised -bd 2 -cursor top_left_arrow
	if {[string length [$body description]]} {
	    label $w.desc -text "$t(description): [$body description]" \
		    -font $propNormFont
	    pack $w.desc -side top -anchor nw
	}
	set charset [$body parameter charset]
	label $w.type \
	    	-text "$t(here_is_text) '$charset' $t(which_cant_be_shown)"
	label $w.modlab -text $t(interpret_as):
	menubutton $w.mode -menu $w.mode.m -indicatoron 1 -relief raised \
	    	-textvariable ${handler}(mode,$body)
	set b($w.mode) howto_display
	button $w.save -text $t(save_to_file) \
		-command "SaveBody $body $fh(toplevel)"
	set b($w.save) save_bodypart
	pack $w.type -side top -anchor w
	pack $w.modlab $w.mode -side left
	pack $w.save -side left -padx 20
	set l 0
	menu $w.mode.m -tearoff 0
	foreach c $option(charsets) {
	    if {0 < [string length $charsetName($c)]} {
		set name "$c ($charsetName($c))"
	    } else {
		set name $c
	    }
	    $w.mode.m add command \
		    -label $name \
		    -command "ShowTextCharset $body $msg $handler $handler \
					      $tag $c"
	}
	if {![info exists msgInfo(show,$body,how)]} {
	    set msgInfo(show,$body,how) $option(charset)
	}
	set fh(mode,$body) $msgInfo(show,$body,how)
	$handler window create insert -window $w -pady 5
	foreach win [concat $w [pack slaves $w]] {
	    bind $win <3> "tk_popup $fh(struct_menu) %X %Y \
			   \[lsearch \[set ${handler}(struct_list)\] \
			   $body\]"
	}
	$handler insert insert "\n" $tag
	$handler mark set ${body}_s insert
	$handler mark set ${body}_e insert
	$handler mark gravity ${body}_s left
	ShowTextCharset $body $msg $handler $handler $tag \
		$msgInfo(show,$body,how)
    }
}

# ShowTextCharset --
#
# Shows text in a nonstandard character set
#
# Arguments:
# body    -	The bodypart to show
# msg     -	The message name
# hd	  -	The array to use for global variables
# w 	  -	The handler which identifies the show text widget
# tag	  -	The tag this entity should have
# charset -	What to assume

proc ShowTextCharset {body msg hd w tag charset} {
    upvar #0 $hd fh \
    	     msgInfo_$msg msgInfo
    global t

    set oldState [$w cget -state]
    $w configure -state normal
    $w mark set oldInsert insert
    $w delete ${body}_s ${body}_e
    $w mark set insert ${body}_s
    $w mark gravity ${body}_e right

    set fh(mode,$body) $charset
    set msgInfo(show,$body,how) $charset
    $w insert insert [$body data false $charset] $tag
    $w mark set insert oldInsert
    $w configure -state $oldState
    $w mark gravity ${body}_e left
}

# ShowTextEnriched --
#
# Show text/enriched entities
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowTextEnriched {handler body msg} {
    global option b charsetName propNormFont
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    set tag t[incr fh(id)]
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
			        \[lsearch \[set ${handler}(struct_list)\] \
			        $body\]"

    if {"::rat_enriched::show" == [info commands ::rat_enriched::show]} {
	rat_enriched::show $handler [$body data false] $tag
    } else {
	$handler insert insert [$body data false] $tag
    }
}

# ShowTextOther --
#
# Show text parts other that text/plain.
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowTextOther {handler body msg} {
    global t b fixedBoldFont
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    set type [$body type]
    set typename [string tolower [lindex $type 0]/[lindex $type 1]]
    set mailcap [$body findShowCommand]
    set width 0

    set w $handler.w[incr fh(id)]
    frame $w -relief raised -bd 2 -cursor top_left_arrow
    if {[string length [lindex $mailcap 4]]} {
	if {![catch {image create bitmap $body.icon \
		-file [lindex $mailcap 4]}]} {
	    label $w.icon -image $body.icon
	    pack $w.icon -side left
	}
    }
    text $w.text -relief flat -cursor top_left_arrow
    $w.text tag configure Bold -font $fixedBoldFont
    if {[string length [$body description]]} {
	set width [string length "$t(description): [$body description]"]
	$w.text insert insert "$t(description): " Bold [$body description]\n
    }
    if {[string length [lindex $mailcap 3]]} {
	set l [string length "$t(type_description): [lindex $mailcap 3]"]
	if { $l > $width } {
	    set width $l
	}
	$w.text insert insert "$t(type_description): " Bold \
		[lindex $mailcap 3]\n
    }
    set typestring [string tolower [lindex $type 0]/[lindex $type 1]]
    set size [RatMangleNumber [$body size]]
    set l [string length "$t(here_is): $typestring ($size bytes)"]
    if { $l > $width } {
	set width $l
    }
    $w.text insert insert "$t(here_is): " Bold "$typestring  ($size bytes)" {}
    $w.text configure \
	    -height [lindex [split [$w.text index end-1c] .] 0] \
	    -width $width \
	    -state disabled
    pack $w.text -side top -anchor w

    if {[string length [lindex $mailcap 0]]} {
	button $w.view_ext -text $t(view) \
		-command "RunMailcap $body [list $mailcap]"
	pack $w.view_ext -side left -padx 10
    }
    if {![info exists msgInfo(show,$body,tp)]} {
	if {[string length [lindex $mailcap 0]]} {
	    set msgInfo(show,$body,tp) 0
	} else {
	    set msgInfo(show,$body,tp) 1
	}
    }
    checkbutton $w.tp -text $t(view_as_text) \
	    -relief raised \
	    -variable msgInfo_${msg}(show,$body,tp) \
	    -command "ShowTextOtherDo $handler $body $msg" \
	    -padx 4 -pady 4
    button $w.save -text $t(save_to_file) \
	    -command "SaveBody $body $fh(toplevel)"
    button $w.view_int -text $t(view_source) -command "ShowSource $body"
    pack $w.tp $w.save $w.view_int -side left -padx 5
    set b($w.tp) view_as_text
    set b($w.save) save_bodypart
    set b($w.view_int) view_source

    $handler window create insert -window $w -padx 5 -pady 5
    foreach win [concat $w [pack slaves $w]] {
	bind $win <3> "tk_popup $fh(struct_menu) %X %Y \
		       \[lsearch \[set ${handler}(struct_list)\] \
		       $body\]"
    }
    $handler insert insert "\n"

    $handler mark set ${body}_s insert
    $handler mark set ${body}_e insert
    $handler mark gravity ${body}_s left
    ShowTextOtherDo $handler $body $msg
}

# ShowTextOtherDo --
#
# Subfunction of ShowTextOther shows not/shows the text as text/plain
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowTextOtherDo {handler body msg} {
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo
    
    set oldState [$handler cget -state]
    $handler configure -state normal
    $handler delete ${body}_s ${body}_e
    $handler mark set insert ${body}_s
    $handler mark gravity ${body}_e right
    if {$msgInfo(show,$body,tp)} {
	ShowTextPlain $handler $body $msg
    }
    $handler mark set insert ${body}_e
    $handler mark gravity ${body}_e left
    $handler configure -state $oldState
}

# ShowMultiAlt --
#
# Show a multipart/alternative object
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowMultiAlt {handler body msg} {
    global GoodTypes
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    if {![info exists msgInfo(alternated,$body)]} {
	set found 0
	set msgInfo(alternated,$body) 1
	set children [$body children]
	foreach c $children {
	    set msgInfo(show,$c) 0
	}
	for {set i [expr {[llength $children]-1}]} {$i >= 0} {incr i -1} {
	    set child [lindex $children $i]
	    set tc $child
	    set typelist [$child type]
	    while {1 == [regexp -nocase multipart [lindex $typelist 0]]} {
		set tc [lindex [$tc children] 0]
		set typelist [$tc type]
	    }
	    set type [string tolower [lindex $typelist 0]/[lindex $typelist 1]]
	    if { -1 != [lsearch $GoodTypes $type] } {
		set msgInfo(show,$child) 1
		set found 1
	        break
	    }
	}
	if {!$found} {
	    foreach child [$body children] {
		set msgInfo(show,$child) 1
	    }
	}
    }

    foreach child [$body children] {
	ShowBody $handler $child $msg
    }
}

# ShowMultiEnc --
#
# Show a multipart/encrypted object (which we failed to decrypt)
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowMultiEnc {handler body msg} {
    upvar #0 $handler fh

    set children [$body children]
    if {[llength $children] != 2} {
	return
    }
    set b [lindex $children 1]
    set tag t[incr fh(id)]
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
			        \[lsearch \[set ${handler}(struct_list)\] \
			        $body\]"

    regsub -all "\r" [$b data false] "" data
    $handler insert insert $data $tag
}

# ShowMultiMixed --
#
# Show a multipart/mixed object
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name

proc ShowMultiMixed {handler body msg} {
    global option t
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    foreach child [$body children] {
	if {![info exists msgInfo(show,$child)]} {
	    set msgInfo(show,$child) $msgInfo(show,$msg)
	}
	ShowBody $handler $child $msg
    }
}

# ShowImage --
#
# Show image/* entities
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# subtype -	The type of image

proc ShowImage {handler body subtype} {
    global option rat_tmp
    global ImageTypes
    upvar #0 $handler fh

    if {[lsearch $ImageTypes $subtype] != "-1"} {
	set tag t[incr fh(id)]
	set filename $rat_tmp/rat.[RatGenId]
	set fid [open $filename w 0600]
	fconfigure $fid -encoding binary
	$body saveData $fid 0 0
	close $fid
	if {[catch {image create photo -file $filename} img]} {
	    Popup $img $fh(toplevel)
	    file delete -force -- $filename
	    ShowDefault $handler $body
	    return
	}
	file delete -force -- $filename
	set imgheight [image height $img]
	set imgwidth [image width $img]
	set frame [frame $handler.f[incr fh(id)] -cursor left_ptr]
	set c [canvas $frame.canvas -width $imgwidth \
		-height $imgheight \
		-xscrollcommand [list $frame.xscroll set] \
		-yscrollcommand [list $frame.yscroll set] \
		-scrollregion [list 0 0 $imgwidth $imgheight]]
	set yscroll [scrollbar $frame.yscroll -command [list $c yview]]
	set xscroll [scrollbar $frame.xscroll -command [list $c xview] \
		-orient horizontal]
	grid $c -row 0 -column 0 -sticky news
	if {[expr {$imgheight>[winfo height $handler]}]} {
	    $frame configure -height [winfo height $handler]
	    grid $yscroll -row 0 -column 1 -sticky ns
	} else {
	    $frame configure -height $imgheight
	}
	if {[expr {$imgwidth>[winfo width $handler]}]} {
	    $frame configure -width [winfo width $handler]
	    grid $xscroll -row 1 -column 0 -sticky ew
	} else {
	    $frame configure -width $imgwidth
	}
	grid columnconfigure $frame 0 -weight 1
	grid rowconfigure $frame 0 -weight 1
	grid propagate $frame 0
	$c create image 0 0 -image $img -anchor nw
	$handler insert insert "\n" $tag	;# virtual spacing1
	$handler insert insert " " "CenterNoSpacing $tag"
	$handler window create insert -window $frame
	$handler insert insert "\n\n" $tag	;# NL plus virtual spacing3
	lappend fh(show_cleanup_cmds) "image delete $img"
	$handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
		\[lsearch \[set ${handler}(struct_list)\] \
		$body\]"
	set binding [list ResizeFrame $frame $handler \
		$imgheight $imgwidth $xscroll $yscroll]
	if {[string first $binding [bind $handler <Configure>]] == -1} {
	    bind $handler <Configure> +$binding
	}
	bind $c <3> "tk_popup $fh(struct_menu) %X %Y \
		\[lsearch \[set ${handler}(struct_list)\] \
		$body\]"
	# Avoid processing new events (like selecting new message)
	update idletasks
	ResizeFrame $frame $handler \
		$imgheight $imgwidth $xscroll $yscroll
    } else {
        ShowDefault $handler $body
    }
}


# ShowDSN --
#
# Show message/delivery-status entities
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The body which contains this DSN

proc ShowDSN {handler body} {
    global t
    upvar #0 $handler fh

    set tag t[incr fh(id)]
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
				 \[lsearch \[set ${handler}(struct_list)\] \
				 $body\]"
    $handler insert insert \n $tag
    set dsn [$body dsn]
    foreach h [lindex $dsn 0] {
	set hn [lindex $h 0]
	if {[info exists t([string tolower $hn])]} {
	    set hn $t([string tolower $hn])
	}
	$handler insert insert [format "%21s: " $hn] "$tag HeaderName"
	$handler insert insert [lindex $h 1]\n $tag
    }
    foreach r [lindex $dsn 1] {
	$handler insert insert \n $tag
	foreach h $r {
	    set hn [lindex $h 0]
	    if {[info exists t([string tolower $hn])]} {
		set hn $t([string tolower $hn])
	    }
	    $handler insert insert [format "%21s: " $hn] "$tag HeaderName"
	    $handler insert insert [lindex $h 1]\n $tag
	}
    }
    $handler insert insert \n $tag
}

# ShowPGPKeys --
#
# Handles embedded pgp keys
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show

proc ShowPGPKeys {handler body} {
    global t b
    upvar #0 $handler fh

    set mailcap [$body findShowCommand]

    set w $handler.w[incr fh(id)]
    frame $w -relief raised -bd 2 -cursor top_left_arrow
    if {[string length [lindex $mailcap 4]]} {
	if {![catch {image create bitmap $body.icon \
		-file [lindex $mailcap 4]}]} {
	    label $w.icon -image $body.icon
	    pack $w.icon -side left
	}
    }
    label $w.label -relief flat -text $t(embedded_pgp_keys)
    pack $w.label -side top -anchor w

    button $w.add -text $t(add_to_keyring) \
	    -command "RatPGP add \[$body data 0\]"
    button $w.save -text $t(save_to_file) \
	    -command "SaveBody $body $fh(toplevel)"
    button $w.view_int -text $t(view_source) -command "ShowSource $body"
    set b($w.add) add_to_keyring
    set b($w.save) save_bodypart
    set b($w.view_int) view_source
    pack $w.add $w.save $w.view_int -side left -padx 5
    $handler window create insert -window $w -padx 5 -pady 5
    foreach win [concat $w [pack slaves $w]] {
	bind $win <3> "tk_popup $fh(struct_menu) %X %Y \
		       \[lsearch \[set ${handler}(struct_list)\] \
		       $body\]"
    }
    $handler insert insert "\n"
}

# ShowDefault --
#
# The default type shower, just iserts a marker that here is an object
# of this type.
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show

proc ShowDefault {handler body} {
    global t fixedBoldFont
    upvar #0 $handler fh

    set type [$body type]
    set typename [string tolower [lindex $type 0]/[lindex $type 1]]
    set mailcap [$body findShowCommand]
    set width 0

    set w $handler.w[incr fh(id)]
    frame $w -relief raised -bd 2 -cursor top_left_arrow
    if {[string length [lindex $mailcap 4]]} {
	if {![catch {image create bitmap $body.icon \
		-file [lindex $mailcap 4]}]} {
	    label $w.icon -image $body.icon
	    pack $w.icon -side left
	}
    }
    text $w.text -relief flat -cursor top_left_arrow
    $w.text tag configure Bold -font $fixedBoldFont
    if {[string length [$body description]]} {
	set width [string length "$t(description): [$body description]"]
	$w.text insert insert "$t(description): " Bold [$body description]\n
    }
    if {[string length [lindex $mailcap 3]]} {
	set l [string length "$t(type_description): [lindex $mailcap 3]"]
	if { $l > $width } {
	    set width $l
	}
	$w.text insert insert "$t(type_description): " \
		Bold [lindex $mailcap 3]\n
    }
    set l [string length "$t(here_is): [string tolower [lindex $type 0]/[lindex $type 1]] ([RatMangleNumber [$body size]] bytes)"]
    if { $l > $width } {
	set width $l
    }
    $w.text insert insert "$t(here_is): " Bold \
	    "[string tolower [lindex $type 0]/[lindex $type 1]]" {} \
	    " ([RatMangleNumber [$body size]] bytes)"
    $w.text configure \
	    -height [lindex [split [$w.text index end-1c] .] 0] \
	    -width $width \
	    -state disabled
    pack $w.text -side top -anchor w

    if {[string length [lindex $mailcap 0]]} {
	button $w.view_ext -text $t(view) \
		-command "RunMailcap $body {$mailcap}"
	set b($w.view_ext) run_mailcap
	pack $w.view_ext -side left -padx 10
    }
    button $w.save -text $t(save_to_file) \
	    -command "SaveBody $body $fh(toplevel)"
    button $w.view_int -text $t(view_source) -command "ShowSource $body"
    set b($w.save) save_bodypart
    set b($w.view_int) view_source
    pack $w.save $w.view_int -side left -padx 5
    $handler window create insert -window $w -padx 5 -pady 5
    foreach win [concat $w [pack slaves $w]] {
	bind $win <3> "tk_popup $fh(struct_menu) %X %Y \
		       \[lsearch \[set ${handler}(struct_list)\] \
		       $body\]"
    }
    $handler insert insert "\n"
}

# ShowHome --
#
# Scroll to the top of the document
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowHome {handler} {
    $handler yview moveto 0
}

# ShowBottom --
#
# Scroll to the bottom of the document
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowBottom {handler} {
    $handler yview moveto 1
}

# ShowPageDown --
#
# Move the show one page down
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowPageDown {handler} {
    $handler yview scroll 1 pages
}

# ShowPageUp --
#
# Move the show one page up
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowPageUp {handler} {
    $handler yview scroll -1 pages
}

# ShowLineDown --
#
# Move the show one line down
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowLineDown {handler} {
    $handler yview scroll 1 units
}

# ShowLineUp --
#
# Move the show one line up
#
# Arguments:
# handler -	The handler which identifies the show text widget

proc ShowLineUp {handler} {
    $handler yview scroll -1 units
}

# SaveBody --
#
# Save a bodypart to file.
#
# Arguments:
# body -	The bodypart to save
# parent -	Parent of window

proc SaveBody {body parent} {
    global idCnt t option

    set convertNL [regexp -nocase text [lindex [$body type] 0]]

    set filename [rat_fbox::run -ok $t(save) -mode save -parent $parent \
	    -title $t(save_to_file) -initialfile [$body filename]]
    if {"" == $filename} {
	return
    }

    if { 0 == [catch [list open $filename w] fh]} {
	$body saveData $fh false $convertNL
	close $fh
    } else {
	RatLog 4 "$t(save_failed): $fh"
    }
}

# BuildStructMenu
#
# Builds the structure menu
#
# Arguments:
# handler -	The handler which identifies the show text widget
# m       -	The menu to build
# msg     -	The message we should display the structure of

proc BuildStructMenu {handler msg} {
    upvar #0 $handler fh \
	msgInfo_$msg msgInfo

    # Clear the old menu
    $fh(struct_menu) delete 0 end
    foreach slave [winfo children $fh(struct_menu)] {
	destroy $slave
    }
    set fh(struct_list) {}

    # Check if we got anything
    if {![string length $msg]} { return }

    if {$msgInfo(browse)} { return }

    BuildStructEntry $handler $fh(struct_menu) $msg [$msg body] ""
    FixMenu $fh(struct_menu)
}

# BuildStructEntry
#
# Builds an entry in the structure menu
#
# Arguments:
# handler -	The handler which identifies the show text widget
# m       -	The menu to build
# msg     -	The message we should display the structure of
# body    -	The bodypart to describe
# preamble-	The preamble to add before this entry

proc BuildStructEntry {handler m msg body preamble} {
    global b
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    if {![string length $body]} { return }

    lappend fh(struct_list) $body
    set sm $m.m[incr fh(id)]
    set typepair [$body type]
    set type [string tolower [lindex $typepair 0]/[lindex $typepair 1]]
    $m add cascade -label $preamble$type -menu $sm
    set b($m,[$m index end]) bodypart_entry
    menu $sm -tearoff 0 \
    	     -disabledforeground [$m cget -activeforeground] \
	     -postcommand "PopulateStructEntry $handler $sm $msg $body $type"


    # See if we have children to show
    switch -glob $type {
    message/rfc822
        {
	    if {[catch {$body message} message]} {
		return
	    }
	    set fh(struct_list) [lreplace $fh(struct_list) end end $message]
	    if {[string length $message]} {
		BuildStructEntry $handler $m $message [$message body] \
			"$preamble  "
	    }
        }
    multipart/*
        {
            foreach c [$body children] {
                BuildStructEntry $handler $m $msg $c "$preamble  "
            }
        }
    }
}

# PopulateStructEntry --
#
# Populates a struct menu entry
#
# Arguments:
# handler - The handler which identifies the show text widget
# m       - Menu to populate
# msg  	  - Message the body belongs to
# body 	  - Body to use
# type 	  - Type of body

proc PopulateStructEntry {handler m msg body type} {
    global t b showParams
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    $m delete 0 end

    if {[string length [$body description]]} {
        $m add command -label [$body description] -state disabled
    }
    set sigstatus [$body sigstatus]
    if {[string compare pgp_none $sigstatus]} {
	$m add command -label "$t(signature): $t($sigstatus)" -state disabled
    }
    if {[$body encoded]} {
	$m add command -label "$t(decoded)" -state disabled
    }

    # Do parameters
    if {[info exists showParams($type)]} {
        set sp $showParams($type)
        foreach param $sp {
            set value [$body parameter $param]
            if {[string length $param]} {
		$m add command -label "$t($param): $value" -state disabled
            }
        }
    } else {
        set sp {{}}
    }
    $m add command -label "$t(size): [RatMangleNumber [$body size]]" \
    		    -state disabled
    $m add separator
    if {![info exists msgInfo(show,$body)]} {
	set msgInfo(show,$body) 1
    }
    $m add checkbutton -label $t(show) -onvalue 1 -offvalue 0 \
    	    -variable msgInfo_${msg}(show,$body) \
    	    -command "Show $handler $fh(current) 0"
    set b($m,[$m index end]) bodypart_show
    $m add command -label $t(save_to_file)... \
	    -command "SaveBody $body $fh(toplevel)"
    set b($m,[$m index end]) bodypart_save
    $m add command -label $t(view_source)... -command "ShowSource $body"
    set b($m,[$m index end]) view_source
    bind $m <Unmap> "after idle {if {\[winfo exists $m\]} {$m delete 0 end}}"
}

# RunMailcap --
#
# Runs the mailcap entry for a body
#
# Arguments:
# body    -	The handler which identifies the entity to show
# mailcap -	The mailcap entry to use

proc RunMailcap {body mailcap} {
    global option t idCnt rat_tmp

    set id id[incr idCnt]
    upvar #0 $id rm
    set cmd "sh -c {[lindex $mailcap 0]}"

    # Fix files
    set rm(fileName) $rat_tmp/rat.[RatGenId]
    if {[regexp %s $cmd]} {
	regsub -all %s $cmd $rm(fileName) cmd
    } else {
	if {[lindex $mailcap 1]} {
	    Popup "$t(cant_pipe): \"$cmd\""
	    unset rm
	    return
	}
	set cmd "cat $rm(fileName) | $cmd"
    }
    set f [open $rm(fileName) w]
    $body saveData $f 0 0
    close $f
    if {[lindex $mailcap 1]} {
	# This command needs a terminal
	RatBgExec ${id}(existStatus) "$option(terminal) $cmd"
    } elseif {[lindex $mailcap 2]} {
	# This command produces lots of output
	set w .$id
	toplevel $w -class TkRat
	wm title $w $t(external_viewer)
	text $w.text \
		-yscroll "$w.scroll set" \
		-relief sunken -bd 1 \
		-setgrid 1 \
		-highlightthickness 0
	Size $w.text extView
	scrollbar $w.scroll \
		-relief sunken \
		-bd 1 \
		-command "$w.text yview" \
		-highlightthickness 0
	button $w.button -text $t(dismiss) -command "RecordPos $w extView; \
		RecordSize $w.text extView; destroy $w"
	pack $w.button -side bottom -pady 5
	pack $w.scroll -side right -fill y
	pack $w.text -expand 1 -fill both
	Place $w extView
	$w.text insert insert [eval "exec $cmd"]
	$w.text configure -state disabled
	file delete -force -- $rm(fileName) &
	unset rm
	return
    } else {
	# This command manages its own output
	RatBgExec ${id}(existStatus) $cmd
    }
    trace variable rm(existStatus) w RunMailcapDone
}

# RunMailcapDone --
#
# This gets called when the show command has run and should clean
# things up.
#
# Arguments:
# name1, name2 -        Variable specifiers
# op           -        Operation

proc RunMailcapDone {name1 name2 op} {
    upvar #0 $name1 rm

    after 30000 "file delete -force -- $rm(fileName)"
    unset rm
}

# SubstArg --
#
# replace all occurences of the symbolic argument (typically, "%u") by
# the effective argument
#
# we profitably avoid the painful 'regsub' function for simple
# substitutions like this one: it brings incredibly tricky
# backslashing issues when the $url contains "&" or "\n". See tcl
# regsub manual page for details.
#
# Arguments:
# cmd       - original command
# symbolArg - string to search and replace with...
# realArg   - ... the effective argument

proc SubstArg {cmd symbolArg realArg} {
    set si end
    while {[set urlArgPos [string last $symbolArg $cmd $si]] != -1} {
	set cmd \
	    [string replace \
		 $cmd \
		 $urlArgPos \
		 [expr $urlArgPos + [string length $symbolArg] -1] \
		 $realArg]
	set si [expr $urlArgPos-1]
    }
    return $cmd
}

# RatShowURL --
#
# Invoke an URL browser.
#
# Arguments:
# url     -	URL to browse
# win     -	Text widget containing URL
# tag     -	Tag of url in text widget
# x, y	  -	Mouse coordinates

proc RatShowURL {url win tag x y} {
    global option urlstatus

    # Flash URL for feedback
    $win tag configure $tag -underline 1
    update idletasks

    # Check if we should abort
    if {-1 == [lsearch -exact [$win tag names @$x,$y] $tag]} {
	return
    }
    RatShowURLLaunch $url $win
}

# RatShowURLLaunch --
#
# Actually launch the URL browser.
#
# Arguments:
# url     -	URL to browse
# win     -	Text widget containing URL

proc RatShowURLLaunch {url win} {
    global option urlstatus

    regsub -all , $url %2c url
    regsub -all {\$} $url %24 url
    regsub -all {&} $url {\\&} url
    # Start viewer
    switch -regexp $option(url_viewer) {
	RatUP {
	    if {[catch {RatUP_ShowURL $url} error]} {
		Popup $error [winfo toplevel $win]
	    }
	}
        netscape|opera {
	    trace variable urlstatus($url) w [list RatMaybeStartBrowser $url]
	    RatBgExec urlstatus($url) "$option($option(url_viewer)) -remote \
		    \"openURL($url,new-window)\" >&/dev/null"
	}
        lynx {
	    set cmd [SubstArg $option(lynx) "%u" $url]
	    if {[catch {eval exec $cmd &} error]} {
		Popup $error [winfo toplevel $win]
	    }
	}
        other {
	    set cmd [SubstArg $option(other_browser) "%u" $url]
	    if {[catch {eval exec $cmd &} error]} {
		Popup $error [winfo toplevel $win]
	    }
	}
    }
}

# RatMaybeStartBrowser --
#
# Start browser, if the first invokation failed.
#
# Arguments:
# url		   - URL to show
# name1, name2, op - trace information

proc RatMaybeStartBrowser {url name1 name2 op} {
    upvar #0 ${name1}($name2) status

    trace vdelete ${name1}($name2) w [list RatMaybeStartBrowser $url]
    if {0 == $status} {
	unset status
	return
    }

    unset status
    global option
    if {[catch {eval exec $option($option(url_viewer)) "$url" &} error]} {
	Popup $error
    }
}

# RatFindURL --
#
# Search for an URL in a part of a text widget
#
# Arguments:
# t	- Text widget
# start - Index to start searching at
# end   - Index to end searching at

proc RatFindURL {t start end} {
    global option

    set exp1 "<([join $option(urlprot) |]):(\[^>\])*>"
    set exp2 "([join $option(urlprot) |])://(\[a-zA-Z0-9\\-\\.@:\]+)+(:(\[0-9\]+))?(/\[^ \"]*)?\[a-zA-Z0-9/%#\]"
    set found [$t search -nocase -regexp -count len "($exp1)|($exp2)" \
		   $start $end]
    if {[string length $found]} {
	if {"<" == [$t get $found]} {
	    return [list $found+1c $found+${len}c-1c]
	} else {
	    return [list $found $found+${len}c]
	}
    } else {
	return {}
    }
}

# RatScrollShow --
#
# Scroll the show text widget and search for URL's if needed
#
# Arguments:
# t       - The text widget
# scrollw - The scrollbar widget
# sfrac   - The new start of shown section
# efrac   - The new end of shown section

proc RatScrollShow {t scrollw sfrac efrac} {
    $scrollw set $sfrac $efrac

    if {[$t compare searched >= @0,20000]} {
	return
    }

    upvar #0 $t fh

    set start searched
    set end [$t index @0,20000+1c+20l]

    set result [RatFindURL $t $start $end]
    while {[llength $result] == 2} {
	set tag t[incr fh(id)]
	set s [lindex $result 0]
	set e [lindex $result 1]
	$t tag add URL $s $e
	$t tag add $tag $s $e
	regsub -all % [$t get $s $e] %% url
	$t tag bind $tag <ButtonRelease-1> [list RatShowURL $url %W $tag %x %y]
	$t tag bind $tag <1> \
		"$t tag configure $tag -underline 0; update idletasks"
	set result [RatFindURL $t $e $end]
    }
    $t mark set searched $end
}

# ResizeFrame --
#
# Resize frames containing scrolled information
#
# Arguments:
# frame   - The frame to resize
# parent  - The frame's parent
# reqHeight - required height to see the scrollable item completely
# reqWidth  - required width to see the scrollable item completely
# xscroll   - The X axis scrollbar
# yscroll   - The Y axis scrollbar
#
# Note:
# A value of -1 for reqWidth or reqHeight will show both scrollbars all the
# time. The frame will then take the entire available space of the parent.

proc ResizeFrame {frame parent reqHeight reqWidth xscroll yscroll} {
    set parWidth [winfo width $parent]
    set parHeight [winfo height $parent]
    set totalHeight $reqHeight
    set totalWidth $reqWidth

    if {![winfo exists $frame]} {
        return
    }

    # Take care of the easy stuff first. If reqWeight or reqHeight is -1 then
    # we *always* want scrollbars
    
    if {$reqHeight == -1 || $reqHeight == -1} {
        $frame configure -height $parHeight -width $parWidth
        return
    }

    # Determine if we need scrollbars. The deal is that if reqWidth or
    # reqHeight is -1 then always put the scrollbars
    if {[expr {$reqHeight > $parHeight}]} {
        incr totalWidth [winfo width $yscroll]
    }
    if {[expr {$reqWidth > $parWidth}]} {
        incr totalHeight [winfo height $xscroll]
    }

    if {[expr {$totalHeight > $parHeight}]} {
        $frame configure -height $parHeight -bg blue
        grid $yscroll -row 0 -column 1 -sticky ns
    } else {
        $frame configure -height $totalHeight
        grid forget $yscroll
    }

    if {[expr {$totalWidth > $parWidth}]} {
        $frame configure -width $parWidth
        grid $xscroll -row 1 -column 0 -sticky ew
    } else {
        $frame configure -width $totalWidth
        grid forget $xscroll
    }
}

# GetHandlerFromExtension --
#
# Determines the real MIME type base on the extension of the file 
# in the attachement. This should only be called if the type is 
# application/octet-stream.
#
# Arguments:
# filename   - The name of the file when saved
#
# Returns:
#   The correct MIME type if it is one known by TkRat. Otherwise, returns
#   application/octet-stream

proc GetHandlerFromExtension {filename} {
	global mimeHandler
	set mimeType [list application octet-stream]
	set extension [string tolower [file extension $filename]]
	# Remove the '.' in the extension
	set extension [string trim $extension .]

	# Check image types
	if {[info exists mimeHandler(image/$extension)]} {
		set mimeType [list image $extension]
	} elseif {[string equal jpg $extension]} {
		# JPEGs often end in .jpg
		set mimeType [list image jpeg]
	}

	return $mimeType
}
