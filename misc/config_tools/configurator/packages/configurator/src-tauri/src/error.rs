/// Error type used by the Tauri command boundary.
///
/// Keep errors as strings so existing frontend error handling remains unchanged.
pub type CommandResult<T> = Result<T, String>;
