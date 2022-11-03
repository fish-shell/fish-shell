# SPDX-FileCopyrightText: © 2016 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
ech\
o echo
#CHECK: echo

buil\
tin echo builtin echo
#CHECK: builtin echo

true; an\
d echo true
#CHECK: true

\i\
\U00000066\
 true
    echo if true
\145n\
d\

#CHECK: if true

'if'\
 true
    echo if true
end
#CHECK: if true
