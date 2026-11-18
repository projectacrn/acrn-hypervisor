use std::fs;
use std::path::{Path, PathBuf};

use itertools::Itertools;
use serde::{Deserialize, Serialize};

use crate::error::CommandResult;
use crate::fsutil::write_file;

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

impl ConfigData {
    fn new() -> Self {
        Self::default()
    }

    pub fn serialize(&self) -> String {
        serde_json::to_string(self).unwrap_or_else(|_| ConfigData::default().serialize())
    }

    fn deserialize(config_json: &str) -> CommandResult<Self> {
        serde_json::from_str(config_json).map_err(|e| e.to_string())
    }
}

#[derive(Debug)]
pub struct UserConfig {
    pub config_write_enable: bool,
    pub config_path: PathBuf,
    pub config_data: ConfigData,
}

impl UserConfig {
    pub fn new() -> Self {
        match Self::ensure_config_file() {
            Ok(config_file_path) => Self::init(config_file_path),
            Err(e) => {
                log::warn!("get config file path error! error: {}", e);
                log::warn!("Use blank config and disable config write to start configurator.");
                Self {
                    config_write_enable: false,
                    config_path: Path::new(".").to_path_buf(),
                    config_data: ConfigData::new(),
                }
            }
        }
    }

    fn ensure_config_file() -> CommandResult<PathBuf> {
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

        let config_dir = config_base.join(".acrn-configurator");
        log::info!("current config_dir is {}.", config_dir.display());
        if !config_dir.is_dir() {
            if let Err(e) = fs::create_dir_all(&config_dir) {
                log::warn!("Create configurator config dir failed, {}", e);
                return Err(e.to_string());
            }
        }

        let default_config_path = config_dir.join("config.json");
        if !default_config_path.is_file() {
            let empty_config = ConfigData::new();
            if let Err(e) = fs::write(&default_config_path, empty_config.serialize()) {
                return Err(e.to_string());
            }
        }
        Ok(default_config_path)
    }

    pub fn init(config_file_path: PathBuf) -> Self {
        let config_json = match fs::read_to_string(&config_file_path) {
            Ok(data) => data,
            Err(e) => {
                log::warn!("read config error! error: {}", e);
                log::warn!("Use default blank config to start due to read config failed.");
                return Self {
                    config_write_enable: false,
                    config_path: config_file_path,
                    config_data: ConfigData::new(),
                };
            }
        };

        let (config_data, config_write_enable) = match ConfigData::deserialize(&config_json) {
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
        Self {
            config_write_enable,
            config_path: config_file_path,
            config_data,
        }
    }

    pub fn save_config(&self) -> CommandResult<()> {
        if !self.config_write_enable {
            return Err(
                "Config writes are disabled to preserve the existing config file.".to_string(),
            );
        }
        write_file(&self.config_path, &self.config_data.serialize()).map_err(|e| {
            log::warn!("Write config error! error:{}", e);
            e
        })
    }

    pub fn add_history(&mut self, history_type: HistoryType, history_path: &Path) {
        let path_string = history_path.to_string_lossy().into_owned();
        let history = match history_type {
            HistoryType::WorkingFolder => &mut self.config_data.history.working_folder,
            HistoryType::Board => &mut self.config_data.history.board_file,
            HistoryType::Scenario => &mut self.config_data.history.scenario_file,
        };
        history.insert(0, path_string);
        *history = history.clone().into_iter().unique().collect();
    }

    pub fn get_history(&self, history_type: HistoryType) -> &[String] {
        match history_type {
            HistoryType::WorkingFolder => &self.config_data.history.working_folder,
            HistoryType::Board => &self.config_data.history.board_file,
            HistoryType::Scenario => &self.config_data.history.scenario_file,
        }
    }

    pub fn force_reset(&mut self) -> CommandResult<()> {
        self.config_data = ConfigData::new();
        self.config_write_enable = true;
        self.save_config()
    }
}

#[cfg(test)]
mod tests {
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
    fn damaged_history_is_not_overwritten() {
        let directory = test_directory("damaged-history");
        let config_path = directory.join("config.json");
        let damaged_config = "{\"history\":";
        fs::write(&config_path, damaged_config).unwrap();
        let mut config = UserConfig::init(config_path.clone());
        config.add_history(HistoryType::Scenario, Path::new("scenario.xml"));

        assert!(config.save_config().is_err());
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

        let config = UserConfig::init(config_path);

        assert!(config.config_write_enable);
        assert_eq!(config.get_history(HistoryType::WorkingFolder), &["/work"]);
        assert!(config.get_history(HistoryType::Board).is_empty());
        assert!(config.get_history(HistoryType::Scenario).is_empty());
        fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn history_is_prepended_and_deduplicated() {
        let mut config = UserConfig {
            config_write_enable: true,
            config_path: PathBuf::new(),
            config_data: ConfigData::default(),
        };
        config.add_history(HistoryType::Board, Path::new("old.board.xml"));
        config.add_history(HistoryType::Board, Path::new("new.board.xml"));
        config.add_history(HistoryType::Board, Path::new("old.board.xml"));

        assert_eq!(
            config.get_history(HistoryType::Board),
            &["old.board.xml", "new.board.xml"]
        );
        assert!(config.get_history(HistoryType::WorkingFolder).is_empty());
        assert!(config.get_history(HistoryType::Scenario).is_empty());
    }
}
