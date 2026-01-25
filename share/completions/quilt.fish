# completions for quilt version 0.69

# quilt [command] -h
complete -c quilt -s h -d 'Show help'

# quilt [--quiltrc file] [--trace] command [options]
complete -c quilt -l quiltrc -r --no-file -d 'Config file to use'
complete -c quilt -l trace -r --no-file -d 'Trace internal shell commands (bash -x)'

# quilt --version
complete -c quilt -l version -d 'Show version'

# quilt add [-P patch] {file} ...
complete -c quilt -a add -r -d 'Add files to patch'

# quilt annotate [-P patch] {file}
complete -c quilt -a annotate -r -d 'Show which patches modify which lines in file'

# quilt applied [patch]
complete -c quilt -a applied -r -d 'List applied patches in the stack'

# quilt delete [-r] [--backup] [patch|-n]
complete -c quilt -a delete -r -d 'Remove patch from the series'

# quilt diff [-p n|-p ab] [-u|-U num|-c|-C num] [--combine patch|-z] [-R] [-P patch] [--snapshot] [--diff=utility] [--no-timestamps] [--no-index] [--sort] [--color[=always|auto|never]] [file ...]
complete -c quilt -a diff -r -d 'Diff files in patch'

# quilt edit file ...
complete -c quilt -a edit -r -d 'Edit files in $EDITOR'

# quilt files [-v] [-a] [-l] [--combine patch] [patch]
complete -c quilt -a files -r -d 'Lists files changed by patch'

# quilt fold [-R] [-q] [-f] [-p strip-level]
complete -c quilt -a fold -r -d 'Integrate patch from stdin into the topmost patch'

# quilt fork [new_name]
complete -c quilt -a fork -r --no-file -d 'Fork the topmost patch'

# quilt graph [--all] [--reduce] [--lines[=num]] [--edge-labels=files] [-T ps] [patch]
complete -c quilt -a graph -r --no-file -d 'Visualize patch dependencies using dot'

# quilt grep [-h|options] {pattern}
complete -c quilt -a grep -r --no-file -d 'Search through source files'

# quilt header [-a|-r|-e] [--backup] [--strip-diffstat] [--strip-trailing-whitespace] [patch]
complete -c quilt -a header -r --no-file -d 'Print/change the header of patch'

# quilt import [-p num] [-R] [-P patch] [-f] [-d {o|a|n}] patchfile ...
complete -c quilt -a import -r -d 'Import external patches'

# quilt mail {--mbox file|--send} [-m text] [-M file] [--prefix prefix] [--sender ...] [--from ...] [--to ...] [--cc ...] [--bcc ...] [--subject ...] [--reply-to message] [--charset ...] [--signature file] [first_patch [last_patch]]
complete -c quilt -a mail -r -d 'Create or send mail messages from patches'

# quilt new [-p n] {patchname}
complete -c quilt -a new -r -d 'Start a new topmost patch'

# quilt next [patch]
complete -c quilt -a next -r -d 'Name of patch after the topmost/given patch'

# quilt patches [-v] [--color[=always|auto|never]] {file} [files...]
complete -c quilt -a patches -r -d 'List patches that modify the file(s)'

# quilt pop [-afRqv] [--refresh] [num|patch]
complete -c quilt -a pop -r -d 'Unapply patches from the stack'

# quilt previous [patch]
complete -c quilt -a previous -r -d 'Name of patch before the topmost/given patch'

# quilt push [-afqvm] [--fuzz=N] [--merge[=merge|diff3]] [--leave-rejects] [--color[=always|auto|never]] [--refresh] [num|patch]
complete -c quilt -a push -r -d 'Apply patches from the series'

# quilt refresh [-p n|-p ab] [-u|-U num|-c|-C num] [-z[new_name]] [-f] [--no-timestamps] [--no-index] [--diffstat] [--sort] [--backup] [--strip-trailing-whitespace] [patch]
complete -c quilt -a refresh -r -d 'Update patch with changes from source'

# quilt remove [-P patch] {file} ...
complete -c quilt -a remove -r -d 'Remove files from the patch'

# quilt rename [-P patch] new_name
complete -c quilt -a rename -r -d 'Rename the topmost/given patch'

# quilt revert [-P patch] {file} ...

# quilt series [--color[=always|auto|never]] [-v]
complete -c quilt -a series -r --no-file -d 'List all patches in the series'

# quilt setup [-d path-prefix] [-v] [--sourcedir dir] [--fuzz=N] [--spec-filter FILTER] [--slow|--fast] {specfile|seriesfile}
complete -c quilt -a setup -r -d 'Initializes a source tree from an rpm spec or a quilt series'

# quilt snapshot [-d]
complete -c quilt -a snapshot -r --no-file -d 'Take a snapshot of the working state'

# quilt top
complete -c quilt -a top -d 'Name of topmost patch in the stack'

# quilt unapplied [patch]
complete -c quilt -a unapplied -r -d 'List unapplied patches in the series'

# quilt upgrade
complete -c quilt -a upgrade -d 'Upgrade meta-data from older version of quilt'
