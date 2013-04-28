#!/usr/bin/env fish
#
# This file produces command specific completions for svn. Meant to be executed
# from the root directory (so the completions get put in the right place).

. build_tools/make_vcs_completions_generic.fish

write_completions svn >share/completions/svn.fish
