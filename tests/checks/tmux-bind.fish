# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

# Test moving around with up-or-search on a multi-line commandline.
isolated-tmux send-keys 'echo 12' M-Enter 'echo ab' C-p 345 C-n cde
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo 12345
# CHECK: echo abcde
