# Runtime options
complete -c mono -s h -l help -d 'Show help'
complete -c mono -s V -l version -a number -f -d 'Show version'

complete -c mono -l aot -a '(__fish_append , \\
  asmonly\t"Instruct the AOT compiler to output assembly code instead of an object file" \\
  bind-to-runtime-version\t"Force the generated AOT files to be bound to the runtime version of the compiling Mono" \\
  data-outfile\t"Instruct the AOT code generator to output certain data constructs into a separate file" \\
  direct-icalls\t"Invoke icalls directly instead of going through the operating system symbol lookup operation" \\
  direct-pinvoke\t"Invoke PInvoke methods directly instead of going through the operating system symbol lookup operation" \\
  dwarfdebug\t"Instruct the AOT compiler to emit DWARF debugging information" \\
  full\t"Create binaries which can be used with the --full-aot option" \\
  hybrid\t"Create binaries which can be used with the --hybrid-aot option" \\
  llvm\t"Perform AOT with the LLVM backend instead of the Mono backend where possible" \\
  llvmonly\t"Perform AOT with the LLVM backend exclusively and the Mono backend will not be used" \\
  llvmopts\t"Specify flags to the built-in set of flags passed to the LLVM optimizer" \\
  llvmllc\t"Specify flags to the built-in set of flags passed to the LLVM static compiler (llc)" \\
  mcpu\t"Allow AOT mode to use all instructions current CPU supports" \\
  dedup-include\t"Pass compilation where we compile the methods that we had previously skipped" \\
  info\t"Print the architecture the AOT in this copy of Mono targets and quit" \\
  interp\t"Generate all required wrappers, so that it is possible to run --interpreter without any  code generation at runtime" \\
  depfile\t"Output a gcc -M style dependency file" \\
  ld-flags\t"Specify flags to pass to the C linker (if the current AOT mode calls for invoking it)" \\
  llvm-path\t"Same for the llvm tools \\\'opt\\\' and \\\'llc\\\'" \\
  msym-dir\t"Instruct the AOT compiler to generate offline sequence points .msym files" \\
  mtriple\t"Use the GNU style target triple <TRIPLE> to determine some code generation options" \\
  nimt-trampolines\t"Precreate IMT trampolines in the AOT image" \\
  ngsharedvt-trampolines\t"Precreate value type generic sharing trampolines in the AOT image" \\
  nodebug\t"Instruct the AOT compiler to not output any debugging information" \\
  no-direct-calls\t"Prevent the AOT compiler from generating direct calls to a method" \\
  nrgctx-trampolines\t"Precreate generic sharing trampolines in the AOT image" \\
  nrgctx-fetch-trampolines\t"Precreate generic sharing fetch trampolines in the AOT image" \\
  ntrampolines\t"Precreate method trampolines in the AOT image" \\
  outfile\t"Instruct the AOT compiler to save the output to the specified file" \\
  print-skipped-methods\t"Output the skipped methods to the console" \\
  profile\t"Specify a file to use for profile-guided optimization" \\
  profile-only\t"" \\
  readonly-value\t"Override the value of a static readonly field" \\
  save-temps,keep-temps\t"Instruct the AOT compiler to keep temporary files" \\
  soft-debug\t"Instruct the compiler to generate sequence point checks" \\
  static\t"Create  an ELF object file (.o) or .s file which can be statically linked into an executable when embedding the mono runtime" \\
  stats\t"Print various stats collected during AOT compilation" \\
  temp-path\t"Specify path to store temporary files created during AOT compilation" \\
  threads\t"Use multiple threads when compiling the methods" \\
  tool-prefix\t"Prepend <PREFIX> to the name of tools ran by the AOT compiler" \\
  verbose\t"Print additional information about type loading failures" \\
  write-symbols,no-write-symbols\t"Instruct the AOT compiler to emit (or not emit) debug symbol information" \\
  no-opt\t"Instruct the AOT compiler tot no call opt when compiling with LLVM")' \
    -f -d 'Precompile CIL code to native code'

complete -c mono -l aot-path -d 'Show additional directories to search for AOT images'
complete -c mono -l apply-bindings \
    -d 'Apply the assembly bindings from the specified configuration file when running the AOT compiler'
complete -c mono -l assembly-loader \
    -a 'strict\t"Check that the public key token, culture and version of a candidate assembly matches the requested strong name" legacy\t"Allow candidate as long as the name matches"' \
    -f -d 'Use assembly loader mode'
complete -c mono -l attach -a disable -f -d 'Disable the attach functionality'
complete -c mono -l config -d 'Read configuration from file instead of using default one'

complete -c mono -l debugger-agent -a '(__fish_append , \\
  address\t"Specify the IP address where your debugger client is listening to" \\
  loglevel\t"Specify the diagnostics log level for" \\
  logfile\t"Instruct the AOT code generator to output certain data constructs into a separate file" \\
  server\t"Specify the file where the log will be stored, it defaults to standard output" \\
  setpgid\t"If set to yes, Mono will call setpgid(0, 0) on startup" \\
  suspend\t"Suspend the vm on startup until it connects successfully to a debugger front end" \\
  transport\t"Specify the transport that the debugger will use to communicate")' \
    -f -d 'Use a debugging agent inside the Mono runtime and connect it to a client user interface'

