import type {OpenDialogOptions} from "@tauri-apps/plugin-dialog";

export interface DirEntry {
    path: string;
    children?: DirEntry[] | null;
}

export type HistoryType = 'WorkingFolder' | 'Board' | 'Scenario';
export type HistoryTypes = HistoryType;

export interface FsPort {
    readText(path: string): Promise<string>;
    writeTextAtomic(path: string, content: string): Promise<void>;
    list(path: string, recursive?: boolean): Promise<DirEntry[]>;
    removeFile(path: string): Promise<void>;
    createDir(path: string, recursive?: boolean): Promise<void>;
    join(...parts: string[]): string;
}

export type DialogResult = string | string[] | null;

export interface DialogPort {
    open(options: OpenDialogOptions): Promise<DialogResult>;
}

export interface HistoryPort {
    get(type: HistoryType): Promise<string[]>;
    add(type: HistoryType, path: string): Promise<void>;
}

export interface PlatformPorts {
    fs: FsPort;
    dialog: DialogPort;
    history: HistoryPort;
    getHome(): Promise<string>;
}

export interface WebMockInitialState {
    files?: Record<string, string>;
    history?: Partial<Record<HistoryType, string[]>>;
    home?: string;
}
