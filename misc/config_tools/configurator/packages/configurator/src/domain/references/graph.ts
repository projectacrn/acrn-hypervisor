export type ReferenceRemoval = 'drop-entry' | 'drop-connection'
export type ReferencePruning = 'fewer-than-2-siblings'

export interface RefRule {
    path: string
    onRemove: ReferenceRemoval
    pruneWhen?: ReferencePruning
}

/** VM names stored outside scenario.vm. [] marks an array path segment. */
export const VM_NAME_REFERENCES: readonly RefRule[] = [
    {
        path: 'hv.FEATURES.IVSHMEM.IVSHMEM_REGION[].IVSHMEM_VMS.IVSHMEM_VM[].VM_NAME',
        onRemove: 'drop-entry',
        pruneWhen: 'fewer-than-2-siblings'
    },
    {
        path: 'hv.vuart_connections.vuart_connection[].endpoint[].vm_name',
        onRemove: 'drop-connection'
    },
    {
        path: 'hv.CACHE_REGION.CACHE_ALLOCATION[].POLICY[].VM',
        onRemove: 'drop-entry'
    }
]

export interface PathSegment {
    key: string
    collection: boolean
}

export function parseReferencePath(path: string): PathSegment[] {
    return path.split('.').map(part => ({
        key: part.endsWith('[]') ? part.slice(0, -2) : part,
        collection: part.endsWith('[]')
    }))
}
