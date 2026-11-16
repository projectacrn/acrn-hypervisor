import {afterEach, describe, expect, it} from 'vitest'
import fs from 'fs'
import os from 'os'
import path from 'path'
import {copyFolder} from '../thirdLib/tauri-plugin'

const temporaryDirectories = []

afterEach(async () => {
    await Promise.all(temporaryDirectories.splice(0).map(directory =>
        fs.promises.rm(directory, {recursive: true, force: true})
    ))
})

describe('third-party build copying', () => {
    it('finishes every recursive file copy before resolving', async () => {
        const directory = await fs.promises.mkdtemp(path.join(os.tmpdir(), 'acrn-copy-'))
        temporaryDirectories.push(directory)
        const source = path.join(directory, 'source')
        const destination = path.join(directory, 'destination')
        await fs.promises.mkdir(path.join(source, 'nested'), {recursive: true})
        const content = Buffer.alloc(2 * 1024 * 1024, 'x')
        await fs.promises.writeFile(path.join(source, 'nested', 'library.dat'), content)

        await copyFolder(source, destination, true)

        const copied = await fs.promises.readFile(path.join(destination, 'nested', 'library.dat'))
        expect(copied.equals(content)).toBe(true)
    })

    it('fails the build when the source folder is missing', async () => {
        const directory = await fs.promises.mkdtemp(path.join(os.tmpdir(), 'acrn-copy-'))
        temporaryDirectories.push(directory)

        await expect(copyFolder(
            path.join(directory, 'missing'),
            path.join(directory, 'destination'),
            true
        )).rejects.toThrow('Source folder does not exist')
    })
})
