#![cfg_attr(
    all(not(debug_assertions), target_os = "windows"),
    windows_subsystem = "windows"
)]

mod commands;
mod config;
mod error;
mod fsutil;
mod state;

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(state::AppState::new())
        .invoke_handler(tauri::generate_handler![
            commands::history::get_history,
            commands::history::add_history,
            commands::workspace::set_working_folder,
            commands::workspace::write_board,
            commands::history::force_reset,
            commands::workspace::get_home,
            commands::fs::acrn_read,
            commands::fs::acrn_write,
            commands::fs::acrn_is_file,
            commands::fs::acrn_read_dir,
            commands::fs::acrn_create_dir,
            commands::fs::acrn_remove_dir,
            commands::fs::acrn_remove_file,
            commands::workspace::open_devtools
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
