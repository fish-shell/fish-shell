complete -xc rg -s A -l after-context -d 'Show NUM lines after each match.'
complete -xc rg -s B -l before-context -d 'Show NUM lines before each match.'
complete -xc rg -l color -d 'Controls when to use color.' -a "never auto always ansi"
complete -xc rg -l colors -d 'Configure color settings and styles.'
complete -xc rg -s C -l context -d 'Show NUM lines before and after each match.'
complete -c rg -l context-separator -d 'Set the context separator string.'
complete -xc rg -l dfa-size-limit -d 'The upper size limit of the regex DFA.'
complete -xc rg -l engine -d 'Specify which regexp engine to use.' -a "default pcre2 auto"
complete -rc rg -s f -l file -d 'Search for patterns from the given file.'
complete -rc rg -s g -l glob -d 'Include or exclude files.'
complete -rc rg -l iglob -d 'Include or exclude files case insensitively.'
complete -rc rg -l ignore-file -d 'Specify additional ignore files.'
complete -xc rg -s M -l max-columns -d 'Don\'t print lines longer than this limit.'
complete -xc rg -s m -l max-count -d 'Limit the number of matches.'
complete -xc rg -l max-depth -d 'Descend at most NUM directories.'
complete -xc rg -l max-filesize -d 'Ignore files larger than NUM in size.'
complete -xc rg -l path-separator -d 'Set the path separator.'
complete -rc rg -l pre -d 'search outputs of COMMAND FILE for each FILE'
complete -rc rg -l pre-glob -d 'Include or exclude files from a preprocessing command.'
complete -c rg -l regex-size-limit -d 'The upper size limit of the compiled regex.'
complete -xc rg -s e -l regexp -d 'A pattern to search for.'
complete -xc rg -s r -l replace -d 'Replace matches with the given text.'
complete -xc rg -l sort -d 'Sort results in ascending order. Implies --threads=1.' -a "path modified accessed created none"
complete -xc rg -l sortr -d 'Sort results in descending order. Implies --threads=1.' -a "path modified accessed created none"
complete -xc rg -s j -l threads -d 'The approximate number of threads to use.'
complete -xc rg -l type-add -d 'Add a new glob for a file type.'
complete -xc rg -l type-clear -d 'Clear globs for a file type.'
complete -xc rg -s T -l type-not -d 'Do not search files matching TYPE.'
complete -c rg -l auto-hybrid-regex -d 'Dynamically use PCRE2 if necessary.'
complete -c rg -l no-auto-hybrid-regex
complete -c rg -l binary -d 'Search binary files.'
complete -c rg -l no-binary
complete -c rg -l block-buffered -d 'Force block buffering.'
complete -c rg -l no-block-buffered
complete -c rg -s b -l byte-offset -d 'Print the 0-based byte offset for each matching line.'
complete -c rg -s s -l case-sensitive -d 'Search case sensitively (default).'
complete -c rg -l column -d 'Show column numbers.'
complete -c rg -l no-column
complete -c rg -l no-context-separator
complete -c rg -s c -l count -d 'Only show the count of matching lines for each file.'
complete -xc rg -l count-matches -d 'Only show the count of individual matches for each file.'
complete -c rg -l crlf -d 'Support CRLF line terminators (useful on Windows).'
complete -c rg -l no-crlf
complete -c rg -l debug -d 'Show debug messages.'
complete -c rg -l trace
complete -c rg -l no-encoding
complete -c rg -l files -d 'Print each file that would be searched.'
complete -c rg -s l -l files-with-matches -d 'Only print the paths with at least one match.'
complete -c rg -l files-without-match -d 'Only print the paths that contain zero matches.'
complete -c rg -s F -l fixed-strings -d 'Treat the pattern as a literal string.'
complete -c rg -l no-fixed-strings
complete -c rg -s L -l follow -d 'Follow symbolic links.'
complete -c rg -l no-follow
complete -c rg -l glob-case-insensitive -d 'Process all glob patterns case insensitively.'
complete -c rg -l no-glob-case-insensitive
complete -c rg -l heading -d 'Print matches grouped by each file.'
complete -c rg -l no-heading -d 'Don\'t group matches by each file.'
complete -c rg -l hidden -d 'Search hidden files and directories.'
complete -c rg -l no-hidden
complete -c rg -s i -l ignore-case -d 'Case insensitive search.'
complete -c rg -l ignore-file-case-insensitive -d 'Process ignore files case insensitively.'
complete -c rg -l no-ignore-file-case-insensitive
complete -c rg -l include-zero -d 'Include files with zero matches in summary'
complete -c rg -s v -l invert-match -d 'Invert matching.'
complete -c rg -l json -d 'Show search results in a JSON Lines format.'
complete -c rg -l no-json
complete -c rg -l line-buffered -d 'Force line buffering.'
complete -c rg -l no-line-buffered
complete -c rg -s n -l line-number -d 'Show line numbers.'
complete -c rg -s N -l no-line-number -d 'Suppress line numbers.'
complete -c rg -s x -l line-regexp -d 'Only show matches surrounded by line boundaries.'
complete -c rg -l max-columns-preview -d 'Print a preview for lines exceeding the limit.'
complete -c rg -l no-max-columns-preview
complete -c rg -l mmap -d 'Search using memory maps when possible.'
complete -c rg -l no-mmap -d 'Never use memory maps.'
complete -c rg -s U -l multiline -d 'Enable matching across multiple lines.'
complete -c rg -l no-multiline
complete -c rg -l multiline-dotall -d 'Make \'.\' match new lines when multiline is enabled.'
complete -c rg -l no-multiline-dotall
complete -c rg -l no-config -d 'Never read configuration files.'
complete -c rg -l no-ignore -d 'Don\'t respect ignore files.'
complete -c rg -l ignore
complete -c rg -l no-ignore-dot -d 'Don\'t respect .ignore files.'
complete -c rg -l ignore-dot
complete -c rg -l no-ignore-exclude -d 'Don\'t respect local exclusion files.'
complete -c rg -l ignore-exclude
complete -c rg -l no-ignore-files -d 'Don\'t respect --ignore-file arguments.'
complete -c rg -l ignore-files
complete -c rg -l no-ignore-global -d 'Don\'t respect global ignore files.'
complete -c rg -l ignore-global
complete -c rg -l no-ignore-messages -d 'Suppress gitignore parse error messages.'
complete -c rg -l ignore-messages
complete -c rg -l no-ignore-parent -d 'Don\'t respect ignore files in parent directories.'
complete -c rg -l ignore-parent
complete -c rg -l no-ignore-vcs -d 'Don\'t respect VCS ignore files.'
complete -c rg -l ignore-vcs
complete -c rg -l no-messages -d 'Suppress some error messages.'
complete -c rg -l messages
complete -c rg -l no-pcre2-unicode -d 'Disable Unicode mode for PCRE2 matching.'
complete -c rg -l pcre2-unicode
complete -c rg -l no-require-git -d 'Do not require a git repository to use gitignores.'
complete -c rg -l require-git
complete -c rg -l no-unicode -d 'Disable Unicode mode.'
complete -c rg -l unicode
complete -c rg -s 0 -l null -d 'Print a NUL byte after file paths.'
complete -c rg -l null-data -d 'Use NUL as a line terminator instead of \\n.'
complete -c rg -l one-file-system -d 'Do not descend into directories on other file systems.'
complete -c rg -l no-one-file-system
complete -c rg -s o -l only-matching -d 'Print only matches parts of a line.'
complete -c rg -l passthru -d 'Print both matching and non-matching lines.'
complete -c rg -s P -l pcre2 -d 'Enable PCRE2 matching.'
complete -c rg -l no-pcre2
complete -c rg -l pcre2-version -d 'Print the version of PCRE2 that ripgrep uses.'
complete -c rg -l no-pre
complete -c rg -s p -l pretty -d 'Alias for --color always --heading --line-number.'
complete -c rg -s q -l quiet -d 'Do not print anything to stdout.'
complete -c rg -s z -l search-zip -d 'Search in compressed files.'
complete -c rg -l no-search-zip
complete -c rg -s S -l smart-case -d 'Smart case search.'
complete -c rg -l sort-files -d 'DEPRECATED'
complete -c rg -l no-sort-files
complete -c rg -l stats -d 'Print statistics about this ripgrep search.'
complete -c rg -l no-stats
complete -c rg -s a -l text -d 'Search binary files as if they were text.'
complete -c rg -l no-text
complete -c rg -l trim -d 'Trim prefixed whitespace from matches.'
complete -c rg -l no-trim
complete -c rg -l type-list -d 'Show all supported file types.'
complete -c rg -s u -l unrestricted -d 'Reduce the level of "smart" searching.'
complete -c rg -l vimgrep -d 'Show results in vim compatible format.'
complete -c rg -s H -l with-filename -d 'Print the file path with the matched lines.'
complete -c rg -s I -l no-filename -d 'Never print the file path with the matched lines.'
complete -c rg -s w -l word-regexp -d 'Only show matches surrounded by word boundaries.'
complete -c rg -s h -l help -d 'Prints help information. Use --help for more details.'
complete -c rg -s V -l version -d 'Prints version information'

