# options.tcl --
#
# This file contains defaults for all the options. These are just the
# built in defaults.
#
#
#  TkRat software and its included text is Copyright 1996-2002 by
#  Martin Forssén
#
#  The full text of the legal notice is contained in the file called
#  COPYRIGHT, included with this distribution.


# OptionsInit --
#
# Initialize the options to their default values
#
# Arguments:

proc OptionsInit {} {
    global env option tkrat_version ratCurrent

    # Last run version
    set option(last_version) ""

    # The date of the last version used
    set option(last_version_date) 0

    # Want information about changes?
    set option(info_changes) 1

    # UI language
    set option(language) en

    # Search path for global configuration files
    set option(global_config_path) $env(CONFIG_DIR)

    # Personal config directory
    if {![info exists option(ratatosk_dir)]} {
	set option(ratatosk_dir) ~/.ratatosk
    }

    # Database directory
    set option(dbase_dir) $option(ratatosk_dir)/db

    # Directory to backup database messages to
    set option(dbase_backup) $option(ratatosk_dir)/backup

    # Directory to store outgoing messages
    set option(send_cache) $option(ratatosk_dir)/send

    # How long to wait between expiring the database (in days)
    set option(expire_interval) 7

    # Directory for message hold
    set option(hold_dir) $option(ratatosk_dir)/hold

    # Userprocedures file
    set option(userproc) $option(ratatosk_dir)/userproc

    # Window size and positions file
    set option(placement) $option(ratatosk_dir)/placement

    # Main window name
    set option(main_window_name) "TkRat v$tkrat_version: %f (%r)"

    # Main window geometry
    set option(main_geometry) +0+50

    # Icon name
    set option(icon_name) "TkRat v$tkrat_version: %f"

    # Default folder specification
    set option(default_folder) "INBOX file {} INBOX"

    # Format of list of messages
    set option(list_format) "%4S %6d  %-24n %4B %t%s"

    # Which headers we should show
    set option(show_header) selected

    # Which the selected headers are:
    set option(show_header_selection) {From Subject Date To CC Reply-To}

    # Default permissions mask
    set option(permissions) 0600

    # Geometry of compose window
    set option(compose_geometry) +0+50

    # Which headers to compose by default
    set option(compose_headers) {To Subject Cc}

    # Which editor to use (%s will be expanded to a filename)
    set option(editor) "emacs %s"

    # True if we always want to use the external editor
    set option(always_editor) 0

    # Which domain we should pretend we are from
    set option(r0,masquerade_as) {}

    # List of SMTP hosts
    set option(r0,smtp_hosts) {localhost}

    # Default sening protocol
    set option(r0,sendprot) smtp

    # Default sending program
    set option(r0,sendprog) /usr/lib/sendmail

    # Can the sending program handle eightbit data
    set option(r0,sendprog_8bit) true

    # Default signature file
    set option(r0,signature) ~/.signature

    # If we should default to request DSN
    set option(r0,dsn_request) 0

    # The default reply_to address
    set option(r0,reply_to) ""

    # The defailt bcc address
    set option(r0,bcc) ""

    # The default From: address (may be empty)
    set option(r0,from) {}

    # Role name
    set option(r0,name) Standard

    # Default save outgoing
    set option(r0,save_outgoing) {}

    # PGP-key id
    set option(r0,pgp_keyid) {}

    # Default domain for unqualified addresses (defaults to domain of From:)
    set option(r0,uqa_domain) ""

    # Name for SMTP HELO/EHLO exchange (defaults to domain of From:)
    set option(r0,smtp_helo) ""

    # Default role
    set option(default_role) r0

    # List of roles
    set option(roles) {r0}

    # Which domain we are in
    set option(domain) {}

    # Default character set for tcl
    set option(charset) iso-8859-1

    # Leader string for replies
    set option(reply_lead) {> }

    # True (1) if we should show the watcher
    set option(watcher_enable) 1

    # Time between checking for new mail in different folders
    set option(watcher_time) {30}

    # Geometry of watcher
    set option(watcher_geometry) -140+0

    # Watcher window name
    set option(watcher_name) Watcher

    # Watcher max height
    set option(watcher_max_height) 10

    # Which messages the watcher shall show ('new' or 'all')
    set option(watcher_show) new

    # How many times the bell should be run when new messages arive
    set option(watcher_bell) 2

    # Format of list of messages in watcher
    set option(watcher_format) "%4S %-24n %s"

    # Print command
    set option(print_command) "lpr -P %p %s"

    # Headers to print
    set option(print_header) selected

    # Directory for temporary files
    set option(tmp) /tmp

    # Custom file command
    set option(mimeprog) "file"

    # Subject for replies to messages without subject
    set option(no_subject) "Re: (no subject)"

    # Default folder sort method
    set option(folder_sort) threaded

    # Message attribution
    set option(attribution) "On %d, %n wrote:"

    # Forwarded tag
    set option(forwarded_message) "------ Forwarded message ------"

    # File typing
    set option(typetable) { {*GIF* image/gif}
			    {*JPEG* image/jpeg}
			    {*JPG* image/jpeg}
			    {*PNG* image/png}
			    {"*HTML document*" text/html}
			    {"*8-bit u-law*" audio/basic}
			    {*MP3* audio/mp3}
			    {*PostScript* application/postscript}
			    {*PDF* application/pdf}
			    {*text* text/plain}
			    {*data* application/octet-stream}}

    # True if we want to see ALL messages from c-client (including babble)
    set option(see_bable) 0

    # True if we have looked for alias files
    set option(scan_aliases) 0

    # Number of messages to remember
    set option(num_messages) 10

    # True if we should lookup usernames in the local passwd-list
    set option(lookup_name) 1

    # Default database expiration type
    set option(def_extype) remove

    # Default database expiration time
    set option(def_exdate) +365

    # How many messages are required for one chunk (in dbase backup)
    set option(chunksize) 100

    # Where we should store dsn files
    set option(dsn_directory) $option(ratatosk_dir)/DSN

    # If we should remove delivery reports from folders
    set option(dsn_snarf_reports) 1

    # How many days each DSN entry should be kept in the list
    set option(dsn_expiration) 7

    # How verbose we should be when recieving DSN's
    set option(dsn_verbose) {{failed notify} {delayed status} {delivered status} {relayed status} {expanded none}}

    # Which message we should select when a folder is opened
    set option(start_selection) first_new

    # How long log messages should show (in ms)
    set option(log_timeout) 3

    # The font size we user
    set option(fontsize) 12

    # Folder window key combination
    set option(folder_key_compose) <Key-m>
    set option(folder_key_close) {<Control-Key-w> <Control-Key-c>}
    set option(folder_key_openfile)  <Control-Key-o>
    set option(folder_key_quit) <Control-Key-q>
    set option(folder_key_nextu) <Key-Tab>
    set option(folder_key_sync) <Control-Key-s>
    set option(folder_key_netsync) <Control-Key-y>
    set option(folder_key_update) <Control-Key-u>
    set option(folder_key_delete) <Key-d>
    set option(folder_key_undelete) <Key-u>
    set option(folder_key_flag) <Key-g>
    set option(folder_key_next) {<Key-Right> <Shift-Key-Down> <Key-n>}
    set option(folder_key_prev) {<Key-Left> <Shift-Key-Up> <Key-p>}
    set option(folder_key_replya) <Key-R>
    set option(folder_key_replys) <Key-r>
    set option(folder_key_forward_i) <Key-f>
    set option(folder_key_forward_a) <Key-F>
    set option(folder_key_home) {<Key-0> <Key-F27>}
    set option(folder_key_bottom) {<Key-F33> <Key-End>}
    set option(folder_key_pagedown) {<Key-space> <Key-F35> <Key-z>}
    set option(folder_key_pageup) {<Key-BackSpace> <Key-F29> <Control-b>}
    set option(folder_key_linedown) {<Key-Down>}
    set option(folder_key_lineup) {<Key-Up>}
    set option(folder_key_cycle_header) <Key-h>
    set option(folder_key_find) <Key-l>
    set option(folder_key_bounce) <Key-b>
    set option(folder_key_markunread) <Key-U>
    set option(folder_key_print) <Key-P>
    set option(folder_key_online) <Key-o>

    # Compose window key combinations
    set option(compose_key_send) <Control-s>
    set option(compose_key_abort) <Control-c>
    set option(compose_key_editor) <Control-o>
    set option(compose_key_undo) <Control-u>
    set option(compose_key_cut) <Control-w>
    set option(compose_key_copy) <Meta-w>
    set option(compose_key_cut_all) <Control-x>
    set option(compose_key_paste) <Control-y>
    set option(compose_key_wrap) <Control-j>

    # If we should check for stolen mail
    set option(mail_steal) 1

    # Data for netscape inbox
    set option(ms_netscape_pref_file) $env(HOME)/.netscape/preferences
    set option(ms_netscape_mtime) 0

    # True if we should remember the window positions
    set option(keep_pos) 1

    # True if we should let the user specify from address.
    set option(use_from) 1

    # The level of verboseness we should use when talking SMTP
    set option(smtp_verbose) 1

    # If we should try to send multiple letters though one channel
    set option(smtp_reuse) 1

    # Override color resources
    set option(override_color) 1

    # The color set
    set option(color_set) {gray85 black}

    # Which icon to set
    set option(icon) normal

    # The default expression mode
    set option(expression_mode) basic

    # If we should start up in iconic mode
    set option(iconic) 0

    # If the compose editor should warn about cutting all text etc
    set option(compose_warn) 1

    # Mailcap path
    set option(mailcap_path) \
	    {~/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap}

    # Terminal command
    set option(terminal) "xterm -e sh -c"

    # Imap port
    set option(imap_port) 143

    # Pop3 port
    set option(pop3_port) 110

    # Default remote user
    set option(remote_user) $env(USER)

    # Default remote host
    set option(remote_host) ""

    # SMTP timeout
    set option(smtp_timeout) 120

    # Should we sent even though we have a bad hostname?
    set option(force_send) 0

    # Should we skip the signature of the message we are replying to
    set option(skip_sig) 1

    # PGP operations
    set option(pgp_version) auto

    # Path to pgp program
    set option(pgp_path) {}

    # Extra pgp options
    set option(pgp_args) {}

    # Name of pgp keyring
    set option(pgp_keyring) {}

    # If we should make a copy of attached files
    set option(copy_attached) 1

    # If we should sign outgoing letters
    set option(pgp_sign) 0

    # If we should encrypt outgoing letters
    set option(pgp_encrypt) 0

    # Default url viewer
    set option(url_viewer) netscape

    # Name (and possibly path) of netscape command
    set option(netscape) {netscape -install}

    # Name (and possibly path) of opera command
    set option(opera) {opera}

    # Name (and path) of lynx command
    set option(lynx) {xterm -T "Lynx:%u" +sb -e lynx "%u"}

    # Name (and path) of other command
    set option(other_browser) {other_browser %u}

    # Color of URL
    set option(url_color) blue

    # System wide aliases
    set option(system_aliases) "System tkrat $env(CONFIG_DIR)/aliases"
    set option(use_system_aliases) 1

    # Personal alias lists
    set option(addrbooks) \
	    [list [list Personal tkrat $option(ratatosk_dir)/aliases]]

    # Default alias book
    set option(default_book) Personal

    # Default browse mode
    set option(browse) folder

    # Caching data
    set option(cache_pgp) 1
    set option(cache_pgp_timeout) 300
    set option(cache_passwd) 1
    set option(cache_passwd_timeout) 300
    set option(cache_conn) 1
    set option(cache_conn_timeout) 10

    # URL protocols
    set option(urlprot) {http https ftp news telnet}

    # Balloon help
    set option(show_balhelp) 1

    # Balloon help delay
    set option(balhelp_delay) 500

    # Message finding fields
    set option(msgfind_format) "%s%n%b%D"

    # Automatically expunge on folder close
    set option(expunge_on_close) 1

    # Checkpoint on window unmap
    set option(checkpoint_on_unmap) 1

    # How often should it checkpoint the mailbox (when deiconfied) (seconds)
    set option(checkpoint_interval) 600

    # List of known character sets
    set iso {}
    foreach e [encoding names] {
	if {[regsub iso8859 $e iso-8859 ne]} {
	    lappend iso $ne
	}
    }
    set option(charsets) [concat us-ascii [lsort -command isosort $iso]]
    lappend option(charsets) iso-2022-jp
    lappend option(charsets) iso-2022-kr
    set option(charset_candidates) \
	[linsert $option(charsets) 1 $option(charset)]

    # Automatically create sender field
    set option(create_sender) 0

    # Unused option which must be here
    set option(tip) {}

    # Alias expansion level 
    set option(alias_expand) 1

    # Dynamic folder behaviour (expanded | closed)
    set option(dynamic_behaviour) expanded

    # If submenus should have a tearoff entry
    set option(tearoff) 0

    # How long to delay certain menus (in milliseconds)
    set option(menu_delay) 200

    # Where to store cached passwords
    set option(pwcache_file) $option(ratatosk_dir)/pwcache

    # If we should add the signature delimiter
    set option(sigdelimit) 1

    # Place where lines wrap
    set option(wrap_length) 72

    # Regexp for finding citation marks
    set option(citexp) {^[ 	]*(([a-zA-Z0-9]+> *)|(>+ *)+)?}

    # Regexp for finding bullet characters
    set option(bullexp) {^(([0-9]+(\.[0-9]+)*[.\)]?)|[-*+o]) *}

    # Should we wrap cited text automatically
    set option(wrap_cited) 0

    # Directory to store local copies of disconnected folders
    set option(disconnected_dir) $option(ratatosk_dir)/disconnected

    # What to synchronize when doing a network synchronization
    # deferred_messages disconnected_mailboxes run_cmd cmd_to_run
    set option(network_sync) {1 1 0 {}}

    # Name of busy cursor
    set option(busy_cursor) watch

    # Regular expression which identifies the Re: part of subjects
    # will be applied with -nocase
    set option(re_regexp) {re:|sv:}

    # Printing defaults
    set option(print_pretty) 1
    set option(print_dest) printer
    if {[info exists env(PRINTER)]} {
	set option(print_printer) $env(PRINTER)
    } else {
	set option(print_printer) ps
    }
    set option(print_file) {tkrat.ps}
    set option(print_papersize) A4
    set option(print_papersizes) {{A4 {596 842}} {A3 {842 1191}}
				  {Letter {612 792}} {Legal {612 1008}}}
    set option(print_orientation) portrait
    set option(print_fontsize) 12
    set option(print_resolution) 72
    set option(print_fontfamily) Times

    # additional Compose/Replies options
    set option(append_sig) 1
    set option(reply_bottom) 1

    # Font options
    set option(override_fonts) 1
    set option(prop_norm) {components Helvetica 12 bold roman 0 0}
    set option(prop_light) {components Helvetica 12 normal roman 0 0}
    set option(fixed_norm) {components Courier 12 normal roman 0 0}
    set option(fixed_bold) {components Courier 12 bold roman 0 0}
    set option(watcher_font) {name 5x7}

    # Debug output dir
    set option(debug_file) $option(ratatosk_dir)/log

    # Wrap mode for shown messages
    set option(wrap_mode) word

    # Use input methodes?
    set option(useinputmethods) 0

    # Path to ssh command
    set option(ssh_path) $env(SSH)

    # Template to ssh command
    set option(ssh_template) {%s %s -l %s exec /etc/r%sd}

    # SSH timeout
    set option(ssh_timeout) 15

    # path to ispell
    set option(ispell_path) ispell

    # Online/offline mode
    set option(start_online_mode) last
    set option(online) 1

    # Template for new folder
    set option(template_folder) [list Template file {} $env(HOME)/FOO]

    # HTML proportional font family
    set option(html_prop_font) {name Times}

    # HTML fixed font family
    set option(html_fixed_font) {name Courier}

    # HTML proportional font sizes
    set option(html_prop_font_sizes) {8 9 10 12 14 18 24}
    
    # HTML fixed font sizes
    set option(html_fixed_font_sizes) {8 9 10 12 14 18 24}
    
    # Show HTML images
    set option(html_show_images) 0
}

