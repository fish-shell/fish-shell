function __fish_print_zfs_snapshots -d "Lists ZFS snapshots"
    set fast_results (zfs list -o name -H)
    printf "%s\n" $fast_results

    # Don't retrieve all snapshots for all datasets until an @ is specified,
    # or if there is only one possible matching dataset. (See #7472)
    set current_token (commandline --current-token)
    set current_dataset (string replace -rf "([^@]+)@?.*" '$1' -- $current_token)
    set filtered_results (string match -ie -- $current_dataset $fast_results)
    if contains -- --force $argv ||
            string match -ieq @ -- $current_token ||
            not set -q filtered_results[2]

        # Ignore errors because the dataset specified could be non-existent
        zfs list -t snapshot -o name -H -d 1 $current_dataset 2>/dev/null
    end
end
