#!/usr/bin/env fish
#
# This is meant to be run by "make style" or "make style-all". It is not meant to
# be run directly from a shell prompt although it can be.
#
# This runs C++ files and fish scripts (*.fish) through their respective code
# formatting programs.
#
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
    set c_files src/*.h src/*.cpp
    set f_files ***.fish
else
    # We haven't been asked to reformat all the source. If there are uncommitted
    # changes reformat those, else reformat the files in the most recent commit.
    set pending (git status --porcelain --short --untracked-files=all | sed -e 's/^ *//')
    if count $pending > /dev/null
        # There are pending changes so lint those files.
        for arg in $pending
            set files $files (string split -m 1 ' ' $arg)[2]
        end
    else
        # No pending changes so lint the files in the most recent commit.
        set files (git show --name-only --pretty=oneline head | tail --lines=+2)
    end

    # Extract just the C/C++ files.
    set c_files (string match -r '^.*\.(?:c|cpp|h)$' -- $files)
    # Extract just the fish files.
    set f_files (string match -r '^.*\.fish$' -- $files)
end

# Run the C++ reformatter if we have any C++ files.
if set -q c_files[1]
    if type -q clang-format
        echo
        echo ========================================
        echo Running clang-format
        echo ========================================
        for file in $c_files
            clang-format $file > $file.new
            if cmp --quiet $file $file.new
                echo $file was correctly formatted
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
        fish_indent < $file > $file.new
        if cmp --quiet $file $file.new
            echo $file was correctly formatted
            rm $file.new
        else
            echo $file was NOT correctly formatted
            mv $file.new $file
        end
    end
end
