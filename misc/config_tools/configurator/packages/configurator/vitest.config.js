import {defineConfig} from 'vitest/config'
import vue from '@vitejs/plugin-vue'
import path from 'path'

export default defineConfig({
    plugins: [vue()],
    resolve: {
        extensions: ['.mjs', '.js', '.ts', '.jsx', '.tsx', '.json', '.vue'],
        alias: {
            '@lljj/vjsf-utils': path.resolve(__dirname, '../vue-json-schema-form/utils')
        }
    },
    test: {
        environment: 'jsdom',
        include: ['tests/**/*.spec.{js,ts}']
    }
})
