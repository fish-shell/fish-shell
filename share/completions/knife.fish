#
# Define functions for knife with arguments (e.g. knife bootstrap [--server])
#

function __fish_knife_scan_eq0
  set cmd (commandline -opc)
  test (count $cmd) -eq 1
end

function __fish_knife_scan_eq1
  set cmd (commandline -opc)
  test (count $cmd) -eq 2; and test $argv[1] = $cmd[2]
end

function __fish_knife_scan_eq2
  set cmd (commandline -opc)
  test (count $cmd) -eq 3; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]
end

function __fish_knife_scan_eq3
  set cmd (commandline -opc)
  test (count $cmd) -eq 4; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]; and test $argv[3] = $cmd[4]
end

function __fish_knife_scan_eq4
  set cmd (commandline -opc)
  test (count $cmd) -eq 5; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]; and test $argv[3] = $cmd[4]; and test $argv[4] = $cmd[5]
end

function __fish_knife_scan_eq5
  set cmd (commandline -opc)
  test (count $cmd) -eq 6; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]; and test $argv[3] = $cmd[4]; and test $argv[4] = $cmd[5]; and test $argv[5] = $cmd[6]
end

#
# Define functions for knife with options (e.g. knife [data bag from file])
# 

function __fish_knife_scan1
  set cmd (commandline -opc)
  test (count $cmd) -gt 1; and test $argv[1] = $cmd[2]
end

function __fish_knife_scan2
  set cmd (commandline -opc)
  test (count $cmd) -gt 2; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]
end

function __fish_knife_scan3
  set cmd (commandline -opc)
  test (count $cmd) -gt 3; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]; and test $argv[3] = $cmd[4]
end

function __fish_knife_scan4
  set cmd (commandline -opc)
  test (count $cmd) -gt 4; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]; and test $argv[3] = $cmd[4]; and test $argv[4] = $cmd[5]
end

function __fish_knife_scan5
  set cmd (commandline -opc)
  test (count $cmd) -gt 5; and test $argv[1] = $cmd[2]; and test $argv[2] = $cmd[3]; and test $argv[3] = $cmd[4]; and test $argv[4] = $cmd[5]; and test $argv[5] = $cmd[6]
end

#
# Define functions for autocomplete option arguments (e.g. knife cookbook upload [COOKBOOK] )
#

function __knife_cookbooks
  set cmd (commandline -opc)
  if [ (echo $cmd | grep "$argv" 2>/dev/null) ]
    if [ -z "$__knife_cookbooks" ]
      set -U __knife_cookbooks (knife cookbook list | awk '{if (NR != 1) print $1}' )
    end
    echo "$__knife_cookbooks" | awk '{gsub(" ", "\n"); print }'
  end
end

function __knife_envs
  set cmd (commandline -opc)
  if [ (echo $cmd | grep "$argv" 2>/dev/null) ]
    if [ -z "$__knife_envs" ]
      set -U __knife_envs (knife environment list | awk '{if (NR != 1) print $1}')
    end
    echo "$__knife_envs" | awk '{gsub(" ", "\n"); print }'
  end
end


#
# Default options for knife
#


complete -c knife -f -s c -l config -d "The configuration file to use"
complete -c knife -f -s V -l verbose -d "More verbose output. Use twice for max verbosity"
complete -c knife -f -l color -d "Use colored output, defaults to false on Windows, true otherwise"
complete -c knife -f -l no-color -d "Use colored output, defaults to false on Windows, true otherwise"
complete -c knife -f -s E -l environment -d "Set the Chef environment (except for in searches, where this will be flagrantly ignored)"
complete -c knife -f -s e -l editor -d "Set the editor to use for interactive commands"
complete -c knife -f -s d -l disable-editing -d "Do not open EDITOR, just accept the data as is"
complete -c knife -f -s h -l help -d "Show this message"
complete -c knife -f -s u -l user -d "API Client Username"
complete -c knife -f -s k -l key -d "API Client Key"
complete -c knife -f -s s -l server-url -d "Chef Server URL"
complete -c knife -f -s y -l yes -d "Say yes to all prompts for confirmation"
complete -c knife -f -l defaults -d "Accept default values for all questions"
complete -c knife -f -l print-after -d "Show the data after a destructive operation"
complete -c knife -f -s F -l format -d "Which format to use for output"
complete -c knife -f -s z -l local-mode -d "Point knife commands at local repository instead of server"
complete -c knife -f -l chef-zero-host -d "Host to start chef-zero on"
complete -c knife -f -l chef-zero-port -d "Port to start chef-zero on"
complete -c knife -f -s v -l version -d "Show chef version"


#
# Other options
#


# knife bootstrap
complete -c knife -f -n '__fish_knife_scan_eq0' -a bootstrap
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s x -l ssh-user -d "The ssh username"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s P -l ssh-password -d "The ssh password"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s p -l ssh-port -d "The ssh port"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s G -l ssh-gateway -d "The ssh gateway"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s A -l forward-agent -d "Enable SSH agent forwarding"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s i -l identity-file -d "The SSH identity file used for authentication"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s N -l node-name -d "The Chef node name for your new node"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l prerelease -d "Install the pre-release chef gems"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-version -d "The version of Chef to install"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-proxy -d "The proxy server for the node being bootstrapped"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-no-proxy -d "Do not proxy locations for the node being bootstrapped; this option is used internally by Opscode"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s d -l distro -d "Bootstrap a distro using a template"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l sudo -d "Execute the bootstrap via sudo"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l use-sudo-password -d "Execute the bootstrap via sudo with password"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l template-file -d "Full path to location of template to use"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s r -l run-list -d "Comma separated list of roles/recipes to apply"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s j -l json-attributes -d "A JSON string to be added to the first run of chef-client"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l host-key-verify -d "Verify host key, enabled by default."
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l no-host-key-verify -d "Verify host key, enabled by default."
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l hint -d "Specify Ohai Hint to be set on the bootstrap target. Use multiple --hint options to specify multiple hints."
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-url -d "URL to a custom installation script"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-install-command -d "Custom command to install chef-client"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-wget-options -d "Add options to wget when installing chef-client"
complete -c knife -f -n '__fish_knife_scan1 bootstrap' -l bootstrap-curl-options -d "Add options to curl when install chef-client"


