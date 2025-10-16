use std::{fs::File, path::PathBuf};

pub struct TempFile {
    file: File,
    path: PathBuf,
}

impl TempFile {
    pub fn get(&self) -> &File {
        &self.file
    }

    pub fn get_mut(&mut self) -> &mut File {
        &mut self.file
    }

    pub fn path(&self) -> &PathBuf {
        &self.path
    }
}

impl Drop for TempFile {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

pub struct TempDir {
    path: PathBuf,
}

impl TempDir {
    pub fn path(&self) -> &PathBuf {
        &self.path
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.path);
    }
}

fn get_tmpdir() -> PathBuf {
    PathBuf::from(std::env::var_os("TMPDIR").unwrap_or("/tmp".into()))
}

fn get_template() -> PathBuf {
    get_tmpdir().join("fish_tmp_XXXXXX")
}

/// Tries to create a new temporary file using `mkstemp`.
/// On success, a [`TempFile`] is returned.
/// When this struct is dropped, the backing file will be deleted.
pub fn new_file() -> std::io::Result<TempFile> {
    let (fd, path) = nix::unistd::mkstemp(&get_template())?;
    let file = File::from(fd);
    Ok(TempFile { file, path })
}

/// Tries to create a new temporary directory using `mkdtemp`.
/// On success, a [`TempDir`] is returned.
/// When this struct is dropped, the backing directory, including all its contents, will be deleted.
pub fn new_dir() -> std::io::Result<TempDir> {
    let path = nix::unistd::mkdtemp(&get_template())?;
    Ok(TempDir { path })
}

#[cfg(test)]
mod tests {
    use std::{
        fs::File,
        io::{Read, Seek, Write},
    };

    #[test]
    fn create_tempfile() {
        super::new_file().unwrap();
    }

    #[test]
    #[should_panic(expected = "file should no longer exist")]
    fn use_tempfile() {
        let mut tempfile = super::new_file().unwrap();
        let expected_content = "test";
        {
            let file = tempfile.get_mut();
            file.write_all(expected_content.as_bytes()).unwrap();
            file.seek(std::io::SeekFrom::Start(0)).unwrap();
        }
        let mut actual_content = String::new();
        {
            let mut file = tempfile.get();
            file.read_to_string(&mut actual_content).unwrap();
        }
        let path = tempfile.path().to_owned();
        drop(tempfile);
        assert_eq!(expected_content, actual_content);
        File::open(&path).expect("file should no longer exist");
    }

    #[test]
    fn create_tempdir() {
        super::new_dir().unwrap();
    }

    #[test]
    #[should_panic(expected = "file should no longer exist")]
    fn use_tempdir() {
        let tempdir = super::new_dir().unwrap();
        let file_path = tempdir.path().join("foo");
        let expected_content = "test";
        {
            let mut file = File::create(&file_path).unwrap();
            file.write_all(expected_content.as_bytes()).unwrap();
        }

        {
            let mut file = File::open(&file_path).unwrap();
            let mut actual_content = String::new();
            file.read_to_string(&mut actual_content).unwrap();
            assert_eq!(expected_content, actual_content);
        }
        drop(tempdir);
        File::open(&file_path).expect("file should no longer exist");
    }
}
