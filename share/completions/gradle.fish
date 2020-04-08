# Gradle is a build system.
# See: https://gradle.org

function __contains_gradle_build
    test -f build.gradle -o -f build.gradle.kts
end

function __create_completion_cache_file
    # Set up cache directory
    if test -z $XDG_CACHE_HOME
        set XDG_CACHE_HOME $HOME/.cache/
    end
    mkdir -m 700 -p $XDG_CACHE_HOME/gradle-completions

    string trim -- $XDG_CACHE_HOME/gradle-completions/(__fish_md5 -s $argv[1] | string split ' = ')[2]
end

##############################
# Configure Tasks Completion #
##############################

# Outside of a Project
function __get_gradle_default_task_completion
    if __contains_gradle_build
        return
    end

    printf '%s\t%s\n' \
        "buildEnvironment" "Displays all buildscript dependencies declared in root project." \
        "components" "Displays the components produced by root project." \
        "dependencies" "Displays all dependencies declared in root project." \
        "dependencyInsight" "Displays the insight into a specific dependency in root project." \
        "dependentComponents" "Displays the dependent components of components in root project." \
        "help" "Displays a help message." \
        "init" "Initializes a new Gradle build." \
        "model" "Displays the configuration model of root project." \
        "projects" "Displays the sub-projects of root project." \
        "properties" "Displays the properties of root project." \
        "tasks" "Displays the tasks runnable from root project." \
        "wrapper" "Generates Gradle wrapper files."
end

complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --exclusive \
    --arguments "(__get_gradle_default_task_completion)"

# Inside of a Project
function __get_gradle_task_completion
    if not __contains_gradle_build
        return
    end

    set -l gradle_cache_file (__create_completion_cache_file "{$PWD}-tasks")
    if not command test -f $gradle_cache_file -a -s $gradle_cache_file
        command gradle -q tasks --all 2>/dev/null | string match --regex '^[a-z][A-z:]+.*' | string replace ' - ' \t >$gradle_cache_file
    end

    # return possible tasks
    string trim -- <$gradle_cache_file
end

complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --condition "__contains_gradle_build" \
    --exclusive \
    --arguments "(__get_gradle_task_completion)"


###############################
# Configure Option Completion #
###############################

function __get_console_completion
    printf '%s\t%s\n' \
        "auto" "Use 'rich' in console otherwise 'plain'" \
        "plain" "Disable color and rich output." \
        "rich" "Enable color and rich output." \
        "verbose" "Enable color, rich output, output task names and outcomes at the lifecycle log level"
end

function __get_property_completion
    printf '%s\t%s\n' \
        "org.gradle.cache.reserved.mb" "Reserve Gradle Daemon memory for operations." \
        "org.gradle.caching" "Set true to enable Gradle build cache." \
        "org.gradle.console" "Set type of console output to generate (plain auto rich verbose)." \
        "org.gradle.daemon.debug" "Set true to debug Gradle Daemon." \
        "org.gradle.daemon.idletimeout" "Kill Gradle Daemon after" \
        "org.gradle.debug" "Set true to debug Gradle Client." \
        "org.gradle.jvmargs" "Set JVM arguments." \
        "org.gradle.java.home" "Set JDK home dir." \
        "org.gradle.logging.level" "Set default Gradle log level (quiet warn lifecycle info debug)." \
        "org.gradle.parallel" "Set true to enable parallel project builds (incubating)." \
        "org.gradle.priority" "Set priority for Gradle worker processes (low normal)." \
        "org.gradle.warning.mode" "Set types of warnings to log (all summary none)." \
        "org.gradle.workers.max" "Set the number of workers Gradle is allowed to use."
end

function __get_priority_completion
    printf '%s\t%s\n' \
        "normal" "Set the default process priority." \
        "low" "Set a low process priority."
end

function __get_warning_mode_completion
    printf '%s\t%s\n' \
        "all" "Log all warnings." \
        "summary" "Suppress all warnings and log a summary at the end of the build." \
        "fail" "Log all warnings and fail the build if there are any warnings." \
        "none" "Suppress all warnings, including the summary at the end of the build."
end

complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'help' \
    --short-option 'h' --short-option '?' \
    --description 'Shows this help message.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'no-rebuild' \
    --short-option 'a' \
    --description 'Do not rebuild project dependencies.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'build-file' \
    --short-option 'b' \
    --require-parameter \
    --description 'Specify the build file.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'build-cache' \
    --description 'Enables the Gradle build cache. Gradle will try to reuse outputs from previous builds.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'settings-file' \
    --short-option 'c' \
    --require-parameter \
    --description 'Specify the settings file.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'configure-on-demand' \
    --description 'Configure necessary projects only. Gradle will attempt to reduce configuration time for large multi-project builds. [incubating]'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'console' \
    --exclusive \
    --description 'Specifies which type of console output to generate.' \
    --arguments "(__get_console_completion)"
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'continue' \
    --description 'Continue task execution after a task failure.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'system-prop' \
    --short-option 'D' \
    --exclusive \
    --description 'Set system property of the JVM (e.g. -Dmyprop=myvalue).' \
    --arguments "(__get_property_completion)"
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'debug' \
    --short-option 'd' \
    --description 'Log in debug mode (includes normal stacktrace).'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'daemon' \
    --description 'Uses the Gradle Daemon to run the build.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'foreground' \
    --description 'Starts the Gradle Daemon in the foreground.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'gradle-user-home' \
    --short-option 'g' \
    --require-parameter \
    --description 'Specifies the gradle user home directory.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'init-script' \
    --short-option 'I' \
    --require-parameter \
    --description 'Specify an initialization script.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'info' \
    --short-option 'i' \
    --description 'Set log level to info.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'include-build' \
    --require-parameter \
    --description 'Include the specified build in the composite.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'dry-run' \
    --short-option 'm' \
    --description 'Run the builds with all task actions disabled.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'max-workers' \
    --exclusive \
    --description 'Configure the number of concurrent workers Gradle is allowed to use.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'no-build-cache' \
    --description 'Disables the Gradle build cache.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'no-configure-on-demand' \
    --description 'Disables the use of configuration on demand. [incubating]'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'no-daemon' \
    --description 'Do not use the Gradle daemon to run the build.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'no-parallel' \
    --description 'Disables parallel execution to build projects.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'no-scan' \
    --description 'Disables the creation of a build scan.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'offline' \
    --description 'Execute the build without accessing network resources.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'project-prop' \
    --short-option 'P' \
    --exclusive \
    --description 'Set project property for the build script (e.g. -Pmyprop=myvalue).'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'project-dir' \
    --short-option 'p' \
    --require-parameter \
    --description 'Specifies the start directory for Gradle. Defaults to current directory.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'parallel' \
    --description 'Build projects in parallel.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'priority' \
    --exclusive \
    --description 'Specifies the scheduling priority for the Gradle daemon and all processes launched by it.' \
    --arguments "(__get_priority_completion)"
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'profile' \
    --description 'Profile build execution time and generates a report in the <build_dir>/reports/profile directory.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'project-cache-dir' \
    --require-parameter \
    --description 'Specify the project-specific cache directory.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'quiet' \
    --short-option 'q' \
    --description 'Log errors only.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'refresh-dependencies' \
    --description 'Refresh the state of dependencies.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'rerun-tasks' \
    --description 'Ignore previously cached task results.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'full-stacktrace' \
    --short-option 'S' \
    --description 'Print out the full (very verbose) stacktrace for all exceptions.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'stacktrace' \
    --short-option 's' \
    --description 'Print out the stacktrace for all exceptions.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'scan' \
    --description 'Creates a build scan.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'status' \
    --description 'Shows status of running and recently stopped Gradle Daemon(s).'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'stop' \
    --description 'Stops the Gradle Daemon if it is running.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'continuous' \
    --short-option 't' \
    --description 'Enables continuous build.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'update-locks' \
    --description 'Perform a partial update of the dependency lock, letting passed in module notations change version. [incubating]'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'version' \
    --short-option 'v' \
    --description 'Print version info.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'warn' \
    --short-option 'w' \
    --description 'Set log level to warn.'
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'warning-mode' \
    --description 'Specifies which mode of warnings to generate.' \
    --arguments "(__get_warning_mode_completion)"
complete --command 'gw' --command 'gradle' --command 'gradlew' \
    --long-option 'write-locks' \
    --description 'Persists dependency resolution for locked configurations, ignoring existing locking information if it exists [incubating]'
