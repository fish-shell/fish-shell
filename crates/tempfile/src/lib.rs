use std::{
    ffi::OsString,
    fs::{File, OpenOptions},
    io::ErrorKind,
    path::PathBuf,
};

use rand::distr::{Alphanumeric, Distribution};

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

/// Creates a random filename with the given prefix.
/// Appends a random sequence of alphanumeric ASCII characters.
pub fn random_filename(mut prefix: OsString) -> OsString {
    let mut rng = rand::rng();
    let suffix_length = 10;
    let random_part: String = Alphanumeric
        .sample_iter(&mut rng)
        .take(suffix_length)
        .map(char::from)
        .collect();
    assert_eq!(random_part.len(), suffix_length);
    prefix.push(random_part);
    prefix
}

/// Tries to create a new file at the path returned by `generate_path`.
/// If a file already exists at this path, `generate_path` will be called again and file creation
/// will be retried. This is repeated until a new file is created, or an error occurs.
pub fn create_file_with_retry(
    generate_path: impl Fn() -> PathBuf,
) -> (PathBuf, std::io::Result<File>) {
    loop {
        let path = generate_path();
        match OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .open(&path)
        {
            Ok(file) => {
                return (path, Ok(file));
            }
            Err(e) => {
                if e.kind() != ErrorKind::AlreadyExists {
                    return (path, Err(e));
                }
            }
        }
    }
}

/// Tries to create a new temporary file in the operating system's default location for temporary
/// files.
/// On success, a [`TempFile`] is returned.
/// When this struct is dropped, the backing file will be deleted.
pub fn new_file() -> std::io::Result<TempFile> {
    let (path, result) = create_file_with_retry(|| {
        std::env::temp_dir().join(random_filename(OsString::from("fish_tmp_")))
    });
    let file = result?;
    Ok(TempFile { file, path })
}

/// Tries to create a new temporary directory using `mkdtemp`.
/// On success, a [`TempDir`] is returned.
/// When this struct is dropped, the backing directory, including all its contents, will be deleted.
pub fn new_dir() -> std::io::Result<TempDir> {
    let template = std::env::temp_dir().join("fish_tmp_XXXXXX");
    let path = nix::unistd::mkdtemp(&template)?;
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
