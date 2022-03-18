import React, {Component} from "react";
import "./ConfigTabBar.css"
import Tab from "./Tab";
import TabSplitter from "./TabSplitter";
import TabAdd from "./TabAdd";
import {ACRNContext} from "../../../../ACRNContext";

export default class ConfigTabBar extends Component {
    constructor(props) {
        super(props);
        this.state = {
            activeVMID: -1
        }
    }

    componentDidMount = () => {
        let {configurator} = this.context
        this.funRegisterID = configurator.programLayer.register(
            configurator.eventName.scenarioDataUpdate, () => {
                this.forceUpdate()
            }
        )
    }

    componentWillUnmount = () => {
        let {configurator} = this.context
        configurator.programLayer.unregister(this.funRegisterID)
    }

    active = (activeVMID = null) => {
        if (activeVMID == null) {
            return this.state.activeVMID
        }
        this.props.callback ? this.props.callback(activeVMID) : null;
        return this.setState({activeVMID: activeVMID})
    }

    tab = (vmConfig, desc) => {
        console.log(vmConfig)
        return <Tab
            key={'vmConfigTab' + vmConfig['@id']}
            VMID={vmConfig['@id']}
            title={vmConfig.name}
            desc={desc}
            active={this.active}
        />
    }


    render = () => {
        let {configurator} = this.context
        let scenarioData = configurator.programLayer.scenarioData

        let PRE_LAUNCHED_VM = scenarioData.vm.PRE_LAUNCHED_VM.map((vmConfig) => {
            return this.tab(vmConfig, 'Pre-Launched')
        })
        let SERVICE_VM = scenarioData.vm.SERVICE_VM.map((vmConfig) => {
            return this.tab(vmConfig, 'ServiceVM')
        })
        let POST_LAUNCHED_VM = scenarioData.vm.POST_LAUNCHED_VM.map((vmConfig) => {
            return this.tab(vmConfig, 'Post-Launched')
        })

        return (
            <div className="p-2 w-100 d-flex align-items-center TabBox">
                <Tab VMID={-1} title='Hypervisor' desc='Global Settings' active={this.active}/>
                <TabSplitter/>
                {PRE_LAUNCHED_VM}
                <TabAdd desc="Pre-launched VM" addVM={() => {
                    configurator.programLayer.addVM('PRE_LAUNCHED_VM')
                }}/>
                {SERVICE_VM}
                {POST_LAUNCHED_VM}
                <TabAdd desc="Post-launched VM" addVM={() => {
                    if (scenarioData.vm.SERVICE_VM.length === 0) {
                        configurator.programLayer.addVM('SERVICE_VM')
                    }
                    configurator.programLayer.addVM('POST_LAUNCHED_VM')
                }}/>
            </div>
        )
    }
}
ConfigTabBar.contextType = ACRNContext