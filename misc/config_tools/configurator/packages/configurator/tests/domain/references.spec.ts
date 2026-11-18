import {describe, expect, it} from 'vitest'
import {VM_NAME_REFERENCES} from '../../src/domain/references/graph'
import {removeVm, renameVm} from '../../src/domain/references/rename'

function scenario() {
    return {
        hv: {
            FEATURES: {
                IVSHMEM: {
                    IVSHMEM_REGION: [
                        {
                            name: 'keep',
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [
                                    {VM_NAME: 'old'}, {VM_NAME: 'pre'}, {VM_NAME: 'post'}
                                ]
                            }
                        },
                        {
                            name: 'prune',
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [{VM_NAME: 'old'}, {VM_NAME: 'post'}]
                            }
                        }
                    ]
                }
            },
            vuart_connections: {
                vuart_connection: [
                    {name: 'drop', endpoint: [{vm_name: 'pre'}, {vm_name: 'old'}]},
                    {name: 'keep', endpoint: [{vm_name: 'pre'}, {vm_name: 'post'}]}
                ]
            },
            CACHE_REGION: {
                CACHE_ALLOCATION: [{
                    CACHE_ID: '0',
                    POLICY: [{VM: 'old', VCPU: 0}, {VM: 'pre', VCPU: 1}]
                }]
            }
        },
        vm: [{name: 'old'}, {name: 'pre'}, {name: 'post'}]
    }
}

describe('VM name references', () => {
    it('declares IVSHMEM, VUART and CAT reference paths', () => {
        expect(VM_NAME_REFERENCES.map(rule => rule.path)).toEqual([
            'hv.FEATURES.IVSHMEM.IVSHMEM_REGION[].IVSHMEM_VMS.IVSHMEM_VM[].VM_NAME',
            'hv.vuart_connections.vuart_connection[].endpoint[].vm_name',
            'hv.CACHE_REGION.CACHE_ALLOCATION[].POLICY[].VM'
        ])
    })

    it('renames every reference without pruning regions or connections', () => {
        const doc = scenario()

        expect(renameVm(doc, 'old', 'renamed')).toBe(doc)

        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toHaveLength(2)
        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0]
            .IVSHMEM_VMS.IVSHMEM_VM[0].VM_NAME).toBe('renamed')
        expect(doc.hv.vuart_connections.vuart_connection[0].endpoint[1].vm_name)
            .toBe('renamed')
        expect(doc.hv.CACHE_REGION.CACHE_ALLOCATION[0].POLICY[0].VM).toBe('renamed')
    })

    it('drops references and preserves the type-specific pruning rules', () => {
        const doc = scenario()

        removeVm(doc, 'old')

        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION.map(region => region.name))
            .toEqual(['keep'])
        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0]
            .IVSHMEM_VMS.IVSHMEM_VM.map(vm => vm.VM_NAME)).toEqual(['pre', 'post'])
        expect(doc.hv.vuart_connections.vuart_connection.map(connection => connection.name))
            .toEqual(['keep'])
        expect(doc.hv.CACHE_REGION.CACHE_ALLOCATION[0].POLICY)
            .toEqual([{VM: 'pre', VCPU: 1}])
        expect(doc.hv.CACHE_REGION.CACHE_ALLOCATION).toHaveLength(1)
    })

    it('handles missing optional reference containers and repeated removals', () => {
        const empty = {hv: {}, vm: []}
        expect(() => removeVm(empty, 'missing')).not.toThrow()

        const doc = scenario()
        removeVm(doc, 'old')
        expect(() => removeVm(doc, 'old')).not.toThrow()
        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toHaveLength(1)
    })
})
