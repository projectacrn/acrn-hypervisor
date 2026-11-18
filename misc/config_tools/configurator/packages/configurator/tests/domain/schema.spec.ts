import {describe, expect, it} from 'vitest'
import {
    deriveActivePciSchema,
    deriveLoadedPciSchema,
    selectFormSchema,
    usedPciDevices
} from '../../src/domain/schema/derive'

function schemas() {
    return {
        HV: {BasicConfigType: {title: 'HV basic'}, AdvancedConfigType: {title: 'HV advanced'}},
        PreLaunchedVM: {
            BasicConfigType: {
                definitions: {
                    PCIDevsConfiguration: {
                        properties: {
                            pci_dev: {
                                items: {enum: ['00:02.0'], enumNames: ['00:02.0'], title: 'PCI'}
                            }
                        }
                    }
                }
            },
            AdvancedConfigType: {title: 'pre advanced'}
        },
        ServiceVM: {BasicConfigType: {title: 'service basic'}},
        PostLaunchedVM: {BasicConfigType: {title: 'post basic'}}
    }
}

describe('schema derivation', () => {
    it('replaces loaded PCI enums without mutating the source schemas', () => {
        const source = schemas()
        const originalItems = source.PreLaunchedVM.BasicConfigType.definitions
            .PCIDevsConfiguration.properties.pci_dev.items

        const derived = deriveLoadedPciSchema(source, ['00:04.0', '00:03.0'])
        const items = derived.PreLaunchedVM.BasicConfigType.definitions
            .PCIDevsConfiguration.properties.pci_dev.items

        expect(items).toEqual({
            enum: ['00:04.0', '00:03.0'],
            enumNames: ['00:04.0', '00:03.0'],
            title: 'PCI'
        })
        expect(originalItems.enum).toEqual(['00:02.0'])
        expect(derived.PreLaunchedVM.AdvancedConfigType)
            .toBe(source.PreLaunchedVM.AdvancedConfigType)
    })

    it('unions and sorts the active VM selections with available devices', () => {
        const source = deriveLoadedPciSchema(schemas(), ['00:04.0', '00:02.0'])

        const derived = deriveActivePciSchema(source, ['00:03.0', '00:02.0'])
        const items = derived.PreLaunchedVM.BasicConfigType.definitions
            .PCIDevsConfiguration.properties.pci_dev.items

        expect(items.enum).toEqual(['00:02.0', '00:03.0', '00:04.0'])
        expect(items.enumNames).toEqual(items.enum)
        expect(source.PreLaunchedVM.BasicConfigType.definitions.PCIDevsConfiguration
            .properties.pci_dev.items.enum).toEqual(['00:04.0', '00:02.0'])
    })

    it('collects only array-valued PCI selections for the board engine', () => {
        expect(usedPciDevices([
            {pci_devs: {pci_dev: ['00:01.0', '00:02.0']}},
            {pci_devs: {pci_dev: '00:03.0'}},
            {pci_devs: null},
            {}
        ])).toEqual(['00:01.0', '00:02.0'])
    })

    it('selects the Basic/Advanced schema pair for HV and each VM load order', () => {
        const source = schemas()
        const vms = [
            {'@id': 0, load_order: 'PRE_LAUNCHED_VM'},
            {'@id': 1, load_order: 'SERVICE_VM'},
            {'@id': 2, load_order: 'POST_LAUNCHED_VM'}
        ]

        expect(selectFormSchema(source, -1, vms)).toBe(source.HV)
        expect(selectFormSchema(source, 0, vms)).toBe(source.PreLaunchedVM)
        expect(selectFormSchema(source, 1, vms)).toBe(source.ServiceVM)
        expect(selectFormSchema(source, 2, vms)).toBe(source.PostLaunchedVM)
        expect(selectFormSchema(source, 99, vms)).toBeUndefined()
    })
})
