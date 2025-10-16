use std::{
    fs::{File, OpenOptions},
    path::PathBuf,
    process::Command,
};

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

    pub fn get_path(&self) -> &PathBuf {
        &self.path
    }
}

impl Drop for TempFile {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

/// Tries to create a new temporary file using `mktemp`.
/// On success, a [`TempFile`] is returned.
/// When this struct is dropped, the backing file will be deleted.
pub fn new() -> std::io::Result<TempFile> {
    let mktemp_output = Command::new("mktemp").output()?.stdout;
    let path = PathBuf::from(
        String::from_utf8(mktemp_output)
            .map_err(|e| {
                std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    format!("Path returned by mktemp is not valid UTF-8: {e}"),
                )
            })?
            .trim(),
    );
    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(true)
        .open(&path)?;

    Ok(TempFile { file, path })
}

#[cfg(test)]
mod tests {
    use std::io::{Read, Seek, Write};

    #[test]
    fn create_tempfile() {
        super::new().unwrap();
    }

    #[test]
    fn use_tempfile() {
        let mut tempfile = super::new().unwrap();
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
        drop(tempfile);
        assert_eq!(expected_content, actual_content);
    }
}
