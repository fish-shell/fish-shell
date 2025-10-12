# BSD Vagrant Test Environments

This directory contains Vagrant configurations for testing across multiple BSD operating systems.

Use the `build_fish_on_bsd.sh` script to automatically build fish-shell on multiple BSD VMs:

```bash
./build_fish_on_bsd.sh freebsd_14_0 openbsd_7_4 netbsd_9_3 dragonflybsd_6_4
```

The script will:

- Boot each VM
- Sync the latest files
- Run `cargo build` in `/sync/fish-shell`
- **Halt VMs that build successfully**
- **Leave VMs running if builds fail** (for debugging)
- Show a summary of successes and failures
