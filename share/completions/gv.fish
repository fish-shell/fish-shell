complete -c gv -k -xa "(__fish_complete_suffix .ps)"
complete -c gv -k -xa "(__fish_complete_suffix .ps.gz)"
complete -c gv -k -xa "(__fish_complete_suffix .eps)"
complete -c gv -k -xa "(__fish_complete_suffix .pdf)"
complete -c gv -l monochrome -d 'Display document using only black and white'
complete -c gv -l grayscale -d 'Display document without colors'
complete -c gv -l color -d 'Display document as usual'
complete -c gv -l safer -d 'Start ghostscript in safe mode'
complete -c gv -l nosafer -d 'Do not start ghostscript in safe mode'
complete -c gv -l safedir -d 'Start ghostscript from a safe diretory'
complete -c gv -l nosafedir -d 'Do not start ghostscript from a safe diretory'
complete -c gv -l quiet -d 'Start ghostscript with the -dQUIET option'
complete -c gv -l noquiet -d 'Do not start ghostscript with the -dQUIET option'
complete -c gv -l infoSilent -d 'Do not show any messages in the info popup window'
complete -c gv -l infoErrors -d 'Do not show warning messages in the info popup window'
complete -c gv -l infoAll -d 'Do show all messages in the info popup window'
complete -c gv -l arguments -d 'Start ghostscript with additional options as specified by the string ARGS' -x
complete -c gv -l page -d 'Display the page with label LABEL first' -x
complete -c gv -l center -d 'The page should be centered automatically'
complete -c gv -l nocenter -d 'The page should not be centered automatically'
complete -c gv -l media -d 'Selects the paper size to be used'
complete -c gv -l orientation -d 'Sets the orientation of the page' -xa 'automatic bbox letter legal statement tabloid ledger folio quatro 10x14 executive a3 a4 a5 b4 b5'
complete -c gv -l scale -d 'Selects the scale N, or arbitrary scale f.f' -x
complete -c gv -l scalebase -d 'Selects the scale base N' -x
complete -c gv -l swap -d 'Interchange the meaning of the orientations landscape and seascape'
complete -c gv -l noswap -d 'Do not interchange the meaning of the orientation landscape and seascape'
complete -c gv -l antialias -d 'Use antialiasing'
complete -c gv -l noantialias -d 'Do not use antialiasing'
complete -c gv -l dsc -d 'Dsc comments are respected'
complete -c gv -l nodsc -d 'Dsc comments are not respected'
complete -c gv -l eof -d 'Ignore the postscript EOF comment while scanning documents'
complete -c gv -l noeof -d 'Do not ignore the postscript EOF comment while scanning documents'
complete -c gv -l pixmap -d 'Use backing pixmap'
complete -c gv -l nopixmap -d 'Do not use backing pixmap'
complete -c gv -l watch -d 'Watch the document file for changes'
complete -c gv -l nowatch -d 'Do not watch the document file for changes'
complete -c gv -l help -d 'Print a help message and exit'
complete -c gv -l usage -d 'Print a usage message and exit'
complete -c gv -l resize -d 'Fit the size of the window to the size of the page'
complete -c gv -l noresize -d 'Do not fit the size of the window to the size of the page'
complete -c gv -o geometry -d 'Set geometry'
complete -c gv -l ad -d 'Read and use additional resources from FILE (higher priority)' -r
complete -c gv -l style -d 'Read and use additional resources from FILE (lower priority)' -r
complete -c gv -l password -d 'Sets the password for opening encrypted PDF files' -x
complete -c gv -l spartan -d 'Shortcut for --style=gv_spartan.dat'
complete -c gv -l widgetless -d 'Shortcut for --style=gv_widgetless.dat'
complete -c gv -l fullscreen -d 'Start in fullscreen mode (needs support from WM)'
complete -c gv -l presentation -d 'Presentation mode '
complete -c gv -l version -d 'Show gv version and exit'
