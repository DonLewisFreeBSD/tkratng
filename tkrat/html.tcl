# html.tcl --
#
# This file contains code which handles the actual displaying of an HTML
# message or attachment
#
#
#  TkRat software and its included text is Copyright 1996-2000 by
#  Martin Forss�n
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Don't fail if the http package isn't available. It'll just fail when it
# comes time to fetch the image
catch {package require http}

bind HtmlClip <Motion> {
    global htmlWinCursor
    set parent [winfo parent %W]
    set url [$parent href %x %y] 
    if {![info exists htmlWinCursor($parent)]} {
	set htmlWinCursor($parent) [lindex [$parent configure -cursor] end]
    }
    if {[string length $url] > 0} {
	if {[string length $htmlWinCursor($parent)] == 0} {
	    set htmlWinCursor($parent) "hand2"
	    $parent configure -cursor $htmlWinCursor($parent)
	}
    } else {
	if {[string length $htmlWinCursor($parent)] > 0} {
	    set htmlWinCursor($parent) ""
	    $parent configure -cursor {}
	}
    }
}
bind HtmlClip <Button-1> {
    set ::htmlWinClick [[winfo parent %W] href %x %y]
}
bind HtmlClip <ButtonRelease-1> {
    if { ![string compare $::htmlWinClick [[winfo parent %W] href %x %y]]} {
	set url $::htmlWinClick
	RatShowURLLaunch $url [winfo parent [winfo parent %W]]
    }
}
bind Html <Destroy> {
	ClearHtmlImages %W
}


# Contains the list of most recently used images
set htmlImageList [list]

# ShowTextHtml2 --
#
# Show text/html entities, should handle different fonts...
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name
proc ShowTextHtml2 {handler body msg} {
    global idCnt
    upvar \#0 $handler fh \
        msgInfo_$msg msgInfo

    set tag t[incr idCnt]
    if {[info tclversion] < 8.5} {
        set frame [frame $handler.f[incr idCnt] -width [winfo width $handler]\
                       -height [winfo height $handler] -cursor left_ptr]
        set htmlwin $frame.html
    } else {
        set htmlwin $handler.f[incr idCnt]
    }
    # -base foo is there because if it is removed, Tkhtml crashes. When the
    # bug is fixed, it can be removed.
    html $htmlwin -base "foo" \
        -fontcommand HtmlFontCmd \
        -resolvercommand HtmlResolverCmd \
        -imagecommand [list HtmlImageCmd $htmlwin] \
        -background [$handler cget -background] \
        -width [winfo width $handler] \
        -exportselection true \
        -bd 0
    $htmlwin parse [$body data false]
    # Now that the data is parsed, check if there is a base set
    set base [$htmlwin token find base]
    if {[llength $base] > 0} {
	# Ok, the correct base is the first one found. Since it is a list, get
	# it.
	set base [lindex $base 0]
	# The base will be right after the href argument
	set idx [lsearch $base href]
	incr idx
	# Get the real base
	set base [lrange $base $idx $idx]
	# set the base of the widget with the correct version now
	$htmlwin configure -base $base
    }
    $handler insert insert " " "Center $tag"
    if {[info tclversion] < 8.5} {
        $htmlwin configure \
            -xscrollcommand [list $frame.xscroll set] \
            -yscrollcommand [list $frame.yscroll set]
        set yscroll [scrollbar $frame.yscroll -command [list $htmlwin yview]]
        set xscroll [scrollbar $frame.xscroll -command [list $htmlwin xview] \
                         -orient horizontal]
        bind $frame <Destroy> {
            bind [winfo parent %W] <Configure> {}
        }
        grid $htmlwin -row 0 -column 0 -sticky news
        grid $yscroll -row 0 -column 1 -sticky ns
        grid $xscroll -row 1 -column 0 -sticky ew
        grid columnconfigure $frame 0 -weight 1
        grid rowconfigure $frame 0 -weight 1
        grid propagate $frame 0
        $handler window create insert -window $frame
        set binding [list ResizeFrame $frame $handler -1 -1 \
                         $xscroll $yscroll]
        if {[string first $binding [bind $handler <Configure>]] == -1} {
            bind $handler <Configure> +$binding
        }
    } else {
        $handler window create insert -window $htmlwin

        # This is ugly. For some reason does the widget not know its size
        # when the Configure event arrives here. But after a short delay
        # it does.
        bind $htmlwin <Configure> {after 100 {HtmlReconfHeight %W}}
    }
    $handler insert insert "\n" $tag
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
				 \[lsearch \[set ${handler}(struct_list)\] \
				 $body\]"
    bind $htmlwin.x <3> "tk_popup $fh(struct_menu) %X %Y \
			  \[lsearch \[set ${handler}(struct_list)\] \
			  $body\]"

    lappend fh(width_adjust) $htmlwin
}

# HtmlReconfHeight --
#
# Reconfigures the height of the html widget to whatever is needed to
# show the text
#
# Arguments:
# w - html widget

proc HtmlReconfHeight {w} {
    if {[winfo exists $w]} {
        set h [lindex [$w coords] 1]
        $w configure -height $h
    }
}


