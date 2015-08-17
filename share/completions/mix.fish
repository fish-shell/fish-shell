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


complete -f -c mix -n '__fish_mix_needs_command' -a archive         -d "Archive this project into a .ez file"
complete -f -c mix -n '__fish_mix_needs_command' -a clean           -d "Clean generated application files"
complete -f -c mix -n '__fish_mix_needs_command' -a cmd             -d "Executes the given command"
complete    -c mix -n '__fish_mix_needs_command' -a compile         -d "Compile source files"
complete -f -c mix -n '__fish_mix_needs_command' -a deps            -d "List dependencies and their status"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.clean      -d "Remove the given dependencies' files"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.compile    -d "Compile dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.get        -d "Get all out of date dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.unlock     -d "Unlock the given dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a deps.update     -d "Update the given dependencies"
complete -f -c mix -n '__fish_mix_needs_command' -a do              -d "Executes the tasks separated by comma"
complete -f -c mix -n '__fish_mix_needs_command' -a escriptize      -d "Generates an escript for the project"
complete -f -c mix -n '__fish_mix_needs_command' -a help            -d "Print help information for tasks"
complete    -c mix -n '__fish_mix_needs_command' -a local           -d "List local tasks"
complete    -c mix -n '__fish_mix_needs_command' -a local.install   -d "Install a task or an archive locally"
complete    -c mix -n '__fish_mix_needs_command' -a local.rebar     -d "Install rebar locally"
complete    -c mix -n '__fish_mix_needs_command' -a local.uninstall -d "Uninstall local tasks or archives"
complete    -c mix -n '__fish_mix_needs_command' -a new             -d "Creates a new Elixir project"
complete -f -c mix -n '__fish_mix_needs_command' -a run             -d "Run the given file or expression"
complete -f -c mix -n '__fish_mix_needs_command' -a test            -d "Run a project's tests"

# archive subcommand
complete -f -c mix -n '__fish_mix_using_command archive' -s o -d "specify output file name"
complete -f -c mix -n '__fish_mix_using_command archive' -l no-compile -d "skip compilation"

# clean subcommand
complete -f -c mix -n '__fish_mix_using_command clean' -l all -d "Clean everything, including dependencies"

# escriptize subcommand
complete -f -c mix -n '__fish_mix_using_command escriptize' -l force -d "forces compilation regardless of modification times"
complete -f -c mix -n '__fish_mix_using_command escriptize' -l no-compile -d "skips compilation to .beam files"

# new subcommand
complete -f -c mix -n '__fish_mix_using_command new' -l bare -d "can be given to not generate an OTP application skeleton"
complete -f -c mix -n '__fish_mix_using_command new' -l module
complete -f -c mix -n '__fish_mix_using_command new' -l umbrella -d "can be given to generate an umbrella project"

# run subcommand
complete -c mix -n '__fish_mix_using_command run' -l eval -s e -d "Evaluates the given code"
complete -c mix -n '__fish_mix_using_command run' -l require -s r -d "Requires pattern before running the command"
complete -c mix -n '__fish_mix_using_command run' -l parallel-require -s pr -d "Requires pattern in parallel"
complete -c mix -n '__fish_mix_using_command run' -l no-halt -d "Does not halt the system after running the command"
complete -c mix -n '__fish_mix_using_command run' -l no-compile -d "Does not compile even if files require compilation"
complete -c mix -n '__fish_mix_using_command run' -l no-start -d "Does not start applications after compilation"

# test subcommand
complete -c mix -n '__fish_mix_using_command test' -l trace -d "run tests with detailed reporting. Automatically sets `--max-cases` to 1"
complete -c mix -n '__fish_mix_using_command test' -l max-cases -d "set the maximum number of cases running async"
complete -c mix -n '__fish_mix_using_command test' -l cover -d "the directory to include coverage results"
complete -c mix -n '__fish_mix_using_command test' -l force -d "forces compilation regardless of modification times"
complete -c mix -n '__fish_mix_using_command test' -l no-compile -d "do not compile, even if files require compilation"
complete -c mix -n '__fish_mix_using_command test' -l no-start -d "do not start applications after compilation"
complete -c mix -n '__fish_mix_using_command test' -l no-color -d "disable color in the output"

# help subcommand
complete -f -c mix -n '__fish_mix_using_command help' -a archive         -d "Archive this project into a .ez file"
complete -f -c mix -n '__fish_mix_using_command help' -a clean           -d "Clean generated application files"
complete -f -c mix -n '__fish_mix_using_command help' -a cmd             -d "Executes the given command"
complete -f -c mix -n '__fish_mix_using_command help' -a compile         -d "Compile source files"
complete -f -c mix -n '__fish_mix_using_command help' -a deps            -d "List dependencies and their status"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.clean      -d "Remove the given dependencies' files"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.compile    -d "Compile dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.get        -d "Get all out of date dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.unlock     -d "Unlock the given dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a deps.update     -d "Update the given dependencies"
complete -f -c mix -n '__fish_mix_using_command help' -a do              -d "Executes the tasks separated by comma"
complete -f -c mix -n '__fish_mix_using_command help' -a escriptize      -d "Generates an escript for the project"
complete -f -c mix -n '__fish_mix_using_command help' -a local           -d "List local tasks"
complete -f -c mix -n '__fish_mix_using_command help' -a local.install   -d "Install a task or an archive locally"
complete -f -c mix -n '__fish_mix_using_command help' -a local.rebar     -d "Install rebar locally"
complete -f -c mix -n '__fish_mix_using_command help' -a local.uninstall -d "Uninstall local tasks or archives"
complete -f -c mix -n '__fish_mix_using_command help' -a new             -d "Creates a new Elixir project"
complete -f -c mix -n '__fish_mix_using_command help' -a run             -d "Run the given file or expression"
complete -f -c mix -n '__fish_mix_using_command help' -a test            -d "Run a project's tests"
