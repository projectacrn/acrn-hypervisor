#![cfg_attr(
    all(not(debug_assertions), target_os = "windows"),
    windows_subsystem = "windows"
)]

mod configurator;

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            configurator::get_history,
            configurator::add_history,
            configurator::set_working_folder,
            configurator::write_board,
            configurator::force_reset,
            configurator::get_home,
            configurator::acrn_read,
            configurator::acrn_write,
            configurator::acrn_is_file,
            configurator::acrn_read_dir,
            configurator::acrn_create_dir,
            configurator::acrn_remove_dir,
            configurator::acrn_remove_file,
            configurator::open_devtools
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
