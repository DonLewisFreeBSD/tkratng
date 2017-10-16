# rat_enriched --
#
# Insert enriched text into text widget
# Enriched text is defined in rfc1896
# For now we just have a minimal implementation
#

package provide rat_enriched 1.0

namespace eval rat_enriched {
    namespace export show
}


# rat_enriched::show --
#
# Insert the given enriched text into teh given text widget.
#
# Arguments:
# w    - text widget to insert into
# data - Text to insert
# tag  - Tag to put on inserted data

proc rat_enriched::show {w data tag} {
    # Replace all '=' by 'EqUaL'
    regsub -all = $data EqUaL data

    # Convert linenedings as per the rfc
    regsub -all "\n" $data {=} data
    regsub -all {(^|[^=])=([^=]|$)} $data {\1 \2} data
    regsub -all {=(=+)} $data {\1} data
    regsub -all {=} $data "\n" data

    # Convert <<
    regsub -all "<" $data {=} data
    regsub -all "==" $data {<} data

    # Remove parameters
    regsub -all {=param>[^=]*=/param>} $data {} data
    # Remove all tokens
    regsub -all {=/?[a-zA-Z]+>} $data {} data
    # Restore the '='
    regsub -all EqUaL $data = data

    # Configure text widget
    $w tag configure enriched -wrap word
    $w insert end $data [list enriched tag]
}
