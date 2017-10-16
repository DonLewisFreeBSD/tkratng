# rat_spellutil.tcl --
#
# Utility functions for spell checking
#
#
#  TkRat software and its included text is Copyright 1996-2004 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.

package provide rat_spellutil 1.0

namespace eval rat_spellutil {
    namespace export get_dicts find_best_dict

    # The actual command used
    variable spell_cmd ""
    variable spell_args ""

    # True if we use aspell, false for ispell
    variable is_aspell 1

    # A list of installed dictionaries
    variable dictionaries {}

    # Old command
    variable old_spell_path ""

    # Reinit failed
    variable reinit_failed 0
}

# rat_spellutil::reinit_if_needed
#
# Resets the configration if the spelling command is changed
#
# Arguments:
# ignore_errors - True if we should ignore errors

proc rat_spellutil::reinit_if_needed {{ignore_errors 0}} {
    global option t

    if {$rat_spellutil::spell_cmd != ""
        && $option(spell_path) == $rat_spellutil::old_spell_path} {
        return
    }

    if {$rat_spellutil::reinit_failed} {
        return
    }

    set rat_spellutil::dictionaries {}
    set rat_spellutil::old_spell_path $option(spell_path)

    set cmd_args {}
    if {$option(spell_path) != "auto"} {
        if {[llength $option(spell_path)] > 1} {
            lappend candidates [lindex $option(spell_path) 0]
            set cmd_args [lrange $option(spell_path) 1 end]
        } elseif {[file isfile $option(spell_path)]
            && [file executable $option(spell_path)]} {
            lappend candidates $option(spell_path)
        } elseif {[file isdirectory $option(spell_path)]
                  && [file executable $option(spell_path)/ispell]} {
            lappend candidates $option(spell_path)/ispell
        } elseif {[file isdirectory $option(spell_path)]
                  && [file executable $option(spell_path)/aspell]} {
            lappend candidates $option(spell_path)/aspell
        } else {
            lappend candidates $option(spell_path)
        }
    } else {
        lappend candidates ispell
        lappend candidates aspell
    }

    set rat_spellutil::spell_cmd ""
    foreach cmd $candidates {
        if {![catch {eval "exec -- $cmd $cmd_args -v"} out]} {
            set rat_spellutil::spell_cmd $cmd
            set rat_spellutil::spell_args $cmd_args
            if {[string match -nocase "*aspell*" \
                     [lindex [split $out "\n"] 0]]} {
                set rat_spellutil::is_aspell 1
            } else {
                if {![regexp -nocase {ispell version 3.[12].*} $out]} {
                    set rat_spellutil::spell_cmd ""
                    continue
                }
                set rat_spellutil::is_aspell 0
            }
            break
        }
    }
    if {"" == $rat_spellutil::spell_cmd && !$ignore_errors} {
        tk_dialog .spellutil_err $t(spell_checking) $t(no_spell) error 0 $t(ok)
        set rat_spellutil::reinit_failed 1
        set option(autospell) 0
    }
}

# rat_spellutil::populate_dicts --
#
# Construct the list of available dictionaries
#
# Arguments:

proc rat_spellutil::populate_dicts {} {
    global option

    set cmd $rat_spellutil::spell_cmd
    set args $rat_spellutil::spell_args
    if {$rat_spellutil::is_aspell} {
        if {[catch {eval "exec -- $cmd $args config dict-dir"} libdir]} {
            return
        }
        set expr "*.{alias,multi}"
    } else {
        if {[catch {eval "exec -- $cmd $args -vv"} out]} {
            return
        }
        set libdir ""
        foreach l [split $out "\n"] {
            if {"LIBDIR" == [lindex $l 0]} {
                set libdir [lindex $l 2]
                break
            }
        }
        set expr "*.aff"
    }

    if {"" == $libdir} {
        return
    }
    foreach langs [glob -nocomplain -directory $libdir $expr] {
        set l [file root [file tail $langs]]
        if {"default" != $l} {
            lappend rat_spellutil::dictionaries $l
        }
    }

    if {0 == [llength $option(auto_dicts)]} {
        set option(auto_dicts) [create_auto_dicts]
    }
}

