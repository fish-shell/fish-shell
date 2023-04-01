function __ghoti_print_lpr_options --description 'Print lpr options'
    lpoptions -l 2>/dev/null | sed 's+\(.*\)/\(.*\):.*$+\1\t\2+'

end
