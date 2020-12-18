# Completions for the .NET CLI tools

set -l build_configurations Debug Release
set -l msbuild_verbosity_levels q quiet m minimal n normal d detailed diag diagnostic

# Runtime options
complete -f -c dotnet -n __fish_is_first_arg -l additionalprobingpath -d "Path containing probing policy and assemblies to probe"
complete -f -c dotnet -n __fish_is_first_arg -l additional-deps -d "Path to additional deps.json"
complete -f -c dotnet -n __fish_is_first_arg -l depsfile -d "Path to deps.json"
complete -f -c dotnet -n __fish_is_first_arg -l fx-version -d "Version of the .NET runtime to use"
complete -x -c dotnet -n __fish_is_first_arg -l roll-forward -a "LatestPatch Minor LatestMinor Major LatestMajor Disable" -d "Roll forward to framework version"
complete -f -c dotnet -n __fish_is_first_arg -l runtimeconfig -d "Path to runtimeconfig.json"

# SDK options
complete -f -c dotnet -s d -l diagnostics -d "Enable diagnostic output"
complete -f -c dotnet -s h -l help -d "Show help"
complete -f -c dotnet -n __fish_is_first_arg -l info -d "Display .NET information"
complete -f -c dotnet -n __fish_is_first_arg -l list-runtimes -d "Display the installed runtimes"
complete -f -c dotnet -n __fish_is_first_arg -l list-sdks -d "Display the installed SDKs"
complete -f -c dotnet -n __fish_is_first_arg -l version -d "Display .NET SDK version"

# SDK commands
complete -f -c dotnet -n __fish_use_subcommand -a add -d "Add a package/reference"
complete -f -c dotnet -n __fish_use_subcommand -a build -d "Build a .NET project"
complete -f -c dotnet -n __fish_use_subcommand -a build-server -d "Interact with build servers"
complete -f -c dotnet -n __fish_use_subcommand -a clean -d "Clean build outputs"
complete -f -c dotnet -n __fish_use_subcommand -a help -d "Show help"
complete -f -c dotnet -n __fish_use_subcommand -a list -d "List project references"
complete -f -c dotnet -n __fish_use_subcommand -a msbuild -d "Run MSBuild commands"
complete -f -c dotnet -n __fish_use_subcommand -a new -d "Create a new .NET project"
complete -f -c dotnet -n __fish_use_subcommand -a nuget -d "Run additional NuGet commands"
complete -f -c dotnet -n __fish_use_subcommand -a pack -d "Create a NuGet package"
complete -f -c dotnet -n __fish_use_subcommand -a publish -d "Publish a .NET project for deployment"
complete -f -c dotnet -n __fish_use_subcommand -a remove -d "Remove a package/reference"
complete -f -c dotnet -n __fish_use_subcommand -a restore -d "Restore dependencies"
complete -f -c dotnet -n __fish_use_subcommand -a run -d "Run the application from source"
complete -f -c dotnet -n __fish_use_subcommand -a sln -d "Modify Visual Studio solution files"
complete -f -c dotnet -n __fish_use_subcommand -a store -d "Store assemblies"
complete -f -c dotnet -n __fish_use_subcommand -a test -d "Run unit tests"
complete -f -c dotnet -n __fish_use_subcommand -a tool -d "Manage .NET tool"
complete -f -c dotnet -n __fish_use_subcommand -a vstest -d "Run VSTest commands"

# Project commands
set -l project_commands package reference
## package
complete -f -c dotnet -n "__fish_seen_subcommand_from add && not __fish_seen_subcommand_from $project_commands nuget sln" -a package -d "Add a NuGet package reference"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && not __fish_seen_subcommand_from $project_commands nuget sln tool" -a package -d "List all package references"
complete -f -c dotnet -n "__fish_seen_subcommand_from remove && not __fish_seen_subcommand_from $project_commands nuget sln" -a package -d "Remove a NuGet package reference"
## reference
complete -f -c dotnet -n "__fish_seen_subcommand_from add && not __fish_seen_subcommand_from $project_commands nuget sln" -a reference -d "Add a P2P reference"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && not __fish_seen_subcommand_from $project_commands nuget sln tool" -a reference -d "List all P2P references"
complete -f -c dotnet -n "__fish_seen_subcommand_from remove && not __fish_seen_subcommand_from $project_commands nuget sln" -a reference -d "Remove a P2P reference"

