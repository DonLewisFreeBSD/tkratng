# html.tcl --
#
# This file contains code which handles the actual displaying of an HTML
# message or attachment
#
#
#  TkRat software and its included text is Copyright 1996-2000 by
#  Martin Forssén
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

# ShowTextHtml --
#
# Show text/html entities, should handle different fonts...
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name
proc ShowTextHtml {handler body msg} {
    upvar #0 $handler fh \
    	     msgInfo_$msg msgInfo

    set tag t[incr fh(id)]
    set frame [frame $handler.f[incr fh(id)] -width [winfo width $handler]\
	    -height [winfo height $handler] -cursor left_ptr]
    # -base foo is there because if it is removed, Tkhtml crashes. When the
    # bug is fixed, it can be removed.
    set htmlwin [html $frame.html -base "foo" -fontcommand HtmlFontCmd \
	    -resolvercommand HtmlResolverCmd \
	    -imagecommand HtmlImageCmd \
	    -xscrollcommand [list $frame.xscroll set] \
	    -yscrollcommand [list $frame.yscroll set]]
    $htmlwin parse [$body data false]
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
    $handler insert insert " " "Center $tag"
    $handler window create insert -window $frame
    $handler insert insert "\n" $tag
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
				 \[lsearch \[set ${handler}(struct_list)\] \
				 $body\]"
    set binding [list ResizeFrame $frame $handler -1 -1 \
           $xscroll $yscroll]
    if {[string first $binding [bind $handler <Configure>]] == -1} {
        bind $handler <Configure> +$binding
	}
    bind $htmlwin.x <3> "tk_popup $fh(struct_menu) %X %Y \
			  \[lsearch \[set ${handler}(struct_list)\] \
			  $body\]"
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
    set f $option(html_prop_font)
    set sizelist $option(html_prop_font_sizes)
    # Default weight is Normal
    set w normal
    # Default angle is roman
    set a roman

    foreach o $args {
        if {[string equal "fixed" "$o"]} {
            set f $option(html_fixed_font)
	    set sizelist $option(html_fixed_font_sizes)
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
# src: SRC element of the <IMG> tag
# width: width of the image (added automatically, could be empty)
# height: height of the image (added automatically, could be empty)
# args: Other attributes given to the <IMG> tag
#
# Returns:
#   The name of an image if it could be constructed correctly, an empty string
#   otherwise
proc HtmlImageCmd {src width height args} {
    global option
    if {$option(html_show_images) == 0 || ![string match http://* $src]} {
	return ""
    }
    if {[catch {::http::geturl $src} token]} {
	return ""
    }
    
    set filename $option(tmp)/rat.[RatGenId]
    set fid [open $filename w 0600]
    fconfigure $fid -encoding binary
    puts -nonewline $fid [::http::data $token]
    close $fid
    
    if {[catch {image create photo -file $filename} img]} {
	file delete -force -- $filename
	return ""
    }
    file delete -force -- $filename
    return $img
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
    return "foo"
}
