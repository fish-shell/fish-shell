function __fish_print_mounted --description 'Print mounted devices'
    if test -r /etc/mtab
        # In mtab, spaces are replaced by a literal '\040'
        # So it's safe to get the second "field" and then replace it
        # \011 encodes a tab, \012 encodes a newline and \\ encodes a backslash
        # This will probably break on newlines because of our splitting, though
        # and tabs because of descriptions
        string replace -r '\S+ (\S+) .*' '$1' </etc/mtab | string replace -a "\040" " " | string replace -a "\011" \t | string replace -a "\012" \n | string replace -a "\\\\" "\\"
    else
        mount | string replace -r '^(\S+) \S+ (\S+) .*' '$1\n$2' | string replace -r '[\d.]*:/' / | string match '/*'
    end
end