# NuGet commands
set -l nuget_commands add delete disable enable list locals push remove update verify
## add
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a add -d "Add a NuGet source"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && not __fish_seen_subcommand_from client-cert source" -a client-cert -d "Add a client certificate configuration"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && not __fish_seen_subcommand_from client-cert source" -a source -d "Add a NuGet source"
## delete
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a delete -d "Delete a package from the server"
## disable
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a disable -d "Disable a NuGet source"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from disable && not __fish_seen_subcommand_from source" -a source -d "Disable a NuGet source"
## enable
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a enable -d "Enable a NuGet source"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from enable && not __fish_seen_subcommand_from source" -a source -d "Enable a NuGet source"
## list
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a list -d "List configured NuGet sources"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from list && not __fish_seen_subcommand_from client-cert source" -a client-cert -d "List all the client certificates in the configuration"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from list && not __fish_seen_subcommand_from client-cert source" -a source -d "List all configured NuGet sources"
## locals
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a locals -d "Clear/List local NuGet resources"
## push
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a push -d "Push a package to the server"
## remove
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a remove -d "Remove a NuGet source"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from remove && not __fish_seen_subcommand_from client-cert source" -a client-cert -d "Remove the client certificate configuration"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from remove && not __fish_seen_subcommand_from client-cert source" -a source -d "Remove a NuGet source"
## update
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a update -d "Update a NuGet source"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && not __fish_seen_subcommand_from client-cert source" -a client-cert -d "Update the client certificate configuration"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && not __fish_seen_subcommand_from client-cert source" -a source -d "Update a NuGet source"
## verify
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -a verify -d "Verify a signed NuGet package"

# .NET tool commands
set -l tool_commands install uninstall update list run search restore
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a install -d "Install global/local tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a uninstall -d "Uninstall a global/local tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a update -d "Update a global tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a list -d "List tools installed globally/locally"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a run -d "Run local tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a search -d "Search .NET tools in NuGet.org"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && not __fish_seen_subcommand_from $tool_commands" -a restore -d "Restore tools defined in the local tool manifest"

# add options
## package command
complete -x -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -s v -l version -d "Version of the package to add"
complete -x -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -s f -l framework -d "Add the reference only when targeting a specific framework"
complete -f -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -s n -l no-restore -d "Add the reference without performing restore preview and compatibility check"
complete -f -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -s s -l source -d "NuGet package source to use during the restore"
complete -f -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -l package-directory -d "Directory to restore packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from package" -l prerelease -d "Allow prerelease packages to be installed"
## reference command
complete -x -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from reference" -s f -l framework -d "Add the reference only when targeting a specific framework"
complete -f -c dotnet -n "__fish_seen_subcommand_from add && __fish_seen_subcommand_from reference" -l interactive -d "Allow interactive input/action"

# build options
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -s o -l output -d "Output directory to place built artifacts"
complete -x -c dotnet -n "__fish_seen_subcommand_from build" -s f -l framework -d "Target framework to build"
complete -x -c dotnet -n "__fish_seen_subcommand_from build" -s c -l configuration -a "$build_configurations" -d "Configuration to use"
complete -x -c dotnet -n "__fish_seen_subcommand_from build" -s r -l runtime -d "Target runtime to build"
complete -x -c dotnet -n "__fish_seen_subcommand_from build" -l version-suffix -d "Set the value of the \$(VersionSuffix) property to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -l no-incremental -d "Don't use incremental building"
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -l no-dependencies -d "Don't build P2P references"
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -l nologo -d "Don't display the startup banner or the copyright message"
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -l no-restore -d "Don't restore the project before building"
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from build" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
complete -f -c dotnet -n "__fish_seen_subcommand_from build" -l force -d "Force all dependencies to be resolved"

# build-server commands and options
## Command
complete -f -c dotnet -n "__fish_seen_subcommand_from build-server && not __fish_seen_subcommand_from shutdown" -a shutdown -d "Shutdown build servers"
## Options
complete -f -c dotnet -n "__fish_seen_subcommand_from build-server && __fish_seen_subcommand_from shutdown" -l msbuild -d "Shutdown the MSBuild build server"
complete -f -c dotnet -n "__fish_seen_subcommand_from build-server && __fish_seen_subcommand_from shutdown" -l vbcscompiler -d "Shutdown the VB/C# compiler build server"
complete -f -c dotnet -n "__fish_seen_subcommand_from build-server && __fish_seen_subcommand_from shutdown" -l razor -d "Shutdown the Razor build server"

