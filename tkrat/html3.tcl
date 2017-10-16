# html.tcl --
#
# This file contains code which handles the actual displaying of an HTML
# message or attachment
#
#
#  TkRat software and its included text is Copyright 1996-2006 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

# Don't fail if the http package isn't available. It'll just fail when it
# comes time to fetch the image
catch {package require http}

namespace eval rat_html3 {
}

# ShowTextHtml --
#
# Show text/html entities, should handle different fonts...
#
# Arguments:
# handler -	The handler which identifies the show text widget
# body    -	The bodypart to show
# msg     -	The message name
proc ShowTextHtml3 {handler body msg} {
    global idCnt option
    upvar \#0 $handler fh \
        msgInfo_$msg msgInfo

    # Window name
    if {[info tclversion] < 8.5} {
        set frame [frame $handler.f[incr idCnt] -cursor left_ptr \
                       -width [winfo width $handler]\
                       -height [winfo height $handler]]
        set htmlwin $frame.html
    } else {
        set htmlwin $handler.f[incr idCnt]
    }

    # Font sizes
    set s $option(font_size)
    for {set i 0} {$i < 4} {incr i} {
        if {$s > 10} {
            incr s -2
        } else {
            incr s -1
        }
    }
    for {set i 0} {$i < 7} {incr i} {
        lappend fonttable $s
        if {$s > 9} {
            incr s 2
        } else {
            incr s
        }
    }

    html $htmlwin \
        -shrink true \
        -fonttable $fonttable \
        -imagecmd [list rat_html3::imagecmd $htmlwin] \
        -width [winfo width $handler]

    $htmlwin parse -final [$body data false]

    set tag t[incr idCnt]
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
        set id [$handler window create insert -window $frame]
        set binding [list ResizeFrame $frame $handler -1 -1 \
                         $xscroll $yscroll]
        if {[string first $binding [bind $handler <Configure>]] == -1} {
            bind $handler <Configure> +$binding
        }
    } else {
        set id [$handler window create insert -window $htmlwin]

        set bbox [$htmlwin bbox [$htmlwin node]]
        set width [expr [lindex $bbox 2] - [lindex $bbox 0]]
        $htmlwin configure -width $width
        if {$width > $fh(width)} {
            set fh(width) $width
        }
    }
    lappend htmlids $id
    $handler insert insert "\n" $tag
    $handler tag bind $tag <3> "tk_popup $fh(struct_menu) %X %Y \
				 \[lsearch \[set ${handler}(struct_list)\] \
				 $body\]"
    bind $htmlwin <3> "tk_popup $fh(struct_menu) %X %Y \
			  \[lsearch \[set ${handler}(struct_list)\] \
			  $body\]"

    lappend fh(width_adjust) $htmlwin
}

# rat_html3::imagecmd --
#
# Fetches and creates an image to display in a HTML message
#
# Arguments:
# w: The HTML widget used to display images
# uri: The URI of the image
#
# Returns:
#   The name of an image if it could be constructed correctly, an empty string
#   otherwise

proc rat_html3::imagecmd {w uri} {
    upvar \#0 rat_html3::imagemap_$w map
    global rat_html3::images
    
    # Check cached images
    if {[info exists map] && [info exists map($uri)]} {
 	return $map($uri)
    }

    if {[string match cid:* $uri]} {
        set filename [get_embedded_image $uri]
    } else {
        set filename [get_external_image $uri]
    }
    
    set img ""
    if {$filename != "" && ![catch {image create photo -file $filename} img]} {
	set map($uri) $img
    } else {
        set img ""
    }
    file delete -force -- $filename

    if {"" == $img} {
        set img [image create photo]
    }
    lappend rat_html3::images($w) $img
    return $img
}

# rat_html3::get_embedded_image --
#
# Extract an image from an related bodypart
#
# Arguments:
# uri: The URI of the image
#
# Returns:
#   The name of a file which contains the image data. Or an empty string
#   if no image was downloaded.

proc rat_html3::get_embedded_image {url} {
    global related option rat_tmp

    if {![regsub "cid:" $url {} id]
        || ![info exists related($id)]} {
        return ""
    }

    set filename $rat_tmp/htmlimg.[RatGenId]
    set fid [open $filename w 0600]
    fconfigure $fid -encoding binary
    $related($id) saveData $fid false false
    close $fid

    return $filename
}

# rat_html3::get_external_image --
#
# Fetches an external image to display in a HTML message
#
# Arguments:
# uri: The URI of the image
#
# Returns:
#   The name of a file which contains the image data. Or an empty string
#   if no image was downloaded.

proc rat_html3::get_external_image {uri} {
    global option rat_tmp

    # Abort if...
    # ...we should not load external images
    # ...the uri does not start with http
    # ...the actual fetch failed
    if {$option(html_show_images) == 0
        || ![string match http://* $uri]
        || [catch {::http::geturl $uri} token]} {
        return ""
    }
    
    # Store image in a file
    set filename $rat_tmp/extimg.[RatGenId]
    set fid [open $filename w 0600]
    fconfigure $fid -encoding binary
    puts -nonewline $fid [::http::data $token]
    close $fid

    return $filename
}
