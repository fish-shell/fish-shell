function __fish_travis_needs_command
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 ]
    return 0
  end
  return 1
end

function __fish_travis_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

# Commands
complete -c travis -f -n '__fish_travis_needs_command' -a 'accounts'        -d "Displays accounts and their subscription status"
complete -c travis -f -n '__fish_travis_needs_command' -a 'branches'        -d "Displays the most recent build for each branch"
complete -c travis -f -n '__fish_travis_needs_command' -a 'cache'           -d "Lists or deletes repository caches"
complete -c travis -f -n '__fish_travis_needs_command' -a 'cancel'          -d "Cancels a job or build"
complete -c travis -f -n '__fish_travis_needs_command' -a 'console'         -d "Interactive shell"
complete -c travis -f -n '__fish_travis_needs_command' -a 'disable'         -d "Disables a project"
complete -c travis -f -n '__fish_travis_needs_command' -a 'enable'          -d "Enables a project"
complete -c travis -f -n '__fish_travis_needs_command' -a 'encrypt'         -d "Encrypts values for the .travis.yml"
complete -c travis -f -n '__fish_travis_needs_command' -a 'encrypt-file'    -d "Encrypts a file and adds decryption steps to .travis.yml"
complete -c travis -f -n '__fish_travis_needs_command' -a 'endpoint'        -d "Displays or changes the API endpoint"
complete -c travis -f -n '__fish_travis_needs_command' -a 'env'             -d "Show or modify build environment variables"
complete -c travis -f -n '__fish_travis_needs_command' -a 'help'            -d "Helps you out when in dire need of information"
complete -c travis -f -n '__fish_travis_needs_command' -a 'history'         -d "Displays a projects build history"
complete -c travis -f -n '__fish_travis_needs_command' -a 'init'            -d "Generates a .travis.yml and enables the project"
complete -c travis -f -n '__fish_travis_needs_command' -a 'lint'            -d "Display warnings for a .travis.yml"
complete -c travis -f -n '__fish_travis_needs_command' -a 'login'           -d "Authenticates against the API and stores the token"
complete -c travis -f -n '__fish_travis_needs_command' -a 'logout'          -d "Deletes the stored API token"
complete -c travis -f -n '__fish_travis_needs_command' -a 'logs'            -d "Streams test logs"
complete -c travis -f -n '__fish_travis_needs_command' -a 'monitor'         -d "Live monitor for what's going on"
complete -c travis -f -n '__fish_travis_needs_command' -a 'open'            -d "Opens a build or job in the browser"
complete -c travis -f -n '__fish_travis_needs_command' -a 'pubkey'          -d "Prints out a repository's public key"
complete -c travis -f -n '__fish_travis_needs_command' -a 'raw'             -d "Makes an (authenticated) API call and prints out the result"
complete -c travis -f -n '__fish_travis_needs_command' -a 'report'          -d "Generates a report useful for filing issues"
complete -c travis -f -n '__fish_travis_needs_command' -a 'repos'           -d "Lists repositories the user has certain permissions on"
complete -c travis -f -n '__fish_travis_needs_command' -a 'requests'        -d "Lists recent requests"
complete -c travis -f -n '__fish_travis_needs_command' -a 'restart'         -d "Restarts a build or job"
complete -c travis -f -n '__fish_travis_needs_command' -a 'settings'        -d "Access repository settings"
complete -c travis -f -n '__fish_travis_needs_command' -a 'setup'           -d "Sets up an addon or deploy target"
complete -c travis -f -n '__fish_travis_needs_command' -a 'show'            -d "Displays a build or job"
complete -c travis -f -n '__fish_travis_needs_command' -a 'sshkey'          -d "Checks, updates or deletes an SSH key"
complete -c travis -f -n '__fish_travis_needs_command' -a 'status'          -d "Checks status of the latest build"
complete -c travis -f -n '__fish_travis_needs_command' -a 'sync'            -d "Triggers a new sync with GitHub"
complete -c travis -f -n '__fish_travis_needs_command' -a 'token'           -d "Outputs the secret API token"
complete -c travis -f -n '__fish_travis_needs_command' -a 'version'         -d "Outputs the client version"
complete -c travis -f -n '__fish_travis_needs_command' -a 'whatsup'         -d "Lists most recent builds"
complete -c travis -f -n '__fish_travis_needs_command' -a 'whoami'          -d "Outputs the current user"

# General Options
complete -c travis -f                                      -s h -l help                     -d "Display help"
complete -c travis -f -n 'not __fish_travis_needs_command' -s i -l interactive              -d "Be interactive and colorful"
complete -c travis -f -n 'not __fish_travis_needs_command'      -l no-interactive           -d "Don't be interactive and colorful"
complete -c travis -f -n 'not __fish_travis_needs_command' -s E -l explode                  -d "Don't rescue exceptions"
complete -c travis -f -n 'not __fish_travis_needs_command'      -l no-explode               -d "Rescue exceptions"
complete -c travis -f -n 'not __fish_travis_needs_command'      -l skip-version-check       -d "Don't check if travis client is up to date"
complete -c travis -f -n 'not __fish_travis_needs_command'      -l skip-completion-check    -d "Don't check if auto-completion is set up"
