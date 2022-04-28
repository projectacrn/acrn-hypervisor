import {mockIPC} from "@tauri-apps/api/mocks";
import mockData from './data/data.json';

export default function mock() {

    const origin_confirm = window.confirm;
    window.confirm = async (message) => origin_confirm(message);

    // mock custom tauri command
    mockIPC(async (cmd, args) => {
        const packageInfo = await import('../package.json');

        function handle() {
            switch (cmd) {
                case 'get_home':
                    return 'C:\\Users\\Axel'
                case 'get_history':
                    return JSON.stringify(mockData.history[args.historyType])
                case 'acrn_read':
                    return mockData.files[args.filePath]
                default:
                    if (args?.message?.cmd === "getAppVersion") {
                        return packageInfo.version;
                    }

                    console.log(cmd, args)
                    return {}
            }
        }

        return handle()
    })
}