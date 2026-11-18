use std::fs;
use std::path::PathBuf;
use std::sync::Mutex;

use crate::config::UserConfig;
use crate::error::CommandResult;
use crate::fsutil::write_file;

#[derive(Debug, Default)]
pub struct Workspace {
    pub working_folder: PathBuf,
}

impl Workspace {
    pub fn write_board(&self, board_name: &str, contents: &str) -> CommandResult<()> {
        let board_xml_path = self
            .working_folder
            .join(format!("{}.board.xml", board_name));
        let board_file_name = board_xml_path
            .file_name()
            .map(|name| name.to_string_lossy().to_lowercase())
            .ok_or_else(|| "Invalid board file name.".to_string())?;

        // Preserve all existing boards until the replacement is durable.
        write_file(&board_xml_path, contents)?;

        for entry in fs::read_dir(&self.working_folder).map_err(|e| e.to_string())? {
            let entry = entry.map_err(|e| e.to_string())?;
            let path = entry.path();
            let file_name = entry.file_name().to_string_lossy().to_lowercase();
            if path.is_file() && file_name.ends_with(".board.xml") && file_name != board_file_name {
                fs::remove_file(&path)
                    .map_err(|e| format!("Can not delete file:{} error: {}", path.display(), e))?;
            }
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct AppState {
    pub config: Mutex<UserConfig>,
    pub workspace: Mutex<Workspace>,
}

impl AppState {
    pub fn new() -> Self {
        Self {
            config: Mutex::new(UserConfig::new()),
            workspace: Mutex::new(Workspace::default()),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::fs;
    use std::sync::atomic::{AtomicU64, Ordering};

    use super::*;

    static TEST_DIRECTORY_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn test_directory(name: &str) -> PathBuf {
        let path = std::env::temp_dir().join(format!(
            "acrn-configurator-{}-{}-{}",
            name,
            std::process::id(),
            TEST_DIRECTORY_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir_all(&path).unwrap();
        path
    }

    #[test]
    fn board_replacement_treats_working_folder_literally() {
        let directory = test_directory("literal-board-path");
        let working_folder = directory.join("work[1]");
        let sibling = directory.join("work1");
        fs::create_dir(&working_folder).unwrap();
        fs::create_dir(&sibling).unwrap();
        fs::write(working_folder.join("old.board.xml"), "old").unwrap();
        fs::write(sibling.join("old.board.xml"), "unrelated").unwrap();
        let workspace = Workspace { working_folder };

        workspace.write_board("new", "new board").unwrap();

        assert_eq!(
            fs::read_to_string(workspace.working_folder.join("new.board.xml")).unwrap(),
            "new board"
        );
        assert!(!workspace.working_folder.join("old.board.xml").exists());
        assert_eq!(
            fs::read_to_string(sibling.join("old.board.xml")).unwrap(),
            "unrelated"
        );
        fs::remove_dir_all(directory).unwrap();
    }
}
