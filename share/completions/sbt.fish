############
# COMMANDS #
############

# 1. In general it's not recommended to run commands one by one, because sbt startup is quite slow
# 2. Only commands without arguments are completed, because any commands with args have to be quoted: `sbt "show key"`
# 3. Only those commands are completed that could be useful to run as one off, like `sbt new ...`, the rest should be run in an interactive sbt shell
# 4. Tasks is somewhat dynamic and depends on the project definition, so only most common are offered
# 5. Same about settings (none are offered)

# These commands can be combined in any order
complete -c sbt -f -a '(string split "\n" "
    about            Display basic information about sbt and the build
    clean            Delete files produced by the build
    compile          Compile sources
    console          Scala REPL: project classes
    consoleQuick     Scala REPL: only project dependencies
    doc              Generate API documentation
    help             Display help message
    package          Produce the main artifact
    plugins          List currently available plugins
    projects         List the names of available projects
    publish          Publish artifacts to a repository
    publishLocal     Publish artifacts to the local Ivy repository
    publishM2        Publish artifacts to the local Maven repository
    run              Run a main class
    settings         List the settings defined for the current project
    shell            Launch an interactive sbt prompt
    tasks            List the tasks defined for the current project
    test             Execute all tests
" | string trim | string replace -r "\s+" "\t")' \
    -n 'not contains -- "new" (commandline -cpx);
    and not contains -- "client" (commandline -cpx)'

# These cannot be combined with any other commands and require an argument
complete -c sbt -f -n 'test  (count (commandline -cpx)) = 1 ' -a new -d 'Create a new sbt project from the given template'
complete -c sbt -f -n 'test  (count (commandline -cpx)) = 1 ' -a client -d 'Connect to a server with an interactive sbt prompt'

###########
# OPTIONS #
###########

# This is based on the output of `sbt -help`:

# general options without arguments
complete -c sbt -o help -s h -f -d "Print options help message"
complete -c sbt -o verbose -s v -f -d "Print more details"
complete -c sbt -o debug -s d -f -d "Set log level to debug"
complete -c sbt -o no-colors -f -d "Disable ANSI color codes"
complete -c sbt -o sbt-create -f -d "Launch even if there's no sbt project"
complete -c sbt -o no-share -f -d "Use all local caches"
complete -c sbt -o no-global -f -d "Use global caches, but not global ~/.sbt directory"
complete -c sbt -o batch -f -d "Disable interactive mode"

# general options with arguments
complete -c sbt -o sbt-dir -d "Specify path to global settings/plugins" -r # path
complete -c sbt -o sbt-boot -d "Specify path to shared boot directory" -r # path
complete -c sbt -o ivy -d "Specify path to local Ivy repository" -r # path
complete -c sbt -o mem -d "Set memory options" -x # integer? (default: -Xms1024m -Xmx1024m -XX:ReservedCodeCacheSize=128m -XX:MaxPermSize=256m)
complete -c sbt -o port -d "Turn on JVM debugging, open at the given port" -r # port

# sbt version
complete -c sbt -o sbt-version -d "Use specified version of sbt" -x -a '0.13.(seq 0 6)\t"" 1.0.0\t""'
complete -c sbt -o sbt-jar -d "Use specified jar as the sbt launcher" -r # jar path
complete -c sbt -o sbt-rc -d "Use an RC version of sbt" -f
complete -c sbt -o sbt-snapshot -d "Use a snapshot version of sbt" -f

# java-related
complete -c sbt -o java-home -d "Alternate JAVA_HOME" -r # path
complete -c sbt -o D -d "Pass -D option directly to the Java runtime" -x # -Dkey=val
complete -c sbt -o J-X -d "Pass -X option directly to the Java runtime" -x # -X*
complete -c sbt -o S-X -d "Pass -X option to sbt's scalacOptions" -x # -X*
# TODO: list available -X options if it's possible to do in an automatic way
