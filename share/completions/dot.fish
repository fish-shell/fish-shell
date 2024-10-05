function __fish_graphviz__print_colors
    begin
        __fish_graphviz__print_x11_colors
        __fish_graphviz__print_svg_colors
    end | sort | uniq
end

function __fish_graphviz__print_shapes
    echo box \
        polygon \
        ellipse \
        oval \
        circle \
        point \
        egg \
        triangle \
        plaintext \
        plain \
        diamond \
        trapezium \
        parallelogram \
        house \
        pentagon \
        hexagon \
        septagon \
        octagon \
        doublecircle \
        doubleoctagon \
        tripleoctagon \
        invtriangle \
        invtrapezium \
        invhouse \
        Mdiamond \
        Msquare \
        Mcircle \
        rect \
        rectangle \
        square \
        star \
        none \
        underline \
        cylinder \
        note \
        tab \
        folder \
        box3d \
        component \
        promoter \
        cds \
        terminator \
        utr \
        primersite \
        restrictionsite \
        fivepoverhang \
        threepoverhang \
        noverhang \
        assembly \
        signature \
        insulator \
        ribosite \
        rnastab \
        proteasesite \
        proteinstab \
        rpromoter \
        rarrow \
        larrow \
        lpromoter
end

function __fish_graphviz__print_formats
    echo bmp \
        cgimage \
        canon \
        dot \
        gv \
        xdot \
        xdot1.2 \
        xdot1.4 \
        eps \
        exr \
        fig \
        gd \
        gd2 \
        gif \
        gtk \
        ico \
        imap \
        imap_np \
        ismap \
        cmap \
        cmapx \
        cmapx_np \
        jpg \
        jpeg \
        jpe \
        2000 \
        jp2 \
        json \
        json0 \
        dot_json \
        xdot_json \
        pdf \
        pic \
        pct \
        pict \
        plain \
        plain-ext \
        png \
        pov \
        ps \
        ps2 \
        psd \
        sgi \
        svg \
        svgz \
        tga \
        tif \
        tiff \
        tk \
        vml \
        vmlz \
        vrml \
        wbmp \
        webp \
        xlib \
        x11
end

function __fish_graphviz__print_svg_colors
    set colors aliceblue antiquewhite aqua aquamarine azure \
        beige bisque black blanchedalmond blue \
        blueviolet brown burlywood cadetblue chartreuse \
        chocolate coral cornflowerblue cornsilk crimson \
        cyan darkblue darkcyan darkgoldenrod darkgray \
        darkgreen darkgrey darkkhaki darkmagenta darkolivegreen \
        darkorange darkorchid darkred darksalmon darkseagreen \
        darkslateblue darkslategray darkslategrey darkturquoise darkviolet \
        deeppink deepskyblue dimgray dimgrey dodgerblue \
        firebrick floralwhite forestgreen fuchsia gainsboro \
        ghostwhite gold goldenrod gray grey \
        green greenyellow honeydew hotpink indianred \
        indigo ivory khaki lavender lavenderblush \
        lawngreen lemonchiffon lightblue lightcoral lightcyan \
        lightgoldenrodyellow lightgray lightgreen lightgrey lightpink \
        lightsalmon lightseagreen lightskyblue lightslategray lightslategrey \
        lightsteelblue lightyellow lime limegreen linen \
        magenta maroon mediumaquamarine mediumblue mediumorchid \
        mediumpurple mediumseagreen mediumslateblue mediumspringgreen mediumturquoise \
        mediumvioletred midnightblue mintcream mistyrose moccasin \
        navajowhite navy oldlace olive olivedrab \
        orange orangered orchid palegoldenrod palegreen \
        paleturquoise palevioletred papayawhip peachpuff peru \
        pink plum powderblue purple red \
        rosybrown royalblue saddlebrown salmon sandybrown \
        seagreen seashell sienna silver skyblue \
        slateblue slategray slategrey snow springgreen \
        steelblue tan teal thistle tomato \
        turquoise violet wheat white whitesmoke \
        yellow yellowgreen

    string join \n -- $colors
end

