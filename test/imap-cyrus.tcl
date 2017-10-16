# IMAP-definieition and functions for dealing with an
# Cyrus imap-server running on localhost.

set imap_n user.$env(USER).test

set cyrus_dir /var/spool/imap

set mailServer(localhost) [list localhost 143 {debug} $env(USER)]

set imap_def [list Test imap {} localhost ${imap_n}]
set imap_def1 [list Test imap {} localhost ${imap_n}1]
set imap_def2 [list Test imap {} localhost ${imap_n}2]
set imap_fn1 ${imap_n}1
set imap_fn2 ${imap_n}2
set dis_def [list Test dis {} localhost $imap_n]
set imap_map $dir/disconnected/localhost:143/$imap_n+maf+imap/mappings
set start_uid 1

proc init_imap_folder {def} {
    global LEAD

    RatDeleteFolder $def
    if [catch {RatCreateFolder $def} result] {
	puts "$LEAD Failed to create folder $result [list $def]"
	exit 1
    }
}

proc cleanup_imap_folder {def} {
    RatDeleteFolder $def
}

proc insert_imap {def args} {
    global dir hdr env

    foreach m $args {
	set f [open "|/usr/pd/cyrus/bin/deliver -m [lindex $def 4]" w]
	puts $f [join [lrange [split $m "\n"] 1 end] "\n"]
	close $f
    }
}
