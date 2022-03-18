import {appWindow} from "@tauri-apps/api/window";

class WindowHelper {
    minimal = (cb = null) => {
        appWindow.minimize().then(() => {
            cb ? cb() : false
        })
    }

    maxmal = (cb = null) => {
        appWindow.toggleMaximize().then(() => {
            cb ? cb() : false
        })
    }

    close = () => {
        appWindow.close().then()
    }
}


export const windowHelper = new WindowHelper()