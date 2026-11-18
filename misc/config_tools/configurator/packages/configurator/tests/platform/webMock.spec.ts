import {describe, expect, it} from 'vitest';
import {createWebMockPlatform} from '../../src/platform/webMock';

describe('browser platform mock', () => {
    it('provides an in-memory filesystem with recursive directory entries', async () => {
        const platform = createWebMockPlatform({
            files: {
                '/work/board.xml': 'board',
                '/work/nested/scenario.xml': 'scenario',
            },
        });

        await expect(platform.fs.readText('/work/board.xml')).resolves.toBe('board');
        await platform.fs.writeTextAtomic('/work/scenario.xml', 'new scenario');
        await expect(platform.fs.readText('/work/scenario.xml')).resolves.toBe('new scenario');

        const entries = await platform.fs.list('/work', true);
        expect(entries.map(entry => entry.path)).toEqual([
            '/work/board.xml', '/work/nested', '/work/scenario.xml',
        ]);
        expect(entries[1].children).toEqual([
            {path: '/work/nested/scenario.xml', children: null},
        ]);

        await platform.fs.removeFile('/work/board.xml');
        await expect(platform.fs.readText('/work/board.xml')).rejects.toThrow('File does not exist');
    });

    it('supports directory creation and the same path joining contract', async () => {
        const platform = createWebMockPlatform();
        await platform.fs.createDir('/work/nested', true);
        await expect(platform.fs.list('/work/nested')).resolves.toEqual([]);
        await expect(platform.fs.list('/work/nested/')).resolves.toEqual([]);
        expect(platform.fs.join('/work', 'nested', 'scenario.xml')).toBe('/work/nested/scenario.xml');
    });

    it('keeps history newest-first and removes duplicates', async () => {
        const platform = createWebMockPlatform({history: {Board: ['/old.xml', '/current.xml']}});

        await platform.history.add('Board', '/new.xml');
        await platform.history.add('Board', '/old.xml');
        await expect(platform.history.get('Board')).resolves.toEqual([
            '/old.xml', '/new.xml', '/current.xml',
        ]);
    });

    it('provides browser defaults for home and cancelled file dialogs', async () => {
        const platform = createWebMockPlatform();
        await expect(platform.getHome()).resolves.toBe('C:\\Users\\Axel');
        await expect(platform.dialog.open({multiple: false})).resolves.toBeNull();
    });

    it('supports Windows parent directories, separators, and recursive entries', async () => {
        const platform = createWebMockPlatform({
            files: {'C:\\work\\nested\\board.xml': 'board'},
        });

        await expect(platform.fs.list('C:\\work', true)).resolves.toEqual([
            {
                path: 'C:\\work\\nested',
                children: [{path: 'C:\\work\\nested\\board.xml', children: null}],
            },
        ]);

        await platform.fs.createDir('C:\\work', true);
        await platform.fs.createDir('C:\\work\\empty', false);
        await expect(platform.fs.list('C:\\work\\empty')).resolves.toEqual([]);
        await expect(platform.fs.list('C:\\work\\empty\\')).resolves.toEqual([]);
        await expect(platform.fs.createDir('C:\\missing\\child', false))
            .rejects.toThrow('Parent directory does not exist: C:\\missing');
    });

    it('creates non-recursive directories at filesystem and relative roots', async () => {
        const platform = createWebMockPlatform({files: {'/seed/file.txt': 'seed'}});

        await platform.fs.createDir('/', true);
        await platform.fs.createDir('/root-child', false);
        await expect(platform.fs.list('/root-child')).resolves.toEqual([]);

        await platform.fs.createDir('standalone', false);
        await expect(platform.fs.list('standalone')).resolves.toEqual([]);

        await platform.fs.createDir('/deep/nested');
        await expect(platform.fs.list('/deep/nested')).resolves.toEqual([]);
    });

    it('reports missing files, directories, and non-recursive parents', async () => {
        const platform = createWebMockPlatform();

        await expect(platform.fs.readText('/missing.txt'))
            .rejects.toThrow('File does not exist: /missing.txt');
        await expect(platform.fs.list('/missing'))
            .rejects.toThrow('Directory does not exist: /missing');
        await expect(platform.fs.createDir('/missing/child', false))
            .rejects.toThrow('Parent directory does not exist: /missing');
        await expect(platform.fs.removeFile('/missing.txt'))
            .rejects.toThrow('File does not exist: /missing.txt');
    });
});
