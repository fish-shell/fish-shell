#!/usr/bin/env fish

# Finds global variables by parsing the output of 'nm'
# for object files in this directory.
# This was written for macOS nm.

set -l FISH_SOURCE_DIR $argv[1]
if not test -d "$FISH_SOURCE_DIR"
    echo "FISH_SOURCE_DIR not given"
    exit 1
end

set -g whitelist \
    # unclear what this is \
    l_constinit \
    # hacks to work around missing ncurses strings on mac \
    sitm_esc ritm_esc dim_esc \


# In our nm regex, we are interested in data (dD) and bss (bB) segments.
set -g nm_regex '^([^ ]+) ([dDbB])'

set -l total_globals 0
set -l boring_files \
    fish_key_reader.cpp.o \
    fish_tests.cpp.o \
    fish_indent.cpp.o \


# return if we should ignore the given symbol name
function should_ignore
    set -l symname $argv[1]
    string match -q '*guard variable for*' $symname
    and return 0
    contains $symname $whitelist
    and return 0
    return 1
end

# echo a cleaned-up symbol name, e.g. replacing template gunk
function cleanup_syname
    set -l symname $argv[1]
    set symname (string replace --all 'std::__1::basic_string<wchar_t, std::__1::char_traits<wchar_t>, std::__1::allocator<wchar_t> >' 'wcstring' $symname)
    set symname (string replace --all 'std::__1::vector<wcstring, std::__1::allocator<wcstring > >' 'wcstring_list_t' $symname)
    echo $symname
end

# Output the declaration for a symbol name in a given file.
function print_decl -a FISH_SOURCE_DIR objfile symname
    set -l varname (string split '::' $symname)[-1]
    set -l srcfile (basename $objfile .o)
    set -l srcpath $FISH_SOURCE_DIR/src/$srcfile

    # A leading underscore indicates a global, strip it.
    set varname (string replace --regex '^_' '' $varname)

    if not test -f "$srcpath"
        echo "Could not find $srcpath"
    end
    # Guess the variable as the first usage of the name.
    # Strip everything after the first =.
    set -l vardecl (egrep -m 1 " $varname\\b" $srcpath | cut -f -1 -d '=' | string trim)
    if test -z "$vardecl"
        echo "COULD_NOT_FIND_$varname"
        return 1
    end
    echo $vardecl
    return 0
end

# Return if a variable declaration is "thread safe".
function decl_is_threadsafe
    set -l vardecl $argv[1]
    # decls starting with 'const ' or containing ' const ' are assumed safe.
    string match -q --regex '(^|\\*| )const ' $vardecl
    and return 0

    # Ordinary types indicating a safe variable.
    set -l safes relaxed_atomic_bool_t std::mutex std::condition_variable std::once_flag sig_atomic_t
    for safe in $safes
        string match -q "*$safe*" $vardecl
        and return 0
    end

    # Template types indicate a safe variable.
    set safes owning_lock mainthread_t std::atomic relaxed_atomic_t latch_t
    for safe in $safes
        string match -q "*$safe<*" $vardecl
        and return 0
    end
end

for file in ./**.o
    set -l filename (basename $file)
    # Skip boring files.
    contains $filename $boring_files
    and continue
    for line in (nm -p -P -U $file | egrep $nm_regex)
        set -l matches (string match --regex $nm_regex -- $line)
        or continue
        set -l symname (cleanup_syname (echo $matches[2] | c++filt))
        should_ignore $symname
        and continue
        set -l vardecl (print_decl $FISH_SOURCE_DIR $filename $symname)
        decl_is_threadsafe $vardecl
        and continue
        echo $filename $symname $matches[3] ":" $vardecl
        set total_globals (math $total_globals + 1)
    end
end

echo "Total: $total_globals"