#knife client
complete -c knife -f -n '__fish_knife_scan_eq0' -a client
#knife client bulk delete
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a bulk
complete -c knife -f -n '__fish_knife_scan_eq2 client bulk' -a delete
complete -c knife -f -n '__fish_knife_scan3 client bulk delete' -s D -l delete-validators -d "Force deletion of clients if they're validators"
#knife client create
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a create
complete -c knife -f -n '__fish_knife_scan2 client create' -s f -l file -d "Write the key to a file"
complete -c knife -f -n '__fish_knife_scan2 client create' -s a -l admin -d "Create the client as an admin"
complete -c knife -f -n '__fish_knife_scan2 client create' -l validator -d "Create the client as a validator"
#knife client delete
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a delete
complete -c knife -f -n '__fish_knife_scan2 client delete' -s D -l delete-validators -d "Force deletion of client if it's a validator"
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a edit
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a list
complete -c knife -f -n '__fish_knife_scan2 client list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a reregister
complete -c knife -f -n '__fish_knife_scan2 client reregister' -s f -l file -d "Write the key to a file"
complete -c knife -f -n '__fish_knife_scan_eq1 client' -a show
complete -c knife -f -n '__fish_knife_scan2 client show' -s a -l attribute -d "Show one or more attributes"



complete -c knife -f -n '__fish_knife_scan_eq0' -a configure
complete -c knife -f -n '__fish_knife_scan1 configure' -s r -l repository -d "The path to the chef-repo"
complete -c knife -f -n '__fish_knife_scan1 configure' -s i -l initial -d "Use to create a API client, typically an administrator client on a freshly-installed server"
complete -c knife -f -n '__fish_knife_scan1 configure' -l admin-client-name -d "The name of the client, typically the name of the admin client"
complete -c knife -f -n '__fish_knife_scan1 configure' -l admin-client-key -d "The path to the private key used by the client, typically a file named admin.pem"
complete -c knife -f -n '__fish_knife_scan1 configure' -l validation-client-name -d "The name of the validation client, typically a client named chef-validator"
complete -c knife -f -n '__fish_knife_scan1 configure' -l validation-key -d "The path to the validation key used by the client, typically a file named validation.pem"
complete -c knife -f -n '__fish_knife_scan_eq1 configure' -a client



complete -c knife -f -n '__fish_knife_scan_eq0' -a cookbook
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a bulk
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook bulk' -a delete
complete -c knife -f -n '__fish_knife_scan3 cookbook bulk delete' -s p -l purge -d "Permanently remove files from backing data store"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a create
complete -c knife -f -n '__fish_knife_scan2 cookbook create' -s o -l cookbook-path -d "The directory where the cookbook will be created"
complete -c knife -f -n '__fish_knife_scan2 cookbook create' -s r -l readme-format -d "Format of the README file, supported formats are 'md' (markdown) and 'rdoc' (rdoc)"
complete -c knife -f -n '__fish_knife_scan2 cookbook create' -s I -l license -d "License for cookbook, apachev2, gplv2, gplv3, mit or none"
complete -c knife -f -n '__fish_knife_scan2 cookbook create' -s C -l copyright -d "Name of Copyright holder"
complete -c knife -f -n '__fish_knife_scan2 cookbook create' -s m -l email -d "Email address of cookbook maintainer"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a delete
complete -c knife -f -n '__fish_knife_scan2 cookbook delete' -s a -l all -d "delete all versions"
complete -c knife -f -n '__fish_knife_scan2 cookbook delete' -s p -l purge -d "Permanently remove files from backing data store"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a download
complete -c knife -f -n '__fish_knife_scan2 cookbook download' -s N -l latest -d "The version of the cookbook to download"
complete -c knife -f -n '__fish_knife_scan2 cookbook download' -s d -l dir -d "The directory to download the cookbook into"
complete -c knife -f -n '__fish_knife_scan2 cookbook download' -s f -l force -d "Force download over the download directory if it exists"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a list
complete -c knife -f -n '__fish_knife_scan2 cookbook list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan2 cookbook list' -s a -l all -d "Show all available versions."
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a metadata
complete -c knife -f -n '__fish_knife_scan2 cookbook metadata' -a '(__knife_cookbooks cookbook metadata)'
complete -c knife -f -n '__fish_knife_scan2 cookbook metadata' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan2 cookbook metadata' -s a -l all -d "Generate metadata for all cookbooks, rather than just a single cookbook"
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook metadata' -a from
complete -c knife -f -n '__fish_knife_scan_eq3 cookbook metadata from' -a file
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a show
complete -c knife -f -n '__fish_knife_scan2 cookbook show' -a '(__knife_cookbooks cookbook show)'
complete -c knife -f -n '__fish_knife_scan2 cookbook show' -s f -l fqdn -d "The FQDN of the host to see the file for"
complete -c knife -f -n '__fish_knife_scan2 cookbook show' -s p -l platform -d "The platform to see the file for"
complete -c knife -f -n '__fish_knife_scan2 cookbook show' -s V -l platform-version -d "The platform version to see the file for"
complete -c knife -f -n '__fish_knife_scan2 cookbook show' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a test
complete -c knife -f -n '__fish_knife_scan2 cookbook test' -a '(__knife_cookbooks cookbook test)'
complete -c knife -f -n '__fish_knife_scan2 cookbook test' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan2 cookbook test' -s a -l all -d "Test all cookbooks, rather than just a single cookbook"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a upload
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -a '(__knife_cookbooks cookbook upload)'
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -l freeze -d "Freeze this version of the cookbook so that it cannot be overwritten"
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -s a -l all -d "Upload all cookbooks, rather than just a single cookbook"
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -l force -d "Update cookbook versions even if they have been frozen"
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -l concurrency -d "How many concurrent threads will be used"
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -s E -l environment -d "Set ENVIRONMENT's version dependency match the version you're uploading."
complete -c knife -f -n '__fish_knife_scan2 cookbook upload' -s d -l include-dependencies -d "Also upload cookbook dependencies"
complete -c knife -f -n '__fish_knife_scan_eq1 cookbook' -a site
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a download
complete -c knife -f -n '__fish_knife_scan3 cookbook site download' -s f -l file -d "The filename to write to"
complete -c knife -f -n '__fish_knife_scan3 cookbook site download' -l force -d "Force download deprecated version"
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a install
complete -c knife -f -n '__fish_knife_scan3 cookbook site install' -s D -l skip-dependencies -d "Skips automatic dependency installation."
complete -c knife -f -n '__fish_knife_scan3 cookbook site install' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan3 cookbook site install' -s B -l branch -d "Default branch to work with"
complete -c knife -f -n '__fish_knife_scan3 cookbook site install' -s b -l use-current-branch -d "Use the current branch"
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a list
complete -c knife -f -n '__fish_knife_scan3 cookbook site list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a search
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a share
complete -c knife -f -n '__fish_knife_scan3 cookbook site share' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a show
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a unshare
complete -c knife -f -n '__fish_knife_scan_eq2 cookbook site' -a vendor
complete -c knife -f -n '__fish_knife_scan3 cookbook site vendor' -s D -l skip-dependencies -d "Skips automatic dependency installation."
complete -c knife -f -n '__fish_knife_scan3 cookbook site vendor' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan3 cookbook site vendor' -s B -l branch -d "Default branch to work with"
complete -c knife -f -n '__fish_knife_scan3 cookbook site vendor' -s b -l use-current-branch -d "Use the current branch"



