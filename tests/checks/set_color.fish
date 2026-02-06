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

string escape (set_color --bold=red)
# CHECKERR: set_color: --bold=red: option does not take an argument
#CHECKERR: {{.*}}checks/set_color.fish (line {{\d+}}):
#CHECKERR: set_color --bold=red
#CHECKERR: ^
#CHECKERR: in command substitution
#CHECKERR: called on line {{\d+}} of file {{.*}}checks/set_color.fish
#CHECKERR: (Type 'help set_color' for related documentation)

string escape (set_color --strikethrough red --background=normal)
# CHECK: \e\[31\;49\;9m
string escape (set_color --strikethrough red --background=blue)
# CHECK: \e\[31\;44\;9m

string escape (set_color --bold red --background=normal)
# CHECK: \e\[31\;49\;1m
string escape (set_color --bold red --background=blue)
# CHECK: \e\[31\;44\;1m

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

string escape (set_color f00 --background=00f --underline-color=0f0 --bold --dim --italics --reverse --strikethrough --underline=curly)
# CHECK: \e\[38\;2\;255\;0\;0\;48\;2\;0\;0\;255\;58:2::0:255:0\;1\;4:3\;3\;2\;9m\e\[7m
