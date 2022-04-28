export default interface Config {
    "$schema"?: string
    package?: PackageConfig
    tauri: TauriConfig
    build: BuildConfig
    plugins?: {}
}

interface PackageConfig {
    /// App name.
    productName?: string,
    /// App version. It is a semver version number or a path to a `package.json` file contaning the `version` field.
    version?: string
}

interface TauriConfig {
    pattern?: {
        /**
         * Brownfield pattern.
         */
        Brownfield: any,
        /// Isolation pattern. Recommended for security purposes.
        Isolation: {
            /// The dir containing the index.html file that contains the secure isolation application.
            dir: string,
        },
    }

    windows: WindowConfig[]
    /**
     * app's CLI definition
     */
    cli?: {}
    bundle?: {}
    allowlist?: {}
    security?: {}
    updater?: {}
    system_tray?: {}
    macos_private_api?: {}

}

interface WindowConfig {
    /// The window identifier. It must be alphanumeric.
    label?: string,
    /// The window webview URL.
    url?: string,
    /// Whether the file drop is enabled or not on the webview. By default it is enabled.
    ///
    /// Disabling it is required to use drag and drop on the frontend on Windows.
    file_drop_enabled?: boolean,
    /// Whether or not the window starts centered or not.
    center?: boolean,
    /// The horizontal position of the window's top left corner
    x?: number,
    /// The vertical position of the window's top left corner
    y?: number,
    /// The window width.
    width?: number,
    /// The window height.
    height?: number,
    /// The min window width.
    min_width?: number,
    /// The min window height.
    min_height?: number,
    /// The max window width.
    max_width?: number,
    /// The max window height.
    max_height?: number,
    /// Whether the window is resizable or not.
    resizable?: boolean,
    /// The window title.
    title?: string,
    /// Whether the window starts as fullscreen or not.
    fullscreen?: boolean,
    /// Whether the window will be initially hidden or focused.
    focus?: boolean,
    /// Whether the window is transparent or not.
    ///
    /// Note that on `macOS` this requires the `macos-private-api` feature flag, enabled under `tauri.conf.json > tauri > macosPrivateApi`.
    /// WARNING: Using private APIs on `macOS` prevents your application from being accepted for the `App Store`.
    transparent?: boolean,
    /// Whether the window is maximized or not.
    maximized?: boolean,
    /// Whether the window is visible or not.
    visible?: boolean,
    /// Whether the window should have borders and bars.
    decorations?: boolean,
    /// Whether the window should always be on top of other windows.
    always_on_top?: boolean,
    /// Whether or not the window icon should be added to the taskbar.
    skip_taskbar?: boolean,
}

interface BuildConfig {
    /// The binary used to build and run the application.
    runner?: string,
    /// The path or URL to use on development.
    devPath?: string,
    /// The path to the app's dist dir. This path must contain your index.html file.
    distDir?: string,
    /// A shell command to run before `tauri dev` kicks in.
    ///
    /// The TAURI_PLATFORM, TAURI_ARCH, TAURI_FAMILY, TAURI_PLATFORM_VERSION, TAURI_PLATFORM_TYPE and TAURI_DEBUG environment variables are set if you perform conditional compilation.
    beforeDevCommand?: string,
    /// A shell command to run before `tauri build` kicks in.
    ///
    /// The TAURI_PLATFORM, TAURI_ARCH, TAURI_FAMILY, TAURI_PLATFORM_VERSION, TAURI_PLATFORM_TYPE and TAURI_DEBUG environment variables are set if you perform conditional compilation.
    beforeBuildCommand?: string,
    /// Features passed to `cargo` commands.
    features?: string[],
    /// Whether we should inject the Tauri API on `window.__TAURI__` or not.
    withGlobalTauri?: boolean,
}