complete -c knife -f -n '__fish_knife_scan_eq0' -a data
complete -c knife -f -n '__fish_knife_scan_eq1 data' -a bag
complete -c knife -f -n '__fish_knife_scan_eq2 data bag' -a create
complete -c knife -f -n '__fish_knife_scan3 data bag create' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan3 data bag create' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan_eq2 data bag' -a delete
complete -c knife -f -n '__fish_knife_scan_eq2 data bag' -a edit
complete -c knife -f -n '__fish_knife_scan3 data bag edit' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan3 data bag edit' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan_eq2 data bag' -a from
complete -c knife -f -n '__fish_knife_scan_eq3 data bag from' -a file
complete -c knife -f -n '__fish_knife_scan4 data bag from file' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan4 data bag from file' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan4 data bag from file' -s a -l all -d "Upload all data bags or all items for specified data bags"
complete -c knife -f -n '__fish_knife_scan_eq2 data bag' -a list
complete -c knife -f -n '__fish_knife_scan3 data bag list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq2 data bag' -a show
complete -c knife -f -n '__fish_knife_scan3 data bag show' -s s -l secret -d "The secret key to use to decrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan3 data bag show' -l secret-file -d "A file containing the secret key to use to decrypt data bag item values"



complete -c knife -f -n '__fish_knife_scan_eq0' -a knife
complete -c knife -f -n '__fish_knife_scan1 knife' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 knife' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 knife' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"



complete -c knife -f -n '__fish_knife_scan_eq0' -a delete
complete -c knife -f -n '__fish_knife_scan1 delete' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 delete' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 delete' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 delete' -s r -l recurse -d "Delete directories recursively."
complete -c knife -f -n '__fish_knife_scan1 delete' -l no-recurse -d "Delete directories recursively."
complete -c knife -f -n '__fish_knife_scan1 delete' -l both -d "Delete both the local and remote copies."
complete -c knife -f -n '__fish_knife_scan1 delete' -l local -d "Delete the local copy (leave the remote copy)."



complete -c knife -f -n '__fish_knife_scan_eq0' -a deps
complete -c knife -f -n '__fish_knife_scan1 deps' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 deps' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 deps' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 deps' -l recurse -d "List dependencies recursively (default: true). Only works with --tree."
complete -c knife -f -n '__fish_knife_scan1 deps' -l no-recurse -d "List dependencies recursively (default: true). Only works with --tree."
complete -c knife -f -n '__fish_knife_scan1 deps' -l tree -d "Show dependencies in a visual tree. May show duplicates."
complete -c knife -f -n '__fish_knife_scan1 deps' -l remote -d "List dependencies on the server instead of the local filesystem"



complete -c knife -f -n '__fish_knife_scan_eq0' -a diff
complete -c knife -f -n '__fish_knife_scan1 diff' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 diff' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 diff' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 diff' -l recurse -d "List directories recursively."
complete -c knife -f -n '__fish_knife_scan1 diff' -l no-recurse -d "List directories recursively."
complete -c knife -f -n '__fish_knife_scan1 diff' -l name-only -d "Only show names of modified files."
complete -c knife -f -n '__fish_knife_scan1 diff' -l name-status -d "Only show names and statuses of modified files: Added, Deleted, Modified, and Type Changed."
complete -c knife -f -n '__fish_knife_scan1 diff' -l diff-filter -d "Select only files that are Added (A), Deleted (D), Modified (M), or have their type (i.e. regular file, directory) changed (T). Any combination of the filter characters (including none) can be used. When * (All-or-none) is added to the combination, all paths are selected if there is any file that matches other criteria in the comparison; if there is no file that matches other criteria, nothing is selected."
complete -c knife -f -n '__fish_knife_scan1 diff' -l cookbook-version -d "Version of cookbook to download (if there are multiple versions and cookbook_versions is false)"



complete -c knife -f -n '__fish_knife_scan_eq0' -a download
complete -c knife -f -n '__fish_knife_scan1 download' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 download' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 download' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 download' -l recurse -d "List directories recursively."
complete -c knife -f -n '__fish_knife_scan1 download' -l no-recurse -d "List directories recursively."
complete -c knife -f -n '__fish_knife_scan1 download' -l purge -d "Delete matching local files and directories that do not exist remotely."
complete -c knife -f -n '__fish_knife_scan1 download' -l no-purge -d "Delete matching local files and directories that do not exist remotely."
complete -c knife -f -n '__fish_knife_scan1 download' -l force -d "Force upload of files even if they match (quicker and harmless, but doesn't print out what it changed)"
complete -c knife -f -n '__fish_knife_scan1 download' -l no-force -d "Force upload of files even if they match (quicker and harmless, but doesn't print out what it changed)"
complete -c knife -f -n '__fish_knife_scan1 download' -s n -l dry-run -d "Don't take action, only print what would happen"
complete -c knife -f -n '__fish_knife_scan1 download' -l diff -d "Turn off to avoid uploading existing files; only new (and possibly deleted) files with --no-diff"
complete -c knife -f -n '__fish_knife_scan1 download' -l no-diff -d "Turn off to avoid uploading existing files; only new (and possibly deleted) files with --no-diff"
complete -c knife -f -n '__fish_knife_scan1 download' -l cookbook-version -d "Version of cookbook to download (if there are multiple versions and cookbook_versions is false)"



