use std::borrow::Borrow;
use std::path::{Path, PathBuf};

use itertools::Itertools;
use serde::{Deserialize, Serialize};
use tauri::WebviewWindow;

use std::fs::{self, File};
use std::io;
use std::io::prelude::*;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

#[derive(Serialize, Deserialize, Clone, Copy, Debug)]
#[repr(u16)]
#[non_exhaustive]
pub enum HistoryType {
    WorkingFolder = 1,
    Board,
    Scenario,
}

#[derive(Serialize, Deserialize, Debug, Default)]
#[serde(default)]
pub struct History {
    pub working_folder: Vec<String>,
    pub board_file: Vec<String>,
    pub scenario_file: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug, Default)]
#[serde(default)]
pub struct ConfigData {
    pub history: History,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Configurator {
    pub config_write_enable: bool,
    pub config_path: PathBuf,

    pub config_data: ConfigData,
    pub working_folder: String,
}

static TEMP_FILE_COUNTER: AtomicU64 = AtomicU64::new(0);

#[cfg(not(windows))]
fn replace_file(source: &Path, destination: &Path) -> Result<(), String> {
    fs::rename(source, destination).map_err(|e| e.to_string())
}

#[cfg(windows)]
fn replace_file(source: &Path, destination: &Path) -> Result<(), String> {
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

pub fn write_file(path: PathBuf, content: String) -> Result<(), String> {
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
        match fs::metadata(&path) {
            Ok(metadata) => fs::set_permissions(&temporary_path, metadata.permissions())
                .map_err(|e| e.to_string())?,
            Err(error) if error.kind() == io::ErrorKind::NotFound => {}
            Err(error) => return Err(error.to_string()),
        }
        file.sync_all().map_err(|e| e.to_string())?;
        replace_file(&temporary_path, &path)
    })();

    if result.is_err() {
        let _ = fs::remove_file(&temporary_path);
    }
    result
}

impl ConfigData {
    fn new() -> ConfigData {
        let history = History {
            working_folder: vec![],
            board_file: vec![],
            scenario_file: vec![],
        };
        ConfigData { history }
    }

    pub fn serialize(&self) -> String {
        serde_json::to_string(&self).unwrap_or_else(|_| {
            let default = ConfigData::new();
            ConfigData::serialize(&default)
        })
    }

    /// deserialize data
    fn deserialize(config_json: String) -> Result<ConfigData, String> {
        match serde_json::from_str(&config_json.to_string()) {
            Ok(config_data) => Ok(config_data),
            Err(e) => Err(e.to_string()),
        }
    }
}

impl Configurator {
    pub fn new() -> Self {
        match Self::ensure_config_file() {
            Ok(config_file_path) => {
                // read config.json
                Self::init(config_file_path)
            }
            Err(e) => {
                log::warn!("get config file path error! error: {}", e.to_string());
                log::warn!("Use blank config and disable config write to start configurator.");
                Self {
                    config_write_enable: false,
                    config_path: Path::new(".").to_path_buf(),
                    config_data: ConfigData::new(),
                    working_folder: "".to_string(),
                }
            }
        }
    }

    fn ensure_config_file() -> Result<PathBuf, String> {
        // get config_dir or home_dir path
        // Todo: 讨论fallback逻辑是否可行
        let config_base = match dirs::config_dir() {
            None => {
                log::info!("get config_dir error! fallback to get home_dir.");
                match dirs::home_dir() {
                    None => {
                        return Err("get config_dir and home_dir error!".to_string());
                    }
                    Some(path) => path,
                }
            }
            Some(path) => path,
        };

        // get acrn-configurator dir path and check it exist
        let config_dir = config_base.join(".acrn-configurator");
        log::info!("current config_dir is {}.", config_dir.display());
        if !config_dir.is_dir() {
            match fs::create_dir_all(&config_dir) {
                Err(e) => {
                    //Todo: 明确无法创建 .acrn-configurator 文件夹时的处理逻辑
                    log::warn!("Create configurator config dir failed, {}", e.to_string());
                    return Err(e.to_string());
                }
                _ => {}
            }
        }

        // get config.json file path and check it exist
        let default_config_path = config_dir.join("config.json");
        if !default_config_path.is_file() {
            let empty_config = ConfigData::new();
            match fs::write(&default_config_path, empty_config.serialize()) {
                Ok(_) => {}
                Err(e) => return Err(e.to_string()),
            };
        }
        Ok(default_config_path)
    }

