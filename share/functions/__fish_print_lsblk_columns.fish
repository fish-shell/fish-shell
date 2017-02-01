function __fish_print_lsblk_columns --description 'Print available lsblk columns'
    lsblk --help | sed '1,/Available columns:/d; /^$/,$d; s/^\s\+//; s/\s/\t/'

end
