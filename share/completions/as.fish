# AS - the portable GNU assembler
# See: https://www.gnu.org/software/binutils

complete -c as -o ac -d 'Omit false conditionals'
complete -c as -o ad -d 'Omit debugging directives'
complete -c as -o ag -d 'Include general information'
complete -c as -o ah -d 'Include high-level source'
complete -c as -o al -d 'Include assembly'
complete -c as -o am -d 'Include macro expansions'
complete -c as -o an -d 'Omit forms processing'
complete -c as -o as -d 'Include symbols'

complete -c as -l alternate -d 'Initially turn on alternate macro syntax'
complete -c as -l compress-debug-sections -a 'none zlib zlib-gnu zlib-gabi' -x -d 'Compress DWARF debug sections using zlib'
complete -c as -l nocompress-debug-sections -d 'Don\'t compress DWARF debug sections'
complete -c as -s D -d 'Produce assembler debugging messages'
complete -c as -l debug-prefix-map -x -d 'Remap debug information'
complete -c as -l defsym -x -d 'Redefine symbols'
complete -c as -l execstack -d 'Require executable stack for this object'
complete -c as -l noexecstack -d 'Don\'t require executable stack for this object'
complete -c as -l size-check -a 'error\tDefault warning' -x -d 'ELF .size directive check'
complete -c as -l elf-stt-common -a 'yes no' -d 'Generate ELF common symbols with STT_COMMON type'
complete -c as -l sectname-subst -d 'Enable section name substitution sequences'
complete -c as -s f -d 'Skip whitespace and comment preprocessing'
complete -c as -l gen-debug -s g -d 'Generate debugging information'
complete -c as -l gstabs -d 'Generate STABS debugging information'
complete -c as -l 'gstabs+' -d 'Generate STABS debug info with GNU extensions'
complete -c as -l gdwarf-2 -d 'Generate DWARF2 debugging information'
complete -c as -l gdwarf-sections -d 'Generate per-function section names for DWARF line information'
complete -c as -l hash-size -x -d 'Set the hash table size close'
complete -c as -l help -d 'Show help message and exit'
complete -c as -l target-help -d 'Show target specific options'
complete -c as -s I -r -d 'Add DIR to search list for .include directives'
complete -c as -s J -d 'Don\'t warn about signed overflow'
complete -c as -s K -d 'Warn when differences altered for long displacements'
complete -c as -s L -d 'Keep local symbols'
complete -c as -l mri -s M -d 'Assemble in MRI compatibility mode'
complete -c as -l MD -r -d 'Write dependency information in given file'
complete -c as -o no-pad-sections -d 'Do not pad the end of sections to alignment boundaries'
complete -c as -s o -r -d 'Set object-file output file name'
complete -c as -s R -d 'Fold data section into text section'
complete -c as -l reduce-memory-overheads -d 'Prefer smaller memory use'
complete -c as -l statistics -d 'Print various measured statistics from execution'
complete -c as -l strip-local-absolute -d 'Strip local absolute symbols'
complete -c as -l traditional-format -d 'Use same format as native assembler when possible'
complete -c as -l version -d 'Print assembler version number and exit'
complete -c as -l no-warn -s W -d 'Suppress warnings'
complete -c as -l warn -d 'Don\'t suppress warnings'
complete -c as -l fatal-warnings -d 'Treat warnings as errors'
complete -c as -s Z -d 'Generate object file even after errors'
complete -c as -l listing-lhs-width
complete -c as -l listing-lhs-width2
complete -c as -l listing-rhs-width
complete -c as -l listing-cont-lines
complete -c as -s '@' -r -d 'Read option from given file'
complete -c as -s V -d 'Print assembler version number'
complete -c as -s n -d 'Don\'t optimize code alignment'
complete -c as -s q -d 'Quieten some warnings'
complete -c as -l 32 -d 'Generate 32 bits code'
complete -c as -l 64 -d 'Generate 64 bits code'
complete -c as -l x32 -d 'Generate x32 code'
complete -c as -o msse2avx -d 'Encode SSE instructions with VEX prefix'
complete -c as -o msse-check -a 'none error warning' -x -d 'Check SSE instructions'
complete -c as -o moperand-check -a 'none error warning' -x -d 'Check operand combinations for validity'
complete -c as -o mavxscalar -a '128 256' -x -d 'Encode scalar AVX instructions with specific vector length'
complete -c as -o mevexlig -a '128 256 512' -x -d 'Encode scalar EVEX instructions with specific vector length'
complete -c as -o mevexwig -a '0 1' -x -d 'Encode EVEX instructions with specific EVEX.W'
complete -c as -o mevexrcig -a 'rne rd ru rz' -x -d 'Encode EVEX instructions with specific EVEX.RC'
complete -c as -o mmnemonic -a 'att\tAT\&T intel\tIntel' -x -d 'Use specfied mnemonic'
complete -c as -o msyntax -a 'att\tAT\&T intel\tIntel' -x -d 'Use specfied syntax'
complete -c as -o mindex-reg -d 'Support pseudo index registers'
complete -c as -o mnaked-reg -d 'Don\'t require \'%\' prefix for registers'
complete -c as -o mold-gcc -d 'Support old (<= 2.8.1) versions of gcc'
complete -c as -o madd-bnd-prefix -d 'Add BND prefix for all valid branches'
complete -c as -o mshared -d 'Disable branch optimization for shared code'
complete -c as -o momit-lock-prefix -a 'yes no' -x -d 'Strip all lock prefixes'
complete -c as -o mfence-as-lock-add -a 'yes no' -x -d 'Encode lfence mfence and sfence as lock addl $0x0 (%{re}sp)'
complete -c as -o mrelax-relocations -a 'yes no' -x -d 'Generate relax relocations'
complete -c as -o mamd64 -d 'Accept only AMD64 ISA'
complete -c as -o mintel64 -d 'Accept only Intel64 ISA'

