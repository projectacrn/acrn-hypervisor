export function complexScenario() {
    return {
        '@board': 'golden-board',
        '@scenario': 'characterization',
        hv: {
            DEBUG_OPTIONS: {
                RELEASE: 'n',
                NPK_LOGLEVEL: 5
            },
            FEATURES: {
                RDT: {
                    RDT_ENABLED: 'y',
                    CDP_ENABLED: 'n',
                    VCAT_ENABLED: 'n'
                },
                IVSHMEM: {
                    IVSHMEM_ENABLED: 'y',
                    IVSHMEM_REGION: [
                        {
                            '@id': 'shm0',
                            NAME: 'pre-service',
                            PROVIDED_BY: 'Hypervisor',
                            IVSHMEM_SIZE: 2,
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [
                                    {VM_NAME: 'pre', VBDF: '00:10.0'},
                                    {VM_NAME: 'service', VBDF: '00:10.0'}
                                ]
                            }
                        },
                        {
                            '@id': 'shm1',
                            NAME: 'service-post',
                            PROVIDED_BY: 'Hypervisor',
                            IVSHMEM_SIZE: 4,
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [
                                    {VM_NAME: 'service', VBDF: '00:11.0'},
                                    {VM_NAME: 'post', VBDF: '00:11.0'}
                                ]
                            }
                        }
                    ]
                }
            },
            CACHE_REGION: {
                CACHE_ALLOCATION: [
                    {
                        CACHE_ID: '0',
                        CACHE_LEVEL: 3,
                        POLICY: [
                            {VM: 'pre', VCPU: 0, TYPE: 'Unified', CLOS_MASK: '0x0f'},
                            {VM: 'service', VCPU: 0, TYPE: 'Code', CLOS_MASK: '0xf0'},
                            {VM: 'post', VCPU: 0, TYPE: 'Data', CLOS_MASK: '0xcc'}
                        ]
                    }
                ]
            },
            vuart_connections: {
                vuart_connection: [
                    {
                        name: 'pre-to-service',
                        type: 'legacy',
                        endpoint: [
                            {vm_name: 'pre', io_port: '0x3f8'},
                            {vm_name: 'service', io_port: '0x2f8'}
                        ]
                    }
                ]
            }
        },
        vm: [
            {
                '@id': 0,
                name: 'pre',
                load_order: 'PRE_LAUNCHED_VM',
                vm_type: 'RTVM',
                cpu_affinity: {pcpu: [{pcpu_id: 0, real_time_vcpu: 'y'}]}
            },
            {
                '@id': 1,
                name: 'service',
                load_order: 'SERVICE_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: null
            },
            {
                '@id': 2,
                name: 'post',
                load_order: 'POST_LAUNCHED_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: {pcpu: [{pcpu_id: 2, real_time_vcpu: 'n'}]}
            }
        ]
    }
}

export function referenceScenario() {
    return {
        hv: {
            FEATURES: {
                IVSHMEM: {
                    IVSHMEM_REGION: [
                        {
                            '@id': 'keep',
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [
                                    {VM_NAME: 'pre', VBDF: '00:10.0'},
                                    {VM_NAME: 'old', VBDF: '00:10.0'},
                                    {VM_NAME: 'post', VBDF: '00:10.0'}
                                ]
                            }
                        },
                        {
                            '@id': 'prune',
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [
                                    {VM_NAME: 'old', VBDF: '00:11.0'},
                                    {VM_NAME: 'post', VBDF: '00:11.0'}
                                ]
                            }
                        },
                        {
                            '@id': 'untouched',
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [
                                    {VM_NAME: 'pre', VBDF: '00:12.0'},
                                    {VM_NAME: 'post', VBDF: '00:12.0'}
                                ]
                            }
                        }
                    ]
                }
            },
            vuart_connections: {
                vuart_connection: [
                    {
                        name: 'renamed',
                        endpoint: [{vm_name: 'pre'}, {vm_name: 'old'}]
                    },
                    {
                        name: 'removed',
                        endpoint: [{vm_name: 'old'}, {vm_name: 'post'}]
                    },
                    {
                        name: 'untouched',
                        endpoint: [{vm_name: 'pre'}, {vm_name: 'post'}]
                    }
                ]
            }
        },
        vm: [
            {'@id': 0, name: 'pre'},
            {'@id': 1, name: 'old'},
            {'@id': 2, name: 'post'}
        ]
    }
}

export function catScenario() {
    return {
        hv: {
            FEATURES: {
                RDT: {RDT_ENABLED: 'y', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'}
            },
            CACHE_REGION: {
                CACHE_ALLOCATION: [
                    {
                        CACHE_ID: '0',
                        CACHE_LEVEL: 3,
                        POLICY: [
                            {VM: 'pre', VCPU: 0, TYPE: 'Unified', CLOS_MASK: '0x55'},
                            {VM: 'service', VCPU: 0, TYPE: 'Unified', CLOS_MASK: '0x0f'}
                        ]
                    }
                ]
            }
        },
        vm: [
            {
                '@id': 0,
                name: 'pre',
                load_order: 'PRE_LAUNCHED_VM',
                vm_type: 'RTVM',
                cpu_affinity: {pcpu: [{pcpu_id: 0, real_time_vcpu: 'y'}]}
            },
            {
                '@id': 1,
                name: 'service',
                load_order: 'SERVICE_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: null
            },
            {
                '@id': 2,
                name: 'post',
                load_order: 'POST_LAUNCHED_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: {
                    pcpu: [
                        {pcpu_id: 1, real_time_vcpu: 'n'},
                        {pcpu_id: 2, real_time_vcpu: 'n'}
                    ]
                }
            }
        ]
    }
}
