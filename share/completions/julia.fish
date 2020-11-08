# Completions for the Julia Programming Language

complete -c julia -s v -l version -d "Display version"
complete -c julia -s h -l help -d "Print help"
complete -c julia -l help-hidden -d "Print help of uncommon options"

complete -c julia -l project -d "Set DIR as the home project/environment"
complete -r -c julia -s J -l sysimage -d "Start up with SYSIMAGE"
complete -r -c julia -s H -l home -d "Set location of julia executable"
complete -x -c julia -l startup-file -a "yes no" -d "Load ~/.julia/config/startup.jl"
complete -x -c julia -l handle-signals -a "yes\tEnable no\tDisable" -d "Julia's default signal handlers"
complete -x -c julia -l sysimage-native-code -a "yes no" -d "Use native code from SYSIMAGE"
complete -x -c julia -l compiled-modules -a "yes\tEnable no\tDisable" -d "Incremental precompilation of modules"

complete -x -c julia -s e -l eval -d "Evaluate EXPR"
complete -x -c julia -s E -l print -d "Evaluate EXPR and display the result"
complete -r -c julia -s L -l load -d "Load FILE immediately on all processors"

complete -x -c julia -s t -l threads -a auto -d "The number of worker threads"
complete -x -c julia -s p -l procs -a auto -d "The number of local worker processes"
complete -c julia -l machine-file -d "Run processes on hosts listed in FILE"

complete -c julia -s i -d "Interactive mode"
complete -c julia -s q -l quiet -d "Quiet startup"
complete -x -c julia -l banner -a "yes\tEnable no\tDisable auto" -d "Startup banner"
complete -x -c julia -l color -a "yes\tEnable no\tDisable auto" -d "Color text"
complete -x -c julia -l history-file -a "yes no" -d "Load/Save history"

complete -x -c julia -l depwarn -a "yes\tEnable no\tDisable error" -d "Syntax and method deprecation warnings"
complete -x -c julia -l warn-overwrite -a "yes\tEnable no\tDisable" -d "Method overwrite warnings"
complete -x -c julia -l warn-scope -a "yes\tEnable no\tDisable" -d "Warning for ambiguous top-level scope"

complete -x -c julia -s C -l cpu-target -d "Limit usage of CPU features"
complete -x -c julia -s O -l optimize -a "0 1 2 3" -d "Set the optimization level"
complete -x -c julia -s g -d "Set the level of debug info generation"
complete -x -c julia -l inline -a "yes\tpermit no\tprohibit" -d "Control inlining"
complete -x -c julia -l check-bounds -a "yes\tAlways no\tNever" -d "Emit bounds checks"
complete -x -c julia -l math-mode -a "ieee\tDisallow fast\tEnable" -d "Unsafe floating point optimizations"

complete -x -c julia -l code-coverage -a "none user all" -d "Count executions of source lines"
complete -x -c julia -l track-allocation -a "none user all" -d "Count bytes allocated by each source line"
complete -c julia -l bug-report -d "Launch a bug report session"

# Uncommon options
complete -x -c julia -l compile -a "yes\tEnable no\tDisable all\t'Request exhaustive compilation' min" -d "JIT compiler"
complete -c julia -l output-o -d "Generate an object file"
complete -c julia -l output-ji -d "Generate a SYSIMAGE"
complete -c julia -l output-unopt-bc -d "Generate unoptimized LLVM bitcode"
complete -c julia -l output-jit-bc -d "Dump all IR"
complete -c julia -l output-bc -d "Generate LLVM bitcode"
complete -c julia -l output-asm -d "Generate an assembly"
complete -x -c julia -l output-incremental -a no -d "Generate an incremental output file"
complete -x -c julia -l trace-compile -a "stdout stderr" -d "Print precompile statements"