function __fish_complete_as_march
    # Complete: CPU[,EXTENSION...]

    set -l cpus generic32 generic64 i386 i486 i586 i686 pentium pentiumpro pentiumii pentiumiii pentium4 prescott nocona core core2 corei7 l1om k1om iamcu k6 k6_2 athlon opteron k8 amdfam10 bdver1 bdver2 bdver3 bdver4 znver1 btver1 btver2

    set -l extensions 8087 287 387 687 mmx sse sse2 sse3 ssse3 sse4.1 sse4.2 sse4 avx avx2 avx512f avx512cd avx512er avx512pf avx512dq avx512bw avx512vl vmx vmfunc smx xsave xsaveopt xsavec xsaves aes pclmul fsgsbase rdrnd f16c bmi2 fma fma4 xop lwp movbe cx16 ept lzcnt hle rtm invpcid clflush nop syscall rdtscp 3dnow 3dnowa padlock svme sse4a abm bmi tbm adx rdseed prfchw smap mpx sha clflushopt prefetchwt1 se1 clwb avx512ifma avx512vbmi avx512_4fmaps avx512_4vnniw avx512_vpopcntdq clzero mwaitx ospke rdpid ptwrite no87 no287 no387 no687 nommx nosse nosse2 nosse3 nossse3 nosse4.1 nosse4.2 nosse4 noavx noavx2 noavx512f noavx512cd noavx512er noavx512pf noavx512dq noavx512bw noavx512vl noavx512ifma noavx512vbmi noavx512_4fmaps noavx512_4vnniw noavx512_vpopcntdq

    set -l token (commandline -ct)
    set -l opts (string split -- , $token)

    if not set -q opts[2]
        # We are completing the CPU part
        printf '%s\tCPU\n' $cpus
    else
        # We are completing the EXTENSION part
        printf '%s\tExtension\n' (string join -- , $opts)$extensions
    end
end

complete -c as -o march -x -a '(__fish_complete_as_march)'
complete -c as -o mtune -x -a 'generic32 generic64 i8086 i186 i286 i386 i486 i586 i686 pentium pentiumpro pentiumii pentiumiii pentium4 prescott nocona core core2 corei7 l1om k1om iamcu k6 k6_2 athlon opteron k8 amdfam10 bdver1 bdver2 bdver3 bdver4 znver1 btver1 btver2' -d 'Optimize for given CPU'
