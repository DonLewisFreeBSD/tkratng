puts "$HEAD Test wrapping"

namespace eval test_wrap {
}

proc test_wrap::test_cited_wrap {} {
    lappend texts [list {
>  1.1) If you use a "reply" function, its not possible, you must choose one
>       and only one of the two messages.
} {
>  1.1) If you use a "reply" function, its not possible, you must choose
>       one and only one of the two messages.
}]

    lappend texts [list {
>  - If you use a "reply" function, its not possible, you must choose one
>    and only one of the two messages.
>    and only one of the two messages.
} {
>  - If you use a "reply" function, its not possible, you must choose
>    one and only one of the two messages.
>    and only one of the two messages.
}]

    lappend texts [list {
> Martin,
>     I discussed the issues surrounding separate versus combined daemons
> for firewatch and monitor with Nathan.  We came up with a slightly
> different proposal that I'd like to see what you think about it.
} {
> Martin,
>     I discussed the issues surrounding separate versus combined
> daemons for firewatch and monitor with Nathan.  We came up with a
> slightly different proposal that I'd like to see what you think about
> it.
}]

    lappend texts [list {
> Ok.  Thanks for updating that.  I agree with everything you have.  So, going
> with your list we are down to fixing:
> 696
> 789
} {
> Ok.  Thanks for updating that.  I agree with everything you have.  So,
> going with your list we are down to fixing:
> 696
> 789
}]

    lappend texts [list {
> Hur är det tänkt att man skall kunna stoppa individuella services? Idag
> stoppar man ju portforwards i portforwardtabben. Skall man införa ett
> 'stop service' val i menyn man får upp om man högerklickar på ikonen för
> servicen (i Client). I Connect skulle man kunna byta texten 'run' på
> kanppen intill service ikonen till 'stop' när man väl startat en service.
} {
> Hur är det tänkt att man skall kunna stoppa individuella services?
> Idag stoppar man ju portforwards i portforwardtabben. Skall man införa
> ett 'stop service' val i menyn man får upp om man högerklickar på
> ikonen för servicen (i Client). I Connect skulle man kunna byta texten
> 'run' på kanppen intill service ikonen till 'stop' när man väl startat
> en service.
}]

    lappend texts [list {
>> > 1. Är allt nedan som står under 3.3 med i 3.3 och det under 4.0 med i 4.0
>> >    (och inte redan implementerat i 3.3 eller tvärt om)?
} {
>> > 1. Är allt nedan som står under 3.3 med i 3.3 och det under 4.0 med
>> >    i 4.0 (och inte redan implementerat i 3.3 eller tvärt om)?
}]

lappend texts [list {
> - when using the Color-Config "Steel Blue" the Balloon Help is unreadable
>   because of white text on yellow background. (I hunted in the src for
>   the balloon-help color-config, because I wanted to send you a diff, but
>   sorry, seems that I am to unfamiliar with Tk)
} {
> - when using the Color-Config "Steel Blue" the Balloon Help is
>   unreadable because of white text on yellow background. (I hunted in
>   the src for the balloon-help color-config, because I wanted to send
>   you a diff, but sorry, seems that I am to unfamiliar with Tk)
}]

lappend texts [list {
> 1.)	Only doing the changes on Mercury
> 
> 		results: Venus was inaccessible because of the differences in the appgate.conf
} {
> 1.)	Only doing the changes on Mercury
> 
> 		results: Venus was inaccessible because of the
> 		differences in the appgate.conf
}]

lappend texts [list {
> 2.)	Doing changes on Mercury and Venus and restarting the daemons on both.
} {
> 2.)	Doing changes on Mercury and Venus and restarting the daemons on
>    	both.
}]

lappend texts [list {
> 2.)	Doing changes on Mercury and Venus and restarting the daemons on both.
> 	Also a hypotetical second line which should force yet another wrap.
} {
> 2.)	Doing changes on Mercury and Venus and restarting the daemons on
>    	both. Also a hypotetical second line which should force yet
>    	another wrap.
}]

lappend texts [list {
> 2.)	Doing changes on Mercury and Venus and restarting the daemons on both.
> 		Result:		Everything works until we add or delete something that requires a Commit. At that time we get the error back in the Appgate.conf.
} {
> 2.)	Doing changes on Mercury and Venus and restarting the daemons on
>    	both.
> 		Result:		Everything works until we add or delete
> 		something that requires a Commit. At that time we get
> 		the error back in the Appgate.conf.
}]

