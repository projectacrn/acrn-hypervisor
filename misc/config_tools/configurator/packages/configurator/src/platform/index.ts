import {createTauriPlatform} from "./tauri";
import {createWebMockPlatform} from "./webMock";
import type {PlatformPorts} from "./types";

let platform: PlatformPorts | undefined;

export function isTauri(): boolean {
    return typeof window !== 'undefined' && Boolean((window as any).__TAURI_INTERNALS__);
}

export function setPlatform(implementation: PlatformPorts): void {
    platform = implementation;
}

export function getPlatform(): PlatformPorts {
    if (!platform) {
        platform = isTauri() ? createTauriPlatform() : createWebMockPlatform();
    }
    return platform;
}
