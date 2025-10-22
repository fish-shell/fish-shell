/// This module contains tests that assert the functionality and behavior of the rust standard
/// library, to ensure we can safely use its abstractions to perform low-level operations.
#[cfg(test)]
mod tests {
    use std::fs::File;
    use std::os::fd::AsRawFd;

    #[test]
    fn test_fd_cloexec() {
        // Just open a file. Any file.
        let file = File::create("test_file_for_fd_cloexec").unwrap();
        let fd = file.as_raw_fd();
        unsafe {
            assert_eq!(
                libc::fcntl(fd, libc::F_GETFD) & libc::FD_CLOEXEC,
                libc::FD_CLOEXEC
            );
        }
        let _ = std::fs::remove_file("test_file_for_fd_cloexec");
    }
}