function __fish_complete_rg_types
    for item in (rg --type-list)
        if set -l match (string match -r '^(.*?)\:\s*(.*?)$' -- "$item")
            set -l name $match[2]
            set -l val $match[3]
            set -l spl (string split ', ' -- "$val")
            # If there are more than 4 example extensions take the
            # first 3 and end with ellipsis.
            if test (count $spl) -gt 4
                set val (string join ', ' -- $spl[1..3])
                set val "$val, ..."
            end
            echo "$name"\t"$val"
        end
    end
end

set -l __fish_rg_encodings \
    ascii us-ascii arabic chinese cyrillic greek greek8 hebrew korean logical \
    visual mac macintosh csmacintosh x-mac-cyrillic x-mac-roman x-mac-ukrainian \
    866 ibm819 ibm866 csibm866 big5 big5-hkscs cn-big5 csbig5 x-x-big5 cp819 \
    cp866 cp1250 cp1251 cp1252 cp1253 cp1254 cp1255 cp1256 cp1257 cp1258 \
    x-cp1250 x-cp1251 x-cp1252 x-cp1253 x-cp1254 x-cp1255 x-cp1256 x-cp1257 \
    x-cp1258 csiso2022jp csiso2022kr csiso88596e csiso88598e csiso88596i \
    csiso88598i csisolatin0 csisolatin1 csisolatin2 csisolatin3 csisolatin4 \
    csisolatin5 csisolatin6 csisolatin7 csisolatin8 csisolatin9 csisolatinarabic \
    csisolatincyrillic csisolatingreek csisolatinhebrew ecma-114 ecma-118 \
    asmo-708 elot_928 sun_eu_greek euc-jp euc-kr x-euc-jp cseuckr \
    cseucpkdfmtjapanese gbk x-gbk csiso58gb231280 gb18030 gb2312 csgb2312 \
    gb_2312 gb_2312-80 hz-gb-2312 iso-2022-cn iso-2022-cn-ext iso-2022-jp \
    iso-2022-kr iso88591 iso8859-1 iso88592 iso8859-2 iso88593 iso8859-3 \
    iso88594 iso8859-4 iso88595 iso8859-5 iso88596 iso8859-6 iso88597 iso8859-7 \
    iso88598 iso8859-8 iso88599 iso8859-9 iso885910 iso8859-10 iso885911 \
    iso8859-11 iso885913 iso8859-13 iso885914 iso8859-14 iso885915 iso8859-15 \
    iso-8859-1 iso-8859-2 iso-8859-3 iso-8859-4 iso-8859-5 iso-8859-6 iso-8859-7 \
    iso-8859-8 iso-8859-9 iso-8859-10 iso-8859-11 iso-8859-6-e iso-8859-8-e \
    iso-8859-6-i iso-8859-8-i iso-8859-13 iso-8859-14 iso-8859-15 iso-8859-16 \
    iso_8859-1 iso_8859-2 iso_8859-3 iso_8859-4 iso_8859-5 iso_8859-6 iso_8859-7 \
    iso_8859-8 iso_8859-9 iso_8859-15 iso_8859-1:1987 iso_8859-2:1987 \
    iso_8859-6:1987 iso_8859-7:1987 iso_8859-3:1988 iso_8859-4:1988 \
    iso_8859-5:1988 iso_8859-8:1988 iso_8859-9:1989 iso-ir-58 iso-ir-100 \
    iso-ir-101 iso-ir-109 iso-ir-110 iso-ir-126 iso-ir-127 iso-ir-138 iso-ir-144 \
    iso-ir-148 iso-ir-149 iso-ir-157 koi koi8 koi8-r koi8-ru koi8-u koi8_r \
    cskoi8r ks_c_5601-1987 ks_c_5601-1989 ksc5691 ksc_5691 csksc56011987 latin0 \
    latin1 latin2 latin3 latin4 latin5 latin6 l0 l1 l2 l3 l4 l5 l6 l9 shift-jis \
    shift_jis csshiftjis sjis x-sjis ms_kanji ms932 utf8 utf-8 utf-16 utf-16be \
    utf-16le unicode-1-1-utf-8 windows-31j windows-874 windows-949 windows-1250 \
    windows-1251 windows-1252 windows-1253 windows-1254 windows-1255 \
    windows-1256 windows-1257 windows-1258 dos-874 tis-620 ansi_x3.4-1968 \
    x-user-defined auto none

complete -xc rg -s t -l type -a '(__fish_complete_rg_types)'

complete -xc rg -s E -l encoding -d 'Specify the text encoding of files to search.' -a "$__fish_rg_encodings"