complete -c knife -f -n '__fish_knife_scan_eq0' -a edit
complete -c knife -f -n '__fish_knife_scan1 edit' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 edit' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 edit' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 edit' -l local -d "Show local files instead of remote"



complete -c knife -f -n '__fish_knife_scan_eq0' -a list
complete -c knife -f -n '__fish_knife_scan1 list' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 list' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 list' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 list' -s R -d "List directories recursively"
complete -c knife -f -n '__fish_knife_scan1 list' -s d -d "When directories match the pattern, do not show the directories' children"
complete -c knife -f -n '__fish_knife_scan1 list' -l local -d "List local directory instead of remote"
complete -c knife -f -n '__fish_knife_scan1 list' -s f -l flat -d "Show a list of filenames rather than the prettified ls-like output normally produced"
complete -c knife -f -n '__fish_knife_scan1 list' -s 1 -d "Show only one column of results"
complete -c knife -f -n '__fish_knife_scan1 list' -s p -d "Show trailing slashes after directories"



complete -c knife -f -n '__fish_knife_scan_eq0' -a show
complete -c knife -f -n '__fish_knife_scan1 show' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 show' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 show' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 show' -l local -d "Show local files instead of remote"



complete -c knife -f -n '__fish_knife_scan_eq0' -a upload
complete -c knife -f -n '__fish_knife_scan1 upload' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 upload' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 upload' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 upload' -l recurse -d "List directories recursively."
complete -c knife -f -n '__fish_knife_scan1 upload' -l no-recurse -d "List directories recursively."
complete -c knife -f -n '__fish_knife_scan1 upload' -l purge -d "Delete matching local files and directories that do not exist remotely."
complete -c knife -f -n '__fish_knife_scan1 upload' -l no-purge -d "Delete matching local files and directories that do not exist remotely."
complete -c knife -f -n '__fish_knife_scan1 upload' -l force -d "Force upload of files even if they match (quicker for many files). Will overwrite frozen cookbooks."
complete -c knife -f -n '__fish_knife_scan1 upload' -l no-force -d "Force upload of files even if they match (quicker for many files). Will overwrite frozen cookbooks."
complete -c knife -f -n '__fish_knife_scan1 upload' -l freeze -d "Freeze cookbooks that get uploaded."
complete -c knife -f -n '__fish_knife_scan1 upload' -l no-freeze -d "Freeze cookbooks that get uploaded."
complete -c knife -f -n '__fish_knife_scan1 upload' -s n -l dry-run -d "Don't take action, only print what would happen"
complete -c knife -f -n '__fish_knife_scan1 upload' -l diff -d "Turn off to avoid uploading existing files; only new (and possibly deleted) files with --no-diff"
complete -c knife -f -n '__fish_knife_scan1 upload' -l no-diff -d "Turn off to avoid uploading existing files; only new (and possibly deleted) files with --no-diff"



complete -c knife -f -n '__fish_knife_scan_eq0' -a xargs
complete -c knife -f -n '__fish_knife_scan1 xargs' -l repo-mode -d "Specifies the local repository layout. Values: static, everything, hosted_everything. Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 xargs' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 xargs' -l concurrency -d "Maximum number of simultaneous requests to send (default: 10)"
complete -c knife -f -n '__fish_knife_scan1 xargs' -l local -d "Xargs local files instead of remote"
complete -c knife -f -n '__fish_knife_scan1 xargs' -s p -l pattern -d "Pattern on command line (if these are not specified, a list of patterns is expected on standard input). Multiple patterns may be passed in this way."
complete -c knife -f -n '__fish_knife_scan1 xargs' -l diff -d "Whether to show a diff when files change (default: true)"
complete -c knife -f -n '__fish_knife_scan1 xargs' -l no-diff -d "Whether to show a diff when files change (default: true)"
complete -c knife -f -n '__fish_knife_scan1 xargs' -l dry-run -d "Prevents changes from actually being uploaded to the server."
complete -c knife -f -n '__fish_knife_scan1 xargs' -l force -d "Force upload of files even if they are not changed (quicker and harmless, but doesn't print out what it changed)"
complete -c knife -f -n '__fish_knife_scan1 xargs' -l no-force -d "Force upload of files even if they are not changed (quicker and harmless, but doesn't print out what it changed)"
complete -c knife -f -n '__fish_knife_scan1 xargs' -s J -l replace-first -d "String to replace with filenames. -J will only replace the FIRST occurrence of the replacement string."
complete -c knife -f -n '__fish_knife_scan1 xargs' -s I -l replace -d "String to replace with filenames. -I will replace ALL occurrence of the replacement string."
complete -c knife -f -n '__fish_knife_scan1 xargs' -s n -l max-args -d "Maximum number of arguments per command line."
complete -c knife -f -n '__fish_knife_scan1 xargs' -s s -l max-chars -d "Maximum size of command line, in characters"
complete -c knife -f -n '__fish_knife_scan1 xargs' -s t -d "Print command to be run on the command line"
complete -c knife -f -n '__fish_knife_scan1 xargs' -s 0 -d "Use the NULL character () as a separator, instead of whitespace"



complete -c knife -f -n '__fish_knife_scan_eq0' -a environment
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a compare
complete -c knife -f -n '__fish_knife_scan2 environment compare' -a '(__knife_envs environment compare)'
complete -c knife -f -n '__fish_knife_scan2 environment compare' -s a -l all -d "Show all cookbooks"
complete -c knife -f -n '__fish_knife_scan2 environment compare' -s m -l mismatch -d "Only show mismatching versions"
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a create
complete -c knife -f -n '__fish_knife_scan2 environment create' -s d -l description -d "The environment description"
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a edit
complete -c knife -f -n '__fish_knife_scan_eq2 environment edit' -a '(__knife_envs environment edit)'
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a from
complete -c knife -f -n '__fish_knife_scan_eq2 environment from' -a file
complete -c knife -f -n '__fish_knife_scan3 environment from file' -s a -l all -d "Upload all environments"
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a list
complete -c knife -f -n '__fish_knife_scan2 environment list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq1 environment' -a show
complete -c knife -f -n '__fish_knife_scan2 environment show' -a '(__knife_envs environment show)'
complete -c knife -f -n '__fish_knife_scan2 environment show' -s a -l attribute -d "Show one or more attributes"



