# There a several different implementations of netcat.
# Try to figure out which is the current used one
# and load the right set of completions.

complete -c nc -w (basename (realpath (which nc)))
