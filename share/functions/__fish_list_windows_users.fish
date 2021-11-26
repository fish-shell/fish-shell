function __fish_list_windows_users --description 'Lists Windows user names'
  wmic useraccount get name | tail +2
end