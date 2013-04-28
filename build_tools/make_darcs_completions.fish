#!/usr/bin/env fish
#
# This file produces command specific completions for darcs. Meant to be 
# executed from the root directory (so the completions get put in the right 
# place).

. build_tools/make_vcs_completions_generic.fish

set darcs_comp 'complete -c darcs -n "not __fish_use_subcommand" -a "(test -f _darcs/prefs/repos; and cat _darcs/prefs/repos)" --description "Darcs repo"'
set darcs_comp $darcs_comp 'complete -c darcs -a "test predist boringfile binariesfile" -n "contains setpref (commandline -poc)" --description "Set the specified option" -x'

write_completions darcs $darcs_comp >share/completions/darcs.fish
