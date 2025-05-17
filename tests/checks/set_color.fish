#RUN: %fish %s

string escape (set_color normal)
# CHECK: \e\[m
string escape (set_color reset)
# CHECK: \e\[m

string escape (set_color red yellow)
# CHECK: \e\[31m
string escape (set_color red reset yellow)
# CHECK: \e\[31m

string escape (set_color --background=reset)
# CHECKERR: set_color: Unknown color 'reset'

string escape (set_color --bold red --background=normal)
# CHECK: \e\[31m\e\[49m\e\[1m
string escape (set_color --bold red --background=blue)
# CHECK: \e\[31m\e\[44m\e\[1m

string escape (set_color --background=f00 --background=green --background=00f)
# CHECK: \e\[48\;2\;255\;0\;0m
string escape (set_color --background=green)
# CHECK: \e\[42m
fish_term256=0 string escape (set_color --background=f00 --background=green --background=00f)
# CHECK: \e\[42m

string escape (set_color --underline=curly)
# CHECK: \e\[4:3m
string escape (set_color -ucurly)
# CHECK: \e\[4:3m
string escape (set_color -u)
# CHECK: \e\[4m
set_color --underline=asdf
# CHECKERR: set_color: invalid underline style: asdf
set_color -ushort
# CHECKERR: set_color: invalid underline style: short

string escape (set_color --underline-color=red)
# CHECK: \e\[58:5:1m
string escape (set_color --underline-color=normal)
# CHECK: \e\[59m

string escape (set_color --underline=double)
# CHECK: \e\[4:2m
string escape (set_color --underline=dotted)
# CHECK: \e\[4:4m
string escape (set_color --underline=dashed)
# CHECK: \e\[4:5m
