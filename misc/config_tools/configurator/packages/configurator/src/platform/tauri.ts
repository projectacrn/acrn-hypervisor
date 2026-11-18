import {invoke} from "@tauri-apps/api/core";
import * as dialog from "@tauri-apps/plugin-dialog";
import type {OpenDialogOptions} from "@tauri-apps/plugin-dialog";
import type {
    DialogPort,
    DialogResult,
    DirEntry,
    FsPort,
    HistoryPort,
    HistoryType,
    PlatformPorts,
} from "./types";

export function joinPath(...parts: string[]): string {
    const nonEmptyParts = parts.filter(part => part.length > 0);
    if (nonEmptyParts.length === 0) {
        return '';
    }

    return nonEmptyParts.slice(1).reduce((result, part) => {
        const separator = result.includes('\\') && !result.includes('/') ? '\\' : '/';
        const child = part.replace(/^[\\/]+/, '');
        if (result.endsWith('/') || result.endsWith('\\')) {
            return result + child;
        }
        return result + separator + child;
    }, nonEmptyParts[0]);
}

export class TauriFs implements FsPort {
    readText(path: string): Promise<string> {
        return invoke<string>('acrn_read', {filePath: path});
    }

    async writeTextAtomic(path: string, content: string): Promise<void> {
        await invoke('acrn_write', {filePath: path, contents: content});
    }

    list(path: string, recursive = false): Promise<DirEntry[]> {
        return invoke<DirEntry[]>('acrn_read_dir', {path, recursive});
    }

    async removeFile(path: string): Promise<void> {
        await invoke('acrn_remove_file', {path});
    }

    async createDir(path: string, recursive = true): Promise<void> {
        await invoke('acrn_create_dir', {path, recursive});
    }

    join(...parts: string[]): string {
        return joinPath(...parts);
    }
}

export class TauriDialog implements DialogPort {
    open(options: OpenDialogOptions): Promise<DialogResult> {
        return dialog.open(options) as Promise<DialogResult>;
    }
}

export class TauriHistory implements HistoryPort {
    async get(type: HistoryType): Promise<string[]> {
        const historyJsonText = await invoke<string>('get_history', {historyType: type});
        return typeof historyJsonText === 'string' ? JSON.parse(historyJsonText) : [];
    }

    async add(type: HistoryType, path: string): Promise<void> {
        await invoke('add_history', {historyType: type, historyPath: path});
    }
}

export function createTauriPlatform(): PlatformPorts {
    return {
        fs: new TauriFs(),
        dialog: new TauriDialog(),
        history: new TauriHistory(),
        getHome: () => invoke<string>('get_home'),
    };
}
