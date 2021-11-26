function __fish_list_windows_drives --description 'Lists Windows drives'
  wmic logicaldisk get name | tail +2
end