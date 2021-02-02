# Completions for libavif (https://github.com/AOMediaCodec/libavif)

complete -c avifenc -s h -l help -d "Show syntax help"
complete -c avifenc -s V -l version -d "Show the version number"
complete -x -c avifenc -s j -l jobs -d "Number of jobs"
complete -r -c avifenc -s o -l output -d "Filename of the output file"
complete -c avifenc -s l -l lossless -d "Set all defaults to encode losslessly"
complete -x -c avifenc -s d -l depth -a "8 10 12" -d "Output depth"
complete -x -c avifenc -s y -l yuv -a "444 422 420 400" -d "Output format"
complete -c avifenc -l stdin -d "Read y4m frames from stdin"
complete -x -c avifenc -l cicp -l nclx -d "Set CICP values"
complete -x -c avifenc -s r -l range -d "YUV range"
complete -x -c avifenc -l min -a "(seq 0 63)" -d "Set min quantizer for color"
complete -x -c avifenc -l max -a "(seq 0 63)" -d "Set max quantizer for color"
complete -x -c avifenc -l minalpha -a "(seq 0 63)" -d "Set min quantizer for alpha"
complete -x -c avifenc -l maxalpha -a "(seq 0 63)" -d "Set max quantizer for alpha"
complete -x -c avifenc -l tilerowslog2 -a "(seq 0 6)" -d "Set log2 of number of tile rows"
complete -x -c avifenc -l tilecolslog2 -a "(seq 0 6)" -d "Set log2 of number of tile columns"
complete -x -c avifenc -s s -l speed -a "
    0\tSlowest
    (seq 1 9)
    10\tFastest
    default\t'Codec internal defaults'
    d\t'Codec internal defaults'
    " -d "Encoder speed"
complete -x -c avifenc -s c -l codec -a "
    aom\tlibaom
    rav1e\trav1e
    svt\tSVT-AV1
    " -d "AV1 codec to use"
complete -r -c avifenc -l exif -d "Filename of the associated Exif metadata"
complete -r -c avifenc -l xmp -d "Filename of the associated XMP metadata"
complete -r -c avifenc -l icc -d "Filename of the associated ICC profile"
complete -x -c avifenc -s a -l advanced -d "Pass a codec-specific key/value pair to the codec"
complete -x -c avifenc -l duration -d "Set all following frame durations"
complete -x -c avifenc -l timescale -l fps -d "Set the timescale"
complete -x -c avifenc -s k -l keyframe -d "Set the forced keyframe interval"
complete -c avifenc -l ignore-icc -d "Ignore an embedded ICC profile"
complete -x -c avifenc -l pasp -d "Add pasp property"
complete -x -c avifenc -l clap -d "Add clap property"
complete -x -c avifenc -l irot -d "Add irot property"
complete -x -c avifenc -l imir -d "Add imir property"
