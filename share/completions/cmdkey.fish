function __cmdkey_generate_args --description 'Function to generate args'
  if not __fish_seen_argument --windows 'add' --windows 'generic'
    echo -e '/add\tAdd a user name and password to the list
/generic\tAdd generic credentials to the list'
  end

  if not __fish_seen_argument --windows 'smartcard' --windows 'user'
    echo -e '/smartcard\tRetrieve the credential from a smart card
/user:\tSpecify the user or account name to store with this entry'
  end

  if __fish_seen_argument --windows 'user'
    echo -e '/pass\tSpecify the password to store with this entry'
  end

  echo -e '/delete\tDelete a user name and password from the list
/list:\tDisplay the list of stored user names and credentials
/?\tShow help'
end

complete --command cmdkey --no-files --arguments '(__cmdkey_generate_args)'