# rat_spellutil::create_auto_dicts --
#
# Create a suitable default for auto dicts. This is needed if we
# use aspell since we may have lots of duplicates of dictionaries.
# We assume that rat_spellutil::dictionaries and rat_spellutil::is_aspell
# are initialized
#
# Arguments:

proc rat_spellutil::create_auto_dicts {} {
    set auto_dicts {}
    if {$rat_spellutil::is_aspell} {
        foreach d $rat_spellutil::dictionaries {
            if {-1 != [string first "_" $d]} {
                if {[string match "*-w-accents" $d]
                    || -1 == [lsearch -exact $rat_spellutil::dictionaries \
                                  "$d-w-accents"]} {
                    lappend auto_dicts $d
                }
            }
        }
    }
    return $auto_dicts
}

# rat_spellutil::get_dicts --
#
# Return list of available dictionaries
#
# Arguments:
# ignore_errors - True if we should ignore errors

proc rat_spellutil::get_dicts {{ignore_errors 0}} {
    reinit_if_needed $ignore_errors
    if {0 == [llength $rat_spellutil::dictionaries]} {
        populate_dicts
    }
    return $rat_spellutil::dictionaries
}

# rat_spellutil::get_cmd --
#
# Return the actual spell check command used, or an empty string if
# none has been found.
#
# Arguments:

proc rat_spellutil::get_cmd {} {
    reinit_if_needed 1
    return $rat_spellutil::spell_cmd
}

# rat_spellutil::find_best_dict --
#
# Find the best dictionary for the given words
#
# Arguments:
# words - Words to test with

proc rat_spellutil::find_best_dict {words} {
    global option

    if {0 == [llength $option(auto_dicts)]} {
        set dicts [get_dicts]
        # The get_dicts call may have populated auto_dicts...
        if {0 != [llength $option(auto_dicts)]} {
            set dicts $option(auto_dicts)
        }
    } else {
        set dicts $option(auto_dicts)
    }
    set to_use [lindex $dicts 0]
    regsub -all {[^[:alnum:][:blank:][:punct:]]+} $words { } safe_words
    set best [llength $safe_words]
    reinit_if_needed
    if {"" == $rat_spellutil::spell_cmd} {
        return none
    }
    if {$rat_spellutil::is_aspell} {
        set list_option "list"
    } else {
        set list_option "-l"
    }
    foreach d $dicts {
        set cmd $rat_spellutil::spell_cmd
        set args $rat_spellutil::spell_args
        catch {eval "exec -- echo $safe_words |  $cmd $args $list_option -d $d"} out
        set errors [llength $out]
        if {$errors < $best} {
            set best $errors
            set to_use $d
        }
    }
    return $to_use
}

# rat_spellutil::launch_spell --
#
# Launch the spell process
#
# Arguments:
# dict - Dictionary to use

proc rat_spellutil::launch_spell {dict} {
    global option t

    reinit_if_needed
    if {"" == $rat_spellutil::spell_cmd} {
        return ""
    }

    if {$rat_spellutil::is_aspell} {
        set cmd "$rat_spellutil::spell_cmd $rat_spellutil::spell_args -a -d $dict"
    } else {
        set cmd "$rat_spellutil::spell_cmd $rat_spellutil::spell_args -a -S -P -d $dict"
    }
    if {[catch {open "|$cmd |& cat" r+} spell_fd]} {
	tk_dialog .spellutil_err $t(spell_checking) \
            "$t(failed_to_execute_spell). $spell_fd" error 0 $t(ok)
        return ""
    }
    fconfigure $spell_fd -buffering line
    # the first line ispell spits out is some ispell info
    # it's not real important to us
    flush $spell_fd
    gets $spell_fd line
    if {![regexp {^3.[12].*} [lindex $line 4]]} {
	tk_dialog .spellutil_err $t(spell_checking) \
            "$t(failed_to_execute_spell). $line" error 0 $t(ok)
        return ""
    }

    return $spell_fd
}
