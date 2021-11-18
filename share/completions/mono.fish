function __complete_mono --description 'Internal completion function for appending string to the commandline' --argument-names separator
  set --erase argv[1]
  set --local str (commandline --current-token --cut-at-cursor | string replace --regex --filter "(.*$separator)[^$separator]*" '$1' | string replace --regex -- '--.*=' '')

  for i in "$str"$argv
    echo -e $i
  end
end

complete --command mono --short-option h --long-option help --description 'Show help and exit'
complete --command mono --short-option V --long-option version --arguments 'number' --no-files --description 'Show version and exit'

# Runtime options
complete --command mono --long-option aot --arguments '(__complete_mono , \'asmonly\tInstruct the AOT compiler to output assembly code instead of an object file\'\\
  \'bind-to-runtime-version\tForce the generated AOT files to be bound to the runtime version of the compiling Mono\'\\
  \'data-outfile\tInstruct the AOT code generator to output certain data constructs into a separate file\'\\
  \'direct-icalls\tInvoke icalls directly instead of going through the operating system symbol lookup operation\'\\
  \'direct-pinvoke\tInvoke PInvoke methods directly instead of going through the operating system symbol lookup operation\'\\
  \'dwarfdebug\tInstruct the AOT compiler to emit DWARF debugging information\'\\
  \'full\tCreate binaries which can be used with the --full-aot option\'\\
  \'hybrid\tCreate binaries which can be used with the --hybrid-aot option\'\\
  \'llvm\tPerform AOT with the LLVM backend instead of the Mono backend where possible\'\\
  \'llvmonly\tPerform AOT with the LLVM backend exclusively and the Mono backend will not be used\'\\
  \'llvmopts\tSpecify flags to the built-in set of flags passed to the LLVM optimizer\'\\
  \'llvmllc\tSpecify flags to the built-in set of flags passed to the LLVM static compiler (llc)\'\\
  \'mcpu\tAllow AOT mode to use all instructions current CPU supports\'\\
  \'dedup-include\tPass compilation where we compile the methods that we had previously skipped\'\\
  \'info\tPrint the architecture the AOT in this copy of Mono targets and quit\'\\
  \'interp\tGenerate all required wrappers, so that it is possible to run --interpreter without any  code generation at runtime\'\\
  \'depfile\tOutput a gcc -M style dependency file\'\\
  \'ld-flags\tSpecify flags to pass to the C linker (if the current AOT mode calls for invoking it)\'\\
  \'llvm-path\tSame for the llvm tools \\\'opt\\\' and \\\'llc\\\'\'\\
  \'msym-dir\tInstruct the AOT compiler to generate offline sequence points .msym files\'\\
  \'mtriple\tUse the GNU style target triple <TRIPLE> to determine some code generation options\'\\
  \'nimt-trampolines\tPrecreate IMT trampolines in the AOT image\'\\
  \'ngsharedvt-trampolines\tPrecreate value type generic sharing trampolines in the AOT image\'\\
  \'nodebug\tInstruct the AOT compiler to not output any debugging information\'\\
  \'no-direct-calls\tPrevent the AOT compiler from generating a direct calls to a method\'\\
  \'nrgctx-trampolines\tPrecreate generic sharing trampolines in the AOT image\'\\
  \'nrgctx-fetch-trampolines\tPrecreate generic sharing fetch trampolines in the AOT image\'\\
  \'ntrampolines\tPrecreate method trampolines in the AOT image\'\\
  \'outfile\tInstruct the AOT compiler to save the output to the specified file\'\\
  \'print-skipped-methods\tOutput the skipped methods to the console\'\\
  \'profile\tSpecify a file to use for profile-guided optimization\'\\
  \'profile-only\t\'\\
  \'readonly-value\tOverride the value of a static readonly field\'\\
  \'save-temps,keep-temps\tInstruct the AOT compiler to keep temporary files\'\\
  \'soft-debug\tInstruct the compiler to generate sequence point checks\'\\
  \'static\tCreate  an ELF object file (.o) or .s file which can be statically linked into an executable when embedding the mono runtime\'\\
  \'stats\tPrint various stats collected during AOT compilation\'\\
  \'temp-path\tSpecify path to store temporary files created during AOT compilation\'\\
  \'threads\tUse multiple threads when compiling the methods\'\\
  \'tool-prefix\tPrepend <PREFIX> to the name of tools ran by the AOT compiler\'\\
  \'verbose\tPrint additional information about type loading failures\'\\
  \'write-symbols,no-write-symbols\tInstruct the AOT compiler to emit (or not emit) debug symbol information\'\\
  \'no-opt\tInstruct the AOT compiler tot no call opt when compiling with LLVM\')'\
  --no-files --description 'Precompile CIL code to native code'

