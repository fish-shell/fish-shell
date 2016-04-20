#!/usr/bin/env fish
#
# This is meant to be run by "make style" or "make style-all". It is not meant to
# be run directly from a shell prompt although it can be.
#
# This runs C++ files and fish scripts (*.fish) through their respective code
# formatting programs.
#
set git_clang_format no
set c_files
set f_files
set all no

if test "$argv[1]" = "--all"
    set all yes
    set -e argv[1]
end

if set -q argv[1]
    echo "Unexpected arguments: '$argv'"
    exit 1
end

if test $all = yes
    set files (git status --porcelain --short --untracked-files=all | sed -e 's/^ *[^ ]* *//')
    if set -q files[1]
        echo
        echo You have uncommited changes. Cowardly refusing to restyle the entire code base.
        echo
        exit 1
    end
    set c_files src/*.h src/*.cpp
    set f_files ***.fish
else
    # We haven't been asked to reformat all the source. If there are uncommitted changes reformat
    # those using `git clang-format`. Else reformat the files in the most recent commit.
    set files (git status --porcelain --short --untracked-files=all | sed -e 's/^ *[^ ]* *//')
    if set -q files[1]
        set git_clang_format yes
    else
        # No pending changes so lint the files in the most recent commit.
        set files (git show --name-only --pretty=oneline | tail --lines=+2)
    end

    # Extract just the C/C++ files.
    set c_files (string match -r '^.*\.(?:c|cpp|h)$' -- $files)
    # Extract just the fish files.
    set f_files (string match -r '^.*\.fish$' -- $files)
end

# Run the C++ reformatter if we have any C++ files.
if set -q c_files[1]
    if test $git_clang_format = yes
        if type -q git-clang-format
            echo
            echo ========================================
            echo Running git-clang-format
            echo ========================================
            git add $c_files
            git-clang-format
        else
            echo
            echo 'WARNING: Cannot find git-clang-format command'
            echo
        end
    else if type -q clang-format
        echo
        echo ========================================
        echo Running clang-format
        echo ========================================
        for file in $c_files
            clang-format $file >$file.new
            if cmp --quiet $file $file.new
                echo $file was correctly formatted
                rm $file.new
            else
                echo $file was NOT correctly formatted
                chmod --reference=$file $file.new
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
if set -q f_files[1]
    if not type -q fish_indent
        make fish_indent
        set PATH . $PATH
    end
    echo
    echo ========================================
    echo Running fish_indent
    echo ========================================
    for file in $f_files
        fish_indent <$file >$file.new
        if cmp --quiet $file $file.new
            echo $file was correctly formatted
            rm $file.new
        else
            echo $file was NOT correctly formatted
            chmod --reference=$file $file.new
            mv $file.new $file
        end
    end
end
