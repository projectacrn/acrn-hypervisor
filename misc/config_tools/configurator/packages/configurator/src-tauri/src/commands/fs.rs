use std::fs::{self, File};
use std::io::prelude::*;
use std::path::PathBuf;

use crate::error::CommandResult;
use crate::fsutil::{self, DirEntry};

#[tauri::command]
pub fn acrn_read(file_path: &str) -> CommandResult<String> {
    let mut file = File::open(file_path).map_err(|e| e.to_string())?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)
        .map_err(|e| e.to_string())?;
    Ok(contents)
}

#[tauri::command]
pub fn acrn_write(file_path: &str, contents: &str) -> CommandResult<()> {
    fsutil::write_file(&PathBuf::from(file_path), contents)
}

#[tauri::command]
pub fn acrn_is_file(path: &str) -> bool {
    fs::metadata(path)
        .map(|metadata| metadata.is_file())
        .unwrap_or(false)
}

#[tauri::command]
pub fn acrn_create_dir(path: &str, recursive: bool) -> CommandResult<()> {
    if recursive {
        fs::create_dir_all(path).map_err(|e| e.to_string())
    } else {
        fs::create_dir(path).map_err(|e| e.to_string())
    }
}

#[tauri::command]
pub fn acrn_read_dir(path: &str, recursive: bool) -> CommandResult<Vec<DirEntry>> {
    fsutil::read_dir(path, recursive).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn acrn_remove_dir(path: &str) -> CommandResult<()> {
    fs::remove_dir_all(path).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn acrn_remove_file(path: &str) -> CommandResult<()> {
    fs::remove_file(path).map_err(|e| e.to_string())
}
