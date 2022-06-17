import path from "path";
import child_process from "child_process"

import {defineConfig} from 'vite'
import vue from '@vitejs/plugin-vue'
import tauri from "./thirdLib/tauri-plugin";

let versionMatcher = /release_([\d.]+)/;

let branchVersion = child_process.execSync('git rev-parse --abbrev-ref HEAD').toString()
if (versionMatcher.test(branchVersion)) {
    branchVersion = versionMatcher.exec(branchVersion)[1].toString()
} else {
    branchVersion = 'latest'
}

const packageVersion = child_process.execSync('git describe --dirty')
console.log('branchVersion: ' + branchVersion)
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
        branchVersion: JSON.stringify(branchVersion),
        packageVersion: JSON.stringify(packageVersion.toString())
    }
})
