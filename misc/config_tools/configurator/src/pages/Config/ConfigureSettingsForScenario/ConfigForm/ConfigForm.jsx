import React, {Component} from "react"
import Form from "../../../../lib/bs4rjsf"
import {ACRNContext} from "../../../../ACRNContext";
import SelectWidget from "./IVSHMEM_VM/SelectWidget";
import TextWidget from "./IVSHMEM_VM/TextWidget";

// import CustomTemplateField from "./CustomTemplateField/CustomTemplateField";

export class ConfigForm extends Component {
    constructor(props) {
        super(props);
    }

    setFormData = (data) => {
        let {configurator} = this.context
        let VMID = data['@id']

        if (VMID == null) {
            configurator.programLayer.scenarioData.hv = data
            return
        }

        let load_order = data['load_order']
        for (let index = 0; index < configurator.programLayer.scenarioData.vm[load_order].length; index++) {
            if (configurator.programLayer.scenarioData.vm[load_order][index]['@id'] === VMID) {
                configurator.programLayer.scenarioData.vm[load_order][index] = data
            }
        }

    }


    getParams = (VMID, mode) => {
        let {configurator} = this.context
        let schema, formData;
        if (VMID === -1) {
            schema = configurator.hvSchema[mode]
            formData = configurator.programLayer.scenarioData.hv
        } else {
            let VMData = null;
            configurator.programLayer.getOriginScenarioData().vm.map((vmConfig) => {
                if (vmConfig['@id'] === VMID) {
                    VMData = vmConfig
                }
            })
            schema = configurator.vmSchemas[VMData.load_order][mode]
            formData = VMData
        }

        return {schema, formData}
    }


    render = () => {
        let VMID = this.props.VMID
        let mode = this.props.mode

        let params = this.getParams(VMID, mode)
        let uiSchema = {
            basic: {
                DEBUG_OPTIONS: {
                    BUILD_TYPE: {
                        "ui:widget": "radio"
                    }
                },
                FEATURES: {
                    IVSHMEM: {
                        IVSHMEM_REGION: {
                            items: {
                                IVSHMEM_VMS: {
                                    IVSHMEM_VM: {
                                        "ui:style": {
                                            border: "1px solid gray",
                                            padding: "1rem",
                                            borderRadius: "7px"
                                        },
                                        items: {
                                            VM_NAME: {
                                                "ui:grid": 7,
                                                "ui:widget": 'VM_NAME',
                                                "ui:descLabel": true,
                                                "ui:descLabelAli": 'H',
                                                "ui:descLabelMT": true
                                            },
                                            VBDF: {
                                                "ui:grid": 5,
                                                "ui:widget": 'VBDF',
                                                "ui:descLabel": true,
                                                "ui:descLabelAli": 'V',
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                },
                vuart_connections: {
                    vuart_connection: {
                        items: {
                            endpoint: {
                                items: {
                                    vm_name: {
                                        "ui:grid": 6,
                                        "ui:widget": 'VBDF',
                                        "ui:descLabel": true,
                                        "ui:descLabelAli": 'V',
                                    },
                                    io_port: {
                                        "ui:grid": 6,
                                        "ui:widget": 'VBDF',
                                        "ui:descLabel": true,
                                        "ui:descLabelAli": 'V',
                                    },
                                }
                            }
                        }
                    }
                }
            },
            additionalProperties: {
                "ui:widget": "hidden"
            }
        }

        let widgets = {
            VM_NAME: SelectWidget,
            VBDF: TextWidget
        }


        return <div>
            <p>* are required fields</p>
            <Form
                noHtml5Validate={true}
                liveValidate={true}
                schema={params.schema}
                uiSchema={uiSchema}
                widgets={widgets}
                formData={params.formData}
                onChange={(e) => {
                    let data = e.formData
                    this.setFormData(data)
                }}
            />
        </div>
    }
}

ConfigForm.contextType = ACRNContext