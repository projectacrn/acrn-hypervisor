use std::borrow::Borrow;
use std::ops::Add;
use std::path::{Path, PathBuf};

use glob::{glob_with, MatchOptions};
use itertools::Itertools;
use serde::{Deserialize, Serialize};
use tauri::Window;

use std::fs::{self, File};
use std::io;
use std::io::prelude::*;

#[derive(Serialize, Deserialize, Clone, Copy, Debug)]
#[repr(u16)]
#[non_exhaustive]
pub enum HistoryType {
    WorkingFolder = 1,
    Board,
    Scenario,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct History {
    pub working_folder: Vec<String>,
    pub board_file: Vec<String>,
    pub scenario_file: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug)]
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

pub fn write_file(path: PathBuf, content: String) -> Result<(), String> {
    fs::write(path, content).map_err(|e| e.to_string())
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
        log::info!("current config_dir is {}.", config_dir.to_str().unwrap());
        if !config_dir.is_dir() {
            match fs::create_dir(&config_dir) {
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

        let config_data = match ConfigData::deserialize(config_json) {
            Ok(config_data) => {
                log::info!("success load config: {}", config_data.serialize());
                config_data
            }
            Err(e) => {
                log::warn!("Deserialize json data error! error: {}", e);
                log::warn!("Use default blank config to start due to deserialize config failed.");
                ConfigData::new()
            }
        };
        log::info!("Using config: {}", config_data.serialize());
        Configurator {
            config_write_enable: true,
            config_path: config_file_path,
            config_data,
            working_folder: "".to_string(),
        }
    }

    pub fn save_config(&self) {
        if !self.config_write_enable {
            return;
        }
        match fs::write(&self.config_path, self.config_data.serialize()) {
            Ok(_) => {}
            Err(e) => {
                log::warn!("Write config error! error:{}", e.to_string())
            }
        }
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

    pub fn force_reset(&mut self) {
        self.config_data = ConfigData::new();
        self.save_config()
    }

    pub fn set_working_folder(&mut self, working_folder: String) {
        self.working_folder = working_folder
    }

    pub fn write_board(&self, board_name: String, board_xml_string: String) -> Result<(), String> {
        let options = MatchOptions {
            case_sensitive: false,
            ..Default::default()
        };
        let pattern = self.working_folder.clone().add("/.*\\.board\\.xml");
        let files = match glob_with(&pattern, options).map_err(|e| e.to_string()) {
            Ok(files) => files,
            Err(e) => return Err(e.to_string()),
        };
        for entry in files {
            match entry {
                Ok(filepath) => match fs::remove_file(&filepath) {
                    Ok(_) => {}
                    Err(e) => {
                        let err_msg = format!(
                            "Can not delete file:{} error: {}",
                            filepath.to_str().unwrap_or_else(|| "").to_string(),
                            e.to_string()
                        );
                        log::warn!("{}", err_msg);
                        return Err(err_msg);
                    }
                },
                Err(e) => {
                    log::error!("find old board error! error:{}", e.to_string())
                }
            }
        }

        let board_basename = board_name.add(".board.xml");
        let board_xml_path = Path::new(&self.working_folder).join(board_basename);
        write_file(board_xml_path, board_xml_string)
    }
}

static mut WORKING_FOLDER: String = String::new();

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
pub fn add_history(history_type: HistoryType, history_path: String) -> Result<(), &'static str> {
    let path_buf = Path::new(&history_path);
    if !(path_buf.is_dir() || path_buf.is_file()) {
        return Err("Not a validate dir or file path.");
    }
    let mut configurator = Configurator::new();
    configurator.add_history(history_type, path_buf);
    configurator.save_config();
    Ok(())
}

#[tauri::command]
pub fn set_working_folder(working_folder: String) -> Result<(), ()> {
    unsafe {
        WORKING_FOLDER = working_folder;
    }
    Ok(())
}

#[tauri::command]
pub fn write_board(board_name: String, contents: String) -> Result<(), String> {
    let mut configurator = Configurator::new();
    unsafe {
        configurator.set_working_folder(WORKING_FOLDER.clone());
    }
    configurator.write_board(board_name, contents)
}

#[tauri::command]
pub fn force_reset() -> Result<(), ()> {
    let mut configurator = Configurator::new();
    configurator.force_reset();
    Ok(())
}

#[tauri::command]
pub fn get_home() -> Result<String, ()> {
    match dirs::home_dir() {
        None => Ok(String::new()),
        Some(path) => Ok(path.to_str().unwrap().to_string()),
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
    let mut file = File::create(file_path).map_err(|e| e.to_string())?;
    file.write_all(contents.as_bytes())
        .map_err(|e| e.to_string())?;
    Ok(())
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
        let path = entry.path().to_str().unwrap().to_string();
        let children = if recursive && entry.file_type()?.is_dir() {
            Some(read_dir(&path, true)?)
        } else {
            None
        };
        entries.push(DirEntry { path, children });
    }
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
pub fn open_devtools(window: Window) {
    window.open_devtools()
}
