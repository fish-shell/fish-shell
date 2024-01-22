# Completions for Hugo (https://gohugo.io)

# Functions

function __fish_hugo_command
    set -l tokens (commandline -xpc)
    test (count $tokens) -le 1; and return 1
    set -l command ""
    set -l skip 1
    for token in $tokens[2..-1]
        if test $skip -eq 0
            set skip 1
        else
            switch $token
                case --config --logFile
                    set skip 0
                case "-*"
                case "*"
                    set command (string trim -l "$command $token")
            end
        end
    end
    test (count $command) -eq 0; and return 1
    echo $command
end

function __fish_hugo_using_command
    set -l command (__fish_hugo_command)
    test "$command" = "$argv"
end

function __fish_hugo_using_main_like_command
    set -l command (__fish_hugo_command)
    switch "$command"
        case "" benchmark server
            return 0
    end
    return 1
end

# Global options

complete -c hugo -l config -f -d "Configuration file"
complete -c hugo -l debug -f -d "Debug output"
complete -c hugo -l log -f -d "Enable logging"
complete -c hugo -l logFile -r -d "Log file"
complete -c hugo -l quiet -f -d "Quiet mode"
complete -c hugo -s v -l verbose -f -d "Verbose output"
complete -c hugo -l verboseLog -f -d "Verbose logging"

# Main-like commands

complete -c hugo -n __fish_hugo_using_main_like_command -s b -l baseURL -r -d "Hostname and path to the root"
complete -c hugo -n __fish_hugo_using_main_like_command -s B -l buildDrafts -f -d "Include content marked as draft"
complete -c hugo -n __fish_hugo_using_main_like_command -s E -l buildExpired -f -d "Include expired content"
complete -c hugo -n __fish_hugo_using_main_like_command -s F -l buildFuture -f -d "Include content with publishdate in the future"
complete -c hugo -n __fish_hugo_using_main_like_command -l cacheDir -f -a "(__fish_complete_directories (commandline -ct) 'Cache directory')" -d "Cache directory"
complete -c hugo -n __fish_hugo_using_main_like_command -l canonifyURLs -f -d "Canonicalize all relative URLs using baseurl"
complete -c hugo -n __fish_hugo_using_main_like_command -l cleanDestinationDir -f -d "Remove files from destination not found in static directories"
complete -c hugo -n __fish_hugo_using_main_like_command -s c -l contentDir -f -a "(__fish_complete_directories (commandline -ct) 'Content directory')" -d "Content directory"
complete -c hugo -n __fish_hugo_using_main_like_command -s d -l destination -f -a "(__fish_complete_directories (commandline -ct) 'Destination directory')" -d "Destination directory"
complete -c hugo -n __fish_hugo_using_main_like_command -l disable404 -f -d "Do not build 404 page"
complete -c hugo -n __fish_hugo_using_main_like_command -l disableKinds -r -d "Disable different kinds of pages"
complete -c hugo -n __fish_hugo_using_main_like_command -l disableRSS -f -d "Do not build RSS files"
complete -c hugo -n __fish_hugo_using_main_like_command -l disableSitemap -f -d "Do not build sitemap files"
complete -c hugo -n __fish_hugo_using_main_like_command -l enableGitInfo -f -d "Add Git revision, date and author info to the pages"
complete -c hugo -n __fish_hugo_using_main_like_command -l forceSyncStatic -f -d "Copy all files when static is changed"
complete -c hugo -n __fish_hugo_using_main_like_command -s h -l help -f -d "Help for hugo"
complete -c hugo -n __fish_hugo_using_main_like_command -l i18n-warnings -f -d "Print missing translations"
complete -c hugo -n __fish_hugo_using_main_like_command -l ignoreCache -f -d "Ignore the cache directory"
complete -c hugo -n __fish_hugo_using_main_like_command -s l -l layoutDir -f -a "(__fish_complete_directories (commandline -ct) 'Layout directory')" -d "Layout directory"
complete -c hugo -n __fish_hugo_using_main_like_command -l noChmod -f -d "Do not sync permission mode of files"
complete -c hugo -n __fish_hugo_using_main_like_command -l noTimes -f -d "Do not sync modification time of files"
complete -c hugo -n __fish_hugo_using_main_like_command -l pluralizeListTitles -f -d "Pluralize titles in lists using inflect"
complete -c hugo -n __fish_hugo_using_main_like_command -l preserveTaxonomyNames -f -d "Preserve taxonomy names as written"
complete -c hugo -n __fish_hugo_using_main_like_command -l renderToMemory -f -d "Render to memory"
complete -c hugo -n __fish_hugo_using_main_like_command -s s -l source -f -a "(__fish_complete_directories (commandline -ct) 'Source directory')" -d "Source directory"
complete -c hugo -n __fish_hugo_using_main_like_command -l stepAnalysis -f -d "Display memory and timing of different steps of the program"
complete -c hugo -n __fish_hugo_using_main_like_command -l templateMetrics -f -d "Display metrics about template executions"
complete -c hugo -n __fish_hugo_using_main_like_command -l templateMetricsHints -f -d "Calculate some improvement hints when combined with --templateMetrics"
complete -c hugo -n __fish_hugo_using_main_like_command -s t -l theme -r -d "Theme to use"
complete -c hugo -n __fish_hugo_using_main_like_command -l themesDir -f -a "(__fish_complete_directories (commandline -ct) 'Themes directory')" -d "Themes directory"
complete -c hugo -n __fish_hugo_using_main_like_command -l uglyURLs -f -d "Use /filename.html instead of /filename/"
complete -c hugo -n __fish_hugo_using_main_like_command -s w -l watch -f -d "Watch filesystem for changes and recreate as needed"