complete -c knife -f -n '__fish_knife_scan_eq0' -a exec
complete -c knife -f -n '__fish_knife_scan1 exec' -s E -l exec -d "a string of Chef code to execute"
complete -c knife -f -n '__fish_knife_scan1 exec' -s p -l script-path -d "A colon-separated path to look for scripts in"



complete -c knife -f -n '__fish_knife_scan_eq0' -a help



complete -c knife -f -n '__fish_knife_scan_eq0' -a index
complete -c knife -f -n '__fish_knife_scan_eq1 index' -a rebuild
complete -c knife -f -n '__fish_knife_scan2 index rebuild' -s y -l yes -d "don't bother to ask if I'm sure"



complete -c knife -f -n '__fish_knife_scan_eq0' -a node
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a bulk
complete -c knife -f -n '__fish_knife_scan_eq2 node bulk' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a create
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a edit
complete -c knife -f -n '__fish_knife_scan2 node edit' -s a -l all -d "Display all attributes when editing"
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a environment
complete -c knife -f -n '__fish_knife_scan_eq2 node environment' -a set
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a from
complete -c knife -f -n '__fish_knife_scan_eq2 node from' -a file
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a list
complete -c knife -f -n '__fish_knife_scan2 node list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a run
complete -c knife -f -n '__fish_knife_scan_eq2 node run' -a list
complete -c knife -f -n '__fish_knife_scan_eq3 node run list' -a add
complete -c knife -f -n '__fish_knife_scan4 node run list add' -s a -l after -d "Place the ENTRY in the run list after ITEM"
complete -c knife -f -n '__fish_knife_scan4 node run list add' -s b -l before -d "Place the ENTRY in the run list before ITEM"
complete -c knife -f -n '__fish_knife_scan_eq3 node run list' -a remove
complete -c knife -f -n '__fish_knife_scan_eq3 node run list' -a set
complete -c knife -f -n '__fish_knife_scan_eq1 node' -a show
complete -c knife -f -n '__fish_knife_scan2 node show' -s m -l medium -d "Include normal attributes in the output"
complete -c knife -f -n '__fish_knife_scan2 node show' -s l -l long -d "Include all attributes in the output"
complete -c knife -f -n '__fish_knife_scan2 node show' -s a -l attribute -d "Show one or more attributes"
complete -c knife -f -n '__fish_knife_scan2 node show' -s r -l run-list -d "Show only the run list"
complete -c knife -f -n '__fish_knife_scan2 node show' -s E -l environment -d "Show only the Chef environment"



complete -c knife -f -n '__fish_knife_scan_eq0' -a raw
complete -c knife -f -n '__fish_knife_scan1 raw' -s m -l method -d "Request method (GET, POST, PUT or DELETE). Default: GET"
complete -c knife -f -n '__fish_knife_scan1 raw' -l pretty -d "Pretty-print JSON output. Default: true"
complete -c knife -f -n '__fish_knife_scan1 raw' -l no-pretty -d "Pretty-print JSON output. Default: true"
complete -c knife -f -n '__fish_knife_scan1 raw' -s i -l input -d "Name of file to use for PUT or POST"



complete -c knife -f -n '__fish_knife_scan_eq0' -a recipe
complete -c knife -f -n '__fish_knife_scan_eq1 recipe' -a list



complete -c knife -f -n '__fish_knife_scan_eq0' -a role
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a bulk
complete -c knife -f -n '__fish_knife_scan_eq2 role bulk' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a create
complete -c knife -f -n '__fish_knife_scan2 role create' -s d -l description -d "The role description"
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a edit
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a from
complete -c knife -f -n '__fish_knife_scan_eq2 role from' -a file
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a list
complete -c knife -f -n '__fish_knife_scan2 role list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq1 role' -a show
complete -c knife -f -n '__fish_knife_scan2 role show' -s a -l attribute -d "Show one or more attributes"



complete -c knife -f -n '__fish_knife_scan_eq0' -a search
complete -c knife -f -n '__fish_knife_scan1 search' -s a -l attribute -d "Show one or more attributes"
complete -c knife -f -n '__fish_knife_scan1 search' -s m -l medium -d "Include normal attributes in the output"
complete -c knife -f -n '__fish_knife_scan1 search' -s l -l long -d "Include all attributes in the output"
complete -c knife -f -n '__fish_knife_scan1 search' -s o -l sort -d "The order to sort the results in"
complete -c knife -f -n '__fish_knife_scan1 search' -s b -l start -d "The row to start returning results at"
complete -c knife -f -n '__fish_knife_scan1 search' -s R -l rows -d "The number of rows to return"
complete -c knife -f -n '__fish_knife_scan1 search' -s r -l run-list -d "Show only the run list"
complete -c knife -f -n '__fish_knife_scan1 search' -s i -l id-only -d "Show only the ID of matching objects"
complete -c knife -f -n '__fish_knife_scan1 search' -s q -l query -d "The search query; useful to protect queries starting with -"



complete -c knife -f -n '__fish_knife_scan_eq0' -a serve
complete -c knife -f -n '__fish_knife_scan1 serve' -l repo-mode -d "Specifies the local repository layout. Values: static (only environments/roles/data_bags/cookbooks), everything (includes nodes/clients/users), hosted_everything (includes acls/groups/etc. for Enterprise/Hosted Chef). Default: everything/hosted_everything"
complete -c knife -f -n '__fish_knife_scan1 serve' -l chef-repo-path -d "Overrides the location of chef repo. Default is specified by chef_repo_path in the config"
complete -c knife -f -n '__fish_knife_scan1 serve' -l chef-zero-host -d "Overrides the host upon which chef-zero listens. Default is 127.0.0.1."



