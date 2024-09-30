set -l command dot

complete -c $command -f

complete -c $command -s '?' -d 'Show help'

for version_option in v V
    complete -c $command -s $version_option -d 'Show version'
end

set -l graph_attributes = _background \
    bb \
    'beautify true false' \
    bgcolor \
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
    fontname \
    'fontnames svg' \
    fontpath \
    'fontsize 14.0' \
    'forcelabels true false' \
    'gradientangle 0' \
    href \
    id \
    imagepath \
    inputscale \
    K \
    label \
    label_scheme \
    labeljust \
    labelloc \
    landscape \
    layerlistsep \
    layers \
    layerselect \
    layersep \
    layout \
    levels \
    levelsgap \
    lheight \
    linelength \
    lp \
    lwidth \
    margin \
    maxiter \
    mclimit \
    mindist \
    mode \
    model \
    newrank \
    nodesep \
    nojustify \
    normalize \
    notranslate \
    nslimit \
    nslimit1 \
    oneblock \
    ordering \
    orientation \
    outputorder \
    overlap \
    overlap_scaling \
    overlap_shrink \
    pack \
    packmode \
    pad \
    page \
    pagedir \
    quadtree \
    quantum \
    rankdir \
    ranksep \
    ratio \
    remincross \
    repulsiveforce \
    resolution \
    root \
    rotate \
    rotation \
    scale \
    searchsize \
    sep \
    showboxes \
    size \
    smoothing \
    sortv \
    splines \
    start \
    style \
    stylesheet \
    target \
    TBbalance \
    tooltip \
    truecolor \
    URL \
    viewport \
    voro_margin \
    xdotversion

set -l edge_attributes area \
    class \
    color \
    colorscheme \
    comment \
    distortion \
    fillcolor \
    fixedsize \
    fontcolor \
    fontname \
    fontsize \
    gradientangle \
    group \
    height \
    href \
    id \
    image \
    imagepos \
    imagescale \
    label \
    labelloc \
    layer \
    margin \
    nojustify \
    ordering \
    orientation \
    penwidth \
    peripheries \
    pin \
    pos \
    rects \
    regular \
    root \
    samplepoints \
    shape \
    shapefile \
    showboxes \
    sides \
    skew \
    sortv \
    style \
    target \
    tooltip \
    URL \
    vertices \
    width \
    xlabel \
    xlp \
    z

set -l node_attributes area \
    class \
    color \
    colorscheme \
    comment \
    distortion \
    fillcolor \
    fixedsize \
    fontcolor \
    fontname \
    fontsize \
    gradientangle \
    group \
    height \
    href \
    id \
    image \
    imagepos \
    imagescale \
    label \
    labelloc \
    layer \
    margin \
    nojustify \
    ordering \
    orientation \
    penwidth \
    peripheries \
    pin \
    pos \
    rects \
    regular \
    root \
    samplepoints \
    shape \
    shapefile \
    showboxes \
    sides \
    skew \
    sortv \
    style \
    target \
    tooltip \
    URL \
    vertices \
    width \
    xlabel \
    xlp \
    z

complete -c $command \
    -a '(__fish_complete_key_value_pairs $graph_attributes)' \
    -d 'Specify a graph option'

complete -c $command \
    -a '(__fish_complete_key_value_pairs $edge_attributes)' \
    -d 'Specify an edge option'

complete -c $command \
    -a '(__fish_complete_key_value_pairs $node_attributes)' \
    -d 'Specify a node option'

## 
__fish_complete_key_value_pairs = "type executable library" "id first second"

complete --erase true
complete -c true -s o -l option -r \
    -a '(__fish_complete_key_value_pairs = "type executable library" "id first second")' \
    -d 'Some description'
