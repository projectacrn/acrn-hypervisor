const isTauri = !!window.__TAURI_IPC__;
window.isDev = process.env.NODE_ENV === 'development';

if (isTauri) {
    let openCount = 0

    function openDevTools() {
        openCount++;
        console.log(`openCount ${openCount} of 5`)
        if (openCount >= 5) {
            invoke('open_devtools', {})
        }
    }

    window.openDevTools = openDevTools;
} else {
    (async () => {
        // Patch Browser function to mock Tauri env
        let mockJS = await import('../tests/mock');
        mockJS.default();
    })()
}

import {createApp} from 'vue';
import App from './App.vue';

import router from "./router";
import {invoke} from "@tauri-apps/api/tauri";
import BootstrapVue3 from 'bootstrap-vue-3'
import naive from 'naive-ui';

const app = createApp(App);
app.use(BootstrapVue3);
app.use(naive);
app.use(router);
app.config.unwrapInjectedRef = true


async function main() {
    console.log("Pyodide Load Begin")
    let t1 = Date.now();
    let WASMPythonLoader = await import('./pyodide');
    await WASMPythonLoader.default()
    let t2 = Date.now();
    console.log("Pyodide Load Time: " + (t2 - t1) + "ms")

    async function setWindowSystemInfo() {
        let homeDir = await invoke("get_home");
        let pathSplit = homeDir.indexOf("\\") > 0 ? "\\" : "/";
        window.systemInfo = {
            homeDir, pathSplit
        }
    }

    await setWindowSystemInfo();

    app.mount('#app')
}

main();