complete -c knife -f -n '__fish_knife_scan_eq0' -a ssh
complete -c knife -f -n '__fish_knife_scan1 ssh' -s C -l concurrency -d "The number of concurrent connections"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s a -l attribute -d "The attribute to use for opening the connection - default depends on the context"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s m -l manual-list -d "QUERY is a space separated list of servers"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s x -l ssh-user -d "The ssh username"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s P -l ssh-password -d "The ssh password - will prompt if flag is specified but no password is given"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s p -l ssh-port -d "The ssh port"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s G -l ssh-gateway -d "The ssh gateway"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s A -l forward-agent -d "Enable SSH agent forwarding"
complete -c knife -f -n '__fish_knife_scan1 ssh' -s i -l identity-file -d "The SSH identity file used for authentication"
complete -c knife -f -n '__fish_knife_scan1 ssh' -l host-key-verify -d "Verify host key, enabled by default."
complete -c knife -f -n '__fish_knife_scan1 ssh' -l no-host-key-verify -d "Verify host key, enabled by default."



complete -c knife -f -n '__fish_knife_scan_eq0' -a ssl
complete -c knife -f -n '__fish_knife_scan_eq1 ssl' -a check
complete -c knife -f -n '__fish_knife_scan_eq1 ssl' -a fetch



complete -c knife -f -n '__fish_knife_scan_eq0' -a status
complete -c knife -f -n '__fish_knife_scan1 status' -s r -l run-list -d "Show the run list"
complete -c knife -f -n '__fish_knife_scan1 status' -s s -l sort-reverse -d "Sort the status list by last run time descending"
complete -c knife -f -n '__fish_knife_scan1 status' -s H -l hide-healthy -d "Hide nodes that have run chef in the last hour"



complete -c knife -f -n '__fish_knife_scan_eq0' -a tag
complete -c knife -f -n '__fish_knife_scan_eq1 tag' -a create
complete -c knife -f -n '__fish_knife_scan_eq1 tag' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 tag' -a list



complete -c knife -f -n '__fish_knife_scan_eq0' -a user
complete -c knife -f -n '__fish_knife_scan_eq1 user' -a create
complete -c knife -f -n '__fish_knife_scan2 user create' -s f -l file -d "Write the private key to a file"
complete -c knife -f -n '__fish_knife_scan2 user create' -s a -l admin -d "Create the user as an admin"
complete -c knife -f -n '__fish_knife_scan2 user create' -s p -l password -d "Password for newly created user"
complete -c knife -f -n '__fish_knife_scan2 user create' -l user-key -d "Public key for newly created user. By default a key will be created for you."
complete -c knife -f -n '__fish_knife_scan_eq1 user' -a delete
complete -c knife -f -n '__fish_knife_scan_eq1 user' -a edit
complete -c knife -f -n '__fish_knife_scan_eq1 user' -a list
complete -c knife -f -n '__fish_knife_scan2 user list' -s w -l with-uri -d "Show corresponding URIs"
complete -c knife -f -n '__fish_knife_scan_eq1 user' -a reregister
complete -c knife -f -n '__fish_knife_scan2 user reregister' -s f -l file -d "Write the private key to a file"
complete -c knife -f -n '__fish_knife_scan_eq1 user' -a show
complete -c knife -f -n '__fish_knife_scan2 user show' -s a -l attribute -d "Show one or more attributes"



