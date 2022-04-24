const fs = require('fs')
const path = require('path')

import type {ConfigEnv, Plugin, ResolvedConfig} from "vite";
// @ts-ignore
import replace from "@rollup/plugin-replace";

// @ts-ignore
import cli from "@tauri-apps/cli"
import Config from "../src-tauri/types/config"

// @ts-ignore
import tauriConf from "../src-tauri/tauri.json";


interface Options {
    config?: (c: Config, e: ConfigEnv) => Config;
}

function copyFolder(copiedPath, resultPath, direct) {
    if (!direct) {
        copiedPath = path.join(__dirname, copiedPath)
        resultPath = path.join(__dirname, resultPath)
    }

    function createDir(dirPath) {
        fs.mkdirSync(dirPath)
    }

    if (fs.existsSync(copiedPath)) {
        createDir(resultPath)
        /**
         * @des 方式一：利用子进程操作命令行方式
         */
        // child_process.spawn('cp', ['-r', copiedPath, resultPath])

        /**
         * @des 方式二：
         */
        const files = fs.readdirSync(copiedPath, {withFileTypes: true});
        for (let i = 0; i < files.length; i++) {
            const cf = files[i]
            const ccp = path.join(copiedPath, cf.name)
            const crp = path.join(resultPath, cf.name)
            if (cf.isFile()) {
                /**
                 * @des 创建文件,使用流的形式可以读写大文件
                 */
                const readStream = fs.createReadStream(ccp)
                const writeStream = fs.createWriteStream(crp)
                readStream.pipe(writeStream)
            } else {
                try {
                    /**
                     * @des 判断读(R_OK | W_OK)写权限
                     */
                    fs.accessSync(path.join(crp, '..'), fs.constants.W_OK)
                    copyFolder(ccp, crp, true)
                } catch (error) {
                    console.log('folder write error:', error);
                }

            }
        }
    } else {
        console.log('do not exist path: ', copiedPath);
    }
}


export default (options?: Options): Plugin => {
    let tauriConfig: Config = {...tauriConf};
    let viteConfig: ResolvedConfig;

    const tauri = (mode: "dev" | "build"): Promise<any> => {
        // Generate `tauri.conf.json` by `tauri.json`.
        console.log("Generate `tauri.conf.json` by `tauri.json`.")
        let filePath = path.resolve(__dirname, '..', 'src-tauri', 'tauri.conf.json')
        let config = JSON.stringify(tauriConfig)
        try {
            fs.writeFileSync(filePath, config)
            return cli.run([mode], 'tauri')
        } catch (err) {
            console.error(err)
            return Promise.reject(err)
        }
    }

    return {
        ...replace({
            "process.env.IS_TAURI": "true",
            preventAssignment: false
        }),
        name: "tauri-plugin",
        configureServer(server) {
            server?.httpServer?.on("listening", () => {
                if (!process.env.TAURI_SERVE) {
                    process.env.TAURI_SERVE = "true";
                    delete tauriConfig["$schema"]
                    tauri('dev').finally()
                }
            });
        },
        closeBundle() {
            if (!process.env.TAURI_BUILD) {
                process.env.TAURI_BUILD = "true";
                delete tauriConfig["$schema"]
                copyFolder('../thirdLib', '../build/thirdLib', false)
                tauri('build').finally()
            }
        },
        config(viteConfig, env) {
            process.env.IS_TAURI = "true";
            if (options && options.config) {
                options.config(tauriConfig, env);
            }
            if (env.command === "build") {
                viteConfig.base = "/";
            }
        },
        configResolved(resolvedConfig) {
            viteConfig = resolvedConfig;
        },
    };
};