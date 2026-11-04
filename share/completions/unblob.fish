set -l completion "$(_UNBLOB_COMPLETE=fish_source unblob 2>/dev/null)"
string match -rq complete -- "$completion"
and echo $completion | source