# Commands

# benchmark
complete -c hugo -n "not __fish_hugo_command" -f -a benchmark -d "Benchmark Hugo by building the site a number of times"
complete -c hugo -n "__fish_hugo_using_command benchmark" -s n -l count -f -d "Number of times to build the site"
complete -c hugo -n "__fish_hugo_using_command benchmark" -l cpuprofile -r -d "CPU profile file"
complete -c hugo -n "__fish_hugo_using_command benchmark" -s h -l help -f -d "Help for benchmark"
complete -c hugo -n "__fish_hugo_using_command benchmark" -l memprofile -r -d "Memory profile file"
complete -c hugo -n "__fish_hugo_using_command benchmark" -l renderToMemory -f -d "Render to memory"
complete -c hugo -n "__fish_hugo_using_command benchmark" -l stepAnalysis -f -d "Display memory and timing of different steps of the program"
complete -c hugo -n "__fish_hugo_using_command benchmark" -l templateMetrics -f -d "Display metrics about template executions"
complete -c hugo -n "__fish_hugo_using_command benchmark" -l templateMetricsHints -f -d "Calculate some improvement hints when combined with --templateMetrics"

# check
complete -c hugo -n "not __fish_hugo_command" -f -a check -d "Perform some verification checks"
complete -c hugo -n "__fish_hugo_using_command check" -s h -l help -f -d "Help for check"

# check ulimit
complete -c hugo -n "__fish_hugo_using_command check" -f -a ulimit -d "Check system ulimit settings"
complete -c hugo -n "__fish_hugo_using_command check ulimit" -s h -l help -f -d "Help for ulimit"

# config
complete -c hugo -n "not __fish_hugo_command" -f -a config -d "Print the site configuration"
complete -c hugo -n "__fish_hugo_using_command config" -s h -l help -f -d "Help for config"

# convert
complete -c hugo -n "not __fish_hugo_command" -f -a convert -d "Convert the content to different formats"
complete -c hugo -n "__fish_hugo_using_command convert" -s h -l help -f -d "Help for convert"
complete -c hugo -n "__fish_hugo_using_command convert" -s o -l output -f -a "(__fish_complete_directories (commandline -ct) 'Output directory')" -d "Output directory"
complete -c hugo -n "__fish_hugo_using_command convert" -s s -l source -f -a "(__fish_complete_directories (commandline -ct) 'Source directory')" -d "Source directory"
complete -c hugo -n "__fish_hugo_using_command convert" -l unsafe -f -d "Enable less safe operations"

