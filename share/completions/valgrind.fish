# Don't go invoking valgrind unless it is installed

set -l skin tool
if type -q valgrind; and valgrind --version ^/dev/null | string match -qr -- '-2\.[012]\.'
	# In older versions of Valgrind, the skin selection option was
    # '--skin'
	# But someone decided that it would be fun to change this to
    # '--tool' for no good reason
	set skin skin
end

complete -xc valgrind -l $skin --description "Skin" -a "
	memcheck\tHeavyweight\ memory\ checker
	cachegrind\tCache-miss\ profiler
	addrcheck\tLightweight\ memory\ checker
	helgrind\tData-race\ detector
	massif\tHeap\ profiler
"

function __fish_valgrind_skin --argument tool -V skin
	set -l cmd (commandline -cpo)
	# Quote $cmd so the tokens are separated with a space
	if string match -qr -- "--$skin(=| )$tool" "$cmd"
		return 0
    end
	# memcheck is the default tool/skin
	test $tool = memcheck
	and not string match -- $skin $cmd
end

complete -c valgrind -l help --description "Display help and exit"
complete -c valgrind -l help-debug --description "Display help and debug options"
complete -c valgrind -l version --description "Display version and exit"
complete -c valgrind -s q -l quiet --description "Quiet mode"
complete -c valgrind -s v -l verbose --description "Verbose mode"
complete -xc valgrind -l trace-children --description "Valgrind-ise children" -a "yes no"
complete -xc valgrind -l track-fds --description "Track file descriptors" -a "yes no"
complete -xc valgrind -l logfile-fd --description "Log to file descriptor" -a "0 1 2 3 4 5 6 7 8 9"
complete -rc valgrind -l logfile --description "Log to file"
complete -xc valgrind -l logsocket --description "Log to socket"
complete -c valgrind -l demangle -xd "Demangle C++ names" -a "yes no"
complete -xc valgrind -l num-callers --description "Callers in stack trace"
complete -xc valgrind -l error-limit --description "Stop showing errors if too many" -a "yes no"
complete -xc valgrind -l show-below-main --description "Continue trace below main()" -a "yes no"
complete -rc valgrind -l supressions --description "Supress errors from file"
complete -c valgrind -l gen-supressions --description "Print suppressions for detected errors"
complete -xc valgrind -l db-attach --description "Start debugger on error" -a "yes no"
complete -rc valgrind -l db-command --description "Debugger command"
complete -xc valgrind -l input-fd --description "File descriptor for input" -a "0 1 2 3 4 5 6 7 8 9"


# Memcheck-specific options
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l leak-check --description "Check for memory leaks" -a "no\tDon\'t\ check\ for\ memory\ leaks summary\t'Show a leak summary' full\t'Describe memory leaks in detail'"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l show-reachable --description "Show reachable leaked memory" -a "yes\t'Show reachable leaked memory' no\t'Do not show reachable leaked memory'"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l leak-resolution --description "Determines how willing Memcheck is to consider different backtraces to be the same" -a "low\t'Two entries need to match' med\t'Four entries need to match' high\t'All entries need to match'"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l freelist-vol --description "Set size of freed memory pool"
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l partial-loads-ok -d 'How to handle loads of words that are partially addressible' -a 'yes\t"Do not emit errors on partial loads" no\t"Emit errors on partial loads"'
complete -n "__fish_valgrind_skin memcheck" -xc valgrind -l avoid-strlen-errors -d 'Whether to skip error reporting for the strlen function' -a 'yes no'


# Addrcheck-specific options
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l leak-check --description "Check for memory leaks" -a "no\t'Do not check for memory leaks' summary\t'Show a leak summary' full\t'Describe memory leaks in detail'"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l show-reachable --description "Show reachable leaked memory" -a "yes\t'Show reachable leaked memory' no\t'Do not show reachable leaked memory'"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l leak-resolution --description "Determines how willing Addrcheck is to consider different backtraces to be the same" -a "low\t'Two entries need to match' med\t'Four entries need to match' high\t'All entries need to match'"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l freelist-vol --description "Set size of freed memory pool"
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l partial-loads-ok -d 'How to handle loads of words that are partially addressible' -a 'yes\t"Do not emit errors on partial loads" no\t"Emit errors on partial loads"'
complete -n "__fish_valgrind_skin addrcheck" -xc valgrind -l avoid-strlen-errors -d 'Whether to skip error reporting for the strlen function' -a 'yes no'

# Cachegrind-specific options
complete -n "__fish_valgrind_skin cachegrind" -xc valgrind -l I1 --description "Type of L1 instruction cache"
complete -n "__fish_valgrind_skin cachegrind" -xc valgrind -l D1 --description "Type of L1 data cache"
complete -n "__fish_valgrind_skin cachegrind" -xc valgrind -l L2 --description "Type of L2 cache"


# Massif-specific options
complete -c valgrind -n "__fish_valgrind_skin massif" -l alloc-fn --description "Specify a function that allocates memory" -x -a "(__fish_print_function_prototypes)"
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l heap -d 'Profile heap usage' -a 'yes\t"Profile heap usage" no\t"Do not profile heap usage"'
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l heap-admin --description "The number of bytes of heap overhead per allocation"
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l stacks --description "Profile stack usage" -a 'yes\t"Profile stack usage" no\t"Do not profile stack usage"'
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l depth --description "Depth of call chain"
complete -c valgrind -n "__fish_valgrind_skin massif" -x -l format --description "Profiling output format" -a "html\t'Produce html output' text\t'Produce text output'"
