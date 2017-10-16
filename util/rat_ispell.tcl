#----------------------------------------------------------#
#----------------------------------------------------------#
#----------------------------------------------------------#
#
# Namespace Ispell (Version 0.4 + TkRat mod)
#
# Copyright (c) 2000
# Bryan Schofield, Melbourne,Florida USA.  All rights reserved.
#
# ispellTextWidget.tcl (namespace Ispell) is a tk interface to 
# ispell 3.1.x. It is intended to be as plug-in-able as possible.
# To use it simply call: Ispell::CheckTextWidget \$textwidget
# The rest is taken care of by the Ispell namespace. However, before 
# you source it in, you may want to remove the test code at the very
# bottom of the file that generates what you see now. You might also
# trim the three lines that restart with wish.
#
# The author of this software is Bryan Schofield and can be reached via
# the following email address: lewca43@mindspring.com He is hereafter
# refered to as the author.
#
# Redistribution and use in source and binary forms, with or without
# modification, for commercial or non-commercial use, is permitted
# provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#
# This copy has been modified Martin Forssen <maf@dtek.chalmers.se>
# to better fit into the tkrat distribution.

package provide rat_ispell 1.1

namespace eval rat_ispell {
    namespace export CheckTextWidget

    package require rat_spellutil 1.0

    variable ignore
    # ignore is an array where each element corresponds to
    # a word that does not need to be checked
    # i.e. if ignore(cat) exists, then  "cat" will not be checked
    variable replace
    # replace is an array where each element corresponds to
    # a word that is to be replaced upon being encountered
    # i.e. if replace(cat) exists, then  "cat" will be replaced
    # with the value of replace(cat), which might be "A bald cat."
    variable tagBG yellow
    variable tagFG blue
    # These two variable indicate the background and foreground
    # color of the ispell tag that is applied in the text widget
    # as the words are checked.

    variable dictionaries {}
    # A list of installed dictionaries
}

#
# rat_ispell::IsSeparator
#
# Checks if the character at the given point in a text widget is a separator
# character or not.
#
# Arguments:
# w - Text widget
# p - Position
#
# Return:
# 1 if the give character is a separator

proc rat_ispell::IsSeparator {w p} {
    # "'" is a separator unless embedded in a word
    if {"'" == [$w get $p]} {
	if {[$w compare $p == "$p linestart"]
	    || [$w compare $p == "$p lineend"]
	    || 0 == [string is alpha [$w get "$p -1c"]]
	    || 0 == [string is alpha [$w get "$p +1c"]]} {
	    return 1
	} else {
	    return 0
	}
    }
    if {0 == [string is alpha [$w get $p]]} {
	return 1
    } else {
	return 0
    }
}

