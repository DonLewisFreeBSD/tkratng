set env(LIBDIR)         [pwd]/../tkrat
set env(CONFIG_DIR)     .
set env(COMPRESS)       /usr/bin/gzip
set env(CSUFFIX)        .gz
set env(SSH)            /usr/bin/ssh

# This version of the tkrat file is only intended to be used in
# the development tree.
lappend auto_path [pwd]/..
lappend auto_path $env(LIBDIR)

package require ratatosk 2.1

proc InitTestmsgs {} {
    global dir

    uplevel #0 source $dir/../data.tcl
}

InitTestmsgs

# Stubs for commands the library expects to find

proc RatLog {level message time} {
    global debug
    if {1 == $debug || 3 <= $level} {
	puts "Log: $level $time '$message'"
    }
}

proc RatFormatDate {year month day hour min sec} {
    puts "RatFormatDate $year $month $day $hour $min $sec"
    return DATE
}

proc RatLogin {host trial user prot port} {
    global env passwd
    return [list $env(USER) $passwd 0]
}

proc RatWantSave {} {
    return
}

proc RatEncodingCompat {wanted avail} {
    set wanted [string tolower $wanted]
    set avail [string tolower $avail]
    if {![string compare $wanted $avail]} {
	return 1
    }
    if {![string compare us-ascii $wanted] && [regexp iso-8859- $avail]} {
	return 1
    }
    return 0
}

proc RatDSNRecieve {subject action recipient id} {
    puts "RatDSNRecieve"
    puts "\tSubject: $subject"
    puts "\tAction: $action"
    puts "\tRecipient: $recipient"
    puts "\tId: $id"
}

set tkrat_version dev
set tkrat_version_date 20001217
set idCnt 0
set inbox ""
set expAfter {}     
set logAfter {}
set statusBacklog {} 
set currentColor {gray85 black} 
set ratLogBottom 0
set ratLogTop 0

# For c-client based imap-server
set imap_serv uwash
#set imap_serv cyrus
source imap-${imap_serv}.tcl

set option(ratatosk_dir) $dir
source ../tkrat/options.tcl
OptionsInit
RatGenId    # Force load of package
OptionsRead

InitMessages en t
InitCharsetAliases

set option(debug_file) $dir/LOG
set option(folder_sort) natural
