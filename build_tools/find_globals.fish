#!/usr/bin/env fish

# Finds global variables by parsing the output of 'nm'
# for object files in this directory.
# This was written for macOS nm.

set total_globals 0

for file in ./**.o
  set filename (basename $file)
  for line in (nm -p -P -U $file)
    # Look in data (dD) and bss (bB) segments.
    set matches (string match --regex '^([^ ]+) ([dDbB])' -- $line) ; or continue
    echo $filename (echo $matches[2] | c++filt) $matches[3]
    set total_globals (math $total_globals + 1)
  end
end

echo "Total: $total_globals"