# OptionsRead --
#
# Searches the filesystem for ratatoskrc files
#
# Arguments:

proc OptionsRead {} {
    global option globalOption env

    # Read global files
    foreach dir $option(global_config_path) {
	if {[file readable $dir/ratatoskrc]} {
	    source $dir/ratatoskrc
	}
    }
    # Take copy of global options
    foreach name [array names option] {
	set globalOption($name) $option($name)
    }
    # Read local modifications
    if {[file readable $option(ratatosk_dir)/ratatoskrc]} {
	source $option(ratatosk_dir)/ratatoskrc
    }
    # Read local overrides
    if {[file readable $option(ratatosk_dir)/ratatoskrc.tcl]} {
	source $option(ratatosk_dir)/ratatoskrc.tcl
    }

    # Setup list of charset candidates
    set option(charset_candidates) \
	[linsert $option(charsets) 1 $option(charset)]
}

# SaveOptions --
#
# Saves the users changes to the global options to disk.
#
# Arguments:

proc SaveOptions {} {
    global option globalOption

    # Warning message
    set message {#
# BEWARE of making changes to this file. It is automatically generated.
# You can change the values in this file via the preferences window. 
# This file can only contain "set option(<optname>) <value>" lines,
# everything else will be destroyed when the file is regenerated.
}

    set fh [open $option(ratatosk_dir)/ratatoskrc w]
    puts $fh $message
    # Write only changed values to local file
    foreach name [lsort [array names globalOption]] {
	if {[info exists option($name)]} {
	    if {[string compare $option($name) $globalOption($name)]} {
		puts $fh "set option($name) [list $option($name)]"
	    }
	}
    }
    # Write the roles we have defined by ourselves
    foreach name [array names option r*,*] {
	if {![info exists globalOption($name)]} {
	    puts $fh "set option($name) [list $option($name)]"
	}
    }
    close $fh
}

