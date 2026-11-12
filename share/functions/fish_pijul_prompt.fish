function fish_pijul_prompt --description "Prompt function for Pijul"
    # No Pijul, no prompt
    # jq for parsing Pijul’s JSON output more reliably
    command -sq pijul && command -sq jq
    or return 1

    # Walk up to the repository root. Also use by conflict-marker scan.
    set -l dir $PWD
    while not test -e "$dir/.pijul"
        # Reached the filesystem root without finding a Pijul repository
        if test $dir = /
            return 1
        end
        set -l parent (path dirname $dir)
        if test -z $parent -o $parent = $dir
            return 1
        end
        set dir $parent
    end
    set -l root $dir

    set -q fish_color_pijul_normal
    # pijuls are black & bluish
    or set -l fish_color_pijul_normal blue
    set -q fish_color_pijul_conflict
    or set -l fish_color_pijul_conflict red
    #set -q fish_color_pijul_rebasing
    #or set -l fish_color_pijul_rebasing yellow

    set -q fish_prompt_pijul_status_added
    or set -l fish_prompt_pijul_status_added '+'
    set -q fish_prompt_pijul_status_removed
    or set -l fish_prompt_pijul_status_removed -
    set -q fish_prompt_pijul_status_modified
    or set -l fish_prompt_pijul_status_modified '*'
    set -q fish_prompt_pijul_status_moved
    or set -l fish_prompt_pijul_status_moved '⇄'
    set -q fish_prompt_pijul_status_restored
    or set -l fish_prompt_pijul_status_restored '↺'
    set -q fish_prompt_pijul_status_resolved
    or set -l fish_prompt_pijul_status_resolved '✓'
    set -q fish_prompt_pijul_status_untracked
    or set -l fish_prompt_pijul_status_untracked '…'
    set -q fish_prompt_pijul_status_conflict
    or set -l fish_prompt_pijul_status_conflict '!'

    set -q fish_prompt_pijul_status_order
    or set -l fish_prompt_pijul_status_order added removed modified moved restored resolved conflict untracked

    # Escape hatch: skip the working-copy conflict scan for huge repositories
    set -q fish_prompt_pijul_scan_conflicts
    or set -l fish_prompt_pijul_scan_conflicts 1

    # Empty values
    set -l channel ''
    set -l added 0
    set -l removed 0
    set -l modified 0
    set -l moved 0
    set -l restored 0
    set -l resolved 0
    set -l conflict 0
    set -l untracked 0
    set -l conflicts_received false

    set -l pijul_status_json (pijul status --output-format json 2>/dev/null)

    if test $status -eq 0
        # Sum the per-file status labels into a counter object, then emit a
        # tab-separated table where everything else is computed from the root
        # JSON inline.
        set -l status_query '
            . as $root
            | reduce ($root.changes[]?.status[]?) as $s (
                {
                    added: 0,
                    removed: 0,
                    modified: 0,
                    moved: 0,
                    restored: 0,
                    resolved: 0,
                    conflict: 0
                };
                if $s == "new file" then .added += 1
                elif $s == "deleted" then .removed += 1
                elif $s == "modified" then .modified += 1
                elif $s == "moved" then .moved += 1
                elif $s == "restored" then .restored += 1
                elif $s == "conflict resolved" or $s == "zombie fixed" then .resolved += 1
                elif $s == "conflict" then .conflict += 1
                else .
                end
              )
            | [
                ($root.channel // ""),
                .added,
                .removed,
                .modified,
                .moved,
                .restored,
                .resolved,
                ($root | has("conflicts")),
                (if ($root | has("conflicts"))
                    then ($root.conflicts | map(.path) | unique | length)
                    else .conflict
                 end),
                (($root.untracked // []) | length)
              ]
            | @tsv
        '

        set -l stats_json (printf '%s' "$pijul_status_json" | jq -er "$status_query" 2>/dev/null)

        if test $status -eq 0
            set -l stats (string split \t -- "$stats_json")
            set channel $stats[1]
            set added $stats[2]
            set removed $stats[3]
            set modified $stats[4]
            set moved $stats[5]
            set restored $stats[6]
            set resolved $stats[7]
            set conflicts_received $stats[8]
            set conflict $stats[9]
            set untracked $stats[10]

            # Released pijul has no `conflicts` field in its status JSON (tho
            # TODOs to add); regardless, conflicts can still be found by
            # scanning the working copy for Pijul’s conflict markers. This can
            # be fairly expensive so can be enabled or not. In the future, when
            # support is merged/stable, this should be dropped.
            if test "$conflicts_received" != true
                switch $fish_prompt_pijul_scan_conflicts
                    case 0 ''
                        # Scan disabled
                    case '*'
                        set -l conflict_pattern '^(>>>>>>>|<<<<<<<|=======) [0-9]'
                        if command -sq rg
                            # ripgrep should perform better
                            set conflict (rg -l0 --hidden --no-ignore -g '!.pijul/**' "$conflict_pattern" "$root" 2>/dev/null | string split0 | count)
                        else if command -sq find && command -sq grep
                            # … but falling back to find + grep
                            set conflict (find "$root" -path "$root/.pijul" -prune -o -type f -exec grep -l -E "$conflict_pattern" {} \; 2>/dev/null | count)
                        end
                end
            end
        end

    end

    set -l label $channel

    set_color normal
    echo -n '('
    if test $conflict -gt 0
        set_color $fish_color_pijul_conflict
    else
        set_color $fish_color_pijul_normal
    end
    echo -n $label
    set_color normal

    for stat in $fish_prompt_pijul_status_order
        set -l count $$stat
        if test $count -gt 0
            echo -n '|'
            set -l color_var fish_color_pijul_$stat
            if set -q $color_var
                set_color $$color_var
            end
            echo -n $count
            set -l symbol_var fish_prompt_pijul_status_$stat
            echo -n $$symbol_var
            set_color normal
        end
    end
    set_color normal
    echo -n ')'
end
