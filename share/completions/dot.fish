set -l command dot

complete -c $command -f

complete -c $command -s '?' -d 'Show help'

for version_option in v V
    complete -c $command -s $version_option -d 'Show version'
end

set -l graph_attributes _background \
    bb \
    'beautify true false' \
    "bgcolor $(string join ' ' -- (__fish_graphviz__print_colors))" \
    'center true false' \
    'charset utf-8 utf8 iso-8859-1 ISO_8859-1 ISO8859-1 ISO-IR-100 Latin1 l1 latin-1 big-5 big5' \
    class \
    'clusterrank global local none' \
    colorscheme \
    comment \
    'compound true false' \
    'concentrate true false' \
    'Damping 0.99' \
    defaultdist \
    'dim 2' \
    'dimen 2' \
    'diredgeconstraints hier false' \
    'dpi 96.0,0.0' \
    epsilon \
    'esep +3' \
    "fontcolor $(string join ' ' -- (__fish_graphviz__print_colors))" \
    'fontname Times-Roman' \
    'fontnames svg' \
    fontpath \
    'fontsize 14.0' \
    'forcelabels true false' \
    'gradientangle 0' \
    href \
    id \
    imagepath \
    inputscale \
    'K 0.3' \
    label \
    'label_scheme 0 1 2 3' \
    'labeljust r c l' \
    'labelloc t b' \
    'landscape true false' \
    'layerlistsep ,' \
    layers \
    layerselect \
    'layersep :\t' \
    layout \
    'levels INT_MAX' \
    'levelsgap 0.0' \
    lheight \
    'linelength 128' \
    lp \
    lwidth \
    margin \
    maxiter \
    'mclimit 1.0' \
    'mindist 1.0' \
    'mode major KK sgd hier ipsep' \
    'model shortpath circuit subset' \
    'newrank true false' \
    'nodesep 0.25' \
    'nojustify true false' \
    'normalize false' \
    'notranslate true false' \
    nslimit \
    nslimit1 \
    'oneblock true false' \
    'ordering in out' \
    'orientation 0.0' \
    'outputorder breadthfirst' \
    'overlap true false' \
    'overlap_scaling -4' \
    'overlap_shrink true false' \
    'pack true false' \
    'packmode node' \
    'pad 0.0555' \
    page \
    'pagedir BL' \
    'quadtree normal none fase true false 2' \
    quantum \
    'rankdir 0.0' \
    'ranksep 0.5' \
    'ratio fill compress expand auto' \
    'remincross true false' \
    'repulsiveforce 1.0' \
    'resolution 96.0' \
    root \
    'rotate 0' \
    'rotation 0' \
    scale \
    'searchsize 30' \
    'sep +4' \
    'showboxes 0 1 2' \
    size \
    'smoothing none' \
    'sortv 0' \
    splines \
    start \
    'style filled invis' \
    stylesheet \
    target \
    TBbalance \
    tooltip \
    truecolor \
    URL \
    viewport \
    'voro_margin 0.05' \
    xdotversion \
    'sides 0'

set -l edge_attributes class \
    "color $(string join ' ' -- (__fish_graphviz__print_colors))" \
    colorscheme \
    comment \
    "fillcolor $(string join ' ' -- (__fish_graphviz__print_colors))" \
    "fontcolor $(string join ' ' -- (__fish_graphviz__print_colors))" \
    'fontname Times-Roman' \
    'fontsize 14.0' \
    href \
    id \
    label \
    layer \
    'nojustify true false' \
    'penwidth 1.0' \
    pos \
    shapefile \
    'showboxes 0 1 2' \
    'sides 0' \
    'style filled invis' \
    target \
    tooltip \
    URL \
    xlabel \
    xlp \
    lp

set -l node_attributes area \
    class \
    "color $(string join ' ' -- (__fish_graphviz__print_colors))" \
    colorscheme \
    comment \
    'distortion 0.0' \
    "fillcolor $(string join ' ' -- (__fish_graphviz__print_colors))" \
    'fixedsize false true shape' \
    "fontcolor $(string join ' ' -- (__fish_graphviz__print_colors))" \
    'fontname Times-Roman' \
    fontsize \
    'gradientangle 0' \
    group \
    'height 0.5' \
    href \
    id \
    image \
    'imagepos tl tc tr ml mc mr bl bc br' \
    'imagescale false true width height both' \
    'label \N' \
    'labelloc t c b' \
    layer \
    margin \
    'nojustify true false' \
    'ordering in out' \
    'orientation 0.0' \
    'penwidth 1.0' \
    peripheries \
    'pin false' \
    pos \
    rects \
    'regular true false' \
    'root false' \
    'samplepoints 8' \
    "shape $(string join ' ' -- (__fish_graphviz__print_shapes))" \
    shapefile \
    'showboxes 0 1 2' \
    'sides 0' \
    'skew 0' \
    'sortv 0' \
    'style filled invis' \
    target \
    tooltip \
    URL \
    vertices \
    'width 0.75' \
    xlabel \
    xlp \
    'z 0.0'

complete -c $command -s G \
    -a '(__fish_complete_key_value_pairs = $graph_attributes)' \
    -d 'Specify a graph option for a diagram'

complete -c $command -s E \
    -a '(__fish_complete_key_value_pairs = $edge_attributes)' \
    -d 'Specify an edge option for a diagram'

complete -c $command -s N \
    -a '(__fish_complete_key_value_pairs = $node_attributes)' \
    -d 'Specify a node option for a diagram'

complete -c $command -s k \
    -a 'dot neato fdp sfdp circo twopi nop Pretty-nop2 Pretty-osage patchwork' \
    -d 'Specify a render engine for a diagram'

complete -c $command -s T \
    -a '(__fish_graphviz__print_formats)' \
    -d 'Specify a render engine for a diagram'

complete -c $command -s l -d 'Specify a library for a diagram'
complete -c $command -s n -f -d 'Specify a no-op flag for neato'
complete -c $command -s o -d 'Specify the output file for a diagram'
complete -c $command -s O -d 'Whether to infer the output file for a diagram'

complete -c $command -s P \
    -d 'Whether to generate a graph to show plugin configuration of the tool'

complete -c $command -s q -d 'Whether to suppress warnings for a diagram'
complete -c $command -s s -f -d 'Specify the input scale for a diagram'

complete -c $command -s x \
    -d 'Whether to isolated nodes and peninsulas of a diagram'

complete -c $command -s y \
    -d 'Whether to invert the corrdinate system of a diagram'
