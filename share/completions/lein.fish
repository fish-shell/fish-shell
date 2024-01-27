function __fish_lein_needs_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

complete -f -c lein -n __fish_lein_needs_command -a check -d "Check syntax and warn on reflection."
complete -f -c lein -n __fish_lein_needs_command -a classpath -d "Write the classpath of the current project to output-file."
complete -f -c lein -n __fish_lein_needs_command -a clean -d "Remove all files from project's target-path."
complete -f -c lein -n __fish_lein_needs_command -a compile -d "Compile Clojure source into .class files."
complete -f -c lein -n __fish_lein_needs_command -a deploy -d "Build jar and deploy to remote repository."
complete -f -c lein -n __fish_lein_needs_command -a deps -d "Show details about dependencies."
complete -f -c lein -n __fish_lein_needs_command -a do -d "Higher-order task to perform other tasks in succession."
complete -f -c lein -n __fish_lein_needs_command -a help -d "Display a list of tasks or help for a given task or subtask."
complete -f -c lein -n __fish_lein_needs_command -a install -d "Install current project to the local repository."
complete -f -c lein -n __fish_lein_needs_command -a jar -d "Package up all the project's files into a jar file."
complete -f -c lein -n __fish_lein_needs_command -a javac -d "Compile Java source files."
complete -f -c lein -n __fish_lein_needs_command -a light -d "Start a Light Table client for this project"
complete -f -c lein -n __fish_lein_needs_command -a new -d "Generate scaffolding for a new project based on a template."
complete -f -c lein -n __fish_lein_needs_command -a plugin -d "DEPRECATED. Please use the :user profile instead."
complete -f -c lein -n __fish_lein_needs_command -a pom -d "Write a pom.xml file to disk for Maven interoperability."
complete -f -c lein -n __fish_lein_needs_command -a repl -d "Start a repl session either with the current project or standalone."
complete -f -c lein -n __fish_lein_needs_command -a retest -d "Run only the test namespaces which failed last time around."
complete -f -c lein -n __fish_lein_needs_command -a run -d "Run the project's -main function."
complete -f -c lein -n __fish_lein_needs_command -a search -d "Search remote maven repositories for matching jars."
complete -f -c lein -n __fish_lein_needs_command -a show-profiles -d "List all available profiles or display one if given an argument."
complete -f -c lein -n __fish_lein_needs_command -a test -d "Run the project's tests."
complete -f -c lein -n __fish_lein_needs_command -a trampoline -d "Run a task without nesting the project's JVM inside Leiningen's."
complete -f -c lein -n __fish_lein_needs_command -a uberjar -d "Package up the project files and all dependencies into a jar file."
complete -f -c lein -n __fish_lein_needs_command -a update-in -d "Perform arbitrary transformations on your project map."
complete -f -c lein -n __fish_lein_needs_command -a upgrade -d "Upgrade Leiningen to specified version or latest stable."
complete -f -c lein -n __fish_lein_needs_command -a version -d "Print version for Leiningen and the current JVM."
complete -f -c lein -n __fish_lein_needs_command -a with-profile -d "Apply the given task with the profile(s) specified."
