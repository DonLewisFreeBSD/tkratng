# rat_textspell.tcl --
#
# Continuous spell-checking for text widgets. That is checks the text
# as it is written.
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_textspell 1.0

namespace eval rat_textspell {
    namespace export init uninit recheck get_dicts set_dict

    variable words_to_guess 10

    package require rat_spellutil 1.0

    bind RatTextSpell <Destroy> "rat_textspell::destroy_state %W"
    bind RatTextSpell <KeyRelease-Return> "rat_textspell::return_key %W insert"
    bind RatTextSpell <KeyRelease> "rat_textspell::run %W textspell_last_insert insert 0"
    foreach k {Down Up Right Left Shift_L Shift_R Control_L Control_R
        Caps_Lock Alt_L Alt_R Super_L Super_R Meta_L Meta_R} {
        bind RatTextSpell <KeyRelease-$k> { }
    }
    foreach e {KeyPress-Down KeyPress-Up KeyPress-Right KeyPress-Left
        ButtonPress-1} {
        bind RatTextSpellPre <$e> {rat_textspell::movement %W}
        bind RatTextSpell <$e> {rat_textspell::update_last %W}
    }
    foreach e {<<Paste>> <<PasteSelection>> <<RatPasteSelection>>} {
        bind RatTextSpell $e \
            "rat_textspell::run %W textspell_paste_start textspell_paste_end 1"
    }
    bind RatTextSpell <Destroy> "rat_textspell::uninit %W"
}

# rat_textspell::init --
#
# Initializes the textspell stuff
#
# Arguments:
# w	- name of the text widget to attach to
# dict  - dictionary to use, can be "auto"

proc rat_textspell::init {w dict} {
    $w mark set textspell_last_insert 1.0
    $w mark set textspell_paste_start 1.0
    $w mark set textspell_paste_end 1.0
    $w mark gravity textspell_last_insert left
    $w mark gravity textspell_paste_start left
    $w mark gravity textspell_paste_end right

    upvar \#0 rat_textspell::state$w hd
    set hd(dict) $dict
    set hd(ispell_fd) ""

    set hd(oldbindtags) [bindtags $w]
    set i [lsearch -exact $hd(oldbindtags) Text]
    set nt [linsert $hd(oldbindtags) $i RatTextSpellPre]
    lappend nt RatTextSpell
    bindtags $w $nt

    foreach e {<<Paste>> <<PasteSelection>>} {
	bind $w $e {
            %W mark set textspell_paste_start [%W index insert]
            %W mark set textspell_paste_end [%W index insert]
        }
    }

    create_bgstipple $w
    $w tag bind badspell <3> \
        "rat_textspell::popup_menu %W \[%W index @%x,%y\] %X %Y"
    set hd(m) $w.textspell_menu
    menu $hd(m) -tearoff 0

    run $w 1.0 end 1
}

# rat_textspell::uninit --
#
# Removes the textspell stuff
#
# Arguments:
# w	- name of the text widget to detach from

proc rat_textspell::uninit {w} {
    upvar \#0 rat_textspell::state$w hd
    upvar \#0 rat_textspell::cache$w cache

    if {[info exists hd(oldbindtags)]} {
        if {[winfo exists $w]} {
            $w mark unset textspell_paste_start textspell_paste_end
            $w tag remove badspell 1.0 end
            bindtags $w $hd(oldbindtags)
            foreach e {<<Paste>> <<PasteSelection>> <<Copy>>} {
                bind $w $e {}
            }
        }
        catch {close $hd(ispell_fd)}
        unset hd
        catch {unset cache}
        destroy $w.textspell_menu
    }
}

# rat_textspell::recheck --
#
# If the given text widget is running rat_textspell then recheck the
# entire text widget. 
#
# Arguments:
# w	- name of the text widget to recheck

proc rat_textspell::recheck {w} {
    upvar \#0 rat_textspell::state$w hd
    upvar \#0 rat_textspell::cache$w cache

    if {[info exists hd(dict)]} {
        # Restart ispell
        if {$hd(ispell_fd) != ""} {
            catch {close $hd(ispell_fd)}
            set hd(ispell_fd) ""
        }
        if {[info exists cache]} {
            unset cache
        }

        # Check everything
        run $w 1.0 end 1
    }
}

# rat_textspell::get_dicts --
#
# Get a list of possible dictionaries
#
# Arguments:

proc rat_textspell::get_dicts {} {
    return [rat_spellutil::get_dicts]
}

# rat_textspell::set_dict --
#
# Set the dictionary to use
#
# Arguments:
# w	- name of the text widget to operate on
# dict  - dictionary to use

proc rat_textspell::set_dict {w dict} {
    upvar \#0 rat_textspell::state$w hd

    set hd(dict) $dict
    recheck $w
}

# rat_textspell::create_bgstipple --
#
# Creates a and installs a bitmap for bgstipple
#
# Arguments:
# w	- name of the text widget state to work with

