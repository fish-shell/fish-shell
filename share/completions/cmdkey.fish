function __cmdkey_complete_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  set --local previous_token (commandline --tokenize --cut-at-cursor)[-1]

  switch $current_token
    case '/user:*'
      __fish_list_windows_users | awk "{ printf \"%s%s\n\", \"$current_token\", \$0 }"
    case '*'
      if test "$previous_token" = '/delete'
        echo -e '/ras\tDelete remote access entry'
        return
      end

      if not __fish_seen_argument --windows 'add:' --windows 'generic:'
        echo -e '/add:\tAdd a user name and password
/generic:\tAdd generic credentials'
      end

      if not __fish_seen_argument --windows 'smartcard' --windows 'user:'
        echo -e '/smartcard\tRetrieve the credential
/user:\tSpecify the user or account name'
      end

      if __fish_seen_argument --windows 'user:'
        echo -e '/pass:\tSpecify the password'
      end

      echo -e '/delete\tDelete a user name and password
/list:\tDisplay the list of stored user names and credentials
/?\tShow help'
  end
end

complete --command cmdkey --no-files --arguments '(__cmdkey_complete_args)'
