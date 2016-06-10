complete -c xz -s z -l compress --description 'Compress'
complete -c xz -s d -l decompress -l uncompress --description 'Decompress' -a "
(
    __fish_complete_suffix .xz
    __fish_complete_suffix .txz
    __fish_complete_suffix .lzma
    __fish_complete_suffix .tlz
)
"

complete -c xz -s t -l test --description 'Test the integrity of compressed files'
complete -c xz -s l -l list --description 'Print information about compressed files'
complete -c xz -s k -l keep --description 'Don\'t delete the input files'
complete -c xz -s f -l force
complete -c xz -s c -l stdout -l to-stdout --description 'Write to stdout instead of file'
complete -c xz -l single-stream --description 'Decompress only the first .xz stream'
complete -c xz -l no-sparse --description 'Disable creation of sparse files'
complete -c xz -s S -l suffix --description 'Use SUFFIX as the suffix for target file'
complete -c xz -l files --description 'Read the filenames to process from file' -r
complete -c xz -l files0 --description 'Identical to --files but filenames terminate with \\0' -r
complete -c xz -s F -l format -x --description 'Specify file format to compress/decompress' -a "auto xz lzma alone raw"
complete -c xz -s C -l check -x --description 'Specify type of integrity check' -a "none crc32 crc64 sha256"
complete -c xz -l ignore-check --description 'Don\'t verify the integrity check'

for level in (seq 1 9)
    complete -c xz -s $level --description "Select compression level"
end

complete -c xz -s e -l extreme --description 'Use slower variant'
complete -c xz -l fast --description 'Alias of -0'
complete -c xz -l best --description 'Alias of -9'
complete -c xz -l block-size --description 'Set block size' -x
complete -c xz -l block-list --description 'Set block sizes list' -x
complete -c xz -l flush-timeout --description 'Force flush if encoder did not fush after TIMEOUT ms' -x
complete -c xz -l memlimit-compress --description 'Set memory usage limit for compression' -x
complete -c xz -l memlimit-decompress --description 'Set memory usage limit for decompression' -x
complete -c xz -s M -l memlimit -l memory --description 'Set a memory usage for compression/decompression'
complete -c xz -l no-adjust --description 'Display error and exit if exceed memory usage limit'
complete -c xz -s T -l threads --description 'Specify the number of worker threads to use' -x
complete -c xz -l lsam1 --description 'Add LZMA1 filter to filter chain' -f
complete -c xz -l lzma2 --description 'Add LZMA2 filter to filter chain' -f

for op in x86 powerpc ia64 arm armthumb sparc
    complete -c xz -l $op --description 'Add a branch/call/jump filter to filter chain' -f
end

complete -c xz -l delta --description 'Add Delta filter to filter chain'
complete -c xz -s q -l quiet --description 'Suppress warnings/notices'
complete -c xz -s v -l verbose --description 'Be verbose'
complete -c xz -s Q -l no-warn --description 'Don\'t set the exit status to 2'
complete -c xz -l robot --description 'Print messages in a machine-parsable format'
complete -c xz -l info-memory --description 'Display memory informations (physical, usage limits)'
complete -c xz -s h -l help --description 'Display help'
complete -c xz -s H -l long-help --description 'Display long help'
complete -c xz -s V -l version --description 'Display version'
