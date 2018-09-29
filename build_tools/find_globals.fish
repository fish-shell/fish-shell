#!/usr/bin/env fish

# Finds global variables by parsing the output of 'nm'
# for object files in this directory.
# This was written for macOS nm.

set total_globals 0
set boring_files \
    fish_key_reader.cpp.o \
    fish_tests.cpp.o \
    fish_indent.cpp.o \


set whitelist \
    termsize_lock termsize \
    initial_pid initial_fg_process_group \
    _debug_level \
    sitm_esc ritm_esc dim_esc \
    iothread_init()::inited \
    s_result_queue s_main_thread_request_queue s_read_pipe s_write_pipe \
    s_main_thread_performer_lock s_main_thread_performer_cond s_main_thread_request_q_lock \
    locked_consumed_job_ids \
    env_initialized \


for file in ./**.o
    set filename (basename $file)
    # Skip boring files.
    contains $filename $boring_files
    and continue
    for line in (nm -p -P -U $file)
        # Look in data (dD) and bss (bB) segments.
        set matches (string match --regex '^([^ ]+) ([dDbB])' -- $line)
        or continue
        set symname (echo $matches[2] | c++filt)
        contains $symname $whitelist
        and continue
        echo $filename $symname $matches[3]
        set total_globals (math $total_globals + 1)
    end
end

echo "Total: $total_globals"
