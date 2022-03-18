#![cfg_attr(
all(not(debug_assertions), target_os = "windows"),
windows_subsystem = "windows"
)]

mod filesystem;
mod configurator;

use log::*;

use tauri::{api::cli::get_matches, utils::config::CliConfig, PackageInfo, RunEvent};

fn main() {
    env_logger::init();
    // Init context
    let context = tauri::generate_context!();

    // Build app instance and run
    let app = tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
                  filesystem::fs_rename,
              filesystem::fs_read_dir,
              filesystem::fs_read_text_file,
              filesystem::fs_read_binary_file,
              filesystem::fs_write_text_file,
              filesystem::fs_write_binary_file,
              filesystem::fs_read_dir,
              filesystem::fs_delete_file,
              filesystem::fs_delete_dir,
              filesystem::fs_create_dir,
              filesystem::fs_is_file,
              filesystem::fs_is_dir,
            configurator::get_history,
            configurator::add_history,
            configurator::set_working_folder,
            configurator::write_board,
            configurator::force_reset,
            configurator::get_home
    ])
        .setup(|app| {
            // Handle cli cmdline
            let app_config = app.config();
            let cli_config = app_config.tauri.cli.as_ref().unwrap();
            let package_info = app.package_info();
            handle_cli_arg_data(&cli_config, &package_info);
            Ok(())
        })
        .build(context)
        .expect("error while running tauri application");

    app.run(|_app, event| {
        if let RunEvent::Exit = event {
            info!("Received Exit event");
        }
    });
}


fn handle_cli_arg_data(cli_config: &CliConfig, pkg_info: &PackageInfo) {
    match get_matches(cli_config, pkg_info) {
        Ok(matches) => {
            let mut exit_flag = false;
            if let Some(arg_data) = matches.args.get("help") {
                println!("{}", arg_data.value.as_str().unwrap_or("No help available"));
                exit_flag = true;
            }
            if let Some(arg_data) = matches.args.get("version") {
                println!("{}", arg_data.value.as_str().unwrap_or("No version data available"));
                exit_flag = true
            }
            if exit_flag {
                std::process::exit(1);
            }
        }
        Err(e) => {
            error!("{}", e.to_string());
            std::process::exit(1);
        }
    }
}