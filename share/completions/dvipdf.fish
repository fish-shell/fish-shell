# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c dvipdf -k -x -a "(
           __fish_complete_suffix .dvi
   )"