# clean options
complete -f -c dotnet -n "__fish_seen_subcommand_from clean" -s o -l output -d "Directory containing the build artifacts to clean"
complete -f -c dotnet -n "__fish_seen_subcommand_from clean" -l nologo -d "Don't display the startup banner or the copyright message"
complete -x -c dotnet -n "__fish_seen_subcommand_from clean" -s f -l framework -d "Target framework to clean"
complete -x -c dotnet -n "__fish_seen_subcommand_from clean" -s r -l runtime -d "Target runtime to clean"
complete -x -c dotnet -n "__fish_seen_subcommand_from clean" -s c -l configuration -a "$build_configurations" -d "Configuration to clean"
complete -f -c dotnet -n "__fish_seen_subcommand_from clean" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from clean" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"

# list options
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l outdated -d "List packages that have newer versions"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l deprecated -d "List packages that have been deprecated"
complete -x -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l framework -d "Choose a framework to show its packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l include-transitive -d "List transitive and top-level packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l include-prerelease -d "Consider packages with prerelease versions"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l highest-patch -d "Consider only the packages with a matching major and minor version numbers"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l highest-minor -d "Consider only the packages with a matching major version number"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l config -d "Path to the NuGet config file to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l source -d "NuGet sources to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from list && __fish_seen_subcommand_from package" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"

# new options
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -s l -l list -d "List templates containing the specified name"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -s n -l name -d "Name for the output being created"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -s o -l output -d "Location to place the generated output"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -s i -l install -d "Install a template pack"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -s u -l uninstall -d "Uninstall a template pack"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -l nuget-source -d "Specify a NuGet source to use during install"
complete -x -c dotnet -n "__fish_seen_subcommand_from new" -l type -a "project item" -d "Filter templates based on available types"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -l dry-run -d "Dry run"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -l force -d "Force content to be generated"
complete -x -c dotnet -n "__fish_seen_subcommand_from new" -o lang -l language -a "C# F# VB" -d "Language of the template to create"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -l update-check -d "Check the currently installed template packs for updates"
complete -f -c dotnet -n "__fish_seen_subcommand_from new" -l update-apply -d "Check the currently installed template packs for updates and installs them"

