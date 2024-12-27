#RUN: fish=%fish %fish %s

function fooc; true; end;

# A non-`complete -k` completion
complete -c fooc -fa "alpha delta bravo"

# A `complete -k` completion chronologically and alphabetically before the next completion. You'd
# expect it to come first, but we documented that it will come second.
complete -c fooc -fka "golf charlie echo"

# A `complete -k` completion that is chronologically after and alphabetically after the previous
# one, so a naive sort would place it after but we want to make sure it comes before.
complete -c fooc -fka "india foxtrot hotel"

# Another non-`complete -k` completion
complete -c fooc -fa "kilo juliett lima"

# Generate completions
complete -C"fooc "
# CHECK: india
# CHECK: foxtrot
# CHECK: hotel
# CHECK: golf
# CHECK: charlie
# CHECK: echo
# CHECK: alpha
# CHECK: bravo
# CHECK: delta
# CHECK: juliett
# CHECK: kilo
# CHECK: lima