    pub fn init(config_file_path: PathBuf) -> Configurator {
        let config_json = match fs::read_to_string(&config_file_path) {
            Ok(data) => data,
            Err(e) => {
                log::warn!("read config error! error: {}", e.to_string());
                log::warn!("Use default blank config to start due to read config failed.");
                return Configurator {
                    config_write_enable: false,
                    config_path: config_file_path,
                    config_data: ConfigData::new(),
                    working_folder: "".to_string(),
                };
            }
        };

        let (config_data, config_write_enable) = match ConfigData::deserialize(config_json) {
            Ok(config_data) => {
                log::info!("success load config: {}", config_data.serialize());
                (config_data, true)
            }
            Err(e) => {
                log::warn!("Deserialize json data error! error: {}", e);
                log::warn!("Preserve the damaged config and disable automatic config writes.");
                (ConfigData::new(), false)
            }
        };
        log::info!("Using config: {}", config_data.serialize());
        Configurator {
            config_write_enable,
            config_path: config_file_path,
            config_data,
            working_folder: "".to_string(),
        }
    }

    pub fn save_config(&self) -> Result<(), String> {
        if !self.config_write_enable {
            return Err(
                "Config writes are disabled to preserve the existing config file.".to_string(),
            );
        }
        write_file(self.config_path.clone(), self.config_data.serialize()).map_err(|e| {
            log::warn!("Write config error! error:{}", e);
            e
        })
    }

    pub fn add_history(&mut self, history_type: HistoryType, history_path: &Path) {
        let path_string: String = history_path.to_string_lossy().parse().unwrap();
        match history_type {
            HistoryType::WorkingFolder => {
                self.config_data
                    .history
                    .working_folder
                    .insert(0, path_string);
                self.config_data.history.working_folder = self
                    .config_data
                    .history
                    .working_folder
                    .clone()
                    .into_iter()
                    .unique()
                    .collect()
            }
            HistoryType::Board => {
                self.config_data.history.board_file.insert(0, path_string);
                self.config_data.history.board_file = self
                    .config_data
                    .history
                    .board_file
                    .clone()
                    .into_iter()
                    .unique()
                    .collect()
            }
            HistoryType::Scenario => {
                self.config_data
                    .history
                    .scenario_file
                    .insert(0, path_string);
                self.config_data.history.scenario_file = self
                    .config_data
                    .history
                    .scenario_file
                    .clone()
                    .into_iter()
                    .unique()
                    .collect()
            }
        };
    }

    pub fn get_history(&self, history_type: HistoryType) -> &[String] {
        match history_type {
            HistoryType::WorkingFolder => self.config_data.history.working_folder.borrow(),
            HistoryType::Board => self.config_data.history.board_file.borrow(),
            HistoryType::Scenario => self.config_data.history.scenario_file.borrow(),
        }
    }

    pub fn force_reset(&mut self) -> Result<(), String> {
        self.config_data = ConfigData::new();
        self.config_write_enable = true;
        self.save_config()
    }

    pub fn set_working_folder(&mut self, working_folder: String) {
        self.working_folder = working_folder
    }

