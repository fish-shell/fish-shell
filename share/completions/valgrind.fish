
if valgrind --version | grep -- '-2\.[012]\.' >/dev/null ^/dev/null
	# In older versions of Valgrind, the skin selection option was
    # '--skin'
	set -g skin skin
else
	# But someone decided that it would be fun to change this to
    # '--tool' for no good reason
	set -g skin tool
end

complete -xc valgrind -l $skin -d (N_ "Skin") -a "
	memcheck\tHeavyweight\ memory\ checker
	cachegrind\tCache-miss\ profiler
	addrcheck\tLightweight\ memory\ checker
	helgrind\tData-race\ detector
	massif\tHeap\ profiler
"

eval "function __fish_valgrind_skin; contains -- --$skin=\$argv (commandline -cpo);end"

set -e $skin

complete -c valgrind -l help -d (N_ "Display help and exit")
complete -c valgrind -l help-debug -d (N_ "Display help and debug options")
complete -c valgrind -l version -d (N_ "Display version and exit")
complete -c valgrind -s q -l quiet -d (N_ "Quiet mode")
complete -c valgrind -s v -l verbose -d (N_ "Verbose mode")
complete -xc valgrind -l trace-children -d (N_ "Valgrind-ise children") -a "yes no"
complete -xc valgrind -l track-fds -d (N_ "Track file descriptors") -a "yes no"
complete -xc valgrind -l logfile-fd -d (N_ "Log to file descriptor") -a "0 1 2 3 4 5 6 7 8 9"
complete -rc valgrind -l logfile -d (N_ "Log to file")
complete -xc valgrind -l logsocket -d (N_ "Log to socket")
complete -c valgrind -l demangle -xd "Demangle C++ names" -a "yes no"
complete -xc valgrind -l num-callers -d (N_ "Callers in stack trace")
complete -xc valgrind -l error-limit -d (N_ "Stop showing errors if too many") -a "yes no"
complete -xc valgrind -l show-below-main -d (N_ "Continue trace below main()") -a "yes no"
complete -rc valgrind -l supressions -d (N_ "Supress errors from file")
complete -c valgrind -l gen-supressions -d (N_ "Print suppressions for detected errors")
complete -xc valgrind -l db-attach -d (N_ "Start debugger on error") -a "yes no"
complete -rc valgrind -l db-command -d (N_ "Debugger command")
complete -xc valgrind -l input-fd -d (N_ "File descriptor for input") -a "0 1 2 3 4 5 6 7 8 9"


# Memcheck-specific options
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l leak-check -d (N_ "Check for memory leaks") -a "no\tDon\'t\ check\ for\ memory\ leaks summary\t'Show a leak summary' full\t'Describe memory leaks in detail'"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l show-reachable -d (N_ "Show reachable leaked memory") -a "yes\t'Show reachable leaked memory' no\t'Do not show reachable leaked memory'"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l leak-resolution -d (N_ "Determines how willing Memcheck is to consider different backtraces to be the same") -a "low\t'Two entries need to match' med\t'Four entries need to match' high\t'All entries need to match'"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l freelist-vol -d (N_ "Set size of freed memory pool")
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l partial-loads-ok -d 'How to handle loads of words that are partially addressible' -a 'yes\t"Do not emit errors on partial loads" no\t"Emit errors on partial loads"'
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l avoid-strlen-errors -d 'Whether to skip error reporting for the strlen function' -a 'yes no'


# Addrcheck-specific options
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l leak-check -d (N_ "Check for memory leaks") -a "no\t'Do not check for memory leaks' summary\t'Show a leak summary' full\t'Describe memory leaks in detail'"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l show-reachable -d (N_ "Show reachable leaked memory") -a "yes\t'Show reachable leaked memory' no\t'Do not show reachable leaked memory'"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l leak-resolution -d (N_ "Determines how willing Addrcheck is to consider different backtraces to be the same") -a "low\t'Two entries need to match' med\t'Four entries need to match' high\t'All entries need to match'"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l freelist-vol -d (N_ "Set size of freed memory pool")
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l partial-loads-ok -d 'How to handle loads of words that are partially addressible' -a 'yes\t"Do not emit errors on partial loads" no\t"Emit errors on partial loads"'
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l avoid-strlen-errors -d 'Whether to skip error reporting for the strlen function' -a 'yes no'

# Cachegrind-specific options
complete -n "__fish_valgrind_skin cachegrind" -xc valgrind -l I1 -d (N_ "Type of L1 instruction cache")
complete -n "__fish_valgrind_skin cachegrind" -xc valgrind -l D1 -d (N_ "Type of L1 data cache")
complete -n "__fish_valgrind_skin cachegrind" -xc valgrind -l L2 -d (N_ "Type of L2 cache")


# Massif-specific options
complete -c valgrind -n "__fish_valgrind_skin massif" -l alloc-fn -d (N_ "Specify a function that allocates memory") -x -a "(__fish_print_function_prototypes)"
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l heap -d 'Profile heap usage' -a 'yes\t"Profile heap usage" no\t"Do not profile heap usage"'
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l heap-admin -d (N_ "The number of bytes of heap overhead per allocation")
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l stacks -d (N_ "Profile stack usage") -a 'yes\t"Profile stack usage" no\t"Do not profile stack usage"'
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l depth -d (N_ "Depth of call chain")
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l format -d (N_ "Profiling output format") -a "html\t'Produce html output' text\t'Produce text output'"
