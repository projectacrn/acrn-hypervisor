use std::fs::{self, File};
use std::io;
use std::io::prelude::*;
use std::path::Path;
#[cfg(test)]
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};

use serde::Serialize;

use crate::error::CommandResult;

static TEMP_FILE_COUNTER: AtomicU64 = AtomicU64::new(0);

#[cfg(not(windows))]
fn replace_file(source: &Path, destination: &Path) -> CommandResult<()> {
    fs::rename(source, destination).map_err(|e| e.to_string())
}

#[cfg(windows)]
fn replace_file(source: &Path, destination: &Path) -> CommandResult<()> {
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::Storage::FileSystem::{
        MoveFileExW, MOVEFILE_REPLACE_EXISTING, MOVEFILE_WRITE_THROUGH,
    };

    let source: Vec<u16> = source.as_os_str().encode_wide().chain(Some(0)).collect();
    let destination: Vec<u16> = destination
        .as_os_str()
        .encode_wide()
        .chain(Some(0))
        .collect();
    // SAFETY: both buffers are NUL-terminated and remain alive for the call.
    let result = unsafe {
        MoveFileExW(
            source.as_ptr(),
            destination.as_ptr(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH,
        )
    };
    if result == 0 {
        Err(io::Error::last_os_error().to_string())
    } else {
        Ok(())
    }
}

pub fn write_file(path: &Path, content: &str) -> CommandResult<()> {
    let file_name = path
        .file_name()
        .ok_or_else(|| format!("Invalid destination path: {}", path.display()))?
        .to_string_lossy();
    let temporary_path = path.with_file_name(format!(
        ".{}.{}.{}.tmp",
        file_name,
        std::process::id(),
        TEMP_FILE_COUNTER.fetch_add(1, Ordering::Relaxed)
    ));

    let result = (|| {
        let mut file = File::create(&temporary_path).map_err(|e| e.to_string())?;
        file.write_all(content.as_bytes())
            .map_err(|e| e.to_string())?;
        match fs::metadata(path) {
            Ok(metadata) => fs::set_permissions(&temporary_path, metadata.permissions())
                .map_err(|e| e.to_string())?,
            Err(error) if error.kind() == io::ErrorKind::NotFound => {}
            Err(error) => return Err(error.to_string()),
        }
        file.sync_all().map_err(|e| e.to_string())?;
        replace_file(&temporary_path, path)
    })();

    if result.is_err() {
        let _ = fs::remove_file(&temporary_path);
    }
    result
}

#[derive(Serialize)]
pub struct DirEntry {
    path: String,
    children: Option<Vec<DirEntry>>,
}

pub fn read_dir<P: AsRef<Path>>(path: P, recursive: bool) -> io::Result<Vec<DirEntry>> {
    let path = path.as_ref();
    let mut entries = Vec::new();
    for entry in fs::read_dir(path)? {
        let entry = entry?;
        let path = entry.path();
        let children = if recursive && entry.file_type()?.is_dir() {
            Some(read_dir(&path, true)?)
        } else {
            None
        };
        let path = path
            .to_str()
            .ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!("Path is not valid UTF-8: {}", path.display()),
                )
            })?
            .to_string();
        entries.push(DirEntry { path, children });
    }
    entries.sort_by(|a, b| a.path.cmp(&b.path));
    Ok(entries)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_directory(name: &str) -> PathBuf {
        let path = std::env::temp_dir().join(format!(
            "acrn-configurator-{}-{}-{}",
            name,
            std::process::id(),
            TEMP_FILE_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir_all(&path).unwrap();
        path
    }

    #[test]
    fn write_file_replaces_contents_without_leaving_temporary_files() {
        let directory = test_directory("atomic-write");
        let destination = directory.join("scenario.xml");
        fs::write(&destination, "old scenario").unwrap();

        write_file(&destination, "new scenario").unwrap();

        assert_eq!(fs::read_to_string(&destination).unwrap(), "new scenario");
        assert_eq!(fs::read_dir(&directory).unwrap().count(), 1);
        fs::remove_dir_all(directory).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn write_file_preserves_existing_permissions() {
        use std::os::unix::fs::PermissionsExt;

        let directory = test_directory("preserve-permissions");
        let destination = directory.join("launch_user_vm_id1.sh");
        fs::write(&destination, "old script").unwrap();
        fs::set_permissions(&destination, fs::Permissions::from_mode(0o755)).unwrap();

        write_file(&destination, "new script").unwrap();

        assert_eq!(fs::read_to_string(&destination).unwrap(), "new script");
        assert_eq!(
            fs::metadata(&destination).unwrap().permissions().mode() & 0o777,
            0o755
        );
        fs::remove_dir_all(directory).unwrap();
    }
}