# nuget options
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && not __fish_seen_subcommand_from $nuget_commands" -l version -d "Show version"
## add command
### client-cert command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -s s -l package-source -d "Package source name"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l path -d "Path to certificate file"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l password -d "Password for the certificate"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l store-password-in-clear-text -d "Enable storing password for the certificate"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l store-location -d "Certificate store location"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l store-name -d "Certificate store name"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l find-by -d "Search method to find certificate in certificate store"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l find-value -d "Search the certificate store for the supplied value"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -s f -l force -d "Skip certificate validation"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from client-cert" -l configfile -d "NuGet configuration file"
### source command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from source" -s n -l name -d "Name of the source"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from source" -s u -l username -d "Username to be used"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from source" -s p -l password -d "Password to be used"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from source" -l store-password-in-clear-text -d "Enable storing portable package source credentials"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from source" -l valid-authentication-types -d "Comma-separated list of valid authentication types"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from add && __fish_seen_subcommand_from source" -l configfile -d "NuGet configuration file"
## delete command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from delete" -l force-english-output -d "Run the application with locale set to English"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from delete" -s s -l source -d "Package source to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from delete" -l non-interactive -d "Don't prompt for user input or confirmations"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from delete" -s k -l api-key -d "API key for the server"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from delete" -l no-service-endpoint -d "Doesn't append \"api/v2/package\" to the source URL"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from delete" -l interactive -d "Allow interactive input/action"
## disable command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from disable && __fish_seen_subcommand_from source" -l configfile -d "NuGet configuration file"
## enable command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from enable && __fish_seen_subcommand_from source" -l configfile -d "NuGet configuration file"
## list command
### client-cert command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from list && __fish_seen_subcommand_from client-cert" -l configfile -d "NuGet configuration file"
### source command
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from list && __fish_seen_subcommand_from source" -l format -d "Format of the list command output"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from list && __fish_seen_subcommand_from source" -l configfile -d "NuGet configuration file"
## locals command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from locals" -l force-english-output -d "Run the application with locale set to English"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from locals" -s c -l clear -d "Clear the selected local resources or cache location"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from locals" -s l -l list -d "List the selected local resources or cache location"
## push command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -l force-english-output -d "Run the application with locale set to English"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -s s -l source -d "Package source to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -o ss -l symbol-source -d "Symbol server URL to use"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -s t -l timeout -d "Timeout for pushing to a server"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -s k -l api-key -d "API key for the server"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -o sk -l symbol-api-key -d "API key for the symbol server"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -s d -l disable-buffering -d "Disable buffering when pushing to an HTTP server"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -s n -l no-symbols -d "Doesn't push symbols"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -l no-service-endpoint -d "Doesn't append \"api/v2/package\" to the source URL"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from push" -l skip-duplicate -d "Treat any 409 Conflict response as a warning"
## remove command
### client-cert command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from remove && __fish_seen_subcommand_from client-cert" -s s -l package-source -d "Package source name"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from remove && __fish_seen_subcommand_from client-cert" -l configfile -d "NuGet configuration file"
### source command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from remove && __fish_seen_subcommand_from source" -l configfile -d "NuGet configuration file"
## update command
### client-cert command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -s s -l package-source -d "Package source name"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l path -d "Path to certificate file"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l password -d "Password for the certificate"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l store-password-in-clear-text -d "Enable storing password for the certificate"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l store-location -d "Certificate store location"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l store-name -d "Certificate store name"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l find-by -d "Search method to find certificate in certificate store"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l find-value -d "Search the certificate store for the supplied value"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -s f -l force -d "Skip certificate validation"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from client-cert" -l configfile -d "NuGet configuration file"
### source command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from source" -s s -l source -d "Path to the package source"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from source" -s u -l username -d "Username to be used"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from source" -s p -l password -d "Password to be used"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from source" -l store-password-in-clear-text -d "Enable storing portable package source credentials"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from source" -l valid-authentication-types -d "Comma-separated list of valid authentication types"
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from update && __fish_seen_subcommand_from source" -l configfile -d "NuGet configuration file"
## verify command
complete -f -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from verify" -l all -d "Specify that all verifications possible should be performed"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from verify" -l certificate-fingerprint -d "Verify that the certificate matches with the fingerprints"
complete -x -c dotnet -n "__fish_seen_subcommand_from nuget && __fish_seen_subcommand_from verify" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the verbosity level"

# pack options
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -s o -l output -d "Output directory to place built packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l no-build -d "Don't build the project before packing"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l include-symbols -d "Include packages with symbols in output directory"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l include-source -d "Include PDBs and source files"
complete -x -c dotnet -n "__fish_seen_subcommand_from pack" -s c -l configuration -a "$build_configurations" -d "Configuration to use"
complete -x -c dotnet -n "__fish_seen_subcommand_from pack" -l version-suffix -d "Set the value of the \$(VersionSuffix) property to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -s s -l serviceable -d "Set the serviceable flag in the package"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l nologo -d "Don't display the startup banner or the copyright message"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l no-restore -d "Don't restore the project before building"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
complete -x -c dotnet -n "__fish_seen_subcommand_from pack" -l runtime -d "Target runtime to restore packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l no-dependencies -d "Don't build P2P references"
complete -f -c dotnet -n "__fish_seen_subcommand_from pack" -l force -d "Force all dependencies to be resolved"

# publish options
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -s o -l output -d "Output directory to place the published artifacts"
complete -x -c dotnet -n "__fish_seen_subcommand_from publish" -s f -l framework -d "Target framework to publish"
complete -x -c dotnet -n "__fish_seen_subcommand_from publish" -s r -l runtime -d "Target runtime to publish"
complete -x -c dotnet -n "__fish_seen_subcommand_from publish" -s c -l configuration -a "$build_configurations" -d "Configuration to publish"
complete -x -c dotnet -n "__fish_seen_subcommand_from publish" -l version-suffix -d "Set the value of the \$(VersionSuffix) property to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l manifest -d "Path to a target manifest file"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l no-build -d "Don't build the project before publishing"
complete -x -c dotnet -n "__fish_seen_subcommand_from publish" -l self-contained -d "Publish the .NET runtime with your application"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l no-self-contained -d "Publish your application as a framework dependent application"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l nologo -d "Don't display the startup banner or the copyright message"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l no-restore -d "Don't restore the project before building"
complete -x -c dotnet -n "__fish_seen_subcommand_from publish" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l no-dependencies -d "Don't restore P2P references"
complete -f -c dotnet -n "__fish_seen_subcommand_from publish" -l force -d "Force all dependencies to be resolved"

