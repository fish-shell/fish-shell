/// This module contains tests that assert the functionality and behavior of the rust standard
/// library, to ensure we can safely use its abstractions to perform low-level operations.
#[cfg(test)]
mod tests {
    use std::fs::File;

    use nix::fcntl::{FcntlArg, fcntl};

    #[test]
    fn test_fd_cloexec() {
        // Just open a file. Any file.
        let file = File::create("test_file_for_fd_cloexec").unwrap();
        assert_eq!(
            fcntl(&file, FcntlArg::F_GETFD).unwrap() & libc::FD_CLOEXEC,
            libc::FD_CLOEXEC
        );
        let _ = std::fs::remove_file("test_file_for_fd_cloexec");
    }
}
