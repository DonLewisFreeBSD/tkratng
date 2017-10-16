# mime.tcl --
#
# This file contains procedures to determine the MIME type of a file.
#
#  TkRat software and its included text is Copyright 1996-2000 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


set fileGroksMime 1

# RatType --
#
# Determines the MIME type and encoding of the file.  The algorithm is to
# first determine if the file exists and its encoding, then run the file
# command on it and parse the result. If the file command returns the MIME
# type, it is used as is. If the file command returns a mix of MIME type and
# English text, try to extract the MIME type from the returned string.
# Otherwise tries to find a match in the typetable; if none is found, defaults
# to application/octet-stream.
#
# Arguments: 
# fname - Name of the file to check

proc RatType {fname} {
    global option
    global fileGroksMime
    
    if {![file exists $fname]} {
	error "error opening file $fname"
    }
    
    # Get the encoding
    set encoding [RatEncoding $fname]

    set mimetype ""
    if {-1 == [string first {--mime} $option(mimeprog)] && $fileGroksMime} {
        set cmd "exec $option(mimeprog) --mime [list $fname]"
        if {[catch {eval $cmd} mimetype]} {
            set fileGroksMime 0
            set mimetype ""
        }
    }
    if {"" == $mimetype} {
        set mimetype [eval exec $option(mimeprog) [list $fname] 2>/dev/null]
    }

    # Parse the result
    if {[regexp {^[-a-z0-9A-Z]+/[-a-z0-9A-Z]+$} $mimetype]} {
	# Cool, the MIME type is set for us. Nothing to do!
    } elseif {[regexp {^([-a-z0-9A-Z]+)/([-a-z0-9A-Z]+),.*$} \
                   $mimetype -> partA partB]}  {
	# Almost ok. The MIME type returns with stuff at the end. Strip the
	# stuff and just keep the MIME type. "stuff" is anything after the
	# first comma
	set mimetype "$partA/$partB"
    } elseif {[regexp {^([^:]+): ([-a-z0-9A-Z]+)/([-a-z0-9A-Z]+).*$} \
                   $mimetype -> name partA partB] \
                  && [string equal $name $fname]} {
	# Hmm... not cool. We get back the filename followed by the mime type.
	# We'll assume that there may or may not be a comma after the MIME
	# type.
	set mimetype $partA/$partB
    } else {
	# Ugh! The worst of all posisble worlds: the default file command! We
	# have no MIME type. So let's check it all against the
	# option(filetype) table
	set defaulttype "application/octet-stream"
	foreach {line} $option(typetable) {
	    if {[string match [lindex $line 0] $mimetype]} {
		set defaulttype [lindex $line 1]
		break
	    }
	}
	set mimetype $defaulttype
    }
    
    return [list $mimetype $encoding]
}
