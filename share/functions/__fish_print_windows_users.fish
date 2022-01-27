function __fish_print_windows_users --description 'Print Windows user names'
    wmic useraccount get name | tail +2
end
