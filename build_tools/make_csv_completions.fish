#!/usr/bin/env fish
#
# This file produces command specific completions for csv. Meant to be executed
# from the root directory (so the completions get put in the right place).

. build_tools/make_vcs_completions_generic.fish

write_completions csv >share/completions/csv.fish
