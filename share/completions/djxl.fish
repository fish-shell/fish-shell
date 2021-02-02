# Completions for the JPEG XL Reference Software (https://gitlab.com/wg1/jpeg-xl)

complete -c djxl -s V -l version -d "Print version number"
complete -x -c djxl -l num_reps
complete -c djxl -l use_sjpeg -d "Use sjpeg instead of libjpeg"
complete -x -c djxl -l jpeg_quality -d "JPEG output quality"
complete -x -c djxl -l num_threads -d "The number of threads to use"
complete -x -c djxl -l print_profile -a "0 1" -d "Print timing information"
complete -x -c djxl -l print_info -a "0 1" -d "Print AuxOut"
complete -x -c djxl -l bits_per_sample
complete -x -c djxl -l color_space
complete -x -c djxl -s s -l downsampling -a "1 2 4 8 16" -d "Maximum permissible downsampling factor"
complete -c djxl -l allow_partial_files -d "Allow decoding of truncated files"
complete -c djxl -l allow_more_progressive_steps -d "Allow decoding more progressive steps"
complete -c djxl -s j -l jpeg -d "Decode directly to JPEG"
complete -c djxl -l print_read_bytes -d "Print total number of decoded bytes"
complete -c djxl -s h -l help -d "Print help message"