# ReadUserproc --
#
# Source the users userproc file with some caution
#
# Arguments:

proc ReadUserproc {} {
    global option t
    if {[file readable $option(userproc)]} {
	if {[catch "source $option(userproc)" message]} {
	    Popup "$t(error_in_userproc): $message"
	}
    }
}


# InitCharsetAliases
#
# Initialize the aliases of charcter sets
#
# Arguments:

proc InitCharsetAliases {} {
    global charsetAlias option charsetName charsetMapping \
	    charsetReverseMapping t

    # Mapping to tcl names
    set charsetMapping(us-ascii) ascii
    set charsetMapping(utf-8) utf-8
    foreach e [encoding names] {
	if {[regsub iso8859 $e iso-8859 ne]} {
	    set charsetMapping($ne) $e
	}
    }
    set charsetMapping(iso-2022-jp) iso2022-jp
    set charsetMapping(iso-2022-kr) iso2022-kr
    set charsetMapping(windows-1250) cp1250
    set charsetMapping(windows-1251) cp1251
    set charsetMapping(windows-1252) cp1252
    set charsetMapping(windows-1253) cp1253
    set charsetMapping(windows-1254) cp1254
    set charsetMapping(windows-1255) cp1255
    set charsetMapping(windows-1256) cp1256
    set charsetMapping(windows-1257) cp1257
    set charsetMapping(windows-1258) cp1258
    set charsetMapping(windows-437) cp437
    set charsetMapping(windows-737) cp737
    set charsetMapping(windows-775) cp775
    set charsetMapping(windows-850) cp850
    set charsetMapping(windows-852) cp852
    set charsetMapping(windows-855) cp855
    set charsetMapping(windows-857) cp857
    set charsetMapping(windows-860) cp860
    set charsetMapping(windows-861) cp861
    set charsetMapping(windows-862) cp862
    set charsetMapping(windows-863) cp863
    set charsetMapping(windows-864) cp864
    set charsetMapping(windows-865) cp865
    set charsetMapping(windows-866) cp866
    set charsetMapping(windows-869) cp869
    set charsetMapping(windows-874) cp874
    set charsetMapping(windows-932) cp932
    set charsetMapping(windows-936) cp936
    set charsetMapping(windows-949) cp949
    set charsetMapping(windows-950) cp950
    set charsetMapping(gb2312) ascii
    set charsetMapping(gb1988) ascii
    set charsetMapping(gb12345) ascii
    set charsetMapping($t(system_default_charset)) system

    # These are predefined (remember to only use lowercase letters)
    #set charsetAlias(bar) iso-8859-1

    # Read global files
    foreach dir $option(global_config_path) {
	if {[file readable $dir/charsetAliases]} {
	    source $dir/charsetAliases
	}
    }
    # Read local modifications
    if {[file readable $option(ratatosk_dir)/charsetAliases]} {
	source $option(ratatosk_dir)/charsetAliases
    }

    # Build structure
    catch {unset charsetName}
    foreach c $option(charsets) {
	set charsetName($c) ""
    }
    foreach a [array names charsetAlias] {
	if {[info exists charsetName($charsetAlias($a))]} {
	    lappend charsetName($charsetAlias($a)) $a
	}
    }

    foreach c [array names charsetMapping] {
	set charsetReverseMapping($charsetMapping($c)) $c
    }
}

