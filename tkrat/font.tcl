# font.tcl --
#
# Handles cataloguing and selection of fonts
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


# AddFont --
#
# Add a font to the list of known fonts.
#
# Arguments:
# encoding   - The encoding of this font
# size       - The size difference from "normal"
# attributes - The attributes
# name	     - The name of the font

proc AddFont {encoding size attributes name} {
    return
}


# RemoveFonts --
#
# Remove slected fonts from the list of known fonts.
# We currently ignore the problem that some encodings may become unsupported
#
# Arguments:
# name - Name of fonts to remove (may be regexp)

proc RemoveFonts {name} {
    return
}
