type Schema = Record<string, any>
type ScenarioVm = Record<string, any>

const VM_SCHEMA_BY_LOAD_ORDER: Record<string, string> = {
    PRE_LAUNCHED_VM: 'PreLaunchedVM',
    SERVICE_VM: 'ServiceVM',
    POST_LAUNCHED_VM: 'PostLaunchedVM'
}

function pciItems(preLaunchedSchema: Schema | undefined): Schema | undefined {
    return preLaunchedSchema?.BasicConfigType?.definitions?.PCIDevsConfiguration
        ?.properties?.pci_dev?.items
}

function replacePciItems(preLaunchedSchema: Schema, devices: string[]): Schema {
    const items = pciItems(preLaunchedSchema)
    if (!items) {
        return preLaunchedSchema
    }

    return {
        ...preLaunchedSchema,
        BasicConfigType: {
            ...preLaunchedSchema.BasicConfigType,
            definitions: {
                ...preLaunchedSchema.BasicConfigType.definitions,
                PCIDevsConfiguration: {
                    ...preLaunchedSchema.BasicConfigType.definitions.PCIDevsConfiguration,
                    properties: {
                        ...preLaunchedSchema.BasicConfigType.definitions.PCIDevsConfiguration.properties,
                        pci_dev: {
                            ...preLaunchedSchema.BasicConfigType.definitions.PCIDevsConfiguration.properties.pci_dev,
                            items: {
                                ...items,
                                enum: devices,
                                enumNames: [...devices]
                            }
                        }
                    }
                }
            }
        }
    }
}

function replacePreLaunchedSchema(schemas: Schema, devices: string[]): Schema {
    if (!schemas.PreLaunchedVM) {
        return schemas
    }
    return {
        ...schemas,
        PreLaunchedVM: replacePciItems(schemas.PreLaunchedVM, devices)
    }
}

/** Apply the board engine's currently unassigned PCI device list. */
export function deriveLoadedPciSchema(schemas: Schema, availableDevices: string[]): Schema {
    return replacePreLaunchedSchema(schemas, [...availableDevices])
}

/** Keep the active VM's devices selectable in addition to unassigned devices. */
export function deriveActivePciSchema(schemas: Schema, selectedDevices: string[]): Schema {
    const devices = new Set<string>(pciItems(schemas.PreLaunchedVM)?.enum || [])
    selectedDevices.forEach(device => devices.add(device))
    return replacePreLaunchedSchema(schemas, Array.from(devices).sort())
}

export function usedPciDevices(vms: ScenarioVm[]): string[] {
    const devices: string[] = []
    vms.forEach(vm => {
        if (Array.isArray(vm.pci_devs?.pci_dev)) {
            devices.push(...vm.pci_devs.pci_dev)
        }
    })
    return devices
}

/** Select the complete Basic/Advanced schema pair for the active HV or VM. */
export function selectFormSchema(schemas: Schema, activeVmId: number, vms: ScenarioVm[]): Schema | undefined {
    if (activeVmId === -1) {
        return schemas.HV
    }
    const activeVm = vms.find(vm => vm['@id'] === activeVmId)
    const schemaName = activeVm ? VM_SCHEMA_BY_LOAD_ORDER[activeVm.load_order] : undefined
    return schemaName ? schemas[schemaName] : undefined
}
