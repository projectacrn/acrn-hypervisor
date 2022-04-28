import path from "path";

import {defineConfig} from 'vite'
import vue from '@vitejs/plugin-vue'
import tauri from "./thirdLib/tauri-plugin";

// https://vitejs.dev/config/
export default defineConfig({
    base: './',
    plugins: [vue(), tauri()],
    resolve: {
        extensions: ['.mjs', '.js', '.ts', '.jsx', '.tsx', '.json', '.vue']
    },
    build: {
        outDir: path.resolve(__dirname, 'build')
    }
})