proc rat_textspell::create_bgstipple {w} {
    upvar \#0 rat_textspell::state$w hd
    global rat_tmp

    set height "[font metrics [$w cget -font] -linespace]"
    set stipple $rat_tmp/stipple_$height.bmp
    if {![file exists $stipple]} {
        generate_stipple $height $stipple
    }
    $w tag configure badspell -bgstipple @$stipple -background red
}

# rat_textspell::generate_stipple
#
# Generates a stipple mask which can be used to create underlined text.
# We do this as a stipple to be able to control the color.
#
# Arguments:
# h    - Height of stipple to create
# name - Name of file to produce

proc rat_textspell::generate_stipple {h name} {
    set f [open $name w]
    puts $f "\#define stipple${h}_width 3"
    puts $f "\#define stipple${h}_height $h"
    puts $f "static unsigned char stipple{$h}_bits\[\] = \{"
    for {set i 2} {$i < $h} {incr i} {
        puts -nonewline $f " 0x00,"
    }
    puts $f " 0x02, 0x05\};"
    close $f
}

# rat_textspell::destroy_state --
#
# Destroy the state associated with the text widget
#
# Arguments:
# w	- name of the text widget state to destroy

proc rat_textspell::destroy_state {w} {
    upvar \#0 rat_textspell::state$w hd

    catch {close $hd(ispell_fd)}
}

# rat_textspell::return_key --
#
# Called when the user has pressed the return key
#
# Arguments:
# w   - name of the text widget to check
# pos - Location to start checking at

proc rat_textspell::return_key {w pos} {
    run $w "$pos-1l lineend" $pos 1
}

# rat_textspell::movement --
#
# Called when the cursor is moved (by arrow key or mouse)
#
# Arguments:
# w   - name of the text widget to check
# pos - Location to start checking at

proc rat_textspell::movement {w} {
    if {[$w compare insert == {insert lineend}]} {
        run $w textspell_last_insert insert 1
    }
}

# rat_textspell::update_last --
#
# Called when the cursor is moved (by arrow key or mouse)
# However this function is called after the text widget has moved teh cursor
#
# Arguments:
# w   - name of the text widget to check
# pos - Location to start checking at

proc rat_textspell::update_last {w} {
    $w mark set textspell_last_insert [$w index insert]
}

# rat_textspell::run --
#
# Potentially spellcheck part of the text
#
# Arguments:
# w	- name of the text widget to check
# start - Location to start checking at
# end   - Location to stop checking at

proc rat_textspell::run {w start end check_last} {
    upvar \#0 rat_textspell::state$w hd
    upvar \#0 rat_textspell::cache$w cache

    if {"auto" == $hd(dict) || "" == $hd(dict)} {
        guess_language $w
        if {"auto" == $hd(dict) || "" == $hd(dict)} {
            return
        }
        set start 1.0
    }

    if {"" == $hd(ispell_fd)} {
        set hd(ispell_fd) [rat_spellutil::launch_spell $hd(dict)]
        if {"" == $hd(ispell_fd)} {
            uninit $w
            return
        }
    }

    # Find range to check
    set start_pos [$w search -backwards -regexp "\[ \t.,;:\]" \
                       $start-1c "$start linestart"]
    if {"" == $start_pos} {
        set start_pos "$start linestart"
    }

    $w mark set textspell_last_insert [$w index insert]

    while {1} {
        mark_word $w $start_pos
        if {[$w compare word_start > $start_pos]} {
            $w tag remove badspell $start_pos word_start
        }
        if {([$w compare word_start >= "$end +1c"]
            && "" == [$w search -regexp {[:space:]} $end word_start])
            || [$w compare word_start == word_end]
            || ([$w compare word_end == $end]
                && [$w compare word_end == {word_end lineend}]
                && !$check_last)} {
            break
        }
        set word [$w get word_start word_end]
        if {[info exists hd(ignore,$word)]} {
            set idx 0
        } elseif {[info exists cache($word)]} {
            set idx $cache($word)
        } else {
            puts $hd(ispell_fd) "^$word"
            flush $hd(ispell_fd)
            gets $hd(ispell_fd) result
            if {"" == $result} {
                set idx 0
            } else {
                while {-1 != [gets $hd(ispell_fd) junk] && "" != $junk} {}
                set idx [lsearch -exact {* + -} [string index $result 0]]
            }
        }
        if {-1 == $idx} {
            $w tag add badspell word_start word_end
        } else {
            $w tag remove badspell word_start word_end
        }
        set cache($word) $idx

        set start_pos [$w index word_end]
    }
}

# rat_textspell::guess_language --
#
# Try to extract wenough words from the text to guess the language
#
# Arguments:
# w	- name of the text widget to check

