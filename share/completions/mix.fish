# Completions for the Elixir build tool mix

function __fish_mix_needs_command
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 ]
    return 0
  end
  return 1
end

function __fish_mix_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end


complete -f -c mix -n '__fish_mix_needs_command' -a app.start         -d "Starts all registered apps"
complete -f -c mix -n '__fish_mix_needs_command' -a archive           -d "Lists all archives"
complete -f -c mix -n '__fish_mix_needs_command' -a archive.build     -d "Archives this project into a .ez file"
complete    -c mix -n '__fish_mix_needs_command' -a archive.install   -d "Installs an archive locally"
complete    -c mix -n '__fish_mix_needs_command' -a archive.uninstall -d "Uninstalls archives"
complete -f -c mix -n '__fish_mix_needs_command' -a clean             -d "Deletes generated application files"
complete -f -c mix -n '__fish_mix_needs_command' -a cmd               -d "Executes the given command"
complete    -c mix -n '__fish_mix_needs_command' -a compile           -d "Compiles source files"
complete -f -c mix -n '__fish_mix_needs_command' -a deps              -d "Lists dependencies and their status"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.clean        -d "Deletes the given dependencies' files"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.compile      -d "Compiles dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.get          -d "Gets all out of date dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.unlock       -d "Unlocks the given dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.update       -d "Updates the given dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a do                -d "Executes the tasks separated by comma"
complete -f -c mix -n '__fish_mix_needs_command' -a escript.build     -d "Builds an escript for the project"
complete -f -c mix -n '__fish_mix_needs_command' -a help              -d "Prints help information for tasks"
complete    -c mix -n '__fish_mix_needs_command' -a loadconfig        -d "Loads and persists the given configuration"
complete -f -c mix -n '__fish_mix_needs_command' -a local             -d "Lists local tasks"
complete -f -c mix -n '__fish_mix_needs_command' -a local.hex         -d "Installs Hex locally"
complete -f -c mix -n '__fish_mix_needs_command' -a local.public_keys -d "Manages public keys"
complete -f -c mix -n '__fish_mix_needs_command' -a local.rebar       -d "Installs rebar locally"
complete    -c mix -n '__fish_mix_needs_command' -a new               -d "Creates a new Elixir project"
complete    -c mix -n '__fish_mix_needs_command' -a profile.fprof     -d "Profiles the given file or expression with fprof"
complete -f -c mix -n '__fish_mix_needs_command' -a run               -d "Runs the given file or expression"
complete -f -c mix -n '__fish_mix_needs_command' -a test              -d "Runs a project's tests"

# archive.build subcommand
complete    -c mix -n '__fish_mix_using_command archive.build' -s i -d "specify input directory"
complete -f -c mix -n '__fish_mix_using_command archive.build' -s o -d "specify output file name"
complete -f -c mix -n '__fish_mix_using_command archive.build' -l no-compile -d "skip compilation"

# clean subcommand
complete -f -c mix -n '__fish_mix_using_command clean' -l all -d "Clean everything, including dependencies"

# escript.build subcommand
complete -f -c mix -n '__fish_mix_using_command escript.build' -l force -d "forces compilation regardless of modification times"
complete -f -c mix -n '__fish_mix_using_command escript.build' -l no-compile -d "skips compilation to .beam files"

# new subcommand
complete -f -c mix -n '__fish_mix_using_command new' -l sup      -d "can be given to generate an OTP application skeleton including a supervision tree"
complete -f -c mix -n '__fish_mix_using_command new' -l umbrella -d "can be given to generate an umbrella project"
complete -f -c mix -n '__fish_mix_using_command new' -l app      -d "can be given in order to name the OTP application"
complete -f -c mix -n '__fish_mix_using_command new' -l module   -d "can be given in order to name the modules in the generated code skeleton"

