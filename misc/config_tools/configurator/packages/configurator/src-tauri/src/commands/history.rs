use std::path::Path;

use tauri::State;

use crate::config::HistoryType;
use crate::error::CommandResult;
use crate::state::AppState;

#[tauri::command]
pub fn get_history(history_type: HistoryType, state: State<'_, AppState>) -> Result<String, ()> {
    let config = state.config.lock().map_err(|_| ())?;
    let history = config.get_history(history_type);
    // Filter out empty strings and history paths that no longer exist.
    let clean_history: Vec<&String> = match history_type {
        HistoryType::WorkingFolder => history
            .iter()
            .filter(|s| !s.is_empty())
            .filter(|s| Path::new(s).is_dir())
            .collect(),
        _ => history
            .iter()
            .filter(|s| !s.is_empty())
            .filter(|s| Path::new(s).is_file())
            .collect(),
    };

    let history_json_text =
        serde_json::to_string(&clean_history).unwrap_or_else(|_| String::from("[]"));
    Ok(history_json_text)
}

fn record_history(
    state: &AppState,
    history_type: HistoryType,
    history_path: &Path,
) -> CommandResult<()> {
    // Keep the complete read-modify-write sequence under one lock.
    let mut config = state.config.lock().map_err(|e| e.to_string())?;
    config.add_history(history_type, history_path);
    config.save_config()
}

#[tauri::command]
pub fn add_history(
    history_type: HistoryType,
    history_path: String,
    state: State<'_, AppState>,
) -> CommandResult<()> {
    let path_buf = Path::new(&history_path);
    if !(path_buf.is_dir() || path_buf.is_file()) {
        return Err("Not a validate dir or file path.".to_string());
    }
    record_history(&state, history_type, path_buf)
}

#[tauri::command]
pub fn force_reset(state: State<'_, AppState>) -> CommandResult<()> {
    let mut config = state.config.lock().map_err(|e| e.to_string())?;
    config.force_reset()
}

#[cfg(test)]
mod tests {
    use std::fs;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::sync::{Arc, Mutex};
    use std::thread;

    use super::*;
    use crate::config::UserConfig;
    use crate::state::Workspace;

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
    fn concurrent_history_writes_do_not_lose_entries() {
        const THREADS: usize = 4;
        const WRITES_PER_THREAD: usize = 8;

        let directory = test_directory("concurrent-history");
        let config_path = directory.join("config.json");
        fs::write(&config_path, "{}").unwrap();
        let state = Arc::new(AppState {
            config: Mutex::new(UserConfig::init(config_path)),
            workspace: Mutex::new(Workspace::default()),
        });

        let handles: Vec<_> = (0..THREADS)
            .map(|thread_id| {
                let state = Arc::clone(&state);
                thread::spawn(move || {
                    for write_id in 0..WRITES_PER_THREAD {
                        let path = PathBuf::from(format!("scenario-{thread_id}-{write_id}.xml"));
                        record_history(&state, HistoryType::Scenario, &path).unwrap();
                    }
                })
            })
            .collect();
        for handle in handles {
            handle.join().unwrap();
        }

        let config = state.config.lock().unwrap();
        let history = config.get_history(HistoryType::Scenario);
        assert_eq!(history.len(), THREADS * WRITES_PER_THREAD);
        for thread_id in 0..THREADS {
            for write_id in 0..WRITES_PER_THREAD {
                assert!(history.contains(&format!("scenario-{thread_id}-{write_id}.xml")));
            }
        }
        drop(config);
        fs::remove_dir_all(directory).unwrap();
    }
}