#
# rat_ispell::CheckTextWidget
#
# This is the proc you want. It does the spell checking of the
# the text widget.
#
# Arguments:
# tw - text widget path name
#
# Return:
# nothing
#
proc rat_ispell::CheckTextWidget {tw} {
    global option t

    if {0 == [llength $rat_ispell::dictionaries]} {
        set rat_ispell::dictionaries [rat_spellutil::get_dicts]
    }

    #
    # Since we are at least going to say "Spell Checking Complete"
    # and we might have a mispelled word or three, let's make one
    # toplevel that handles every thing for this session. Better hide
    # it until we need it
    #
    regsub -all "\\." $tw "_" safet
    set w ".ispell$safet"
    if {[winfo exists $w]} {
        return
    }
    upvar \#0 rat_ispell::$w hd
    set hd(wait_string) 1
    set hd(tw) $tw

    toplevel $w -class TkRat
    wm title $w $t(spell_checking)
    option add *HighlightBackground [$w cget -background]

    set l 0
    foreach la {unknown_word replace_with} {
	if {[string length $t($la)] > $l} {
	    set l [string length $t($la)]
	}
    }
    incr l
    set f $w.topframe
    frame $f
    label $f.unkwordL \
        -text $t(unknown_word): -width $l -anchor e
    label $f.unkwordE -width 20 -anchor w
    label $f.repwordL \
        -text $t(replace_with): -width $l -anchor e
    entry $f.repwordE  -width 20
    grid $f.unkwordL -column 0 -row 0 -stick w
    grid $f.unkwordE -column 1 -row 0 -stick w
    grid $f.repwordL -column 0 -row 1 -stick w
    grid $f.repwordE -column 1 -row 1 -stick ew
    grid columnconfigure $f 1 -weight 1

    set f $w.guess
    frame $f 
    set lbx $f.lbx
    listbox $f.lbx \
        -height 7 \
        -yscrollcommand "$f.ys set"
    scrollbar $f.ys \
        -command "$f.lbx yview"
    pack $f.lbx -side left -fill both -expand 1
    pack $f.ys  -side left -fill y

    set f $w.buttons
    frame $f
    button $f.replace \
        -text $t(replace)
    button $f.replaceall \
        -text $t(replace_all)
    button $f.ignore \
        -text $t(ignore)
    button $f.ignoreall \
        -text $t(ignore_all)
    button $f.learn \
        -text $t(learn)
    menubutton $f.lang \
        -menu $f.lang.m \
        -justify center \
        -relief raised -bd 2 -indicatoron 1
    button $f.quit \
        -text $t(dismiss)
    pack $f.replace $f.replaceall $f.ignore $f.ignoreall $f.learn $f.lang\
	    -side top -fill x
    pack $f.quit -side bottom -fill x

    grid $w.topframe -column 0 -row 0 -stick ew
    grid $w.guess    -column 0 -row 1 -stick news 
    grid $w.buttons  -column 1 -row 0 -stick ns   -rowspan 2
    grid columnconfigure $w 0 -weight 1
    grid rowconfigure    $w 1 -weight 1
    
    set m $f.lang.m
    menu $m
    set lwidth 8
    foreach l $rat_ispell::dictionaries {
        $m add command \
            -label [string totitle $l] \
            -command "rat_ispell::SetLanguage $w $l"
        if {[string length $l] > $lwidth} {
            set lwidth [string length $l]
        }
    }
    $f.lang configure -width $lwidth
    set hd(langbutton) $f.lang

    bind $w <Escape> "$f.quit invoke"
    bind $lbx <ButtonRelease-1> "rat_ispell::SyncListBoxAndRepString $w $lbx"
    bind $lbx <Double-1> "set rat_ispell::${w}(returnString) CheckWord-replace"

    bind $w.guess.lbx <Destroy> \
        "::tkrat::winctl::RecordGeometry ispell $w $w.guess.lbx"
    ::tkrat::winctl::SetGeometry ispell $w $w.guess.lbx
    
    #
    # Later on, we will configure the label, entry widgets, and buttons
    # for what we need.
    #
    
    #
    # The plan is to start at the beginning of the text
    # widget and check every word, where a word is defined
    # as a set of characters separated by spaces. Since I can't
    # think of a better way to do it, I guess I'll start at 
    # the first line and check every word on that line
    # until I reach the end, then check the next line and so on.
    #
    # We also want to apply a special tag so that the user can 
    # easily identify what word is in question during the spell 
    # check process
    $tw tag configure ispell \
        -background $rat_ispell::tagBG \
        -foreground $rat_ispell::tagFG \
        -bgstipple gray75
    $tw tag raise ispell


    # Figure out language
    if {"auto" == $option(def_spell_dict)} {
        set lang [rat_ispell::GuessLanguage $tw]
    } else {
        set lang $option(def_spell_dict)
    }
    rat_ispell::SetLanguage $w $lang

    #
    # Run the spell checking
    rat_ispell::CheckText $tw $w

    if {[winfo exists $w]} {
        tkwait window $w
    }
    unset hd

    # Rerun the marking of misspelled words...
    catch {rat_textspell::recheck $tw} err
}

# rat_ispell::CheckText --
#
# Spellchecks the given text widget
#
# Arguments:

