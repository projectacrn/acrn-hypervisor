import {afterEach, describe, expect, it, vi} from 'vitest';
import {invoke} from '@tauri-apps/api/core';
import {clearMocks} from '@tauri-apps/api/mocks';
import packageInfo from '../../package.json';
import {installWebMock} from '../../src/platform/webMock';

const originalConfirm = window.confirm;

describe('installed browser platform mock', () => {
    afterEach(() => {
        clearMocks();
        vi.restoreAllMocks();
        window.confirm = originalConfirm;
    });

    it('installs defaults and preserves confirmation results asynchronously', async () => {
        const confirm = vi.spyOn(window, 'confirm').mockReturnValue(true);
        installWebMock();

        const confirmation = window.confirm('Continue in the browser?');
        expect(confirm).toHaveBeenCalledWith('Continue in the browser?');
        await expect(Promise.resolve(confirmation)).resolves.toBe(true);
        await expect(invoke('get_home')).resolves.toBe('C:\\Users\\Axel');
    });

    it('returns JSON history and persists additions through IPC', async () => {
        installWebMock({history: {Board: ['/existing.xml']}});

        await expect(invoke('get_history', {historyType: 'Board'}))
            .resolves.toBe('["/existing.xml"]');
        await expect(invoke('add_history', {
            historyType: 'Board', historyPath: '/new.xml',
        })).resolves.toBeUndefined();
        await expect(invoke('get_history', {historyType: 'Board'}))
            .resolves.toBe('["/new.xml","/existing.xml"]');
    });

    it('dispatches every filesystem command to the same in-memory state', async () => {
        installWebMock({
            files: {
                '/work/board.xml': 'board',
                '/work/nested/scenario.xml': 'scenario',
            },
        });

        await expect(invoke('acrn_read', {filePath: '/work/board.xml'})).resolves.toBe('board');
        await expect(invoke('acrn_write', {
            filePath: '/work/new.xml', contents: 'new contents',
        })).resolves.toBeUndefined();
        await expect(invoke('acrn_read', {filePath: '/work/new.xml'})).resolves.toBe('new contents');
        await expect(invoke('acrn_read_dir', {path: '/work', recursive: true})).resolves.toEqual([
            {path: '/work/board.xml', children: null},
            {
                path: '/work/nested',
                children: [{path: '/work/nested/scenario.xml', children: null}],
            },
            {path: '/work/new.xml', children: null},
        ]);

        await expect(invoke('acrn_create_dir', {
            path: '/work/empty/nested', recursive: true,
        })).resolves.toBeUndefined();
        await expect(invoke('acrn_read_dir', {
            path: '/work/empty/nested', recursive: false,
        })).resolves.toEqual([]);

        await expect(invoke('acrn_remove_file', {path: '/work/board.xml'}))
            .resolves.toBeUndefined();
        await expect(invoke('acrn_read', {filePath: '/work/board.xml'}))
            .rejects.toThrow('File does not exist: /work/board.xml');
    });

    it('returns the app version and handles unknown commands', async () => {
        const log = vi.spyOn(console, 'log').mockImplementation(() => {});
        installWebMock();

        await expect(invoke('plugin:app|version', {message: {cmd: 'getAppVersion'}}))
            .resolves.toBe(packageInfo.version);

        const args = {message: {cmd: 'anotherCommand'}, value: 7};
        await expect(invoke('unhandled_command', args)).resolves.toEqual({});
        expect(log).toHaveBeenCalledWith('unhandled_command', args);

        await expect(invoke('unhandled_without_args')).resolves.toEqual({});
        expect(log).toHaveBeenCalledWith('unhandled_without_args', {});

        await expect(invoke('unhandled_null_args', null as any)).resolves.toEqual({});
        expect(log).toHaveBeenCalledWith('unhandled_null_args', null);
    });
});