# convert toJSON
complete -c hugo -n "__fish_hugo_using_command convert" -f -a toJSON -d "Convert front matter to JSON"
complete -c hugo -n "__fish_hugo_using_command convert toJSON" -s h -l help -f -d "Help for toJSON"

# convert toTOML
complete -c hugo -n "__fish_hugo_using_command convert" -f -a toTOML -d "Convert front matter to TOML"
complete -c hugo -n "__fish_hugo_using_command convert toTOML" -s h -l help -f -d "Help for toTOML"

# convert toYAML
complete -c hugo -n "__fish_hugo_using_command convert" -f -a toYAML -d "Convert front matter to YAML"
complete -c hugo -n "__fish_hugo_using_command convert toYAML" -s h -l help -f -d "Help for toYAML"

# env
complete -c hugo -n "not __fish_hugo_command" -f -a env -d "Print Hugo version and environment info"
complete -c hugo -n "__fish_hugo_using_command env" -s h -l help -f -d "Help for env"

# gen
complete -c hugo -n "not __fish_hugo_command" -f -a gen -d "Collection of several useful generators"
complete -c hugo -n "__fish_hugo_using_command gen" -s h -l help -f -d "Help for gen"

# gen autocomplete
complete -c hugo -n "__fish_hugo_using_command gen" -f -a autocomplete -d "Generate shell autocompletion script for Hugo"
complete -c hugo -n "__fish_hugo_using_command gen autocomplete" -l completionfile -r -d "Autocompletion file"
complete -c hugo -n "__fish_hugo_using_command gen autocomplete" -s h -l help -f -d "Help for autocomplete"
complete -c hugo -n "__fish_hugo_using_command gen autocomplete" -l type -f -a bash -d "Autocompletion type"

# gen chromastyles
complete -c hugo -n "__fish_hugo_using_command gen" -f -a chromastyles -d "Generate CSS stylesheet for the Chroma code highlighter"
complete -c hugo -n "__fish_hugo_using_command gen chromastyles" -s h -l help -f -d "Help for chromastyles"
complete -c hugo -n "__fish_hugo_using_command gen chromastyles" -l highlightStyle -r -d "Style used for highlighting lines"
complete -c hugo -n "__fish_hugo_using_command gen chromastyles" -l lineStyle -r -d "Style used for line numbers"
complete -c hugo -n "__fish_hugo_using_command gen chromastyles" -l style -r -d "Highlighter style"

# gen doc
complete -c hugo -n "__fish_hugo_using_command gen" -f -a doc -d "Generate Markdown documentation for the Hugo CLI"
complete -c hugo -n "__fish_hugo_using_command gen doc" -l dir -f -a "(__fish_complete_directories (commandline -ct) 'Doc directory')" -d "Doc directory"
complete -c hugo -n "__fish_hugo_using_command gen doc" -s h -l help -f -d "Help for doc"

# gen man
complete -c hugo -n "__fish_hugo_using_command gen" -f -a man -d "Generate man pages for the Hugo CLI"
complete -c hugo -n "__fish_hugo_using_command gen man" -l dir -f -a "(__fish_complete_directories (commandline -ct) 'Man pages directory')" -d "Man pages directory"
complete -c hugo -n "__fish_hugo_using_command gen man" -s h -l help -f -d "Help for man"

# import
complete -c hugo -n "not __fish_hugo_command" -f -a import -d "Import your site from others"
complete -c hugo -n "__fish_hugo_using_command import" -s h -l help -f -d "Help for import"

# import jekyll
complete -c hugo -n "__fish_hugo_using_command import" -f -a jekyll -d "Import from Jekyll"
complete -c hugo -n "__fish_hugo_using_command import jekyll" -l force -f -d "Allow import into non-empty target directory"
complete -c hugo -n "__fish_hugo_using_command import jekyll" -s h -l help -f -d "Help for jekyll"