proc rat_ispell::CheckText {tw w} {
    upvar \#0 rat_ispell::$w hd
    global option t

    wm title $w $t(spell_checking)
    wm protocol $w WM_DELETE_WINDOW ""

    set ispell [rat_spellutil::launch_spell $hd(lang)]
    if {"" == $ispell} {
        return
    }

    set f $w.topframe
    grid configure $f.unkwordL -columnspan 1
    grid $f.unkwordE -row 0 -column 1
    $f.repwordE configure \
        -state normal
    $f.unkwordL configure \
        -text $t(unknown_word): -width [$f.repwordL cget -width] -anchor e
    $f.repwordL configure \
        -foreground [$w.buttons.replace cget -foreground]

    foreach b {replace replaceall ignore ignoreall learn} {
        $w.buttons.$b configure -state normal
    }

    rat_ispell::InitWordGet $tw
    set continue 1
    set doQuit 0
    set doRerun 0
    while {$continue} {
        set word [rat_ispell::GetWord $tw]
        if {"" == $word} {
            break
        }
        if {[regexp -nocase \[a-z\] "$word"] && \
                ![info exists rat_ispell::ignore($hd(lang),$word)]} {
            # Well, it looks like we found a valid word that is 
            # not to ignored so now we have to check it.
            # It is possible that we have already decided to
            # replace all occurances of the word with someting
            # else... let's see
            if {[info exists rat_ispell::replace($hd(lang),$word)]} {
                # Hey, how about it, lets automagically replace it
                set replaceWord 1
                set replacement "$rat_ispell::replace($hd(lang),$word)"
            } else {
                # let's ask ispell if this word is correct.
		$tw tag add ispell word_start word_end
		$tw see word_start
                set action [rat_ispell::CheckWord $ispell $word $w]
                $tw tag remove ispell 1.0 end
                switch -regexp -- "$action" {
                    CheckWord-ok {
                        set replaceWord 0
                    }
                    CheckWord-replaceAll.* {
                        set replaceWord 1
                        set replacement "[lrange $action 1 end]"
                        set rat_ispell::replace($hd(lang),$word) "$replacement"
                    }
                    CheckWord-replace.* {
                        set replaceWord 1
                        set replacement "[lrange $action 1 end]"
                    }
                    CheckWord-ignoreAll {
                        set replaceWord 0
                        set rat_ispell::ignore($hd(lang),$word) 1
                    }
                    CheckWord-ignore {
                        set replaceWord 0
                    }
                    CheckWord-quit {
                        set continue 0
                        set replaceWord 0
			set doQuit 1
                    }
                    SetLanguage {
			set doRerun 1
                        set replaceWord 0
                        set continue 0
                    }
                }
		if {![winfo exists $tw]} {
		    set doQuit 1
		    break
		}
            }
            #
            # Do we need to replace a word, either thru replace-all
            # or on a individual basis ?
            #
            if {$replaceWord} {
                $tw delete word_start word_end
                $tw insert word_start "$replacement"
	    }
	}
        if {[$tw compare ispell_pos >= end]} {
            # We reached the end of the text widget
            set continue 0
        }
    }

    catch {close $ispell}

    if {$doQuit} {
        destroy $w
        return
    }

    if {$doRerun} {
        rat_ispell::CheckText $tw $w
        return
    }

    #
    # Ok, checking complete, we to let the user know
    #
    wm title $w "$t(spell_checking): $t(spell_complete)"
    wm protocol $w WM_DELETE_WINDOW "destroy $w"

    $f.unkwordL configure \
        -text $t(spell_complete) -width 0 -anchor c
    #
    # The only way to keep the toplevel from resizing is to forget
    # $w.unkwordE. This is because "Spell Checking Complete" is
    # much longer than "Unkown Word:". We also need to let it bleed
    # into the next column over.
    #
    grid forget $f.unkwordE
    grid configure $f.unkwordL -columnspan 2    
    $f.repwordE configure \
        -state disabled
    #
    # To keep things looking good, let's change the color of the
    # text on the "Replace With:" label to match that of the
    # the disabled buttons
    #
    $f.repwordL configure \
        -foreground [$w.buttons.replace cget -disabledforeground]

    foreach b {replace replaceall ignore ignoreall learn} {
        $w.buttons.$b configure -state disabled
    }
    $w.buttons.quit configure \
        -command "destroy $w"
}

proc rat_ispell::InitWordGet {tw} {
    $tw mark set word_start 1.0
    $tw mark set word_end 1.0
    $tw mark set ispell_pos 1.0

    $tw mark gravity word_start left
    $tw mark gravity word_end right
    $tw mark gravity ispell_pos right
}