# remove options
## package command
complete -f -c dotnet -n "__fish_seen_subcommand_from remove && __fish_seen_subcommand_from package" -l interactive -d "Allow interactive input/action"
## reference command
complete -x -c dotnet -n "__fish_seen_subcommand_from remove && __fish_seen_subcommand_from reference" -s f -l framework -d "Remove the reference only when targeting a specific framework"

# restore options
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -s s -l source -d "NuGet package source to use for the restore"
complete -x -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -s r -l runtime -d "Target runtime to restore packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l packages -d "Directory to restore packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l disable-parallel -d "Prevent restoring multiple projects in parallel"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l configfile -d "NuGet configuration file to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l no-cache -d "Don't cache packages and HTTP requests"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l ignore-failed-sources -d "Treat package source failures as warnings"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l no-dependencies -d "Don't restore P2P references"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -s f -l force -d "Force all dependencies to be resolved"
complete -x -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l use-lock-file -d "Enable project lock file to be generated"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l locked-mode -d "Don't allow updating project lock file"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l lock-file-path -d "Output location where project lock file is written"
complete -f -c dotnet -n "__fish_seen_subcommand_from restore && not __fish_seen_subcommand_from tool" -l force-evaluate -d "Force restore to reevaluate all dependencies"

# run options
complete -x -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -s c -l configuration -a "$build_configurations" -d "Configuration to run"
complete -x -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -s f -l framework -d "Target framework to run"
complete -x -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -s r -l runtime -d "Target runtime to run"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -s p -l project -d "Path to the project file to run"
complete -x -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l launch-profile -d "Name of the launch profile to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l no-launch-profile -d "Don't attempt to use launchSettings.json"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l no-build -d "Don't build the project before running"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l interactive -d "Allow interactive input/action"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l no-restore -d "Don't restore the project before building"
complete -x -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l no-dependencies -d "Don't restore P2P references"
complete -f -c dotnet -n "__fish_seen_subcommand_from run && not __fish_seen_subcommand_from tool" -l force -d "Force all dependencies to be resolved"

# sln commands and options
## Commands
set -l sln_commands add list remove
complete -f -c dotnet -n "__fish_seen_subcommand_from sln && not __fish_seen_subcommand_from $sln_commands" -a add -d "Add one/more projects to a solution file"
complete -f -c dotnet -n "__fish_seen_subcommand_from sln && not __fish_seen_subcommand_from $sln_commands" -a list -d "List all projects in a solution file"
complete -f -c dotnet -n "__fish_seen_subcommand_from sln && not __fish_seen_subcommand_from $sln_commands" -a remove -d "Remove one/more projects from a solution file"
## Options
complete -f -c dotnet -n "__fish_seen_subcommand_from sln && __fish_seen_subcommand_from add" -l in-root -d "Place project in root of the solution"
complete -f -c dotnet -n "__fish_seen_subcommand_from sln && __fish_seen_subcommand_from add" -s s -l solution-folder -d "Destination solution folder path to add the projects"

# store options
complete -f -c dotnet -n "__fish_seen_subcommand_from store" -s m -l manifest -d "XML file that contains the list of packages to be stored"
complete -x -c dotnet -n "__fish_seen_subcommand_from store" -s f -l framework -d "Target framework to store packages"
complete -x -c dotnet -n "__fish_seen_subcommand_from store" -l framework-version -d "Specify the .NET SDK version"
complete -x -c dotnet -n "__fish_seen_subcommand_from store" -s r -l runtime -d "Target runtime to store packages"
complete -f -c dotnet -n "__fish_seen_subcommand_from store" -s o -l output -d "Output directory to store the given assemblies"
complete -f -c dotnet -n "__fish_seen_subcommand_from store" -s w -l working-dir -d "Working directory used by the command"
complete -f -c dotnet -n "__fish_seen_subcommand_from store" -l skip-optimization -d "Skip the optimization phase"
complete -f -c dotnet -n "__fish_seen_subcommand_from store" -l skip-symbols -d "Skip creating symbol files"
complete -x -c dotnet -n "__fish_seen_subcommand_from store" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"

