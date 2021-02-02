# Completions for libavif (https://github.com/AOMediaCodec/libavif)

complete -c avifdec -s h -l help -d "Show syntax help"
complete -c avifdec -s V -l version -d "Show the version number"
complete -x -c avifdec -s j -l jobs -d "Number of jobs"
complete -x -c avifdec -s c -l codec -a "
    aom\tlibaom
    dav1d\tdav1d
    libgav1\tlibgav1
    " -d "AV1 codec to use"
complete -x -c avifdec -s d -l depth -a "8 16" -d "Output depth"
complete -x -c avifdec -s q -l quality -d "Output quality"
complete -x -c avifdec -s u -l upsampling -a "automatic fastest best nearest bilinear" -d "Chroma upsampling"
complete -c avifdec -s i -l info -d "Display all image information"
complete -c avifdec -l ignore-icc -d "Ignore an embedded ICC profile"
