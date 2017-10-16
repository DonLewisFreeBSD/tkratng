puts "$HEAD Test wrapping"

namespace eval test_wrap {
    # List of text/result pairs
    variable texts {}
}

proc test_wrap::test_wrap {} {
    global LEAD errors verbose
    variable texts

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
> -when using the Color-Config "Steel Blue" the Balloon Help is unreadable
>  because of white text on yellow background. (I hunted in the src for
>  the balloon-help color-config, because I wanted to send you a diff, but
>  sorry, seems that I am to unfamiliar with Tk)
} {
> -when using the Color-Config "Steel Blue" the Balloon Help is
>  unreadable because of white text on yellow background. (I hunted in
>  the src for the balloon-help color-config, because I wanted to send
>  you a diff, but sorry, seems that I am to unfamiliar with Tk)
}]
    foreach te $texts {
	puts "Test wrapping"
	if {[lindex $te 1] != [RatWrapCited [lindex $te 0]]} {
	    puts "$LEAD wrapping failed"
	    incr errors
	    if {$verbose} {
		puts "---- Original ----"
		puts [lindex $te 0]
		puts "---- Expected ----"
		puts [lindex $te 1]
		puts "---- Actual ----"
		puts [RatWrapCited [lindex $te 0]]
	    }
	}
    }
}

test_wrap::test_wrap