# list
complete -c hugo -n "not __fish_hugo_command" -f -a list -d "List various types of content"
complete -c hugo -n "__fish_hugo_using_command list" -s h -l help -f -d "Help for list"
complete -c hugo -n "__fish_hugo_using_command list" -s s -l source -f -a "(__fish_complete_directories (commandline -ct) 'Source directory')" -d "Source directory"

# list drafts
complete -c hugo -n "__fish_hugo_using_command list" -f -a drafts -d "List all drafts"
complete -c hugo -n "__fish_hugo_using_command list drafts" -s h -l help -f -d "Help for drafts"

# list expired
complete -c hugo -n "__fish_hugo_using_command list" -f -a expired -d "List all expired posts"
complete -c hugo -n "__fish_hugo_using_command list expired" -s h -l help -f -d "Help for expired"

# list future
complete -c hugo -n "__fish_hugo_using_command list" -f -a future -d "List all posts dated in the future"
complete -c hugo -n "__fish_hugo_using_command list future" -s h -l help -f -d "Help for future"

# new
complete -c hugo -n "not __fish_hugo_command" -f -a new -d "Create new content"
complete -c hugo -n "__fish_hugo_using_command new" -l editor -r -d "Editor to use"
complete -c hugo -n "__fish_hugo_using_command new" -s h -l help -f -d "Help for new"
complete -c hugo -n "__fish_hugo_using_command new" -s k -l kind -r -d "Content type to create"
complete -c hugo -n "__fish_hugo_using_command new" -s s -l source -f -a "(__fish_complete_directories (commandline -ct) 'Source directory')" -d "Source directory"

# new site
complete -c hugo -n "__fish_hugo_using_command new" -f -a site -d "Create a new site"
complete -c hugo -n "__fish_hugo_using_command new site" -l force -f -d "Create site inside non-empty directory"
complete -c hugo -n "__fish_hugo_using_command new site" -s f -l format -r -d "Config and front matter format"
complete -c hugo -n "__fish_hugo_using_command new site" -s h -l help -f -d "Help for site"

# new theme
complete -c hugo -n "__fish_hugo_using_command new" -f -a theme -d "Create a new theme"
complete -c hugo -n "__fish_hugo_using_command new theme" -s h -l help -f -d "Help for theme"

# server
complete -c hugo -n "not __fish_hugo_command" -f -a server -d "Start high performance web server"
complete -c hugo -n "__fish_hugo_using_command server" -l appendPort -f -d "Append port to baseurl"
complete -c hugo -n "__fish_hugo_using_command server" -l bind -r -d "Interface to which the server will bind"
complete -c hugo -n "__fish_hugo_using_command server" -l disableFastRender -f -d "Enable full re-renders on changes"
complete -c hugo -n "__fish_hugo_using_command server" -l disableLiveReload -f -d "Watch without enabling live browser reload on rebuild"
complete -c hugo -n "__fish_hugo_using_command server" -s h -l help -f -d "Help for server"
complete -c hugo -n "__fish_hugo_using_command server" -l liveReloadPort -r -d "Port for live reloading"
complete -c hugo -n "__fish_hugo_using_command server" -l meminterval -f -d "Interval to poll memory usage"
complete -c hugo -n "__fish_hugo_using_command server" -l memstats -f -d "Memory usage log file"
complete -c hugo -n "__fish_hugo_using_command server" -l navigateToChanged -f -d "Navigate to changed content file on live browser reload"
complete -c hugo -n "__fish_hugo_using_command server" -l noHTTPCache -f -d "Prevent HTTP caching"
complete -c hugo -n "__fish_hugo_using_command server" -s p -l port -r -d "Port on which the server will listen"
complete -c hugo -n "__fish_hugo_using_command server" -l renderToDisk -r -d "Render to destination directory"

# undraft
complete -c hugo -n "not __fish_hugo_command" -f -a undraft -d "Reset the content draft status"
complete -c hugo -n "__fish_hugo_using_command undraft" -s h -l help -f -d "Help for undraft"

# version
complete -c hugo -n "not __fish_hugo_command" -f -a version -d "Print the version number of Hugo"
complete -c hugo -n "__fish_hugo_using_command version" -s h -l help -f -d "Help for version"
