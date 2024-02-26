# Gradle is a build system.
# See: https://gradle.org

function __fish_gradle_contains_build_file
    test -f build.gradle -o -f build.gradle.kts
end

function __fish_gradle_create_completion_cache_file
    set -l xdg_cache_home $XDG_CACHE_HOME
    # Set up cache directory
    if test -z "$xdg_cache_home"
        set xdg_cache_home $HOME/.cache
    end
    mkdir -m 700 -p "$xdg_cache_home/gradle-completions"

    set -l md5Hash (__fish_md5 -s $argv[1] | string replace -r '.* = ' '')
    string trim -- "$xdg_cache_home/gradle-completions/$md5Hash"
end

##############################
# Configure Tasks Completion #
##############################

# Outside of a Project
function __fish_gradle_get_default_task_completion
    if __fish_gradle_contains_build_file
        return
    end

    printf '%s\t%s\n' \
        buildEnvironment "Display buildscript dependencies" \
        components "Display components" \
        dependencies "Display dependencies" \
        dependencyInsight "Display insight of a given dependency" \
        dependentComponents "Display the dependent components" \
        help "Display help message" \
        init "Initialize new Gradle project" \
        model "Display configuration model" \
        projects "Display sub-projects" \
        properties "Display properties" \
        tasks "Display tasks" \
        wrapper "Generate Gradle wrapper files"
end

complete --command gw --command gradle --command gradlew \
    --exclusive \
    --arguments "(__fish_gradle_get_default_task_completion)"

# Inside of a Project
function __fish_gradle_get_task_completion
    if not __fish_gradle_contains_build_file
        return
    end

    set -l gradle_cache_file (__fish_gradle_create_completion_cache_file "{$PWD}-tasks")
    if not test -f "$gradle_cache_file" -a -s "$gradle_cache_file"
        command gradle -q tasks --all 2>/dev/null | string match --regex '^(?!-)[A-Za-z0-9:-]+(?: - .*)?$' | string replace ' - ' \t >"$gradle_cache_file"
    end

    # return possible tasks
    string trim -- <"$gradle_cache_file"
end

complete --command gw --command gradle --command gradlew \
    --condition __fish_gradle_contains_build_file \
    --exclusive \
    --arguments "(__fish_gradle_get_task_completion)"

###############################
# Configure Option Completion #
###############################

function __fish_gradle_get_console_completion
    printf '%s\t%s\n' \
        auto "Use 'rich' in console otherwise 'plain'" \
        plain "Disable color and rich output" \
        rich "Enable color and rich output" \
        verbose "Enable color, rich output, output task names and outcomes at the lifecycle log level"
end

function __fish_gradle_get_property_completion
    printf '%s\t%s\n' \
        "org.gradle.cache.reserved.mb" "Reserve Gradle Daemon memory for operations" \
        "org.gradle.caching" "Enable Gradle build cache" \
        "org.gradle.console" "Set type of console output to generate (plain, auto, rich, verbose)" \
        "org.gradle.daemon.debug" "Enable debug Gradle Daemon" \
        "org.gradle.daemon.idletimeout" "Kill Gradle Daemon after" \
        "org.gradle.debug" "Enable debug Gradle Client" \
        "org.gradle.jvmargs" "Set JVM arguments" \
        "org.gradle.java.home" "Set JDK home dir" \
        "org.gradle.logging.level" "Set default Gradle log level (quiet, warn, lifecycle, info, debug)" \
        "org.gradle.parallel" "Enable parallel project builds (incubating)" \
        "org.gradle.priority" "Set priority for Gradle worker processes (low, normal)" \
        "org.gradle.warning.mode" "Set types of warnings to log (all, summary, fail, none)" \
        "org.gradle.workers.max" "Set the number of workers Gradle is allowed to use"
end

function __fish_gradle_get_priority_completion
    printf '%s\t%s\n' \
        normal "Default process priority" \
        low "Low process priority"
end

function __fish_gradle_get_warning_mode_completion
    printf '%s\t%s\n' \
        all "Log all warnings" \
        summary "Suppress all warnings, log a summary at the end" \
        fail "Log all warnings and fail the build if there are any" \
        none "Suppress all warnings, including the summary at the end"
end

complete --command gw --command gradle --command gradlew \
    --long-option help \
    --short-option h --short-option '?' \
    --description 'Show this help message'
complete --command gw --command gradle --command gradlew \
    --long-option no-rebuild \
    --short-option a \
    --description 'Do not rebuild project dependencies'
complete --command gw --command gradle --command gradlew \
    --long-option build-file \
    --short-option b \
    --require-parameter \
    --description 'Specify build file'
complete --command gw --command gradle --command gradlew \
    --long-option build-cache \
    --description 'Enable Gradle build cache'
complete --command gw --command gradle --command gradlew \
    --long-option settings-file \
    --short-option c \
    --require-parameter \
    --description 'Specify settings file'
complete --command gw --command gradle --command gradlew \
    --long-option configure-on-demand \
    --description 'Configure necessary projects only [incubating]'
complete --command gw --command gradle --command gradlew \
    --long-option console \
    --exclusive \
    --description 'Specify type of console output' \
    --arguments "(__fish_gradle_get_console_completion)"
