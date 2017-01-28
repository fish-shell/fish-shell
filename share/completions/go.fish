# go is a tool for managing Go source code.
# See: https://golang.org

function __fish__go_buildflags_completions -d 'Complete go build commands' --argument-names cmd
        complete -c go -n "__fish_seen_subcommand_from $cmd" -s a -d 'force rebuild'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -s n -d 'print the commands but do not run them'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -s p -r -d 'number parallel builds (default=#cpus)'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o race -d 'enable data race detection'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -s v -d 'print packages being built'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o work -d 'print and preserve work directory'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -s x -d 'print the commands'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o ccflags -r -d 'c compiler flags'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o compiler -x -d 'name of compiler to use, as in runtime' -a 'gccgo gc'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o gccgoflags -r -d 'gccgo compiler/linker flags'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o gcflags -r -d 'go compiler flags'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o installsuffix -r -d 'suffix for installation directory'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o ldflags -r -d 'linker flags'
        complete -c go -n "__fish_seen_subcommand_from $cmd" -o tags -r -d 'build tags'
end

function __fish_complete_go_files -d 'Complete go' --argument-names cmd
        complete -c go -n "__fish_seen_subcommand_from $cmd" -x -a "(
                __fish_complete_suffix .go
        )
        "
end

# build
complete -c go -n '__fish_use_subcommand' -a build -d 'compile packages and dependencies'
__fish__go_buildflags_completions build
__fish_complete_go_files build

# clean
complete -c go -n '__fish_use_subcommand' -a clean -d 'remove object files'
complete -c go -n '__fish_seen_subcommand_from clean' -s i -d "remove the corresponding installed archive or binary (what 'go install' would create)"
complete -c go -n '__fish_seen_subcommand_from clean' -s n -d "print the remove commands it would execute, but not run them"
complete -c go -n '__fish_seen_subcommand_from clean' -s r -d "clean to be applied recursively to all the dependencies of the packages named by the import paths"
complete -c go -n '__fish_seen_subcommand_from clean' -s x -d "clean to print remove commands as it executes them"

# doc
complete -c go -n '__fish_use_subcommand' -a doc -d 'run godoc on package sources'
complete -c go -n '__fish_seen_subcommand_from doc' -s n -d "prints commands that would be executed"
complete -c go -n '__fish_seen_subcommand_from doc' -s x -d "prints commands as they are executed"

# env
complete -c go -n '__fish_use_subcommand' -a env -d 'print Go environment information'

# fix
complete -c go -n '__fish_use_subcommand' -a fix -d 'run go tool fix on packages'

# fmt
complete -c go -n '__fish_use_subcommand' -a fmt -d 'run gofmt on package sources'
complete -c go -n '__fish_seen_subcommand_from fmt' -s n -d "prints commands that would be executed"
complete -c go -n '__fish_seen_subcommand_from fmt' -s x -d "prints commands as they are executed"

# get
complete -c go -n '__fish_use_subcommand' -a get -d 'download and install packages and dependencies'
complete -c go -n '__fish_seen_subcommand_from get' -s d -d "stop after downloading the packages; don't install"
complete -c go -n '__fish_seen_subcommand_from get' -o fix -d "run fix tool on packages before resolving dependencies or building"
complete -c go -n '__fish_seen_subcommand_from get' -s u -d "update remote packages"
__fish__go_buildflags_completions get

# help
complete -c go -n '__fish_use_subcommand' -a help -d 'get help on topic'
complete -c go -n '__fish_seen_subcommand_from help' -xa 'build clean doc env fix fmt get install list run test tool version vet gopath packages remote testflag testfunc' -d "target tool"

# install
complete -c go -n '__fish_use_subcommand' -a install -d 'compile and install packages and dependencies'
__fish__go_buildflags_completions install

# list
complete -c go -n '__fish_use_subcommand' -a list -d 'list packages'
complete -c go -n '__fish_seen_subcommand_from list' -s e -d "tolerate erroneous packages"
complete -c go -n '__fish_seen_subcommand_from list' -s f -r -d "pass in template for formatting"
complete -c go -n '__fish_seen_subcommand_from list' -o json -d "print in JSON format"
complete -c go -n '__fish_seen_subcommand_from list' -o tags -r -d 'list of build tags'

# run
complete -c go -n '__fish_use_subcommand' -a run -d 'compile and run Go program'
__fish__go_buildflags_completions run
__fish_complete_go_files run

# test
complete -c go -n '__fish_use_subcommand' -a test -d 'test packages'
complete -c go -n '__fish_seen_subcommand_from test' -s c -r -d "compile the test binary to pkg.test but do not run it"
complete -c go -n '__fish_seen_subcommand_from test' -s i -d "install dependent packages, but don't run tests"
__fish__go_buildflags_completions test
__fish_complete_go_files test

# tool
complete -c go -n '__fish_use_subcommand' -a tool -d 'run specified go tool'
complete -c go -f -n '__fish_seen_subcommand_from tool' -a '5a 5c 5g 5l 6a 6c 6g 6l addr2line api cgo cov dist fix nm objdump pack pprof prof vet yacc' -d "target tool"
complete -c go -n '__fish_seen_subcommand_from tool' -s n -d "print the command that would be executed but not execute it"

# version
complete -c go -f -n '__fish_use_subcommand' -a version -d 'print Go version'
complete -c go -f -n '__fish_seen_subcommand_from version'

# vet
complete -c go -n '__fish_use_subcommand' -a vet -d 'vet packages'
complete -c go -n '__fish_seen_subcommand_from vet' -s n -d "print the command that would be executed"
complete -c go -n '__fish_seen_subcommand_from vet' -s x -d "prints commands as they are executed"