# test options
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -s s -l settings -d "Settings file to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -s t -l list-tests -d "List the discovered tests"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -l filter -d "Run tests that match the given expression"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -s a -l test-adapter-path -d "Path to the custom adapters to use"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -s l -l logger -d "Logger to use for test results"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -s c -l configuration -a "$build_configurations" -d "Configuration to use"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -s f -l framework -d "Target framework to run tests"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -l runtime -d "Target runtime to test"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -s o -l output -d "Output directory to place built artifacts"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -s d -l diag -d "Enable verbose logging to the specified file"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l no-build -d "Don't build the project before testing"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -s r -l results-directory -d "Directory where the test results will be placed"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -l collect -d "Enable data collector for the test run"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l blame -d "Run the tests in blame mode"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l blame-crash -d "Run the tests in blame mode and enables collecting crash dump"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -l blame-crash-dump-type -d "Type of crash dump to be collected"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l blame-crash-collect-always -d "Enable collecting crash dump on expected"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l blame-hang -d "Run the tests in blame mode and enables collecting hang dump"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -l blame-hang-dump-type -d "Type of crash dump to be collected"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -l blame-hang-timeout -d "Per-test timeout"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l nologo -d "Run tests without displaying the Microsoft TestPlatform banner"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l no-restore -d "Don't restore the project before building"
complete -f -c dotnet -n "__fish_seen_subcommand_from test" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from test" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"

# tool options
## install command
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -s g -l global -d "Specify that the installation is user-wide"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l local -d "Specify a local tool installation"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l tool-path -d "Directory where the tool will be installed"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l version -d "Version of the tool to install"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l configfile -d "NuGet configuration file to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l tool-manifest -d "Path to the manifest file"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l add-source -d "Add an additional NuGet package source to use during installation"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l framework -d "Target framework to install the tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l disable-parallel -d "Prevent restoring multiple projects in parallel"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l ignore-failed-sources -d "Treat package source failures as warnings"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l no-cache -d "Don't cache packages and HTTP requests"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from install" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
## uninstall command
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from uninstall" -s g -l global -d "Specify that the tool to be removed is from a user-wide installation"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from uninstall" -l local -d "Specify that the tool to be removed is a local tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from uninstall" -l tool-path -d "Directory containing the tool to uninstall"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from uninstall" -l tool-manifest -d "Path to the manifest file"
## update command
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -s g -l global -d "Specify that the update is for a user-wide tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l tool-path -d "Directory containing the tool to update"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l local -d "Specify that the tool to be updated is a local tool"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l configfile -d "NuGet configuration file to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l add-source -d "Add an additional NuGet package source to use during the update"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l framework -d "Target framework to update the tool"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l version -d "Version range of the tool package to update"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l tool-manifest -d "Path to the manifest file"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l disable-parallel -d "Prevent restoring multiple projects in parallel"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l ignore-failed-sources -d "Treat package source failures as warnings"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l no-cache -d "Don't cache packages and HTTP requests"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from update" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
## list command
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from list" -s g -l global -d "List user-wide global tools"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from list" -l local -d "List local tools"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from list" -l tool-path -d "Directory containing the tools to list"
## search command
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from search" -l detail -d "Show detail result of the query"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from search" -l skip -d "Specify the number of query results to skip"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from search" -l take -d "Specify the number of query results to show"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from search" -l prerelease -d "Include pre-release packages"
## restore command
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l configfile -d "NuGet configuration file to use"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l add-source -d "Add an additional NuGet package source to use during installation"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l tool-manifest -d "Path to the manifest file"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l disable-parallel -d "Prevent restoring multiple projects in parallel"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l ignore-failed-sources -d "Treat package source failures as warnings"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l no-cache -d "Don't cache packages and HTTP requests"
complete -f -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -l interactive -d "Allow interactive input/action"
complete -x -c dotnet -n "__fish_seen_subcommand_from tool && __fish_seen_subcommand_from restore" -s v -l verbosity -a "$msbuild_verbosity_levels" -d "Set the MSBuild verbosity level"