complete --command gw --command gradle --command gradlew \
    --long-option continue \
    --description 'Continue task execution after task failures'
complete --command gw --command gradle --command gradlew \
    --long-option system-prop \
    --short-option D \
    --exclusive \
    --description 'Set system property of the JVM (e.g. -Dmyprop=myvalue)' \
    --arguments "(__fish_gradle_get_property_completion)"
complete --command gw --command gradle --command gradlew \
    --long-option debug \
    --short-option d \
    --description 'Log in debug mode (incl. normal stacktrace)'
complete --command gw --command gradle --command gradlew \
    --long-option daemon \
    --description 'Uses Gradle Daemon'
complete --command gw --command gradle --command gradlew \
    --long-option foreground \
    --description 'Start Gradle Daemon in foreground'
complete --command gw --command gradle --command gradlew \
    --long-option gradle-user-home \
    --short-option g \
    --require-parameter \
    --description 'Specify gradle user home directory'
complete --command gw --command gradle --command gradlew \
    --long-option init-script \
    --short-option I \
    --require-parameter \
    --description 'Specify initialization script'
complete --command gw --command gradle --command gradlew \
    --long-option info \
    --short-option i \
    --description 'Info log level'
complete --command gw --command gradle --command gradlew \
    --long-option include-build \
    --require-parameter \
    --description 'Include specified build in composite'
complete --command gw --command gradle --command gradlew \
    --long-option dry-run \
    --short-option m \
    --description 'Run builds with all task actions disabled'
complete --command gw --command gradle --command gradlew \
    --long-option max-workers \
    --exclusive \
    --description 'Configure number of concurrent workers'
complete --command gw --command gradle --command gradlew \
    --long-option no-build-cache \
    --description 'Disable Gradle build cache'
complete --command gw --command gradle --command gradlew \
    --long-option no-configure-on-demand \
    --description 'Disable use of configuration on demand [incubating]'
complete --command gw --command gradle --command gradlew \
    --long-option no-daemon \
    --description 'Disable Gradle daemon'
complete --command gw --command gradle --command gradlew \
    --long-option no-parallel \
    --description 'Disable parallel execution'
complete --command gw --command gradle --command gradlew \
    --long-option no-scan \
    --description 'Disable creation of build scan'
complete --command gw --command gradle --command gradlew \
    --long-option offline \
    --description 'Execute build without accessing network resources'
complete --command gw --command gradle --command gradlew \
    --long-option project-prop \
    --short-option P \
    --exclusive \
    --description 'Set project property for build script (e.g. -Pmyprop=myvalue)'
complete --command gw --command gradle --command gradlew \
    --long-option project-dir \
    --short-option p \
    --require-parameter \
    --description 'Specify start directory for Gradle'
complete --command gw --command gradle --command gradlew \
    --long-option parallel \
    --description 'Build projects in parallel'
complete --command gw --command gradle --command gradlew \
    --long-option priority \
    --exclusive \
    --description 'Specify scheduling priority for the Gradle daemon and all processes launched by it' \
    --arguments "(__fish_gradle_get_priority_completion)"
complete --command gw --command gradle --command gradlew \
    --long-option profile \
    --description 'Profile build execution time and generate report'
complete --command gw --command gradle --command gradlew \
    --long-option project-cache-dir \
    --require-parameter \
    --description 'Specify cache directory'
complete --command gw --command gradle --command gradlew \
    --long-option quiet \
    --short-option q \
    --description 'Error log level'
complete --command gw --command gradle --command gradlew \
    --long-option refresh-dependencies \
    --description 'Refresh dependencies state'
complete --command gw --command gradle --command gradlew \
    --long-option rerun-tasks \
    --description 'Ignore previously cached task results'
complete --command gw --command gradle --command gradlew \
    --long-option full-stacktrace \
    --short-option S \
    --description 'Print out full stacktrace'
complete --command gw --command gradle --command gradlew \
    --long-option stacktrace \
    --short-option s \
    --description 'Print out stacktrace'
complete --command gw --command gradle --command gradlew \
    --long-option scan \
    --description 'Creates build scan'
complete --command gw --command gradle --command gradlew \
    --long-option status \
    --description 'Show status of Gradle Daemon(s)'
complete --command gw --command gradle --command gradlew \
    --long-option stop \
    --description 'Stop Gradle Daemon(s)'
complete --command gw --command gradle --command gradlew \
    --long-option continuous \
    --short-option t \
    --description 'Enable continuous build'
complete --command gw --command gradle --command gradlew \
    --long-option update-locks \
    --description 'Perform a partial update of the dependency lock [incubating]'
complete --command gw --command gradle --command gradlew \
    --long-option version \
    --short-option v \
    --description 'Print version info.'
complete --command gw --command gradle --command gradlew \
    --long-option warn \
    --short-option w \
    --description 'Warn log level'
complete --command gw --command gradle --command gradlew \
    --long-option warning-mode \
    --description 'Specify warn mode' \
    --arguments "(__fish_gradle_get_warning_mode_completion)"
complete --command gw --command gradle --command gradlew \
    --long-option write-locks \
    --description 'Persists dependency resolution for locked configurations [incubating]'