proc rat_ispell::GetWord {tw} {
    if {0 == [IsSeparator $tw ispell_pos]} {
        $tw mark set word_start ispell_pos
        while {0 == [IsSeparator $tw "word_start -1c"]
               && [$tw compare "word_start -1c" > 1.0]} {
            $tw mark set word_start "word_start -1c"
        }
    } else {
        $tw mark set word_start "ispell_pos +1c"
        while {1 == [IsSeparator $tw word_start]
               && [$tw compare word_start < end]} {
            $tw mark set word_start "word_start +1c"
        }
    }
    if {[$tw compare word_start >= end]} {
        return ""
    }
    if {-1 != [lsearch -exact \
                   [$tw tag names word_start] no_spell]} {
        set n [$tw tag prevrange no_spell "word_start+1c"]
        $tw mark set ispell_pos "[lindex $n 1] +1c"
        return [rat_ispell::GetWord $tw]
    }
    $tw mark set word_end "word_start +1c"
    while {0 == [IsSeparator $tw word_end]
           && [$tw compare word_end < end]} {
        $tw mark set word_end "word_end +1c"
    }
    $tw mark set ispell_pos word_end
    return [string trim [$tw get word_start word_end]]
}

# rat_ispell::CheckWord
# Checks a word, if word is not known throws up a gui with 
# suggestions if any
#
# Arguments:
#  ispell - file Id of an ispell pipe. Assumed to be ready to
#           accept words
#  word   - the word you want to check
#  w      - toplevel to draw in widgets in if we need to ask the
#           the user anything
#
# Returns:
#  "CheckWord-ok"
#  "CheckWord-replace newWord"
#  "CheckWord-replaceAll newWord"
#  "CheckWord-ignore"
#  "CheckWord-ignoreAll"
#  "CheckWord-quit
#
proc rat_ispell::CheckWord {ispell word w} {
    # ok the skinny on ispell
    # for every line we put to ispell, we will get a line in return
    # the first character of the line will indicate what
    # we need to do
    puts $ispell "^$word"
    flush $ispell
    gets $ispell result
    # we have to get the junk line that ispell prints after the result
    # it's just an empty line
    while {-1 != [gets $ispell junk] && "" != $junk} {}
    set resultCode [string index $result 0]
    if {$resultCode == "#"} {
        set resultCode "lb"
        # A "#" will screw uo my switch
    }
    switch -- $resultCode {
        \* {
            # "*" means the word was found in the main or
            # personal dictionary
            return "CheckWord-ok"
        }
        \+ {
            # "+" means the word was found through affix
            # removal. result will look like this: + ROOTWORD
            # We'll assume the word is correct and ignore 
            # the root word.
            return "CheckWord-ok"
        }
        \- {
            # if ispell was ran with the -C option, the run
            # together words are cosidered valid. If we get
            # a "-" then ispell found the word as a concatenation
            # of two other valid words. I guess since -C was
            # specified, the user thinks this word is ok
            return "CheckWord-ok"
        }
        \& {
            # "&" means this word is incorrect, but ispell has
            # made some guesses as to what the word could be
            # result will look like the following"
            # & word ng nc: guess1, guess2, ...,derivation1, derivation2, ...
            # ng is the number of guesses (near misses)
            # nc is the number of characters between the beginning 
            #    of the line and the beginning of the misspelled word
            #    which will be "0" since we are checking one word
            #    at a time
            # after the colon, we have a comma separated list of
            # guesses then a list of derivations. If the word could be
            # formed by adding (illegal) affixes to a known root, a 
            # list of suggested derivations is supplied
            # unfortunately, we can only determine what are guesses and
            # what are derivations by using ng.
            set guesses "[lrange $result 4 end]"
            set action [rat_ispell::UnknownWord $w $ispell \
		    [lindex $result 1] "$guesses"]
        }
        \? {
            # This is simular to getting a "&", except there are no
            # guesses, only derivations. the format is the same as
            # "&" but ng will always be 0
            set guesses [lrange $result 4 end]
            set action [rat_ispell::UnknownWord $w $ispell \
			    [lindex $result 1] "$word, $guesses"]
        }
        lb { 
            # a "#" means that there are no guesses or derivations
            set action [rat_ispell::UnknownWord $w $ispell [lindex $result 1]]
        }
    }
    return "$action"
}


