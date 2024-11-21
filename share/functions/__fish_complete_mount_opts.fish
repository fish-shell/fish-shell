function __fish_print_user_ids
    if command -sq getent
        for line in (getent passwd)
            set -l v (string split : -- $line)
            printf "%s\t%s\n" $v[3] $v[1]
        end
    end
end

function __fish_print_group_ids
    if command -sq getent
        for line in (getent group)
            set -l v (string split : -- $line)
            printf "%s\t%s\n" $v[3] $v[1]
        end
    end
end

function __fish_complete_mount_opts
    set -l fish_mount_opts \
        async\t'Use asynchronous I/O' \
        atime\t'Update time on each access' \
        noatime\t'Do not update time on each access' \
        auto\t'Mounted with' \
        noauto\t'Not mounted by -a' \
        defaults\t'Use default options' \
        dev\t'Interpret character/block special devices' \
        nodev\t'Do not interpret character/block special devices' \
        diratime\t'Update directory inode access time' \
        nodiratime\t'Don\'t update directory inode access time' \
        dirsync\t'Use synchronous directory operations' \
        exec\t'Permit executables' \
        noexec\t'Do not permit executables' \
        group\t'Any user within device group may mount' \
        iversion\t'Increment i_version field on inode modification' \
        noiversion\t'Don\'t increment i_version field on inode modification' \
        mand\t'Allow mandatory locks' \
        nomand\t'Don\'t allow mandatory locks' \
        _netdev\t'Filesystem uses network' \
        nofail\t'Don\'t report errors' \
        relatime\t'Update inode access times' \
        norelatime\t'Don\'t update inode access times' \
        strictatime \
        nostrictatime \
        lazytime \
        nolazytime \
        suid\t'Allow suid bits' \
        nosuid\t'Ignore suid bits' \
        silent \
        loud \
        owner \
        remount\t'Remount read-only filesystem' \
        ro\t'Mount read-only' \
        rw\t'Mount read-write' \
        sync\t'Use synchronous I/O' \
        user\t'Any user may mount' \
        nouser\t'Only root may mount' \
        users\t'Any user may mount and unmount' \
        protect \
        usemp \
        verbose \
        {grp,no,usr,}quota \
        autodefrag \
        compress \
        compress-force \
        degraded \
        discard \
        enospc_debug \
        flushoncommit \
        inode_cache \
        context=\t'Set SELinux context' \
        uid= \
        gid= \
        ownmask= \
        othmask= \
        setuid= \
        setgid= \
        mode= \
        prefix= \
        volume= \
        reserved= \
        root= \
        bs= \
        alloc_start= \
        check_int{,_data,_print,_mask}= \
        commit= \
        compress= \
        compress-force= \
        device= \
        fatal_errors= \
        max_inline= \
        metadata_ratio= \
        noacl \
        nobarrier \
        nodatacow \
        nodatasum \
        notreelog \
        recovery \
        rescan_uuid_tree \
        skip_balance \
        nospace_cache \
        clear_cache \
        ssd \
        nossd \
        ssd_spread \
        subvol= \
        subvolrootid= \
        thread_pool= \
        user_subvol_rm_allowed \
        acl \
        noacl \
        bsddf \
        minixdf \
        check=none \
        nocheck \
        debug \
        errors={continue,remount-ro,panic} \
        grpid \
        bsdgroups \
        bsdgroups \
        bsdgroups \
        nouid32 \
        grpquota \
        grpquota \
        resgid= \
        resuid= \
        sb= \
        {user,nouser}_xattr \
        journal={update,unum} \
        journal{_dev,_path}= \
        norecovery \
        noload \
        data={journal,ordered,writeback} \
        data_err={ignore,abort} \
        barrier={0,1} \
        user_xattr \
        acl

    set -l token (commandline -tc | string replace -r '^-o' -- '')
    set -l args (string split , -- $token)

    if test (count $args) -ne 0
        set -l last_arg $args[-1]
        set -e args[-1]

        switch (string replace -r '=.*' '=' -- $last_arg)
            case uid=
                set -a fish_mount_opts uid=(__fish_print_user_ids)
            case gid=
                set -a fish_mount_opts gid=(__fish_print_group_ids)
            case setuid=
                set -a fish_mount_opts setuid=(__fish_print_user_ids)
            case setgid=
                set -a fish_mount_opts setgid=(__fish_print_group_ids)
        end
    end

    set -l prefix ''
    if set -q args[1]
        set prefix (string join , -- $args),
    end

    printf '%s\n' $prefix$fish_mount_opts
end