complete --command mono --long-option aot-path --description 'List of additional directories to search for AOT images'
complete --command mono --long-option apply-bindings --description 'Apply the assembly bindings from the specified configuration file when running the AOT compiler'
complete --command mono --long-option assembly-loader --arguments 'strict\t"Check that the public key token, culture and version of a candidate assembly matches the requested strong name" legacy\t"Allow candidate as long as the name matches"' --no-files --description 'Specify assembly loader mode'
complete --command mono --long-option attach --arguments='disable' --no-files --description 'Disable which disables the attach functionality'
complete --command mono --long-option config --description 'Load the specified configuration file instead of the default one(s)'

complete --command mono --long-option debugger-agent --arguments '(__complete_mono , \'address\tSpecify the IP address where your debugger client is listening to\'\\
  \'loglevel\tSpecify the diagnostics log level for\'\\
  \'logfile\tInstruct the AOT code generator to output certain data constructs into a separate file\'\\
  \'server\tSpecify the file where the log will be stored, it defaults to standard output\'\\
  \'setpgid\tIf set to yes, Mono will call setpgid(0, 0) on startup\'\\
  \'suspend\tSuspend the vm on startup until it connects successfully to a debugger front end\'\\
  \'transport\tSpecify the transport that the debugger will use to communicate\')'\
  --no-files --description 'Instruct the Mono runtime to start a debugging agent inside the Mono runtime and connect it to a client user interface'

complete --command mono --long-option desktop --description 'Configure the virtual machine to be better suited for desktop applications'
complete --command mono --long-option full-aot --description 'Instruct the Mono runtime to not generate any code at runtime and depend exclusively on the code generated from using mono --aot=full previously'
complete --command mono --long-option full-aot-interp --description 'Same as --full-aot with fallback to the interpreter'
complete --command mono --long-option assembly-loader --arguments 'boehm\tBoehm legacy\tSGen' --no-files --description 'Select the Garbage Collector engine for Mono to use'
complete --command mono --long-option gc --arguments 'boehm\tBoehm sgen\tSGen' --no-files --description 'Select the Garbage Collector engine for Mono to use'
complete --command mono --long-option gc-debug --no-files --description 'Command line equivalent of the MONO_GC_DEBUG environment variable'
complete --command mono --long-option gc-params --no-files --description 'Command line equivalent of the MONO_GC_PARAMS environment variable'
complete --command mono --long-option arch --arguments '32\t"32 bit" 64\t"64 bit"' --no-files --description 'Select the bitness of the Mono binary used, if available'
complete --command mono --long-option interpreter --description 'Use Mono interpreter to execute a given assembly'
complete --command mono --long-option hybrid-aot --description 'Run assemblies that have been stripped of IL'
complete --command mono --long-option llvm --description 'Use the LLVM optimization and code generation engine to JIT or AOT compile'
complete --command mono --long-option ffast-math --description 'Allow Mono and LLVM to apply aggressive floating point optimizations'

