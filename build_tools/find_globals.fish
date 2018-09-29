#!/usr/bin/env fish

# Finds global variables by parsing the output of 'nm'
# for object files in this directory.
# This was written for macOS nm.

set total_globals 0
set boring_files fish_key_reader.cpp.o
set whitelist \
    termsize_lock termsize \
    initial_fg_process_group \
    _debug_level \
    sitm_esc ritm_esc dim_esc \
    s_main_thread_performer_lock s_main_thread_performer_cond s_main_thread_request_q_lock

for file in ./**.o
  set filename (basename $file)
  # Skip boring files.
  contains $filename $boring_files ; and continue
  for line in (nm -p -P -U $file)
    # Look in data (dD) and bss (bB) segments.
    set matches (string match --regex '^([^ ]+) ([dDbB])' -- $line) ; or continue
    set symname (echo $matches[2] | c++filt)
    contains $symname $whitelist ; and continue
    echo $filename $symname $matches[3]
    set total_globals (math $total_globals + 1)
  end
end

echo "Total: $total_globals"