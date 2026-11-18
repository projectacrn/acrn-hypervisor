import {beforeEach, describe, expect, it, vi} from 'vitest';
import {invoke} from '@tauri-apps/api/core';
import * as dialog from '@tauri-apps/plugin-dialog';
import {createTauriPlatform, joinPath} from '../../src/platform/tauri';

vi.mock('@tauri-apps/api/core', () => ({invoke: vi.fn()}));
vi.mock('@tauri-apps/plugin-dialog', () => ({open: vi.fn()}));

describe('Tauri platform ports', () => {
    beforeEach(() => {
        vi.resetAllMocks();
    });

    it('maps filesystem operations to the existing commands', async () => {
        (invoke as any).mockResolvedValueOnce('board xml');
        (invoke as any).mockResolvedValueOnce(undefined);
        (invoke as any).mockResolvedValueOnce([{path: '/work/scenario.xml', children: null}]);
        (invoke as any).mockResolvedValue(undefined);
        const platform = createTauriPlatform();

        await expect(platform.fs.readText('/work/board.xml')).resolves.toBe('board xml');
        await platform.fs.writeTextAtomic('/work/scenario.xml', 'scenario xml');
        await expect(platform.fs.list('/work', false)).resolves.toEqual([
            {path: '/work/scenario.xml', children: null},
        ]);
        await platform.fs.createDir('/work/nested', true);
        await platform.fs.removeFile('/work/old.xml');

        expect(invoke).toHaveBeenNthCalledWith(1, 'acrn_read', {filePath: '/work/board.xml'});
        expect(invoke).toHaveBeenNthCalledWith(2, 'acrn_write', {
            filePath: '/work/scenario.xml', contents: 'scenario xml',
        });
        expect(invoke).toHaveBeenNthCalledWith(3, 'acrn_read_dir', {path: '/work', recursive: false});
        expect(invoke).toHaveBeenNthCalledWith(4, 'acrn_create_dir', {path: '/work/nested', recursive: true});
        expect(invoke).toHaveBeenNthCalledWith(5, 'acrn_remove_file', {path: '/work/old.xml'});
    });

    it('preserves history, dialog, and home-directory behavior', async () => {
        (invoke as any).mockResolvedValueOnce('["/work/"]');
        (invoke as any).mockResolvedValueOnce(undefined);
        (invoke as any).mockResolvedValueOnce('/home/test');
        (dialog.open as any).mockResolvedValue('/work/board.xml');
        const platform = createTauriPlatform();

        await expect(platform.history.get('WorkingFolder')).resolves.toEqual(['/work/']);
        await platform.history.add('WorkingFolder', '/work/');
        await expect(platform.getHome()).resolves.toBe('/home/test');
        await expect(platform.dialog.open({directory: true})).resolves.toBe('/work/board.xml');

        expect(invoke).toHaveBeenCalledWith('get_history', {historyType: 'WorkingFolder'});
        expect(invoke).toHaveBeenCalledWith('add_history', {
            historyType: 'WorkingFolder', historyPath: '/work/',
        });
        expect(invoke).toHaveBeenCalledWith('get_home');
        expect(dialog.open).toHaveBeenCalledWith({directory: true});
    });

    it('returns an empty history for a non-string backend response', async () => {
        (invoke as any).mockResolvedValue(undefined);
        await expect(createTauriPlatform().history.get('Board')).resolves.toEqual([]);
    });

    it('joins POSIX and Windows paths without requiring a trailing separator', () => {
        expect(joinPath('/work', 'scenario.xml')).toBe('/work/scenario.xml');
        expect(joinPath('/work/', '/scenario.xml')).toBe('/work/scenario.xml');
        expect(joinPath('C:\\work', 'scenario.xml')).toBe('C:\\work\\scenario.xml');
        expect(joinPath('/', 'scenario.xml')).toBe('/scenario.xml');
        expect(createTauriPlatform().fs.join('/work', 'nested', 'scenario.xml'))
            .toBe('/work/nested/scenario.xml');
    });
});