complete --command mono --short-option o --long-option optimize --arguments '(__complete_mono , \'abcrem\tArray bound checks removal\'\\
  \'all\tTurn on all optimizations\'\\
  \'aot\tUsage of Ahead Of Time compiled code\'\\
  \'branch\tBranch optimizations\'\\
  \'cfold\tConstant folding\'\\
  \'cmov\tConditional moves [arch-dependency]\'\\
  \'deadce\tDead code elimination\'\\
  \'consprop\tConstant propagation\'\\
  \'copyprop\tCopy propagation\'\\
  \'fcmov\tFast x86 FP compares [arch-dependency]\'\\
  \'float32\tPerform 32-bit float arithmetic using 32-bit operations\'\\
  \'gshared\tEnable generic code sharing\'\\
  \'inline\tInline method calls\'\\
  \'intrins\tIntrinsic method implementations\'\\
  \'linears\tLinear scan global reg allocation\'\\
  \'leaf\tLeaf procedures optimizations\'\\
  \'loop\tLoop related optimizations\'\\
  \'peephole\tPeephole postpass\'\\
  \'precomp\tPrecompile all methods before executing Main\'\\
  \'sched\tInstruction scheduling\'\\
  \'shared\tEmit per-domain code\'\\
  \'sse2\tSSE2 instructions on x86 [arch-dependency]\'\\
  \'tailc\tTail recursion and tail calls\'\\
  \'transport\tSpecify the transport that the debugger will use to communicate\')'\
  --no-files --description 'Instruct the Mono runtime to start a debugging agent inside the Mono runtime and connect it to a client user interface'

complete --command mono --long-option response --description 'Provide a response file'
complete --command mono --long-option runtime --arguments '(mono --version)' --no-files --description 'Override autodetected Mono version'
complete --command mono --long-option server --description 'Configure the virtual machine to be better suited for server operations'
complete --command mono --long-option use-map-jit --description 'Instructs Mono to generate code using MAP_JIT on MacOS'
complete --command mono --long-option verify-all --description 'Verify mscorlib and assemblies in the global assembly cache for valid IL, and all user code for IL verifiability'

# Development options
complete --command mono --long-option debug --arguments '(__complete_mono , \'casts\tProduce a detailed error when throwing a InvalidCastException\'\\
  \'mdb-optimizations\tDisable some JIT optimizations which are usually only disabled when running inside the debugger\'\\
  \'gdb\tGenerate and register debugging information with gdb\')'\
  --no-files --description 'Turn on the debugging mode in the runtime'

complete --command mono --long-option profile --description 'Load a profiler module with the given arguments'
complete --command mono --long-option trace --description 'Show method names as they are invoked'
complete --command mono --long-option no-x86-stack-align --description 'Don\'t align stack frames on the x86 architecture'
complete --command mono --long-option jitmap --description 'Generate a JIT method map in a /tmp/perf-PID.map file'

# JIT maintainer options
complete --command mono --long-option bisect --no-files --description 'This flag is used by the automatic optimization bug bisector'
complete --command mono --long-option break --no-files --description 'Insert a breakpoint before the method whose name is \'method\' (namespace.class:methodname)'
complete --command mono --long-option breakonex --no-files --description 'Insert a breakpoint on exceptions'
complete --command mono --long-option compile --no-files --description 'Compiles a method (namespace.name:methodname)'
complete --command mono --long-option compile-all --description 'Compiles all the methods in an assembly'
complete --command mono --long-option graph --arguments 'cfg\t"Control Flow Graph (CFG)" dtree\t"Dominator Tree" code\t"CFG showing code" ssa\t"CFG showing code after SSA translation" optcode\t"CFG showing code after IR optimizations"' --no-files --description 'Generate a postscript file with a graph with the details about the specified method'
complete --command mono --long-option ncompile --description 'Instruct the runtime on the number of times that the method(-s) specified by --compile/--compile-all to be compiled'
complete --command mono --long-option stats --description 'Display information about the work done by the runtime during the execution of an application'
complete --command mono --long-option wapi --arguments 'hps\t"Delete the global semaphore" semdel\t"List the currently used handles"' --no-files --description 'Perform maintenance of the process shared data'
complete --command mono --short-option v --long-option verbose --description 'Increase the verbosity level'
