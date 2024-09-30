set -l command mmdc

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s V -l version -d 'Show version'

complete -c $command -s t -l theme -x \
    -a 'default\tdefault forest dark neutral' \
    -d 'Specify the theme of a chart'

complete -c $command -s w -l width -x \
    -a '800\tdefault' \
    -d 'Specify the width of a chart'

complete -c $command -s h -l height -x \
    -a '600\tdefault' \
    -d 'Specify the height of a chart'

complete -c $command -s i -l input -r -d 'Specify the input file of a chart'
complete -c $command -s o -l output -r \
    -a '(__fish_print_extensions svg md png pdf)' \
    -d 'Specify the output file of a chart'

complete -c $command -s e -l outputFormat -x \
    -a 'svg png pdf' \
    -d 'Specify the output format of a chart'

complete -c $command -s b -l backgroundColor -x \
    -a 'white\tdefault transparent' \
    -d 'Specify the background color of a chart'

complete -c $command -s c -l configFile -r \
    -d 'Specify the configuration file of Mermaid'

complete -c $command -s C -l cssFile -r \
    -d 'Specify the CSS file of a Mermaid'

complete -c $command -s I -l svgId -x \
    -d 'Specify the SVG identifier of a SVG element'

complete -c $command -s s -l scale -x \
    -a '1\tdefault' \
    -d 'Specify the scale factor of a chart'

complete -c $command -s f -l pdfFit -d 'Whether to scale the PDF to fit a chart'
complete -c $command -s q -l quiet -d 'Whether to suppress output of a chart'

complete -c $command -s p -l puppeteerConfigFile -r \
    -d 'Specify the configuration file of Puppeteer'
