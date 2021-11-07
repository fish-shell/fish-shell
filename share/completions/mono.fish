complete -c mono -s h -l help -d 'Show help and exit'
complete -c mono -s V -l version -d 'Show version and exit'

complete -c mono -l aot -a 'asmonly bind-to-runtime-version data-outfile direct-icalls
  direct-pinvoke dwarfdebug full hybrid llvm llvmonly llvmopts llvmllc mcpu dedup-include
  info interp depfile ld-flags llvm-path msym-dir mtriple nimt-trampolines ngsharedvt-trampolines
  nodebug no-direct-calls nrgctx-trampolines nrgctx-fetch-trampolines ntrampolines outfile
  print-skipped-methods profile profile-only readonly-value save-temps,keep-temps soft-debug
  static stats temp-path threads tool-prefix verbose write-symbols,no-write-symbols no-opt'\
  -f -d 'Precompile CIL code to native code'
complete -c mono -l debug -a 'casts mdb-optimizations gdb' -f -d 'Use debug mode'
complete -c mono -l debugger-agent -a 'address loglevel logfile server setpgid suspend
  transport' -f -d 'Use debugger agent'
complete -c mono -l profile -d 'Use profiler module'
complete -c mono -l trace -d 'Trace expression'
complete -c mono -l jitmap -d 'Generate JIT method map in /tmp/perf-PID.map'
