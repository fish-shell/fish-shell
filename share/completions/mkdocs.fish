# completion for mkdocs

complete -f -c mkdocs -s h -l help -d 'Show help and exit'
complete -f -c mkdocs -s v -l verbose -d 'Enable verbose output'
complete -f -c mkdocs -s q -l quiet -d 'Silence warnings'
complete -n 'not __fish_seen_subcommand_from build gh-deploy new serve' -f -c mkdocs -s V -l version -d 'Show the version and exit'

## build
complete -n 'not __fish_seen_subcommand_from build gh-deploy new serve' -f -c mkdocs -a build -d 'Build the MkDocs documentation'
complete -n 'contains build (commandline -pxc)' -f -c mkdocs -s c -l clean -d 'Remove old site_dir before building (the default)'
complete -n 'contains build (commandline -pxc)' -c mkdocs -s f -l config-file -r -d 'Provide a specific MkDocs config'
complete -n 'contains build (commandline -pxc)' -f -c mkdocs -s s -l strict -d 'Enable strict mode. This will cause MkDocs to abort the build on any warnings'
complete -n 'contains build (commandline -pxc)' -c mkdocs -s t -l theme -d 'The theme to use when building your documentation' -xa 'mkdocs readthedocs material'
complete -n 'contains build (commandline -pxc)' -c mkdocs -s e -l theme-dir -r -d 'The theme directory to use when building your documentation'
complete -n 'contains build (commandline -pxc)' -c mkdocs -s d -l site-dir -r -d 'The directory to output the result of the documentation build'

## gh-deploy
complete -n 'not __fish_seen_subcommand_from build gh-deploy new serve' -f -c mkdocs -a gh-deploy -d 'Deploy your documentation to GitHub Pages'
complete -n 'contains gh-deploy (commandline -pxc)' -f -c mkdocs -s c -l clean -d 'Remove old site_dir before building (the default)'
complete -n 'contains gh-deploy (commandline -pxc)' -c mkdocs -s f -l config-file -r -d 'Provide a specific MkDocs config'
complete -n 'contains gh-deploy (commandline -pxc)' -f -c mkdocs -s m -l message -r -d 'A commit message to use when committing to the GitHub Pages remote branch'
complete -n 'contains gh-deploy (commandline -pxc)' -f -c mkdocs -s b -l remote-branch -r -d 'The remote branch to commit to for GitHub Pages'
complete -n 'contains gh-deploy (commandline -pxc)' -f -c mkdocs -s r -l remote-name -r -d 'The remote name to commit to for GitHub Pages'
complete -n 'contains gh-deploy (commandline -pxc)' -f -c mkdocs -l force -d 'Force the push to the repository'

## new
complete -n 'not __fish_seen_subcommand_from build gh-deploy new serve' -f -c mkdocs -a new -r -d 'Create a new MkDocs project'

## serve
complete -n 'not __fish_seen_subcommand_from build gh-deploy new serve' -f -c mkdocs -a serve -d 'Run the builtin development server'
complete -n 'contains serve (commandline -pxc)' -c mkdocs -s f -l config-file -r -d 'Provide a specific MkDocs config'
complete -n 'contains serve (commandline -pxc)' -c mkdocs -s a -l dev-addr -r -d 'IP address and port to serve documentation locally (default: localhost:8000)'
complete -n 'contains serve (commandline -pxc)' -f -c mkdocs -s s -l strict -d 'Enable strict mode. This will cause MkDocs to abort the build on any warnings'
complete -n 'contains serve (commandline -pxc)' -c mkdocs -s t -l theme -d 'The theme to use when building your documentation' -xa 'mkdocs readthedocs material'
complete -n 'contains serve (commandline -pxc)' -c mkdocs -s e -l theme-dir -r -d 'The theme directory to use when building your documentation'
complete -n 'contains serve (commandline -pxc)' -f -c mkdocs -l livereload -d 'Enable the live reloading in the development server (this is the default)'
complete -n 'contains serve (commandline -pxc)' -f -c mkdocs -l no-livereload -d 'Disable the live reloading in the development server'
complete -n 'contains serve (commandline -pxc)' -f -c mkdocs -l dirtyreload -d 'Enable the live reloading in the development server, but only re-build files that have changed'