if [ ( gem list knife-ec2 | grep knife-ec2 ) ]
complete -c knife -f -n '__fish_knife_scan_eq0' -a ec2
complete -c knife -f -n '__fish_knife_scan_eq1 ec2' -a flavor
complete -c knife -f -n '__fish_knife_scan_eq2 ec2 flavor' -a list
complete -c knife -f -n '__fish_knife_scan3 ec2 flavor list' -l aws-credential-file -d "File containing AWS credentials as used by aws cmdline tools"
complete -c knife -f -n '__fish_knife_scan3 ec2 flavor list' -s A -l aws-access-key-id -d "Your AWS Access Key ID"
complete -c knife -f -n '__fish_knife_scan3 ec2 flavor list' -s K -l aws-secret-access-key -d "Your AWS API Secret Access Key"
complete -c knife -f -n '__fish_knife_scan3 ec2 flavor list' -l region -d "Your AWS region"
complete -c knife -f -n '__fish_knife_scan_eq1 ec2' -a instance
complete -c knife -f -n '__fish_knife_scan_eq2 ec2 instance' -a data
complete -c knife -f -n '__fish_knife_scan3 ec2 instance data' -s e -l edit -d "Edit the instance data"
complete -c knife -f -n '__fish_knife_scan3 ec2 instance data' -s r -l run-list -d "Comma separated list of roles/recipes to apply"
complete -c knife -f -n '__fish_knife_scan_eq1 ec2' -a server
complete -c knife -f -n '__fish_knife_scan_eq2 ec2 server' -a create
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l aws-credential-file -d "File containing AWS credentials as used by aws cmdline tools"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s A -l aws-access-key-id -d "Your AWS Access Key ID"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s K -l aws-secret-access-key -d "Your AWS API Secret Access Key"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l region -d "Your AWS region"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s x -l winrm-user -d "The WinRM username"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s P -l winrm-password -d "The WinRM password"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s p -l winrm-port -d "The WinRM port, by default this is 5985"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s i -l identity-file -d "The SSH identity file used for authentication"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s t -l winrm-transport -d "The WinRM transport type. valid choices are [ssl, plaintext]"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s i -l keytab-file -d "The Kerberos keytab file used for authentication"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s R -l kerberos-realm -d "The Kerberos realm used for authentication"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s S -l kerberos-service -d "The Kerberos service used for authentication"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s f -l ca-trust-file -d "The Certificate Authority (CA) trust file used for SSL transport"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s f -l flavor -d "The flavor of server (m1.small, m1.medium, etc)"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s I -l image -d "The AMI for the server"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l iam-profile -d "The IAM instance profile to apply to this instance."
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s G -l groups -d "The security groups for this server; not allowed when using VPC"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s g -l security-group-ids -d "The security group ids for this server; required when using VPC"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l associate-eip -d "Associate existing elastic IP address with instance after launch"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l dedicated_instance -d "Launch as a Dedicated instance (VPC ONLY)"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l placement-group -d "The placement group to place a cluster compute instance"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s T -l tags -d "The tags for this server"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s Z -l availability-zone -d "The Availability Zone"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s N -l node-name -d "The Chef node name for your new node"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s S -l ssh-key -d "The AWS SSH key id"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s x -l ssh-user -d "The ssh username"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s P -l ssh-password -d "The ssh password"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s p -l ssh-port -d "The ssh port"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s w -l ssh-gateway -d "The ssh gateway server"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l prerelease -d "Install the pre-release chef gems"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l bootstrap-version -d "The version of Chef to install"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l bootstrap-proxy -d "The proxy server for the node being bootstrapped"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s d -l distro -d "Bootstrap a distro using a template; default is 'chef-full'"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l template-file -d "Full path to location of template to use"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l ebs-size -d "The size of the EBS volume in GB, for EBS-backed instances"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l ebs-optimized -d "Enabled optimized EBS I/O"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l ebs-no-delete-on-term -d "Do not delete EBS volume on instance termination"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s r -l run-list -d "Comma separated list of roles/recipes to apply"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s j -l json-attributes -d "A JSON string to be added to the first run of chef-client"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s s -l subnet -d "create node in this Virtual Private Cloud Subnet ID (implies VPC mode)"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l private-ip-address -d "allows to specify the private IP address of the instance in VPC mode"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l host-key-verify -d "Verify host key, enabled by default."
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l no-host-key-verify -d "Verify host key, enabled by default."
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l bootstrap-protocol -d "protocol to bootstrap windows servers. options: winrm/ssh"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l fqdn -d "Pre-defined FQDN"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s u -l user-data -d "The EC2 User Data file to provision the instance with"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l hint -d "Specify Ohai Hint to be set on the bootstrap target. Use multiple --hint options to specify multiple hints."
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l ephemeral -d "Comma separated list of device locations (eg - /dev/sdb) to map ephemeral devices"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -s a -l server-connect-attribute -d "The EC2 server attribute to use for SSH connection. Use this attr for creating VPC instances along with --associate-eip"
complete -c knife -f -n '__fish_knife_scan3 ec2 server create' -l associate-public-ip -d "Associate public ip to VPC instance."
complete -c knife -f -n '__fish_knife_scan_eq2 ec2 server' -a delete
complete -c knife -f -n '__fish_knife_scan3 ec2 server delete' -l aws-credential-file -d "File containing AWS credentials as used by aws cmdline tools"
complete -c knife -f -n '__fish_knife_scan3 ec2 server delete' -s A -l aws-access-key-id -d "Your AWS Access Key ID"
complete -c knife -f -n '__fish_knife_scan3 ec2 server delete' -s K -l aws-secret-access-key -d "Your AWS API Secret Access Key"
complete -c knife -f -n '__fish_knife_scan3 ec2 server delete' -l region -d "Your AWS region"
complete -c knife -f -n '__fish_knife_scan3 ec2 server delete' -s P -l purge -d "Destroy corresponding node and client on the Chef Server, in addition to destroying the EC2 node itself. Assumes node and client have the same name as the server (if not, add the '--node-name' option)."
complete -c knife -f -n '__fish_knife_scan3 ec2 server delete' -s N -l node-name -d "The name of the node and client to delete, if it differs from the server name. Only has meaning when used with the '--purge' option."
complete -c knife -f -n '__fish_knife_scan_eq2 ec2 server' -a list
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -l aws-credential-file -d "File containing AWS credentials as used by aws cmdline tools"
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -s A -l aws-access-key-id -d "Your AWS Access Key ID"
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -s K -l aws-secret-access-key -d "Your AWS API Secret Access Key"
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -l region -d "Your AWS region"
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -s n -l no-name -d "Do not display name tag in output"
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -s z -l availability-zone -d "Show availability zones"
complete -c knife -f -n '__fish_knife_scan3 ec2 server list' -s t -l tags -d "List of tags to output"
end