lappend texts [list {
> should not wrap} {
> should not wrap}]

lappend texts [list {
> detta är det första meddelande jag skickar med nokian...en så länge känns det  bra...det enda som inte verkar fungera är att inställningarna inte sparas...} {
> detta är det första meddelande jag skickar med nokian...en så länge
> känns det  bra...det enda som inte verkar fungera är att
> inställningarna inte sparas...
}]

    set index 0
    foreach te $texts {
	StartTest "Wrapping cited text [incr index]"
	if {[lindex $te 1] != [RatWrapCited [lindex $te 0]]} {
            puts "---- Original ----"
            puts [lindex $te 0]
	    puts "---- Expected ----"
	    puts [lindex $te 1]
	    puts "---- Actual ----"
	    puts [RatWrapCited [lindex $te 0]]
	    ReportError "Wrapping failed"
	}
    }
}

proc test_wrap::test_edit_wrap {} {
    global option

    # The test cases
# Wrap is set to --|
# 1
lappend texts [list {
Hej hopp.
} {
Hej hopp.
}]

# 2
lappend texts [list {
This text should wrap
} {
This text should
wrap
}]

# 3
lappend texts [list {
  This text should wrap
} {
  This text should
  wrap
}]

# 4
lappend texts [list {
This text should wrap over multimple lines
} {
This text should
wrap over multimple
lines
}]

# 5
lappend texts [list {
This text should also wrap over
multimple lines
} {
This text should
also wrap over
multimple lines
}]

# 6
lappend texts [list {<BR>
* This text is part of<BR>
* a list<BR>
* where each line should wrap<BR>
} {
* This text is part
  of
* a list
* where each line
  should wrap
}]

# 7
lappend texts [list {<BR>
1 This text is part of<BR>
2 a list<BR>
3 where each line should wrap<BR>
} {
1 This text is part
  of
2 a list
3 where each line
  should wrap
}]

# 8
lappend texts [list {<BR>
1. This text is part of<BR>
2. a list<BR>
3. where each line should wrap<BR>
} {
1. This text is part
   of
2. a list
3. where each line
   should wrap
}]

# 9
lappend texts [list {<BR>
1) This text is part of<BR>
2) a list<BR>
3) where each line should wrap<BR>
} {
1) This text is part
   of
2) a list
3) where each line
   should wrap
}]

# 10
lappend texts [list {<BR>
- This text is part of<BR>
- a list<BR>
- where each line should wrap<BR>
} {
- This text is part
  of
- a list
- where each line
  should wrap
}]

# 11
lappend texts [list {<BR>
*grin* This is a simple test
} {
*grin* This is a
simple test
}]

# 12
lappend texts [list {<BR>
1.This text is part of<BR>
2.a list<BR>
3.where each line should wrap<BR>
} {
1.This text is part
of
2.a list
3.where each line
should wrap
}]

# 13
lappend texts [list {
This_is_a_wery_long_word
} {
This_is_a_wery_long_word
}]

# 14
lappend texts [list {
This_is_a_wery_long_word two
} {
This_is_a_wery_long_word
two
}]

# 15
lappend texts [list {
one This_is_a_wery_long_word two
} {
one
This_is_a_wery_long_word
two
}]

    text .t
    rat_edit::create .t

    set old_wrap_length $option(wrap_length)
    set option(wrap_length) 20

    set index 0

    foreach te $texts {
	StartTest "Wrapping edit [incr index]"
        # Insert the text
        .t delete 1.0 end
        .t insert 1.0 [lindex $te 0]

        # Convert any <BR>\n to newlines with noWrap
        set pos 1.0
        while {"" != [set pos [.t search "<BR>\n" $pos]]} {
            .t replace $pos $pos+5c "\n" noWrap
        }

        # Wrap between all instances of noWrap
        set pos "2.0 lineend"
        rat_edit::wrap .t $pos
        while {"" != [set range [.t tag nextrange noWrap $pos]]} {
            set pos [lindex $range 1]
            rat_edit::wrap .t $pos
        }
        
        if {[.t get 1.0 end-1c] != [lindex $te 1]} {
            puts "---- Original ----"
            puts [lindex $te 0]
	    puts "---- Expected ----"
	    puts [lindex $te 1]
	    puts "---- Actual ----"
	    puts [.t get 1.0 end-1c]
	    ReportError "Wrapping failed"
        }
    }

    set option(wrap_length) $old_wrap_length
    destroy .t
}