proc rat_textspell::guess_language {w} {
    upvar \#0 rat_textspell::state$w hd
    variable words_to_guess
    global option

    # Extract words to use for testing
    set words {}
    set pos 1.0
    while {[llength $words] < $words_to_guess && [$w compare $pos < end]} {
        set nospell [$w tag nextrange no_spell $pos]
        if {0 == [llength $nospell]} {
            set extract_end end
            set next_pos end
        } else {
            set extract_end [lindex $nospell 0]
            set next_pos [lindex $nospell 1]
        }
        foreach word_candidate [split [$w get $pos $extract_end] " \t\n.,:"] {
            if {1 < [string length $word_candidate]} {
                lappend words $word_candidate
            }
            if {[llength $words] > $words_to_guess} {
                break
            }
        }
        set pos $next_pos
    }

    # Did we get enough data?
    if {[llength $words] <= $words_to_guess} {
        return
    }
    
    # Discard the last word, the user is probably still writing it
    set words [lrange $words 0 end-1]

    set hd(dict) [rat_spellutil::find_best_dict $words]
}

# rat_textspell::is_separator --
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

proc rat_textspell::is_separator {w p} {
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

# rat_textspell::mark_word --
#
# Mark the next word with word_start and word_end
#
# Arguments:
# w - Text widget to work with
# p - Start position

proc rat_textspell::mark_word {w p} {
    if {0 == [is_separator $w $p]} {
        $w mark set word_start $p
        while {0 == [is_separator $w "word_start -1c"]
               && [$w compare word_start > 1.0]} {
            $w mark set word_start "word_start -1c"
        }
    } else {
        $w mark set word_start "$p +1c"
        while {1 == [is_separator $w word_start]
               && [$w compare word_start < end]} {
            $w mark set word_start "word_start +1c"
        }
    }
    if {[$w compare word_start >= end]} {
        $w mark set word_end "word_start"
    } elseif {-1 != [lsearch -exact \
                         [$w tag names word_start] no_spell]} {
        set n [$w tag prevrange no_spell "word_start+1c"]
        set new_pos "[lindex $n 1] +1c"
        mark_word $w $new_pos
    } else {
        $w mark set word_end "word_start +1c"
        while {0 == [is_separator $w word_end]
               && [$w compare word_end < end]} {
            $w mark set word_end "word_end +1c"
        }
    }
}

# rat_textspell::popup_menu --
#
# Popup the bad-spell menu
#
# Arguments:
# w   - Text widget to work with
# idx - Index in text pointed at
# x,y - Position to popup menu at

proc rat_textspell::popup_menu {w idx x y} {
    upvar \#0 rat_textspell::state$w hd
    global t

    # Is ispell running?    
    if {"" == $hd(ispell_fd)} {
        return
    }

    $hd(m) delete 0 end

    # Check word?
    mark_word $w $idx
    set word [$w get word_start word_end]
    puts $hd(ispell_fd) "^$word"
    flush $hd(ispell_fd)
    gets $hd(ispell_fd) result
    while {-1 != [gets $hd(ispell_fd) junk] && "" != $junk} {}
    if {[regexp {^[\&\?][^:]+:(.+)$} $result unused words]} {
        foreach word [lrange [split $words ,] 0 19] {
            set trimmed [string trim $word]
            $hd(m) add command -label $trimmed -command \
                [list rat_textspell::replace_word $w $idx $trimmed]
        }
        $hd(m) add separator
    }
    $hd(m) add command -label $t(ignore) \
        -command "rat_textspell::ignore_word $w $idx"
    $hd(m) add command -label $t(learn) \
        -command "rat_textspell::learn_word $w $idx"
    tk_popup $hd(m) $x $y
}

# rat_textspell::replace_word --
#
# Replace a misspelled word with a correct one
#
# Arguments:
# w   - Text widget to work with
# idx - Index in text where word exists
# new - Word to replace with

proc rat_textspell::replace_word {w idx new} {
    upvar \#0 rat_textspell::cache$w cache

    mark_word $w $idx
    set start [$w index word_start]
    set end [$w index word_end]
    set tags [$w tag names $start]
    if {-1 != [set i [lsearch -exact $tags badspell]]} {
        set tags [lreplace $tags $i $i]
    }
    $w delete $start $end
    $w insert $start $new $tags
    set cache($new) 0
}

# rat_textspell::ignore_word --
#
# Start ignoring a word
#
# Arguments:
# w   - Text widget to work with
# idx - Index in text where word exists

proc rat_textspell::ignore_word {w idx} {
    upvar \#0 rat_textspell::state$w hd
    upvar \#0 rat_textspell::cache$w cache

    set word [$w get "$idx wordstart" "$idx wordend"]
    set cache($word) 0
    set hd(ignore,$word) 1
    run $w 1.0 end 1
}

# rat_textspell::learn_word --
#
# Learn a word
#
# Arguments:
# w   - Text widget to work with
# idx - Index in text where word exists

proc rat_textspell::learn_word {w idx} {
    upvar \#0 rat_textspell::state$w hd
    upvar \#0 rat_textspell::cache$w cache

    mark_word $w $idx
    set word [$w get word_start word_end]
    set cache($word) 0
    puts $hd(ispell_fd) "*$word"
    puts $hd(ispell_fd) "\#"
    flush $hd(ispell_fd)
    run $w 1.0 end 1
}
