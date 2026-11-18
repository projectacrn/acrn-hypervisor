const fs = require('fs')
const path = require('path')
const {pipeline} = require('stream/promises')

import type {Plugin, ResolvedConfig} from "vite";
import replace from "@rollup/plugin-replace";

// @ts-ignore
import cli from "@tauri-apps/cli"


async function copyFolder(copiedPath, resultPath, direct) {
    if (!direct) {
        copiedPath = path.join(__dirname, copiedPath)
        resultPath = path.join(__dirname, resultPath)
    }

    if (!fs.existsSync(copiedPath)) {
        throw new Error(`Source folder does not exist: ${copiedPath}`)
    }

    await fs.promises.mkdir(resultPath, {recursive: true})
    const files = await fs.promises.readdir(copiedPath, {withFileTypes: true})
    await Promise.all(files.map(async (file) => {
        const source = path.join(copiedPath, file.name)
        const destination = path.join(resultPath, file.name)
        if (file.isFile()) {
            await pipeline(fs.createReadStream(source), fs.createWriteStream(destination))
        } else {
            await copyFolder(source, destination, true)
        }
    }))
}


export {copyFolder}

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
        async closeBundle() {
            if (!process.env.TAURI_BUILD) {
                process.env.TAURI_BUILD = "true";
                await copyFolder('../thirdLib', '../build/thirdLib', false)
                await tauri('build')
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