# HtmlFontCmd --
#
# Selects font sizes when dislaying html messages
#
# Arguments:
# size: Size of font to display
# args: Other font modifiers (italic bold or fixed)
proc HtmlFontCmd {size args} {
    global option

    # Default family and sizes
    set f $option(font_family_prop)
    foreach s {8 9 10 12 14 18 24} {
        lappend sizelist [expr $s+$option(font_size)-12]
    }
    # Default weight is Normal
    set w normal
    # Default angle is roman
    set a roman

    foreach o $args {
        if {[string equal "fixed" "$o"]} {
            set f $option(font_family_fixed)
        } elseif {[string equal "bold" "$o"]} {
            set w bold
        } elseif {[string equal "italic" "$o"]} {
            set a italic
        }
    }
    # Make sure the list is long enough. If it isn't, use the last value
    if {[llength $sizelist] < $size} {
	set size end
    } else {
	# Decrease the size since the lowest value allowed is 1 and
	# list indices start at 0
	incr size -1
    }
    # Ugh. RatCreateFont already constructs all the components of the font. So
    # we're actually removing information and adding it back just to change the
    # size. Maybe there's a better way.
    return [list [lindex $f 1] [lindex $sizelist $size] $a $w]
}

# HtmlImageCmd --
#
# Fetches and creates an image to display in a HTML message
#
# Arguments:
# frm: The HTML widget used to display images
# src: SRC element of the <IMG> tag
# width: width of the image (added automatically, could be empty)
# height: height of the image (added automatically, could be empty)
# args: Other attributes given to the <IMG> tag
#
# Returns:
#   The name of an image if it could be constructed correctly, an empty string
#   otherwise
proc HtmlImageCmd {frm src width height args} {
    global htmlImageList
    global htmlImageArray
    global HtmlImages
    
    # Don't do anything if the html widget has been destroyed
    if {![winfo exists $frm]} {
	return
    }

    # Check cached images
    if {[lsearch $htmlImageList $src] != -1} {
	return $htmlImageArray($src)
    }

    if {[string match foo/cid:* $src]} {
        set filename [HtmlGetEmbeddedImage $src]
    } else {
        set filename [HtmlGetExternalImage $frm $src $width $height]
    }

    if {"" == $filename} {
        return ""
    }
    
    if {[catch {image create photo -file $filename} img]} {
	file delete -force -- $filename
        set retVal ""
    } else {
	lappend htmlImageList $src
	set htmlImageArray($src) $img
	file delete -force -- $filename
	# Make sure the window still exists before displaying
	if {[winfo exists $frm]} {
	    lappend HtmlImages($frm) $img
	    set retVal $img
	} else {
	    # Otherwise, delete the image
	    image delete $img
	    return
	}
    }
    
    return $retVal
}

# HtmlGetEmbeddedImage --
#
# Extract an image from an related bodypart
#
# Arguments:
# src: SRC element of the <IMG> tag
#
# Returns:
#   The name of a file which contains the image data. Or an empty string
#   if no image was downloaded.
proc HtmlGetEmbeddedImage {src} {
    global related option rat_tmp

    if {![regsub "foo/cid:" $src {} id]
        || ![info exists related($id)]} {
        return ""
    }

    set filename $rat_tmp/rat.[RatGenId]
    set fid [open $filename w 0600]
    fconfigure $fid -encoding binary
    $related($id) saveData $fid false false
    close $fid

    return $filename
}

# HtmlGetExternalImage --
#
# Fetches an external image to display in a HTML message
#
# Arguments:
# frm: The HTML widget used to display images
# src: SRC element of the <IMG> tag
# width: width of the image (added automatically, could be empty)
# height: height of the image (added automatically, could be empty)
#
# Returns:
#   The name of a file which contains the image data. Or an empty string
#   if no image was downloaded.
proc HtmlGetExternalImage {frm src width height} {
    global option rat_tmp

    if {$option(html_show_images) == 0} {
        return ""
    }
    
    if {$width < $option(html_min_image_size) 
	&& $height < $option(html_min_image_size)} {
        # Images that are too small may signal some spam-type of stuff
	return ""
    }

    if {![string match http://* $src]} {
	if {![string match http://* [$frm cget -base]]} {
            # Can't get image because it isn't http
	    return ""
	} else {
	    set src [$frm cget -base]/$src
	}
    }
    
    if {[catch {::http::geturl $src} token]} {
	return ""
    }
    
    set filename $rat_tmp/rat.[RatGenId]
    set fid [open $filename w 0600]
    fconfigure $fid -encoding binary
    puts -nonewline $fid [::http::data $token]
    close $fid

    return $filename
}

# HtmlResolverCmd --
#
# URL resolver for HTML links
#
# Arguments:
# base: The base URI
# uri: the new URI
#
# Returns:
#   The URL if it starts with http://, otherwise returns foo
proc HtmlResolverCmd {base uri} {
    if {[string match http://* $uri]} {
	return $uri
    }
    return $base/$uri
}

# ClearHtmlImages --
#
# Delete images loaded by the HTML widget
#
# Arguments:
# w: Name of widget containing the images
#
# Returns:
# Nothing
proc ClearHtmlImages {w} {
    global HtmlImages

    if {![info exists HtmlImages($w)]} {
	return
    }

    foreach img $HtmlImages($w) {
	catch {image delete $img}
    }
    unset HtmlImages($w)
    return "foo"
}
