#!/usr/bin/env ghoti
#
# Compares the output of two ghoti profile runs and emits the time difference between
# the first and second set of results.
#
# Usage: ./diff_profiles.ghoti profile1.log profile2.log > profile_diff.log

set -l profile1 (cat $argv[1])
set -l profile2 (cat $argv[2])

set -l line_no 0
while set -l next_line_no (math $line_no + 1) && set -q profile1[$next_line_no] && set -q profile2[$next_line_no]
    set line_no $next_line_no

    set -l line1 $profile1[$line_no]
    set -l line2 $profile2[$line_no]

    if not string match -qr '^\d+\t\d+' $line1
        echo $line1
        continue
    end

    set -l results1 (string match -r '^(\d+)\t(\d+)\s+(.*)' $line1)
    set -l results2 (string match -r '^(\d+)\t(\d+)\s+(.*)' $line2)

    # times from both files
    set -l time1 $results1[2..3]
    set -l time2 $results2[2..3]

    # leftover from both files
    set -l remainder1 $results1[4]
    set -l remainder2 $results2[4]

    if not string match -q -- $remainder1 $remainder2
        echo Mismatch on line $line_no:
        echo - $remainder1
        echo + $remainder2
        exit 1
    end

    set -l diff
    set diff[1] (math $time1[1] - $time2[1])
    set diff[2] (math $time1[2] - $time2[2])

    echo $diff[1] $diff[2] $remainder1
end
