import path from "path";

import {defineConfig} from 'vite'

import react from '@vitejs/plugin-react'
import tauri from './src/plugin/tauri-plugin'
import textFileResolver from './src/plugin/text-plugin'


// https://vitejs.dev/config/
export default defineConfig({
    base: './',
    plugins: [react(), tauri(), textFileResolver(/\.(xsd|py|txt)$/)],
    build: {
        outDir: path.resolve(__dirname, 'build')
    }
})