# InitPgp --
#
# Initializes the pgp_version and pgp_prog variables if they are
# no already set.
#
# Arguments:

proc InitPgp {} {
    global option env

    if {"auto" != $option(pgp_version)} {
	return
    }

    set option(pgp_version) 0
    foreach d [split $env(PATH) :] {
	if {[file executable $d/pgpk]} {
	    set option(pgp_version) 5
	    set option(pgp_path) $d
	    set option(pgp_keyring) "~/.pgp/pubring.pkr"
	    return
	}
	if {[file executable $d/gpg]} {
	    set option(pgp_version) gpg-1
	    set option(pgp_path) $d
	    set option(pgp_keyring) "~/.pgp/pubring.pgp"
	}
	if {[file executable $d/pgp]} {
	    catch {exec $d/pgp -v} out
	    set version [lindex [lindex [split $out \n] 0] end]
	    if {[regexp {^6\.} $version]} {
		set option(pgp_version) 6
	    } else {
		set option(pgp_version) 2
	    }
	    set option(pgp_path) $d
	    set option(pgp_keyring) "~/.pgp/pubring.pgp"
	}
    }
}

# isosort --
#
# Sort iso-8859-X charcter set names
#
# Arguments:
# i1, i2 - The names to be compared

proc isosort {i1 i2} {
    set n1 [lindex [split $i1 -] 2]
    set n2 [lindex [split $i2 -] 2]
    if {[string length $n1] != [string length $n2]} {
	return [expr [string length $n1]-[string length $n2]]
    } else {
	return [expr $n1-$n2]
    }
}
