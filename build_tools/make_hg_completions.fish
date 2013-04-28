#!/usr/bin/env fish
#
# This file produces command specific completions for hg. Meant to be executed
# from the root directory (so the completions get put in the right place).

. build_tools/make_vcs_completions_generic.fish

write_completions hg >share/completions/hg.fish
