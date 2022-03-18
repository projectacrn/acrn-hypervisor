use serde::Serialize;
use std::fs::{self, File};
use std::io;
use std::io::prelude::*;
use std::path::Path;

#[tauri::command]
pub fn fs_read_text_file(path: &str) -> Result<String, String> {
    let mut file = File::open(path).map_err(|e| e.to_string())?;
    let mut contents = String::new();
    file
        .read_to_string(&mut contents)
        .map_err(|e| e.to_string())?;
    Ok(contents)
}

#[tauri::command]
pub fn fs_write_text_file(path: &str, contents: &str) -> Result<(), String> {
    let mut file = File::create(path).map_err(|e| e.to_string())?;
    file
        .write_all(contents.as_bytes())
        .map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
pub fn fs_read_binary_file(path: &str) -> Result<Vec<u8>, String> {
    let mut file = File::open(path).map_err(|e| e.to_string())?;
    let mut contents = Vec::new();
    file.read_to_end(&mut contents).map_err(|e| e.to_string())?;
    Ok(contents)
}

#[tauri::command]
pub fn fs_write_binary_file(path: &str, contents: &[u8]) -> Result<(), String> {
    let mut file = File::create(path).map_err(|e| e.to_string())?;
    file.write_all(contents).map_err(|e| e.to_string())?;
    Ok(())
}

#[derive(Serialize)]
pub struct DirEntry {
    path: String,
    children: Option<Vec<DirEntry>>,
}

fn read_dir<P: AsRef<Path>>(
    path: P,
    recursive: bool,
) -> io::Result<Vec<DirEntry>> {
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
pub fn fs_read_dir(
    path: &str,
    recursive: bool,
) -> Result<Vec<DirEntry>, String> {
    read_dir(path, recursive).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn fs_rename(from: &str, to: &str) -> Result<(), String> {
    fs::rename(from, to).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn fs_delete_file(path: &str) -> Result<(), String> {
    fs::remove_file(path).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn fs_delete_dir(path: &str) -> Result<(), String> {
    fs::remove_dir_all(path).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn fs_create_dir(path: &str) -> Result<(), String> {
    fs::create_dir(path).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn fs_is_file(path: &str) -> bool {
    fs::metadata(path).map(|m| m.is_file()).unwrap_or(false)
}

#[tauri::command]
pub fn fs_is_dir(path: &str) -> bool {
    fs::metadata(path).map(|m| m.is_dir()).unwrap_or(false)
}