function __fish_graphviz__print_x11_colors
    set colors aliceblue antiquewhite antiquewhite1 antiquewhite2 antiquewhite3 \
        antiquewhite4 aqua aquamarine aquamarine1 aquamarine2 \
        aquamarine3 aquamarine4 azure azure1 azure2 \
        azure3 azure4 beige bisque bisque1 \
        bisque2 bisque3 bisque4 black blanchedalmond \
        blue blue1 blue2 blue3 blue4 \
        blueviolet brown brown1 brown2 brown3 \
        brown4 burlywood burlywood1 burlywood2 burlywood3 \
        burlywood4 cadetblue cadetblue1 cadetblue2 cadetblue3 \
        cadetblue4 chartreuse chartreuse1 chartreuse2 chartreuse3 \
        chartreuse4 chocolate chocolate1 chocolate2 chocolate3 \
        chocolate4 coral coral1 coral2 coral3 \
        coral4 cornflowerblue cornsilk cornsilk1 cornsilk2 \
        cornsilk3 cornsilk4 crimson cyan cyan1 \
        cyan2 cyan3 cyan4 darkblue darkcyan \
        darkgoldenrod darkgoldenrod1 darkgoldenrod2 darkgoldenrod3 darkgoldenrod4 \
        darkgray darkgreen darkgrey darkkhaki darkmagenta \
        darkolivegreen darkolivegreen1 darkolivegreen2 darkolivegreen3 darkolivegreen4 \
        darkorange darkorange1 darkorange2 darkorange3 darkorange4 \
        darkorchid darkorchid1 darkorchid2 darkorchid3 darkorchid4 \
        darkred darksalmon darkseagreen darkseagreen1 darkseagreen2 \
        darkseagreen3 darkseagreen4 darkslateblue darkslategray darkslategray1 \
        darkslategray2 darkslategray3 darkslategray4 darkslategrey darkturquoise \
        darkviolet deeppink deeppink1 deeppink2 deeppink3 \
        deeppink4 deepskyblue deepskyblue1 deepskyblue2 deepskyblue3 \
        deepskyblue4 dimgray dimgrey dodgerblue dodgerblue1 \
        dodgerblue2 dodgerblue3 dodgerblue4 firebrick firebrick1 \
        firebrick2 firebrick3 firebrick4 floralwhite forestgreen \
        fuchsia gainsboro ghostwhite gold gold1 \
        gold2 gold3 gold4 goldenrod goldenrod1 \
        goldenrod2 goldenrod3 goldenrod4 gray gray0 \
        gray1 gray10 gray100 gray11 gray12 \
        gray13 gray14 gray15 gray16 gray17 \
        gray18 gray19 gray2 gray20 gray21 \
        gray22 gray23 gray24 gray25 gray26 \
        gray27 gray28 gray29 gray3 gray30 \
        gray31 gray32 gray33 gray34 gray35 \
        gray36 gray37 gray38 gray39 gray4 \
        gray40 gray41 gray42 gray43 gray44 \
        gray45 gray46 gray47 gray48 gray49 \
        gray5 gray50 gray51 gray52 gray53 \
        gray54 gray55 gray56 gray57 gray58 \
        gray59 gray6 gray60 gray61 gray62 \
        gray63 gray64 gray65 gray66 gray67 \
        gray68 gray69 gray7 gray70 gray71 \
        gray72 gray73 gray74 gray75 gray76 \
        gray77 gray78 gray79 gray8 gray80 \
        gray81 gray82 gray83 gray84 gray85 \
        gray86 gray87 gray88 gray89 gray9 \
        gray90 gray91 gray92 gray93 gray94 \
        gray95 gray96 gray97 gray98 gray99 \
        green green1 green2 green3 green4 \
        greenyellow grey grey0 grey1 grey10 \
        grey100 grey11 grey12 grey13 grey14 \
        grey15 grey16 grey17 grey18 grey19 \
        grey2 grey20 grey21 grey22 grey23 \
        grey24 grey25 grey26 grey27 grey28 \
        grey29 grey3 grey30 grey31 grey32 \
        grey33 grey34 grey35 grey36 grey37 \
        grey38 grey39 grey4 grey40 grey41 \
        grey42 grey43 grey44 grey45 grey46 \
        grey47 grey48 grey49 grey5 grey50 \
        grey51 grey52 grey53 grey54 grey55 \
        grey56 grey57 grey58 grey59 grey6 \
        grey60 grey61 grey62 grey63 grey64 \
        grey65 grey66 grey67 grey68 grey69 \
        grey7 grey70 grey71 grey72 grey73 \
        grey74 grey75 grey76 grey77 grey78 \
        grey79 grey8 grey80 grey81 grey82 \
        grey83 grey84 grey85 grey86 grey87 \
        grey88 grey89 grey9 grey90 grey91 \
        grey92 grey93 grey94 grey95 grey96 \
        grey97 grey98 grey99 honeydew honeydew1 \
        honeydew2 honeydew3 honeydew4 hotpink hotpink1 \
        hotpink2 hotpink3 hotpink4 indianred indianred1 \
        indianred2 indianred3 indianred4 indigo invis \
        ivory ivory1 ivory2 ivory3 ivory4 \
        khaki khaki1 khaki2 khaki3 khaki4 \
        lavender lavenderblush lavenderblush1 lavenderblush2 lavenderblush3 \
        lavenderblush4 lawngreen lemonchiffon lemonchiffon1 lemonchiffon2 \
        lemonchiffon3 lemonchiffon4 lightblue lightblue1 lightblue2 \
        lightblue3 lightblue4 lightcoral lightcyan lightcyan1 \
        lightcyan2 lightcyan3 lightcyan4 lightgoldenrod lightgoldenrod1 \
        lightgoldenrod2 lightgoldenrod3 lightgoldenrod4 lightgoldenrodyellow lightgray \
        lightgreen lightgrey lightpink lightpink1 lightpink2 \
        lightpink3 lightpink4 lightsalmon lightsalmon1 lightsalmon2 \
        lightsalmon3 lightsalmon4 lightseagreen lightskyblue lightskyblue1 \
        lightskyblue2 lightskyblue3 lightskyblue4 lightslateblue lightslategray \
        lightslategrey lightsteelblue lightsteelblue1 lightsteelblue2 lightsteelblue3 \
        lightsteelblue4 lightyellow lightyellow1 lightyellow2 lightyellow3 \
        lightyellow4 lime limegreen linen magenta \
        magenta1 magenta2 magenta3 magenta4 maroon \
        maroon1 maroon2 maroon3 maroon4 mediumaquamarine \
        mediumblue mediumorchid mediumorchid1 mediumorchid2 mediumorchid3 \
        mediumorchid4 mediumpurple mediumpurple1 mediumpurple2 mediumpurple3 \
        mediumpurple4 mediumseagreen mediumslateblue mediumspringgreen mediumturquoise \
        mediumvioletred midnightblue mintcream mistyrose mistyrose1 \
        mistyrose2 mistyrose3 mistyrose4 moccasin navajowhite \
        navajowhite1 navajowhite2 navajowhite3 navajowhite4 navy \
        navyblue none oldlace olive olivedrab \
        olivedrab1 olivedrab2 olivedrab3 olivedrab4 orange \
        orange1 orange2 orange3 orange4 orangered \
        orangered1 orangered2 orangered3 orangered4 orchid \
        orchid1 orchid2 orchid3 orchid4 palegoldenrod \
        palegreen palegreen1 palegreen2 palegreen3 palegreen4 \
        paleturquoise paleturquoise1 paleturquoise2 paleturquoise3 paleturquoise4 \
        palevioletred palevioletred1 palevioletred2 palevioletred3 palevioletred4 \
        papayawhip peachpuff peachpuff1 peachpuff2 peachpuff3 \
        peachpuff4 peru pink pink1 pink2 \
        pink3 pink4 plum plum1 plum2 \
        plum3 plum4 powderblue purple purple1 \
        purple2 purple3 purple4 rebeccapurple red \
        red1 red2 red3 red4 rosybrown \
        rosybrown1 rosybrown2 rosybrown3 rosybrown4 royalblue \
        royalblue1 royalblue2 royalblue3 royalblue4 saddlebrown \
        salmon salmon1 salmon2 salmon3 salmon4 \
        sandybrown seagreen seagreen1 seagreen2 seagreen3 \
        seagreen4 seashell seashell1 seashell2 seashell3 \
        seashell4 sienna sienna1 sienna2 sienna3 \
        sienna4 silver skyblue skyblue1 skyblue2 \
        skyblue3 skyblue4 slateblue slateblue1 slateblue2 \
        slateblue3 slateblue4 slategray slategray1 slategray2 \
        slategray3 slategray4 slategrey snow snow1 \
        snow2 snow3 snow4 springgreen springgreen1 \
        springgreen2 springgreen3 springgreen4 steelblue steelblue1 \
        steelblue2 steelblue3 steelblue4 tan tan1 \
        tan2 tan3 tan4 teal thistle \
        thistle1 thistle2 thistle3 thistle4 tomato \
        tomato1 tomato2 tomato3 tomato4 transparent \
        turquoise turquoise1 turquoise2 turquoise3 turquoise4 \
        violet violetred violetred1 violetred2 violetred3 \
        violetred4 webgray webgreen webgrey webmaroon \
        webpurple wheat wheat1 wheat2 wheat3 \
        wheat4 white whitesmoke x11gray x11green \
        x11grey x11maroon x11purple yellow yellow1 \
        yellow2 yellow3 yellow4 yellowgreen

    string join \n -- $colors
end

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
    -a "(string split ' ' -- (__fish_graphviz__print_formats))" \
    -d 'Specify an output format for a diagram'

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
