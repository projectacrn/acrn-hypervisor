import {VM_NAME_REFERENCES, parseReferencePath} from './graph'
import type {PathSegment, RefRule} from './graph'

type ScenarioDoc = Record<string, any>

const DROP_ENTRY = Symbol('drop-entry')
const DROP_CONNECTION = Symbol('drop-connection')
type RewriteResult = typeof DROP_ENTRY | typeof DROP_CONNECTION | void

function rewriteReferences(
    value: any,
    segments: PathSegment[],
    index: number,
    outerCollection: number,
    oldName: string,
    newName: string | null,
    rule: RefRule
): RewriteResult {
    if (value === null || typeof value !== 'object') {
        return
    }

    const segment = segments[index]
    const child = value[segment.key]
    if (index === segments.length - 1) {
        if (child !== oldName) {
            return
        }
        if (newName !== null) {
            value[segment.key] = newName
            return
        }
        return rule.onRemove === 'drop-connection' ? DROP_CONNECTION : DROP_ENTRY
    }

    if (child === null || typeof child !== 'object') {
        return
    }

    if (segment.collection) {
        if (!Array.isArray(child)) {
            return
        }
        let dropConnection = false
        const retained: any[] = []
        for (const entry of child) {
            const result = rewriteReferences(
                entry, segments, index + 1, outerCollection, oldName, newName, rule)
            if (result === DROP_ENTRY ||
                (result === DROP_CONNECTION && index === outerCollection)) {
                continue
            }
            if (result === DROP_CONNECTION) {
                dropConnection = true
                continue
            }
            retained.push(entry)
        }
        value[segment.key] = retained
        return dropConnection ? DROP_CONNECTION : undefined
    }

    return rewriteReferences(child, segments, index + 1, outerCollection, oldName, newName, rule)
}

function pruneIvshmemRegions(doc: ScenarioDoc, segments: PathSegment[]): void {
    const outerCollection = segments.findIndex(segment => segment.collection)
    let container: any = doc
    for (let index = 0; index < outerCollection; index++) {
        container = container?.[segments[index].key]
    }
    if (!container) {
        return
    }

    const key = segments[outerCollection].key
    if (!Array.isArray(container[key])) {
        return
    }
    container[key] = container[key].filter((region: any) =>
        (region?.IVSHMEM_VMS?.IVSHMEM_VM?.length ?? 0) >= 2)
}

function applyRule(doc: ScenarioDoc, rule: RefRule, oldName: string, newName: string | null): void {
    const segments = parseReferencePath(rule.path)
    const outerCollection = segments.findIndex(segment => segment.collection)
    rewriteReferences(doc, segments, 0, outerCollection, oldName, newName, rule)
    if (newName === null && rule.pruneWhen === 'fewer-than-2-siblings') {
        pruneIvshmemRegions(doc, segments)
    }
}

/** Rename every declarative reference without pruning otherwise valid entries. */
export function renameVm(doc: ScenarioDoc, oldName: string, newName: string): ScenarioDoc {
    for (const rule of VM_NAME_REFERENCES) {
        applyRule(doc, rule, oldName, newName)
    }
    return doc
}

/** Remove a VM from references and apply each reference type's pruning rule. */
export function removeVm(doc: ScenarioDoc, vmName: string): ScenarioDoc {
    for (const rule of VM_NAME_REFERENCES) {
        applyRule(doc, rule, vmName, null)
    }
    return doc
}