complete -c mono -l desktop \
    -d 'Configure the virtual machine to be better suited for desktop applications'
complete -c mono -l full-aot \
    -d 'Depend exclusively on code generated from previous mono --aot=full'
complete -c mono -l full-aot-interp \
    -d 'Same as --full-aot with fallback to the interpreter'
complete -c mono -l gc -a 'boehm\tBoehm sgen\tSGen' -f -d 'Use the Garbage Collector engine'
complete -c mono -l gc-debug -f -d 'Specify MONO_GC_DEBUG environment variable value'
complete -c mono -l gc-params -f -d 'Specify MONO_GC_PARAMS environment variable value'
test "$(uname)" = Darwin && complete -c mono -l arch -a '32\t"32 bit" 64\t"64 bit"' -f \
    -d 'Use the bitness of the Mono binary used, if available'
complete -c mono -l interpreter -d 'Use Mono interpreter to execute a given assembly'
complete -c mono -l hybrid-aot -d 'Run assemblies that have been stripped of IL'
complete -c mono -l llvm -d 'Use the LLVM optimization and code generation engine to JIT or AOT compile'
complete -c mono -l ffast-math -d 'Use Mono and LLVM aggressive floating point optimizations'

complete -c mono -s o -l optimize -a '(__fish_append , \\
  abcrem\t"Array bound checks removal" \\
  all\t"Turn on all optimizations" \\
  aot\t"Usage of Ahead Of Time compiled code" \\
  branch\t"Branch optimizations" \\
  cfold\t"Constant folding" \\
  cmov\t"Conditional moves [arch-dependency]" \\
  deadce\t"Dead code elimination" \\
  consprop\t"Constant propagation" \\
  copyprop\t"Copy propagation" \\
  fcmov\t"Fast x86 FP compares [arch-dependency]" \\
  float32\t"Perform 32-bit float arithmetic using 32-bit operations" \\
  gshared\t"Enable generic code sharing" \\
  inline\t"Inline method calls" \\
  intrins\t"Intrinsic method implementations" \\
  linears\t"Linear scan global reg allocation" \\
  leaf\t"Leaf procedures optimizations" \\
  loop\t"Loop related optimizations" \\
  peephole\t"Peephole postpass" \\
  precomp\t"Precompile all methods before executing Main" \\
  sched\t"Instruction scheduling" \\
  shared\t"Emit per-domain code" \\
  sse2\t"SSE2 instructions on x86 [arch-dependency]" \\
  tailc\t"Tail recursion and tail calls" \\
  transport\t"Specify the transport that the debugger will use to communicate")' \
    -f -d 'Use optimizations'

complete -c mono -l response -d 'Use a response file'
complete -c mono -l runtime -a '(mono --version)' -f -d 'Use Mono version'
complete -c mono -l server -d 'Optimize the virtual machine to be better suited for server operations'
complete -c mono -l use-map-jit -d 'Generate code using MAP_JIT on MacOS'
complete -c mono -l verify-all \
    -d 'Verify mscorlib and assemblies in the cache for valid IL, and user code for IL verifiability'

# Development options
complete -c mono -l debug -a '(__fish_append , \\
  casts\t"Produce a detailed error when throwing a InvalidCastException" \\
  mdb-optimizations\t"Disable some JIT optimizations which are usually only disabled when running inside the debugger" \\
  gdb\t"Generate and register debugging information with gdb")' \
    -f -d 'Use the debugging mode in the runtime'

complete -c mono -l profile -f -d 'Use a profiler module with the given arguments'
complete -c mono -l trace -f -d 'Show method names as they are invoked'
complete -c mono -l no-x86-stack-align -d 'Don\'t align stack frames on the x86 architecture'
complete -c mono -l jitmap -d 'Generate a JIT method map in a /tmp/perf-PID.map file'

# JIT maintainer options
complete -c mono -l bisect -f -d 'This flag is used by the automatic optimization bug bisector'
complete -c mono -l break -x \
    -d 'Insert a breakpoint before the method whose name is \'method\' (namespace.class:methodname)'
complete -c mono -l breakonex -d 'Use a breakpoint on exceptions'
complete -c mono -l compile -x -d 'Compile a method (namespace.name:methodname)'
complete -c mono -l compile-all -d 'Compile all the methods in an assembly'
complete -c mono -l graph \
    -a 'cfg\t"Control Flow Graph (CFG)"
    dtree\t"Dominator Tree" code\t"CFG showing code"
    ssa\t"CFG showing code after SSA translation"
    optcode\t"CFG showing code after IR optimizations"' \
    -x -d 'Generate a postscript file with a graph with the details about the specified method'
complete -c mono -l ncompile \
    -d 'Tell runtime to compile methods specified by --compile N times'
complete -c mono -l stats \
    -d 'Display information about the work done by the runtime during the execution of an application'
complete -c mono -l wapi \
    -a 'hps\t"Delete the global semaphore"
    semdel\t"List the currently used handles"' \
    -x -d 'Perform maintenance of the process shared data'
complete -c mono -s v -l verbose -d 'Show more messages'