# rat_ispell::UnknownWord
# Brings up a gui that the user can select what to
# about a word that is not known
#
# Arguments:
#  w      - toplevel to draw all the good stuff in.
#  ispell - file Id of an ispell pipe. Assumed to be ready to
#           accept commands
#  word   - the word that is unknown
#  args   - a list of suggestions separated by commas
#
# Returns:
#  command - which will be one of the following:
#   "CheckWord-replace newWord"
#   "CheckWord-replaceAll newWord"
#   "CheckWord-ignore"
#   "CheckWord-ignoreAll"
#   "CheckWord-quit
proc rat_ispell::UnknownWord {w ispell word args} {
    global t
    upvar \#0 rat_ispell::$w hd

    #
    # Re-configure the ispell dialogue box to work and
    # say what we want
    #
    wm title $w "$t(spell_checking): $t(unknown_word) - $word"
    wm protocol $w WM_DELETE_WINDOW \
	    "set rat_ispell::${w}(returnString) CheckWord-quit"

    set f $w.topframe
    $f.unkwordE configure \
        -text "$word" 
    #
    # Just in case ispell doesn't have clue better clear the replacement
    # word entry box
    #
    $f.repwordE configure \
        -textvariable "rat_ispell::${w}(replacement)"

    set lbx $w.guess.lbx

    set f $w.buttons
    $f.replace configure \
        -command "set rat_ispell::${w}(returnString) CheckWord-replace"
    $f.replaceall configure \
        -command "set rat_ispell::${w}(returnString) CheckWord-replaceAll"
    $f.ignore configure \
        -command "set rat_ispell::${w}(returnString) CheckWord-ignore"
    $f.ignoreall configure \
        -command "set rat_ispell::${w}(returnString) CheckWord-ignoreAll"
    $f.learn configure \
        -command "rat_ispell::LearnWord $word $ispell; \
	          set rat_ispell::${w}(returnString) CheckWord-ignoreAll"
    $f.quit configure \
        -command "set rat_ispell::${w}(returnString) CheckWord-quit"
    
    #
    # load the list box with any suggestions
    #
    if {$args != ""} {
        set args [string trim $args "\{\}"]
        foreach guess [split $args ","] {
            $lbx insert end "[string trim $guess]"
        }
        $lbx selection set 0
        rat_ispell::SyncListBoxAndRepString $w $lbx
    } else {
        set hd(replacement) $word
    }
    #
    # let's just wait a while here, so that nothing else gets ahead
    # of us
    #
    set hd(wait_string) 1
    vwait rat_ispell::${w}(returnString)
    set hd(wait_string) 0
    switch -- $hd(returnString) {
        CheckWord-replace {
            set hd(returnString) "$hd(returnString) $hd(replacement)"
        }
        CheckWord-replaceAll {
            set hd(returnString) "$hd(returnString) $hd(replacement)"
        }
    }
    $lbx delete 0 end
    $w.topframe.repwordE delete 0 end

    return $hd(returnString)
}


# rat_ispell::SyncListBoxAndRepString
# Just synchronizes the entry box and the current selection
# of the list box on the "what to do" toplevel
#
proc rat_ispell::SyncListBoxAndRepString {w lbx} {
    upvar \#0 rat_ispell::$w hd

    set index [$lbx curselection]
    if {$index != ""} {
        set hd(replacement) [$lbx get $index]
    }
}


# This proc adds "word" to your personal ispell word list
proc rat_ispell::LearnWord {word ispell} {
    puts $ispell "*$word"
    puts $ispell "\#"
    flush $ispell
}

# rat_ispell::SetLanguage --
#
# Set the language.
#
# Arguments:
# w    - Name of global state variable
# lang - The new language

proc rat_ispell::SetLanguage {w lang} {
    upvar \#0 rat_ispell::$w hd

    $hd(langbutton) configure -text [string totitle $lang]
    set hd(lang) $lang

    if {$hd(wait_string)} {
        set hd(returnString) "SetLanguage"
    } else {
        rat_ispell::CheckText $hd(tw) $w
    }
}

# rat_ispell::GuessLanguage --
#
# Guess the language in the text widget
#
# Arguments:
# tw - Text wisget

proc rat_ispell::GuessLanguage {tw} {
    global option

    set words {}
    rat_ispell::InitWordGet $tw
    while {[llength $words] < 15} {
        set word [rat_ispell::GetWord $tw]
        if {"" == $word} {
            break
        }
        lappend words $word
    }
    return [rat_spellutil::find_best_dict $words]
}