# run subcommand
complete -c mix -n '__fish_mix_using_command run' -l config -s c -d "loads the given configuration file"
complete -c mix -n '__fish_mix_using_command run' -l eval -s e -d "evaluates the given code"
complete -c mix -n '__fish_mix_using_command run' -l require -s r -d "requires pattern before running the command"
complete -c mix -n '__fish_mix_using_command run' -l parallel-require -s pr -d "requires pattern in parallel"
complete -c mix -n '__fish_mix_using_command run' -l no-compile -d "does not compile even if files require compilation"
complete -c mix -n '__fish_mix_using_command run' -l no-deps-check -d "does not check dependencies"
complete -c mix -n '__fish_mix_using_command run' -l no-halt -d "does not halt the system after running the command"
complete -c mix -n '__fish_mix_using_command run' -l no-start -d "does not start applications after compilation"

# test subcommand
complete -c mix -n '__fish_mix_using_command test' -l trace -d "run tests with detailed reporting; automatically sets `--max-cases` to 1"
complete -c mix -n '__fish_mix_using_command test' -l max-cases -d "set the maximum number of cases running async"
complete -c mix -n '__fish_mix_using_command test' -l cover -d "the directory to include coverage results"
complete -c mix -n '__fish_mix_using_command test' -l force -d "forces compilation regardless of modification times"
complete -c mix -n '__fish_mix_using_command test' -l no-compile -d "do not compile, even if files require compilation"
complete -c mix -n '__fish_mix_using_command test' -l no-start -d "do not start applications after compilation"
complete -c mix -n '__fish_mix_using_command test' -l no-color -d "disable color in the output"
complete -c mix -n '__fish_mix_using_command test' -l color -d "enable color in the output"
complete -c mix -n '__fish_mix_using_command test' -l include -d "include tests that match the filter"
complete -c mix -n '__fish_mix_using_command test' -l exclude -d "exclude tests that match the filter"
complete -c mix -n '__fish_mix_using_command test' -l only -d "run only tests that match the filter"
complete -c mix -n '__fish_mix_using_command test' -l seed -d "seeds the random number generator used to randomize test order"
complete -c mix -n '__fish_mix_using_command test' -l timeout -d "set the timeout for the tests"

# help subcommand
complete -f -c mix -n '__fish_mix_using_command help' -a app.start         -d "Starts all registered apps"
complete -f -c mix -n '__fish_mix_using_command help' -a archive           -d "Lists all archives"
complete -f -c mix -n '__fish_mix_using_command help' -a archive.build     -d "Archives this project into a .ez file"
complete -f -c mix -n '__fish_mix_using_command help' -a archive.install   -d "Installs an archive locally"
complete -f -c mix -n '__fish_mix_using_command help' -a archive.uninstall -d "Uninstalls archives"
complete -f -c mix -n '__fish_mix_using_command help' -a clean             -d "Deletes generated application files"
complete -f -c mix -n '__fish_mix_using_command help' -a cmd               -d "Executes the given command"
complete -f -c mix -n '__fish_mix_using_command help' -a compile           -d "Compiles source files"
complete -f -c mix -n '__fish_mix_using_command help' -a deps              -d "Lists dependencies and their status"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.clean        -d "Deletes the given dependencies' files"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.compile      -d "Compiles dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.get          -d "Gets all out of date dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.unlock       -d "Unlocks the given dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.update       -d "Updates the given dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a do                -d "Executes the tasks separated by comma"
complete -f -c mix -n '__fish_mix_using_command help' -a escript.build     -d "Builds an escript for the project"
complete -f -c mix -n '__fish_mix_using_command help' -a help              -d "Prints help information for tasks"
complete -f -c mix -n '__fish_mix_using_command help' -a loadconfig        -d "Loads and persists the given configuration"
complete -f -c mix -n '__fish_mix_using_command help' -a local             -d "Lists local tasks"
complete -f -c mix -n '__fish_mix_using_command help' -a local.hex         -d "Installs Hex locally"
complete -f -c mix -n '__fish_mix_using_command help' -a local.public_keys -d "Manages public keys"
complete -f -c mix -n '__fish_mix_using_command help' -a local.rebar       -d "Installs rebar locally"
complete -f -c mix -n '__fish_mix_using_command help' -a new               -d "Creates a new Elixir project"
complete -f -c mix -n '__fish_mix_using_command help' -a profile.fprof     -d "Profiles the given file or expression with fprof"
complete -f -c mix -n '__fish_mix_using_command help' -a run               -d "Runs the given file or expression"
complete -f -c mix -n '__fish_mix_using_command help' -a test              -d "Runs a project's tests"