if [ ( gem list knife-spork | grep knife-spork ) ]
complete -c knife -f -n '__fish_knife_scan_eq0' -a spork
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a bump
complete -c knife -f -n '__fish_knife_scan2 spork bump' -a '(__knife_cookbooks spork bump)'
complete -c knife -f -n '__fish_knife_scan2 spork bump' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a check
complete -c knife -f -n '__fish_knife_scan2 spork check' -a '(__knife_cookbooks spork check)'
complete -c knife -f -n '__fish_knife_scan2 spork check' -s -a -l all -d "Show all uploaded versions of the cookbook"
complete -c knife -f -n '__fish_knife_scan2 spork check' -l fail -d "If the check fails exit with non-zero exit code"
complete -c knife -f -n '__fish_knife_scan2 spork check' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a data
complete -c knife -f -n '__fish_knife_scan_eq2 spork data' -a bag
complete -c knife -f -n '__fish_knife_scan_eq3 spork data bag' -a create
complete -c knife -f -n '__fish_knife_scan4 spork data bag create' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan4 spork data bag create' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan_eq3 spork data bag' -a delete
complete -c knife -f -n '__fish_knife_scan_eq3 spork data bag' -a edit
complete -c knife -f -n '__fish_knife_scan4 spork data bag edit' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan4 spork data bag edit' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan_eq3 spork data bag' -a from
complete -c knife -f -n '__fish_knife_scan_eq4 spork data bag from' -a file
complete -c knife -f -n '__fish_knife_scan5 spork data bag from file' -s s -l secret -d "The secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan5 spork data bag from file' -l secret-file -d "A file containing the secret key to use to encrypt data bag item values"
complete -c knife -f -n '__fish_knife_scan5 spork data bag from file' -s a -l all -d "Upload all data bags"
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a environment
complete -c knife -f -n '__fish_knife_scan_eq2 spork environment' -a check
complete -c knife -f -n '__fish_knife_scan3 spork environment check' -s f -l fatal -d "Quit on first invalid constraint located"
complete -c knife -f -n '__fish_knife_scan_eq2 spork environment' -a create
complete -c knife -f -n '__fish_knife_scan3 spork environment create' -s d -l description -d "The environment description"
complete -c knife -f -n '__fish_knife_scan_eq2 spork environment' -a delete
complete -c knife -f -n '__fish_knife_scan_eq2 spork environment' -a edit
complete -c knife -f -n '__fish_knife_scan_eq2 spork environment' -a from
complete -c knife -f -n '__fish_knife_scan_eq3 spork environment from' -a file
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a info
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a node
complete -c knife -f -n '__fish_knife_scan_eq2 spork node' -a create
complete -c knife -f -n '__fish_knife_scan_eq2 spork node' -a delete
complete -c knife -f -n '__fish_knife_scan_eq2 spork node' -a edit
complete -c knife -f -n '__fish_knife_scan3 spork node edit' -s a -l all -d "Display all attributes when editing"
complete -c knife -f -n '__fish_knife_scan_eq2 spork node' -a from
complete -c knife -f -n '__fish_knife_scan_eq3 spork node from' -a file
complete -c knife -f -n '__fish_knife_scan_eq2 spork node' -a run
complete -c knife -f -n '__fish_knife_scan_eq3 spork node run' -a list
complete -c knife -f -n '__fish_knife_scan_eq4 spork node run list' -a add
complete -c knife -f -n '__fish_knife_scan5 spork node run list add' -s a -l after -d "Place the ENTRY in the run list after ITEM"
complete -c knife -f -n '__fish_knife_scan_eq4 spork node run list' -a remove
complete -c knife -f -n '__fish_knife_scan_eq4 spork node run list' -a set
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a omni
complete -c knife -f -n '__fish_knife_scan2 spork omni' -a '(__knife_cookbooks spork omni)'
complete -c knife -f -n '__fish_knife_scan2 spork omni' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan2 spork omni' -s D -l include-dependencies -d "Also upload cookbook dependencies during the upload step"
complete -c knife -f -n '__fish_knife_scan2 spork omni' -s l -l bump-level -d "Version level to bump the cookbook (defaults to patch)"
complete -c knife -f -n '__fish_knife_scan2 spork omni' -s e -l environment -a '(__knife_envs)' -d "Environment to promote the cookbook to"
complete -c knife -f -n '__fish_knife_scan2 spork omni' -l remote -d "Make omni finish with promote --remote instead of a local promote"
complete -c knife -f -n '__fish_knife_scan2 spork omni' -s b -l berksfile -d "Path to a Berksfile to operate from"
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a promote
complete -c knife -f -n '__fish_knife_scan2 spork promote' -a '(__knife_envs spork promote) (__knife_cookbooks spork promote)'
complete -c knife -f -n '__fish_knife_scan2 spork promote' -s v -l version -d "Set the environment's version constraint to the specified version"
complete -c knife -f -n '__fish_knife_scan2 spork promote' -l remote -d "Save the environment to the chef server in addition to the local JSON file"
complete -c knife -f -n '__fish_knife_scan2 spork promote' -s b -l berksfile -d "Path to a Berksfile to operate off of"
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a role
complete -c knife -f -n '__fish_knife_scan_eq2 spork role' -a create
complete -c knife -f -n '__fish_knife_scan3 spork role create' -s d -l description -d "The role description"
complete -c knife -f -n '__fish_knife_scan_eq2 spork role' -a delete
complete -c knife -f -n '__fish_knife_scan_eq2 spork role' -a edit
complete -c knife -f -n '__fish_knife_scan_eq2 spork role' -a from
complete -c knife -f -n '__fish_knife_scan_eq3 spork role from' -a file
complete -c knife -f -n '__fish_knife_scan_eq1 spork' -a upload
complete -c knife -f -n '__fish_knife_scan2 spork upload' -s o -l cookbook-path -d "A colon-separated path to look for cookbooks in"
complete -c knife -f -n '__fish_knife_scan2 spork upload' -l freeze -d "Freeze this version of the cookbook so that it cannot be overwritten"
complete -c knife -f -n '__fish_knife_scan2 spork upload' -s D -l include-dependencies -d "Also upload cookbook dependencies"
complete -c knife -f -n '__fish_knife_scan2 spork upload' -s b -l berksfile -d "Path to a Berksfile to operate off of"
end



if [ (gem list knife-windows | grep knife-windows) ]
complete -c knife -f -n '__fish_knife_scan_eq0' -a winrm
complete -c knife -f -n '__fish_knife_scan1 winrm' -s x -l winrm-user -d "The WinRM username"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s P -l winrm-password -d "The WinRM password"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s p -l winrm-port -d "The WinRM port, by default this is 5985"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s i -l identity-file -d "The SSH identity file used for authentication"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s t -l winrm-transport -d "The WinRM transport type. valid choices are [ssl, plaintext]"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s i -l keytab-file -d "The Kerberos keytab file used for authentication"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s R -l kerberos-realm -d "The Kerberos realm used for authentication"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s S -l kerberos-service -d "The Kerberos service used for authentication"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s f -l ca-trust-file -d "The Certificate Authority (CA) trust file used for SSL transport"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s a -l attribute -d "The attribute to use for opening the connection - default is fqdn"
complete -c knife -f -n '__fish_knife_scan1 winrm' -l returns -d "A comma delimited list of return codes which indicate success"
complete -c knife -f -n '__fish_knife_scan1 winrm' -s m -l manual-list -d "QUERY is a space separated list of servers"



complete -c knife -f -n '__fish_knife_scan_eq0' -a windows
complete -c knife -f -n '__fish_knife_scan_eq1 windows' -a helper


complete -c knife -f -s c -l config -d "The configuration file to use"
complete -c knife -f -s V -l verbose -d "More verbose output. Use twice for max verbosity"
complete -c knife -f -l color -d "Use colored output, defaults to false on Windows, true otherwise"
complete -c knife -f -l no-color -d "Use colored output, defaults to false on Windows, true otherwise"
complete -c knife -f -s E -l environment -d "Set the Chef environment (except for in searches, where this will be flagrantly ignored)"
complete -c knife -f -s e -l editor -d "Set the editor to use for interactive commands"
complete -c knife -f -s d -l disable-editing -d "Do not open EDITOR, just accept the data as is"
complete -c knife -f -s h -l help -d "Show this message"
complete -c knife -f -s u -l user -d "API Client Username"
complete -c knife -f -s k -l key -d "API Client Key"
complete -c knife -f -s s -l server-url -d "Chef Server URL"
complete -c knife -f -s y -l yes -d "Say yes to all prompts for confirmation"
complete -c knife -f -l defaults -d "Accept default values for all questions"
complete -c knife -f -l print-after -d "Show the data after a destructive operation"
complete -c knife -f -s F -l format -d "Which format to use for output"
complete -c knife -f -s z -l local-mode -d "Point knife commands at local repository instead of server"
complete -c knife -f -l chef-zero-host -d "Host to start chef-zero on"
complete -c knife -f -l chef-zero-port -d "Port to start chef-zero on"
complete -c knife -f -s v -l version -d "Show chef version"

end
