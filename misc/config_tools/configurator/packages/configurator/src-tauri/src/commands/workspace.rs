use std::path::PathBuf;

use tauri::{State, WebviewWindow};

use crate::error::CommandResult;
use crate::state::AppState;

#[tauri::command]
pub fn set_working_folder(working_folder: String, state: State<'_, AppState>) -> CommandResult<()> {
    let mut workspace = state.workspace.lock().map_err(|e| e.to_string())?;
    workspace.working_folder = PathBuf::from(working_folder);
    Ok(())
}

#[tauri::command]
pub fn write_board(
    board_name: String,
    contents: String,
    state: State<'_, AppState>,
) -> CommandResult<()> {
    let workspace = state.workspace.lock().map_err(|e| e.to_string())?;
    workspace.write_board(&board_name, &contents)
}

#[tauri::command]
pub fn get_home() -> Result<String, ()> {
    match dirs::home_dir() {
        None => Ok(String::new()),
        Some(path) => Ok(path.to_string_lossy().into_owned()),
    }
}

#[tauri::command]
pub fn open_devtools(window: WebviewWindow) {
    window.open_devtools()
}
