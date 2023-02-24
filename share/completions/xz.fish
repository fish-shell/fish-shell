complete -c xz -s z -l compress -d Compress
complete -c xz -s d -l decompress -l uncompress -d Decompress -k -x -a "(__fish_complete_suffix .xz .txz .lzma .tlz)"

complete -c xz -s t -l test -d 'Test the integrity of compressed files'
complete -c xz -s l -l list -d 'Print information about compressed files'
complete -c xz -s k -l keep -d 'Don\'t delete the input files'
complete -c xz -s f -l force
complete -c xz -s c -l stdout -l to-stdout -d 'Write to stdout instead of file'
complete -c xz -l single-stream -d 'Decompress only the first .xz stream'
complete -c xz -l no-sparse -d 'Disable creation of sparse files'
complete -c xz -s S -l suffix -d 'Use SUFFIX as the suffix for target file'
complete -c xz -l files -d 'Read the filenames to process from file' -r
complete -c xz -l files0 -d 'Identical to --files but filenames terminate with \\0' -r
complete -c xz -s F -l format -x -d 'Specify file format to compress/decompress' -a "auto xz lzma alone raw"
complete -c xz -s C -l check -x -d 'Specify type of integrity check' -a "none crc32 crc64 sha256"
complete -c xz -l ignore-check -d 'Don\'t verify the integrity check'

for level in (seq 1 9)
    complete -c xz -s $level -d "Select compression level"
end

complete -c xz -s e -l extreme -d 'Use slower variant'
complete -c xz -l fast -d 'Alias of -0'
complete -c xz -l best -d 'Alias of -9'
complete -c xz -l block-size -d 'Set block size' -x
complete -c xz -l block-list -d 'Set block sizes list' -x
complete -c xz -l flush-timeout -d 'Force flush if encoder did not fush after TIMEOUT ms' -x
complete -c xz -l memlimit-compress -d 'Set memory usage limit for compression' -x
complete -c xz -l memlimit-decompress -d 'Set memory usage limit for decompression' -x
complete -c xz -s M -l memlimit -l memory -d 'Set a memory usage for compression/decompression'
complete -c xz -l no-adjust -d 'Display error and exit if exceed memory usage limit'
complete -c xz -s T -l threads -d 'Specify the number of worker threads to use' -x
complete -c xz -l lsam1 -d 'Add LZMA1 filter to filter chain' -f
complete -c xz -l lzma2 -d 'Add LZMA2 filter to filter chain' -f

for op in x86 powerpc ia64 arm armthumb sparc
    complete -c xz -l $op -d 'Add a branch/call/jump filter to filter chain' -f
end

complete -c xz -l delta -d 'Add Delta filter to filter chain'
complete -c xz -s q -l quiet -d 'Suppress warnings/notices'
complete -c xz -s v -l verbose -d 'Be verbose'
complete -c xz -s Q -l no-warn -d 'Don\'t set the exit status to 2'
complete -c xz -l robot -d 'Print messages in a machine-parsable format'
complete -c xz -l info-memory -d 'Display memory informations (physical, usage limits)'
complete -c xz -s h -l help -d 'Display help'
complete -c xz -s H -l long-help -d 'Display long help'
complete -c xz -s V -l version -d 'Display version'
