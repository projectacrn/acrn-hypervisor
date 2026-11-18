import {createTauriPlatform} from "./platform/tauri";
import {installWebMock} from "./platform/webMock";
import {setPlatform} from "./platform";

const isTauri = !!window.__TAURI_INTERNALS__;
window.isDev = process.env.NODE_ENV === 'development';

import {createApp} from 'vue';
import {createPinia} from 'pinia';
import App from './App.vue';

import router from "./router";
import BootstrapVue3 from 'bootstrap-vue-3'
import naive from 'naive-ui';

const app = createApp(App);
app.use(createPinia());
app.use(BootstrapVue3);
app.use(naive);
app.use(router);
app.config.unwrapInjectedRef = true


async function main() {
    let platform;
    if (isTauri) {
        platform = createTauriPlatform();
    } else {
        const mockData = (await import('../tests/data/data.json')).default;
        platform = installWebMock({files: mockData.files, history: mockData.history});
    }
    setPlatform(platform);

    console.log("Pyodide Load Begin")
    let t1 = Date.now();
    let WASMPythonLoader = await import('./pyodide');
    await WASMPythonLoader.default()
    let t2 = Date.now();
    console.log("Pyodide Load Time: " + (t2 - t1) + "ms")

    async function setWindowSystemInfo() {
        let homeDir = await platform.getHome();
        let pathSplit = homeDir.indexOf("\\") > 0 ? "\\" : "/";
        window.systemInfo = {
            homeDir, pathSplit
        }
    }

    await setWindowSystemInfo();

    app.mount('#app')
}

main();
