# Completions for the JPEG XL Reference Software (https://gitlab.com/wg1/jpeg-xl)

complete -c cjxl -s V -l version -d "Print version number"
complete -c cjxl -l quiet -d "Be more silent"
complete -c cjxl -l container -d "Encode using container format"
complete -x -c cjxl -l print_profile -a "0 1" -d "Print timing information"
complete -x -c cjxl -s d -l distance -a "
    0.0\t'Mathematically lossless'
    1.0\t'Visually lossless'
    " -d "Butteraugli distance"
complete -x -c cjxl -l target_size -d "Target file size (based on bytes)"
complete -x -c cjxl -l target_bpp -d "Target file size (based on BPP)"
complete -x -c cjxl -s q -l quality -a "
    (seq 0 99)
    100\t'Mathematically lossless'
    " -d "Quality setting"
complete -x -c cjxl -s s -l speed -a "
    3\tFastest falcon\tFastest
    4 cheetah
    5 hare
    6 wombat
    7\tDefault squirrel\tDefault
    8 kitten
    9\tSlowest tortoise\tSlowest
    " -d "Encoder speed setting"
complete -c cjxl -s p -l progressive -d "Enable progressive decoding"
complete -c cjxl -l middleout -d "Put center groups first in the file"
complete -c cjxl -l progressive_ac -d "Use the progressive mode for AC"
complete -c cjxl -l qprogressive_ac -d "Use the progressive mode for AC"
complete -x -c cjxl -l progressive_dc -d "Use progressive mode for DC"
complete -c cjxl -s m -l modular -d "Use the modular mode"
complete -c cjxl -l use_new_heuristics -d "Use new encoder heuristics"
complete -c cjxl -s j -l jpeg_transcode -d "Do lossy transcode of input JPEG file"
complete -x -c cjxl -l num_threads -d "Number of worker threads"
complete -x -c cjxl -l num_reps -d "How many times to compress"
complete -x -c cjxl -l noise -a "
    0\tEnable
    1\tDisable
    " -d "Force noise generation"
complete -x -c cjxl -l dots -a "
    0\tEnable
    1\tDisable
    " -d "Force dots generation"
complete -x -c cjxl -l patches -a "
    0\tEnable
    1\tDisable
    " -d "Force patches generation"
complete -x -c cjxl -l resampling -a "1 2 4 8" -d "Subsample all color channels"
complete -x -c cjxl -l epf -d "Edge preserving filter level"
complete -x -c cjxl -l gaborish -a "0 1" -d "Force disable gaborish"
complete -x -c cjxl -l intensity_target -d "Intensity target of monitor in nits"
complete -x -c cjxl -l saliency_num_progressive_steps
complete -x -c cjxl -l saliency_map_filename
complete -x -c cjxl -l saliency_threshold
complete -x -c cjxl -s x -l dec-hints
complete -x -c cjxl -l override_bitdepth -a "
    0\t'Use from image'
    (seq 1 32)\tOverride
    " -d "Store the given bit depth in the metadata"
complete -x -c cjxl -s c -l colortransform -a "
    0\tXYB
    1\tNone
    2\tYCbCr"
complete -x -c cjxl -s Q -l mquality -d "Lossy quality"
complete -x -c cjxl -s I -l iterations -d "Fraction of pixels used to learn MA trees"
complete -x -c cjxl -s C -l colorspace -a "
    0\tRGB
    1\tYCoCg
    (seq 2 37)\tRCT
    " -d "Color transform"
complete -x -c cjxl -s g -l group-size -d "Set group size"
complete -x -c cjxl -s P -l predictor -a "
    0\tZero
    1\tLeft
    2\tTop
    3\tAverage0
    4\tSelect
    5\tGradient
    6\tWeighted
    7\tTopRight
    8\tTopLeft
    9\tLeftLeft
    10\tAverage1
    11\tAverage2
    12\tAverage3
    13\t'TopTop predictive average'
    14\t'Mix Gradient and Weighted'
    15\t'Mix everything'
    " -d "Predictor to use"
complete -x -c cjxl -s E -l extra-properties -d "Number of extra MA tree properties"
complete -x -c cjxl -s N -l near-lossless -d "Apply near-lossless preprocessing"
complete -x -c cjxl -l palette -d "Use a palette"
complete -c cjxl -l lossy-palette -d "Quantize to a lossy palette"
complete -x -c cjxl -s X -l pre-compact -d "Compact channels (globally)"
complete -x -c cjxl -s Y -l post-compact -d "Compact channels (per-group)"
complete -x -c cjxl -s R -l responsive -a "
    0\tFalse
    1\tTrue
    " -d "Do squeeze transform"
complete -c cjxl -s v -l verbose -d "Verbose output"
complete -c cjxl -s h -l help -d "Print help message"
