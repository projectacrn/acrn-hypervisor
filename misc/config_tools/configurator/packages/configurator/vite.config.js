import path from "path";
import child_process from "child_process"

import {defineConfig} from 'vite'
import vue from '@vitejs/plugin-vue'
import tauri from "./thirdLib/tauri-plugin";

const packageVersion = child_process.execSync('git describe --dirty')
console.log("packageVersion: " + packageVersion)

// https://vitejs.dev/config/
export default defineConfig({
    base: './',
    plugins: [vue(), tauri()],
    resolve: {
        extensions: ['.mjs', '.js', '.ts', '.jsx', '.tsx', '.json', '.vue']
    },
    build: {
        outDir: path.resolve(__dirname, 'build')
    },
    define: {
        packageVersion: JSON.stringify(packageVersion.toString())
    }
})