    pub fn write_board(&self, board_name: String, board_xml_string: String) -> Result<(), String> {
        let working_folder = Path::new(&self.working_folder);
        let board_xml_path = working_folder.join(format!("{}.board.xml", board_name));
        let board_file_name = board_xml_path
            .file_name()
            .map(|name| name.to_string_lossy().to_lowercase())
            .ok_or_else(|| "Invalid board file name.".to_string())?;

        // Preserve all existing boards until the replacement is durable.
        write_file(board_xml_path, board_xml_string)?;

        for entry in fs::read_dir(working_folder).map_err(|e| e.to_string())? {
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

static WORKING_FOLDER: Mutex<String> = Mutex::new(String::new());

#[tauri::command]
pub fn get_history(history_type: HistoryType) -> Result<String, ()> {
    let configurator = Configurator::new();
    let history = configurator.get_history(history_type);
    // filter out empty string and not exist history path
    let clean_history: Vec<&String> = match history_type {
        HistoryType::WorkingFolder => history
            .into_iter()
            .filter(|s| !s.is_empty())
            .filter(|s| Path::new(s).is_dir())
            .collect::<Vec<_>>(),
        _ => history
            .into_iter()
            .filter(|s| !s.is_empty())
            .filter(|s| Path::new(s).is_file())
            .collect::<Vec<_>>(),
    };

    let history_json_text =
        serde_json::to_string(&clean_history).unwrap_or_else(|_| String::from("[]"));
    Ok(history_json_text)
}

#[tauri::command]
pub fn add_history(history_type: HistoryType, history_path: String) -> Result<(), String> {
    let path_buf = Path::new(&history_path);
    if !(path_buf.is_dir() || path_buf.is_file()) {
        return Err("Not a validate dir or file path.".to_string());
    }
    let mut configurator = Configurator::new();
    configurator.add_history(history_type, path_buf);
    configurator.save_config()
}

#[tauri::command]
pub fn set_working_folder(working_folder: String) -> Result<(), String> {
    let mut current_working_folder = WORKING_FOLDER.lock().map_err(|e| e.to_string())?;
    *current_working_folder = working_folder;
    Ok(())
}

#[tauri::command]
pub fn write_board(board_name: String, contents: String) -> Result<(), String> {
    let mut configurator = Configurator::new();
    let working_folder = WORKING_FOLDER.lock().map_err(|e| e.to_string())?.clone();
    configurator.set_working_folder(working_folder);
    configurator.write_board(board_name, contents)
}

#[tauri::command]
pub fn force_reset() -> Result<(), String> {
    let mut configurator = Configurator::new();
    configurator.force_reset()
}

#[tauri::command]
pub fn get_home() -> Result<String, ()> {
    match dirs::home_dir() {
        None => Ok(String::new()),
        Some(path) => Ok(path.to_string_lossy().into_owned()),
    }
}

#[derive(Serialize)]
pub struct DirEntry {
    path: String,
    children: Option<Vec<DirEntry>>,
}

#[tauri::command]
pub fn acrn_read(file_path: &str) -> Result<String, String> {
    let mut file = File::open(file_path).map_err(|e| e.to_string())?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)
        .map_err(|e| e.to_string())?;
    Ok(contents)
}

#[tauri::command]
pub fn acrn_write(file_path: &str, contents: &str) -> Result<(), String> {
    write_file(PathBuf::from(file_path), contents.to_string())
}

#[tauri::command]
pub fn acrn_is_file(path: &str) -> bool {
    fs::metadata(path)
        .map(|metadata| metadata.is_file())
        .unwrap_or(false)
}

#[tauri::command]
pub fn acrn_create_dir(path: &str, recursive: bool) -> Result<(), String> {
    if recursive {
        fs::create_dir_all(path).map_err(|e| e.to_string())
    } else {
        fs::create_dir(path).map_err(|e| e.to_string())
    }
}

fn read_dir<P: AsRef<Path>>(path: P, recursive: bool) -> io::Result<Vec<DirEntry>> {
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

#[tauri::command]
pub fn acrn_read_dir(path: &str, recursive: bool) -> Result<Vec<DirEntry>, String> {
    read_dir(path, recursive).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn acrn_remove_dir(path: &str) -> Result<(), String> {
    fs::remove_dir_all(path).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn acrn_remove_file(path: &str) -> Result<(), String> {
    fs::remove_file(path).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn open_devtools(window: WebviewWindow) {
    window.open_devtools()
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

        write_file(destination.clone(), "new scenario".to_string()).unwrap();

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

        write_file(destination.clone(), "new script".to_string()).unwrap();

        assert_eq!(fs::read_to_string(&destination).unwrap(), "new script");
        assert_eq!(
            fs::metadata(&destination).unwrap().permissions().mode() & 0o777,
            0o755
        );
        fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn damaged_history_is_not_overwritten() {
        let directory = test_directory("damaged-history");
        let config_path = directory.join("config.json");
        let damaged_config = "{\"history\":";
        fs::write(&config_path, damaged_config).unwrap();
        let mut configurator = Configurator::init(config_path.clone());
        configurator.add_history(HistoryType::Scenario, Path::new("scenario.xml"));

        assert!(configurator.save_config().is_err());
        assert_eq!(fs::read_to_string(&config_path).unwrap(), damaged_config);
        fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn missing_history_fields_use_empty_defaults() {
        let directory = test_directory("partial-history");
        let config_path = directory.join("config.json");
        fs::write(
            &config_path,
            "{\"history\":{\"working_folder\":[\"/work\"]}}",
        )
        .unwrap();

        let configurator = Configurator::init(config_path);

        assert!(configurator.config_write_enable);
        assert_eq!(
            configurator.get_history(HistoryType::WorkingFolder),
            &["/work"]
        );
        assert!(configurator.get_history(HistoryType::Board).is_empty());
        assert!(configurator.get_history(HistoryType::Scenario).is_empty());
        fs::remove_dir_all(directory).unwrap();
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
        let configurator = Configurator {
            config_write_enable: false,
            config_path: directory.join("unused.json"),
            config_data: ConfigData::new(),
            working_folder: working_folder.to_string_lossy().into_owned(),
        };

        configurator
            .write_board("new".to_string(), "new board".to_string())
            .unwrap();

        assert_eq!(
            fs::read_to_string(working_folder.join("new.board.xml")).unwrap(),
            "new board"
        );
        assert!(!working_folder.join("old.board.xml").exists());
        assert_eq!(
            fs::read_to_string(sibling.join("old.board.xml")).unwrap(),
            "unrelated"
        );
        fs::remove_dir_all(directory).unwrap();
    }
}
