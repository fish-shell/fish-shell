function __ghoti_print_lpr_printers --description 'Print lpr printers'
    lpstat -p 2>/dev/null | sed 's/^\S*\s\(\S*\)\s\(.*\)$/\1\t\2/'


end
