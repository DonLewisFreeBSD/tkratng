# IMAP-definitions and functions for dealing with an
# UWashington imap-server running on localhost.

set imap_fn $dir/imap-folder.[pid]

set mailServer(localhost) \
	[list localhost {} {debug {ssh-cmd {%s %s -l %s exec /usr/sbin/%sd}}} \
	$env(USER)]

set imap_def [list Test imap {} localhost $imap_fn]
set imap_def1 [list Test imap {} localhost ${imap_fn}-1]
set imap_def2 [list Test imap {} localhost ${imap_fn}-2]
set imap_fn1 ${imap_fn}-1
set imap_fn2 ${imap_fn}-2
set dis_def [list Test dis {} localhost $imap_fn]
set imap_map $dir/disconnected/localhost:143$imap_fn+$env(USER)+imap/mappings
set start_uid 11

proc init_imap_folder {def} {
    global hdr

    set fh [open [lindex $def 4] w]
    puts $fh $hdr
    close $fh
}

proc cleanup_imap_folder {def} {
    file delete [lindex $def 4]
}

proc insert_imap {def args} {
    set fh [open [lindex $def 4] a]
    foreach m $args {
	puts $fh $m
    }
    close $fh
}
