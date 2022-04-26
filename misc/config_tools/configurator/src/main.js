const isTauri = !!window.__TAURI_IPC__;

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
}

import {createApp} from 'vue'
import App from './App.vue'
import {loadPyodide} from "/thirdLib/pyodide/pyodide";

import router from "./router";
import {invoke} from "@tauri-apps/api/tauri";
import {mockIPC} from "@tauri-apps/api/mocks";
import BootstrapVue3 from 'bootstrap-vue-3'
import naive from 'naive-ui';

const app = createApp(App);
app.use(BootstrapVue3);
app.use(naive);
app.use(router);
app.config.unwrapInjectedRef = true

if (!isTauri) {
    // Patch Browser function to mock Tauri env
    const origin_confirm = window.confirm;
    window.confirm = async (message) => origin_confirm(message);

    // mock custom tauri command
    let tempData = {
        history: {WorkingFolder: [], Board: [], Scenario: []}
    }
    mockIPC(async (cmd, args) => {
        if (cmd === 'get_home') {
            return 'C:\\Users\\Axel'
        }
        if (cmd === 'dialogOpenFolder') {
            return 'C:\\acrn'
        }
        if (cmd === 'get_history') {
            return JSON.stringify(tempData.history[args.historyType])
        }
        if (cmd === 'add_history') {
            return tempData.history[args.historyType].push(args.path)
        }
        if (args?.message?.cmd === "getAppVersion") {
            const packageInfo = await import('../package.json');
            return packageInfo.version;
        }

        console.log(cmd, args)
        return {}
    })
}

async function main() {
    console.log("Pyodide Load Begin")
    let t1 = Date.now();
    let pyodide = await loadPyodide({
        indexURL: '/thirdLib/pyodide/'
    });
    await pyodide.loadPackage(['micropip', 'lxml', 'beautifulsoup4'])
    await pyodide.runPythonAsync(`
        import micropip
        await micropip.install([
            './thirdLib/xmltodict-0.12.0-py2.py3-none-any.whl',
            './thirdLib/elementpath-2.4.0-py3-none-any.whl',
            './thirdLib/defusedxml-0.7.1-py2.py3-none-any.whl',
            './thirdLib/xmlschema-1.9.2-py3-none-any.whl',
            './thirdLib/acrn_config_tools-3.0-py3-none-any.whl'
        ])
    `)

    function test() {
        let result = pyodide.runPython(`
            import sys
            sys.version
        `)
        console.log(result);
    }

    test()

    // pyodide load success
    window.pyodide = pyodide;
    let t2 = Date.now();
    console.log("Pyodide Load Time: " + (t2 - t1) + "ms")

    let homeDir = await invoke("get_home");
    let pathSplit = "/";
    if (homeDir.indexOf("\\") > 0) {
        pathSplit = "\\"
    }

    window.systemInfo = {
        homeDir, pathSplit
    }

    app.mount('#app')
}

main();
