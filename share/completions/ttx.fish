# completions for ttx, xml font export utility

# Retrieved from http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=IWS-AppendixC
# Not all may be supported by ttx; names *are* case-sensitive and correct as they appear.
set -l table_names name glyf cmap post OS/2 head hmtx hhea kern hdmx LTHS vmtx vhea VDMX loca maxp DSIG PCLT gasp prep fpgm cvt CFF VORG EBDT EBLC EBSC GSUB GPOS GDEF BASE JSTF Silf Glat Gloc Feat mort morx feat acnt bsln just lcar fdsc fmtx prop Zapf opbd trak fvar gvar avar cvar bdat bhed bloc hsty PfEd TeX BDF FFTM

set -l formats raw row bitwise extfile
set -l line_endings LF CR CRLF
set -l woff_fmts woff woff2

complete -f -c ttx -k -a '(__fish_complete_suffix .otf .ttf .ttx .ttc)'
complete -c ttx -f -n "__fish_is_nth_token 1" -k -a '(__fish_complete_suffix .otf .ttf .ttx)'

# General options
complete -c ttx -f -s h -d'Show help message'
complete -c ttx -f -l version -d'Show version info'
complete -c ttx -x -s d -d'Set output folder' -a '(__fish_complete_directories)'
complete -c ttx -r -s o -d'Set output filename' -k -a '(__fish_complete_suffix .otf .ttf .ttx)'
complete -c ttx -f -s f -d'Force output overwrite'
complete -c ttx -f -s v -d'Verbose output'
complete -c ttx -f -s q -d'Quiet mode'
complete -c ttx -f -s a -d'Allow virtual glyph IDs'

# Dump options
complete -c ttx -f -s l -d'List table info'
complete -c ttx -x -s t -d'Dump named table' -a "$table_names"
complete -c ttx -x -s x -d'Exclude named table' -a "$table_names"
complete -c ttx -f -s s -d'Split tables'
complete -c ttx -f -s g -d'Split glyf tables'
complete -c ttx -f -s i -d'Do NOT disassemble TT instructions'
complete -c ttx -x -s z -d'Use FORMAT for EBDT or CBDT' -a "$formats"
complete -c ttx -f -s e -d'Don\'t ignore decompilation errors'
complete -c ttx -x -s y -d'Select font index for TrueType Collection'
complete -c ttx -x -l unicodedata -d'Custom database for character names [UnicodeData.txt]' -k -a '(__fish_complete_suffix .txt)'
complete -c ttx -x -l newline -d'Set EOL format' -a "$line_endings"

# Compile options
complete -c ttx -x -s m -d'Merge named TTF/OTF with SINGLE .ttx input' -k -a '(__fish_complete_suffix .otf .ttf)'
complete -c ttx -f -s b -d'Don\'t recalculate glyph bounding boxes'
complete -c ttx -f -l recalc-timestamp -d'Set font modified timestamp to current time'
complete -c ttx -x -l flavor -d'Set WOFF flavor' -a "$woff_fmts"
complete -c ttx -f -l with-zopfli -d'Compress with zopfli instead of zlib'
