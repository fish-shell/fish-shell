# gradle is a build system.
# See: https://gradle.org

complete -c gradle -l help -s h -d 'Show help'
complete -c gradle -l no-rebuild -s a -d 'Don\'t rebuild dependencies'
complete -c gradle -l build-file -s b -r -d 'Specify build file'
complete -c gradle -l settings-file -s c -r -d 'Specify settings file'
complete -c gradle -l configure-on-demand -d 'Only relevant project are configured'
complete -c gradle -l console -x -d 'Specify console output type' -a 'plan auto rich'
complete -c gradle -l continue -d 'Continue task execution after failure'
complete -c gradle -l system-prop -s D -r -d 'Set system property of the JVM'
complete -c gradle -l debug -s d -d 'Log in debug mode'
complete -c gradle -l daemon -d 'Uses Gradle Daemon to run build'
complete -c gradle -l foreground -d 'Uses Gradle Daemon in foreground'
complete -c gradle -l gradle-user-home -s g -r -d 'Specify gradle user home directory'
complete -c gradle -l initscript -s I -r -d 'Specify an initialization script'
complete -c gradle -l info -s i -d 'Set log level to info'
complete -c gradle -l include-build -r -d 'Include specified build in composite'
complete -c gradle -l dry-run -s m -d 'Runs build with all task actions disabled'
complete -c gradle -l max-workers -x -d 'Configure number of concurrent workers' -a '1\t 2\t 3\t 4\t 5\t 6\t 7\t 8\t 9\t 10\t'
complete -c gradle -l no-daemon -d 'Don\'t use deamon'
complete -c gradle -l offline -d 'Don\'t use network resources'
complete -c gradle -l project-prop -s P -x -d 'Set project property for build script'
complete -c gradle -l project-dir -s p -r -d 'Specify start directory'
complete -c gradle -l parallel -d 'Build project in parallel'
complete -c gradle -l profile -d 'Profile build execution time'
complete -c gradle -l project-cache-dir -r -d 'Specify project cache directory'
complete -c gradle -l quiet -s q -d 'Only log erros'
complete -c gradle -l recompile-scripts -d 'Force build script recompiling'
complete -c gradle -l refresh-dependencies -d 'Refresh state of dependencies'
complete -c gradle -l rerun-tasks -d 'Ignore previously cached dependencies'
complete -c gradle -l full-stacktrace -s S -d 'Print out full stacktrace for all exceptions'
complete -c gradle -l status -d 'Shows status of running andrecently stopped daemon'
complete -c gradle -l stop -d 'Stop daemon if running'
complete -c gradle -l continuous -s t -d 'Enable continuous build'
complete -c gradle -l no-search-upward -s u -d 'Don\'t search in parent folders for settings file'
complete -c gradle -l version -s v -d 'Print version'
complete -c gradle -l exclude-task -s x -x -d 'Specify task to be excluded from execution'

# https://github.com/hanny24/gradle-fish/blob/master/gradle.load
function __cache_or_get_gradle_completion
    # Set up cache directory
    if test -z $XDG_CACHE_HOME
        set XDG_CACHE_HOME $HOME/.cache/
    end
    mkdir -m 700 -p $XDG_CACHE_HOME/gradle-completions

    set -l hashed_pwd (fish_md5 -s $PWD)
    set -l gradle_cache_file $XDG_CACHE_HOME/gradle-completions/$hashed_pwd
    if not test -f $gradle_cache_file; or command test build.gradle -nt $gradle_cache_file
        command gradle -q tasks 2>/dev/null | string match -r '^[[:alnum:]]+ - .*' | string replace ' - ' \t >$gradle_cache_file
    end
    cat $gradle_cache_file
end

function __contains_gradle_build
    test -f build.gradle
end

complete -x -c gradle -n '__contains_gradle_build' -a "(__cache_or_get_gradle_completion)"
