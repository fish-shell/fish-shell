function __fish_print_zfs_snapshots -d "Lists ZFS snapshots"
    set fast_results (zfs list -o name -H)
    printf "%s\n" $fast_results

    # Don't retrieve all snapshots for all datasets until an @ is specified,
    # or if there is only one possible matching dataset. (See #7472)
    set current_token (commandline --current-token)
    set filtered_results (string match -e -- $current_token $fast_results)
    if contains -- --force $argv ||
        not set -q filtered_results[2] ||
        string match -eq @ -- $current_token

        zfs list -t snapshot -o name -H
    end
end
