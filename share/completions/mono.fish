complete -c mono -s h -l help -d 'Show help and exit'
complete -c mono -s V -l version -a number -f -d 'Show version and exit'

complete -c mono -l aot -a 'asmonly bind-to-runtime-version data-outfile direct-icalls
  direct-pinvoke dwarfdebug full hybrid llvm llvmonly llvmopts llvmllc mcpu dedup-include
  info interp depfile ld-flags llvm-path msym-dir mtriple nimt-trampolines ngsharedvt-trampolines
  nodebug no-direct-calls nrgctx-trampolines nrgctx-fetch-trampolines ntrampolines outfile
  print-skipped-methods profile profile-only readonly-value save-temps,keep-temps soft-debug
  static stats temp-path threads tool-prefix verbose write-symbols,no-write-symbols no-opt' \
    -f -d 'Precompile CIL code to native code'
complete -c mono -l debug -a 'casts mdb-optimizations gdb' -f -d 'Use debug mode'
complete -c mono -l debugger-agent -a 'address loglevel logfile server setpgid suspend
  transport' -f -d 'Use debugger agent'
complete -c mono -l profile -d 'Use profiler module'
complete -c mono -l trace -d 'Trace expression'
complete -c mono -l jitmap -d 'Generate JIT method map in /tmp/perf-PID.map'

complete -c mono -l config -F -r -d 'Use config'
complete -c mono -s v -l verbose -d 'Increase verbosity level'
complete -c mono -l runtime -f -d 'Use runtime version'
complete -c mono -l optimize -a 'abcrem all aot branch cfold cmov deadce consprop copyprop
  fcmov float32 gshared inline intrins linears leaf loop peephole precomp sched shared
  sse2 tailc' -f -d 'Use optimizations'
complete -c mono -l attach -a disable -f -d 'Specify attach agent options'
complete -c mono -l llvm -d 'Use LLVM'
complete -c mono -l nollvm -d 'Don\'t use LLVM'
complete -c mono -l gc -a 'sgen boehm' -f -d 'Specify garbage collector'
complete -c mono -l handlers -d 'Use custom handlers'
complete -c mono -l aot-path -f -d 'Add directories for AOT image search'
