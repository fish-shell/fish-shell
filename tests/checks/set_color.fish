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
