# Current as of cwebp 1.0.3.

set -l preset_opts '
    default\t
    photo\t
    picture\t
    drawing\t
    icon\t
    text\t
'

set -l z_opts '
    0\tfastest
    1\t
    2\t
    3\t
    4\t
    5\t
    6\t
    7\t
    8\t
    9\tslowest
'

set -l m_opts '
    0\tfastest
    1\t
    2\t
    3\t
    4\tdefault
    5\t
    6\tslowest
'

set -l segments_opts '
    1\t
    2\t
    3\t
    4\tdefault
'

set -l sharpness_opts '
    0\tmost sharp; default
    1\t
    2\t
    3\t
    4\t
    5\t
    6\t
    7\tleast sharp
'

set -l alpha_method_opts '
    0\t
    1\tdefault
'

set -l alpha_filter_opts '
    none\t
    fast\tdefault
    best\t
'

set -l metadata_opts '
    all\t
    none\tdefault
    exif\t
    icc\t
    xmp\t
'

function __fish_cwebp_is_first_arg_or_its_value -a arg -d 'Like __fish_is_first_arg, but also returns true for the second token after a given parameter'
    set -l tokens (commandline -cx)

    switch (count $tokens)
        case 1
            return 0
        case 2
            if test "$tokens[2]" = "$arg"
                return 0
            end
            return 1
        case '*'
            return 1
    end
end

complete -c cwebp -f -o h -o help -d 'Help (short)'
complete -c cwebp -f -o H -o longhelp -l help -d 'Help (long)'

complete -c cwebp -x -o q -d 'Quality (float; 0…100; default: 75)'
complete -c cwebp -x -o alpha_q -d 'Transparency-compression quality (integer 0…100; default: 100)'

complete -c cwebp -x -n '__fish_cwebp_is_first_arg_or_its_value -preset' -o preset -a $preset_opts -d 'Preset setting'

complete -c cwebp -x -o z -a "$z_opts" -d 'Lossless preset of level'
complete -c cwebp -x -o m -a "$m_opts" -d 'Compression method'
complete -c cwebp -x -o segments -a "$segments_opts" -d 'Number of segments to use'
complete -c cwebp -x -o size -d 'Target size (in bytes)'
complete -c cwebp -x -o psnr -d 'Target PSNR (in dB; typically: 42)'

complete -c cwebp -x -o s -d 'Input size (width x height) for YUV'
complete -c cwebp -x -o sns -d 'Spatial noise shaping (integer 0…100; default: 50)'
complete -c cwebp -x -o f -d 'Filter strength (integer 0…100; default: 60)'
complete -c cwebp -x -o sharpness -a "$sharpness_opts" -d 'Filter sharpness'
complete -c cwebp -o strong -d 'Use strong filter'
complete -c cwebp -o simple -d 'Use simple filter'
complete -c cwebp -o sharp_yuv -d 'Use sharper (and slower) RGB->YUV conversion'
complete -c cwebp -x -o partition_limit -d 'Limit quality to fit 512k limit on first partition (0:no degradation…100:full degradation)'
complete -c cwebp -x -o pass -a '(seq 0 10)' -d 'Analysis pass number'
complete -c cwebp -x -o crop -d 'Crop picture with <x> <y> <v> <h> rectangle'
complete -c cwebp -x -o resize -d 'Resize picture to <w> <h> after any cropping'
complete -c cwebp -o mt -d 'Use multi-threading if available'
complete -c cwebp -o low_memory -d 'Reduce memory usage (slows encoding)'
complete -c cwebp -x -o map -d 'Print map of extra info'
complete -c cwebp -o print_psnr -d 'Print averaged PSNR distortion'
complete -c cwebp -o print_ssim -d 'Print averaged SSIM distortion'
complete -c cwebp -o print_lsim -d 'Print local-similarity distortion'
complete -c cwebp -r -o d -k -a '(__fish_complete_suffix .pgm)' -d 'Dump compressed output to given PGM file'
complete -c cwebp -x -o alpha_method -a "$alpha_method_opts" -d 'Transparency-compression method'
complete -c cwebp -x -o alpha_filter -a "$alpha_filter_opts" -k -d 'Predictive filtering for alpha plane'
complete -c cwebp -x -o exact -d 'Preserve RGB values in transparent area'
complete -c cwebp -x -o blend_alpha -d 'Blend colors against background color (e.g. 0xc0e0d0)'
complete -c cwebp -o noalpha -d 'Discard any transparency information'
complete -c cwebp -o lossless -d 'Encode image losslessly'
complete -c cwebp -x -o near_lossless -d 'Use near-lossless image preprocessing (integer 0…100:off; default: 100)'
complete -c cwebp -x -o hint -a 'photo picture graph' -d 'Specify image-characteristics hint'

complete -c cwebp -x -o metadata -a "$metadata_opts" -k -d 'Comma-separated list of metadata to copy, if present'

complete -c cwebp -o short -d 'Condense printed messages'
complete -c cwebp -o quiet -d 'Don\'t print anything'
complete -c cwebp -o version -d 'Print version number and exit'
complete -c cwebp -o noasm -d 'Disable all assembly optimizations'
complete -c cwebp -o v -d 'Be verbose (print encoding/decoding times)'
complete -c cwebp -o progress -d 'Report encoding progress'

complete -c cwebp -o jpeg_like -d 'Roughly match expected JPEG size'
complete -c cwebp -o af -d 'Auto-adjust filter strength'
complete -c cwebp -x -o pre -d 'Pre-processing filter (integer)'

complete -c cwebp -r -o o -k -a '(__fish_complete_suffix .webp)' -d 'Output to file'
