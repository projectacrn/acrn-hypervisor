import {mockIPC} from "@tauri-apps/api/mocks";
import type {OpenDialogOptions} from "@tauri-apps/plugin-dialog";
import {joinPath} from "./tauri";
import type {
    DialogPort,
    DialogResult,
    DirEntry,
    FsPort,
    HistoryPort,
    HistoryType,
    PlatformPorts,
    WebMockInitialState,
} from "./types";

const DEFAULT_HOME = 'C:\\Users\\Axel';

function hasPathSeparator(path: string): boolean {
    return path.includes('/') || path.includes('\\');
}

export class WebMockFs implements FsPort {
    private files: Map<string, string>;
    private directories = new Set<string>();

    constructor(files: Record<string, string> = {}) {
        this.files = new Map(Object.entries(files));
        for (const path of this.files.keys()) {
            this.rememberParentDirectories(path);
        }
    }

    private rememberParentDirectories(path: string): void {
        const normalized = path.replace(/\\/g, '/');
        const parts = normalized.split('/');
        parts.pop();
        let current = normalized.startsWith('/') ? '/' : '';
        for (const part of parts) {
            if (!part) {
                continue;
            }
            current = current.endsWith('/') ? current + part : current + '/' + part;
            this.directories.add(current);
            if (path.includes('\\')) {
                this.directories.add(current.replace(/\//g, '\\'));
            }
        }
    }

    private parentPath(path: string): string {
        const normalized = path.replace(/\\/g, '/');
        const index = normalized.lastIndexOf('/');
        if (index < 0) {
            return '';
        }
        const parent = normalized.slice(0, index) || '/';
        return path.includes('\\') ? parent.replace(/\//g, '\\') : parent;
    }

    private childPaths(path: string): { files: string[]; directories: string[] } {
        const prefix = path.endsWith('/') || path.endsWith('\\') ? path : path + (path.includes('\\') ? '\\' : '/');
        const directories = new Set<string>();
        const files: string[] = [];

        for (const candidate of this.files.keys()) {
            if (!candidate.startsWith(prefix)) {
                continue;
            }
            const remainder = candidate.slice(prefix.length);
            const separatorIndex = remainder.search(/[\\/]/);
            if (separatorIndex === -1) {
                files.push(candidate);
            } else {
                directories.add(candidate.slice(0, prefix.length + separatorIndex));
            }
        }
        for (const candidate of this.directories) {
            if (candidate.startsWith(prefix)) {
                const remainder = candidate.slice(prefix.length);
                if (remainder && !hasPathSeparator(remainder)) {
                    directories.add(candidate);
                }
            }
        }
        return {files, directories: [...directories]};
    }

    async readText(path: string): Promise<string> {
        if (!this.files.has(path)) {
            throw new Error(`File does not exist: ${path}`);
        }
        return this.files.get(path) as string;
    }

    async writeTextAtomic(path: string, content: string): Promise<void> {
        this.rememberParentDirectories(path);
        this.files.set(path, content);
    }

    private hasDirectory(path: string): boolean {
        const withoutTrailingSeparator = path.length > 1 ? path.replace(/[\\/]+$/, '') : path;
        return this.directories.has(path) || this.directories.has(withoutTrailingSeparator);
    }

    async list(path: string, recursive = false): Promise<DirEntry[]> {
        const direct = this.childPaths(path);
        if (direct.files.length === 0 && direct.directories.length === 0 &&
            !this.hasDirectory(path) && path !== '') {
            throw new Error(`Directory does not exist: ${path}`);
        }
        return this.listSync(path, recursive);
    }

    private listSync(path: string, recursive: boolean): DirEntry[] {
        const direct = this.childPaths(path);
        return [
            ...direct.files.map(filePath => ({path: filePath, children: null})),
            ...direct.directories.map(directoryPath => ({
                path: directoryPath,
                children: recursive ? this.listSync(directoryPath, true) : null,
            })),
        ].sort((left, right) => left.path.localeCompare(right.path));
    }

    async removeFile(path: string): Promise<void> {
        if (!this.files.delete(path)) {
            throw new Error(`File does not exist: ${path}`);
        }
    }

    async createDir(path: string, recursive = true): Promise<void> {
        if (!recursive) {
            const parent = this.parentPath(path);
            if (parent && !this.hasDirectory(parent)) {
                throw new Error(`Parent directory does not exist: ${parent}`);
            }
        }
        this.directories.add(path);
        if (recursive) {
            this.rememberParentDirectories(joinPath(path, 'placeholder'));
        }
    }

    join(...parts: string[]): string {
        return joinPath(...parts);
    }
}

export class WebMockDialog implements DialogPort {
    async open(_options: OpenDialogOptions): Promise<DialogResult> {
        return null;
    }
}

export class WebMockHistory implements HistoryPort {
    private values: Record<HistoryType, string[]>;

    constructor(history: WebMockInitialState['history'] = {}) {
        this.values = {
            WorkingFolder: [...(history?.WorkingFolder ?? [])],
            Board: [...(history?.Board ?? [])],
            Scenario: [...(history?.Scenario ?? [])],
        };
    }

    async get(type: HistoryType): Promise<string[]> {
        return [...this.values[type]];
    }

    async add(type: HistoryType, path: string): Promise<void> {
        this.values[type] = [path, ...this.values[type].filter(value => value !== path)];
    }
}

export function createWebMockPlatform(initialState: WebMockInitialState = {}): PlatformPorts {
    return {
        fs: new WebMockFs(initialState.files),
        dialog: new WebMockDialog(),
        history: new WebMockHistory(initialState.history),
        getHome: async () => initialState.home ?? DEFAULT_HOME,
    };
}

export function installWebMock(initialState: WebMockInitialState = {}): PlatformPorts {
    const platform = createWebMockPlatform(initialState);
    const originalConfirm = window.confirm.bind(window);
    window.confirm = (async (message?: string) => originalConfirm(message)) as unknown as typeof window.confirm;

    mockIPC(async (cmd, args) => {
        const packageInfo = await import('../../package.json');
        switch (cmd) {
            case 'get_home':
                return platform.getHome();
            case 'get_history':
                return JSON.stringify(await platform.history.get(args.historyType));
            case 'add_history':
                return platform.history.add(args.historyType, args.historyPath);
            case 'acrn_read':
                return platform.fs.readText(args.filePath);
            case 'acrn_write':
                return platform.fs.writeTextAtomic(args.filePath, args.contents);
            case 'acrn_read_dir':
                return platform.fs.list(args.path, args.recursive);
            case 'acrn_create_dir':
                return platform.fs.createDir(args.path, args.recursive);
            case 'acrn_remove_file':
                return platform.fs.removeFile(args.path);
            default:
                if (args?.message?.cmd === 'getAppVersion') {
                    return packageInfo.version;
                }
                console.log(cmd, args);
                return {};
        }
    });
    return platform;
}
