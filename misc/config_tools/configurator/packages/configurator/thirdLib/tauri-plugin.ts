const fs = require('fs')
const path = require('path')

import type {Plugin, ResolvedConfig} from "vite";
import replace from "@rollup/plugin-replace";

// @ts-ignore
import cli from "@tauri-apps/cli"


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


export default (): Plugin => {
    let viteConfig: ResolvedConfig;

    const tauri = (mode: "dev" | "build"): Promise<any> => {
        try {
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
                    tauri('dev').finally()
                }
            });
        },
        closeBundle() {
            if (!process.env.TAURI_BUILD) {
                process.env.TAURI_BUILD = "true";
                copyFolder('../thirdLib', '../build/thirdLib', false)
                tauri('build').finally()
            }
        },
        config(viteConfig, env) {
            process.env.IS_TAURI = "true";
        },
        configResolved(resolvedConfig) {
            viteConfig = resolvedConfig;
        },
    };
};