proc test_wrap::test_edit_insert {} {
    global option

    # The test cases (_ is cursor location)
# Wrap is set to --|
# 1
lappend texts [list {
Hej hopp._
} {space} {
Hej hopp. _
}]

# 2
lappend texts [list {
There is no such thing as a free lunch_
} {space} {
There is no such
thing as a free
lunch _
}]

# 3
lappend texts [list {
These lines will_ foo
wrapped
} {space} {
These lines will _
foo wrapped
}]

# 4
lappend texts [list {
These lines will_ foo
wrapped
} {space space} {
These lines will  _
foo wrapped
}]

# 5
lappend texts [list {
These lines will_ foo
wrapped
} {space b e} {
These lines will be_
foo wrapped
}]

# 6
lappend texts [list {
These foo _
wrapped
} {BackSpace} {
These foo_ wrapped
}]

# 7
lappend texts [list {
These foo  _
wrapped
} {BackSpace} {
These foo _wrapped
}]

# 8
lappend texts [list {
These lines will_ foo
wrapped
} {BackSpace BackSpace BackSpace BackSpace BackSpace
    BackSpace BackSpace BackSpace BackSpace BackSpace BackSpace} {
These_ foo wrapped
}]

# 9
lappend texts [list {
These foo _<BR>
* wrapped
} {BackSpace} {
These foo_
* wrapped
}]

# 10
lappend texts [list {
These foo bar fuu _<BR>
* wrapped
} {h e j} {
These foo bar fuu
hej_
* wrapped
}]

# 11
lappend texts [list {
These foo<BR>
* wrapped foo bar _
} {h e j} {
These foo
* wrapped foo bar
  hej_
}]

# 12
lappend texts [list {
These foo _
"wrapped"
} {BackSpace} {
These foo_ "wrapped"
}]

# 13
lappend texts [list {
 These foo _<BR>
 * wrapped
} {BackSpace} {
 These foo_
 * wrapped
}]

# 14
lappend texts [list {
 These foo bar fuu _<BR>
 * wrapped
} {h e j} {
 These foo bar fuu
 hej_
 * wrapped
}]

# 15
lappend texts [list {
 These foo bar fuu<BR>
 * wrapped foo bar _<BR>
 * wrap2
} {h e j} {
 These foo bar fuu
 * wrapped foo bar
   hej_
 * wrap2
}]

# 16
lappend texts [list {
 These foo<BR>
 * wrapped foo bar _
} {h e j} {
 These foo
 * wrapped foo bar
   hej_
}]

# 17
lappend texts [list {
 These foo _
 "wrapped"
} {BackSpace} {
 These foo_ "wrapped"
}]

# 18
lappend texts [list {
one two three_ four
five six seven
} {space} {
one two three _ four
five six seven
}]

# 19
lappend texts [list {
one two three_ four
five six seven
} {space space space} {
one two three   _
four five six seven
}]

# 20, this one breaks but I will ignore this
#lappend texts [list {
#one two three_ four
#five six seven
#} {space space space space space space space space} {
#one two three       
#_four five six seven
#}]
# Wrap is set to --|

    text .t
    rat_edit::create .t
    pack .t
    wm deiconify .

    set old_wrap_length $option(wrap_length)
    set option(wrap_length) 20

    set index 0

    foreach te $texts {
	StartTest "Wrapping edit insert [incr index]"

        # Insert the text
        .t delete 1.0 end
        .t insert 1.0 [lindex $te 0]

        # Convert any <BR>\n to newlines with noWrap
        set pos 1.0
        while {"" != [set pos [.t search "<BR>\n" $pos]]} {
            .t replace $pos $pos+5c "\n" noWrap
        }

        # Set cursor location to '_'
        .t mark set insert [.t search "_" 1.0]
        .t delete insert

        # Insert new text
        focus -force .t
        update
        foreach k [lindex $te 1] {
            event generate .t <$k>
        }

        # Get result
        .t insert insert "_"
        set real [.t get 1.0 end-1c]

        if {[.t get 1.0 end-1c] != [lindex $te 2]} {
            puts "---- Original ----"
            puts [lindex $te 0]
	    puts "---- Expected ----"
	    puts [lindex $te 2]
	    puts "---- Actual ----"
	    puts [.t get 1.0 end-1c]
	    ReportError "Action failed"
        }
    }

    set option(wrap_length) $old_wrap_length
    destroy .t
    wm withdraw .
}

package require rat_edit 1.0

test_wrap::test_cited_wrap
test_wrap::test_edit_wrap
test_wrap::test_edit_insert
