const fs = require('fs')
const path = require('path')

import type {ConfigEnv, Plugin, ResolvedConfig} from "vite";
import replace from "@rollup/plugin-replace";

import cli from "@tauri-apps/cli"
import Config from "../../src-tauri/types/config"

import tauriConf from "../../src-tauri/tauri.json";


interface Options {
    config?: (c: Config, e: ConfigEnv) => Config;
}


export default (options?: Options): Plugin => {
    let tauriConfig: Config = {...tauriConf};
    let viteConfig: ResolvedConfig;

    const tauri = (mode: "dev" | "build"): Promise<any> => {
        // Generate `tauri.conf.json` by `tauri.json`.
        console.log("Generate `tauri.conf.json` by `tauri.json`.")
        let filePath = path.resolve(__dirname, '..', '..', 'src-tauri', 'tauri.conf.json')
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