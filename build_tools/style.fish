#!/usr/bin/env fish
#
# This runs C++ files and fish scripts (*.fish) through their respective code
# formatting programs.
#
set -l git_clang_format no
set -l c_files
set -l fish_files
set -l python_files
set -l all no

if test "$argv[1]" = --all
    set all yes
    set -e argv[1]
end

if set -q argv[1]
    echo "Unexpected arguments: '$argv'"
    exit 1
end

if test $all = yes
    set -l files (git status --porcelain --short --untracked-files=all | sed -e 's/^ *[^ ]* *//')
    if set -q files[1]
        echo
        echo You have uncommitted changes. Cowardly refusing to restyle the entire code base.
        echo
        exit 1
    end
    set c_files src/*.h src/*.cpp src/*.c
    set fish_files (printf '%s\n' share/***.fish)
    set python_files **.py
else
    # We haven't been asked to reformat all the source. If there are uncommitted changes reformat
    # those using `git clang-format`. Else reformat the files in the most recent commit.
    # Select (cached files) (modified but not cached, and untracked files)
    set -l files (git diff-index --cached HEAD --name-only) (git ls-files --exclude-standard --others --modified)
    if set -q files[1]
        set git_clang_format yes
    else
        # No pending changes so lint the files in the most recent commit.
        set files (git diff-tree --no-commit-id --name-only -r HEAD)
    end

    # Extract just the C/C++ files that exist.
    set c_files
    for file in (string match -r '^.*\.(?:c|cpp|h)$' -- $files)
        test -f $file; and set c_files $c_files $file
    end
    # Extract just the fish files.
    set fish_files (string match -r '^.*\.fish$' -- $files)
    set python_files (string match -r '^.*\.py$' -- $files)
end

set -l red (set_color red)
set -l green (set_color green)
set -l blue (set_color blue)
set -l normal (set_color normal)

# Run the C++ reformatter if we have any C++ files.
if set -q c_files[1]
    if test $git_clang_format = yes
        if type -q git-clang-format
            echo === Running "$red"git-clang-format"$normal"
            git add $c_files
            git-clang-format
        else
            echo
            echo 'WARNING: Cannot find git-clang-format command'
            echo
        end
    else if type -q clang-format
        echo === Running "$red"clang-format"$normal"
        for file in $c_files
            cp $file $file.new # preserves mode bits
            clang-format $file >$file.new
            if cmp --quiet $file $file.new
                rm $file.new
            else
                echo $file was NOT correctly formatted
                mv $file.new $file
            end
        end
    else
        echo
        echo 'WARNING: Cannot find clang-format command'
        echo
    end
end

# Run the fish reformatter if we have any fish files.
if set -q fish_files[1]
    if not type -q fish_indent
        make fish_indent
        set PATH . $PATH
    end
    echo === Running "$green"fish_indent"$normal"
    fish_indent -w -- $fish_files
end

if set -q python_files[1]
    if not type -q black
        echo
        echo Please install "`black`" to style python
        echo
    else
        echo === Running "$blue"black"$normal"
        black $python_files
    end
end
