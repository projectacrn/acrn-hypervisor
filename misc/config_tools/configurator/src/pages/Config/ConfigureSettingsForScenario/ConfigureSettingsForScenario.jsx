import React, {Component} from "react";
import "./ConfigureSettingsForScenario.css"
import {Accordion, Button} from "react-bootstrap";
import {FontAwesomeIcon} from "@fortawesome/react-fontawesome";
import {faMinus} from "@fortawesome/free-solid-svg-icons";
import ConfigTabBar from "./ConfigTabBar/ConfigTabBar";
import ConfigForm from "./ConfigForm";
import {ACRNContext} from "../../../ACRNContext";

export default class ConfigureSettingsForScenario extends Component {
    constructor(props) {
        super(props);
        // VMID possible value
        // [-1 0 1 2 3]
        this.state = {
            VMID: -1,
            tabMode: 'basic'
        }
    }

    tabSwitch = (VMID) => {
        this.setState({VMID: VMID})
        document.querySelector('#BasicFormTab').click()
        this.forceUpdate()
    }

    tab = (tabID, title, isActive) => {
        let active = isActive ? 'active' : ''
        return (
            <li key={tabID + 'Tab'} className="nav-item" role="presentation">
                <button className={"nav-link " + active} id={tabID + 'Tab'} data-bs-toggle="tab"
                        data-bs-target={'#' + tabID} type="button" role="tab" aria-controls={tabID}
                        aria-selected="true">{title}
                </button>
            </li>
        )
    }

    tabsContent = (mode, VMID) => {
        let tabID = mode + 'Form'

        return (<div key={tabID} className={"tab-pane fade " + (tabID.indexOf('Basic') !== -1 ? 'show active' : '')}
                     id={tabID} role="tabpanel" aria-labelledby={tabID + 'Tab'}>
            <ConfigForm VMID={VMID} mode={mode.toLowerCase()}/>
        </div>)
    }

    render = () => {
        let {configurator} = this.context
        let scenarioData = configurator.programLayer.scenarioData
        return (<Accordion.Item eventKey="2">
            <Accordion.Header>
                <div className="p-1 w-100 d-flex justify-content-between align-items-center">
                    <div className="fs-4">3. Configure settings for scenario and launch scripts</div>
                    <Button size="lg" onClick={(e) => {
                        configurator.saveScenario()
                            .then(() => {
                                alert('Save successful!')
                            })
                        e.preventDefault();
                        e.stopPropagation();
                    }}>Save Scenario and Launch Scripts</Button>
                </div>
            </Accordion.Header>
            <Accordion.Body>

                <div className="p-4">
                    <ConfigTabBar callback={this.tabSwitch}/>
                </div>
                <div className="p-4">
                    <div className="d-flex justify-content-between">
                        <ul className="nav nav-tabs" role="tablist">
                            {this.tab('BasicForm', 'Basic Parameters', true)}
                            {this.tab('AdvancedForm', 'Advanced Parameters', false)}
                        </ul>
                        <Button className={'deleteVM ' + (this.state.VMID === -1 ? 'd-none' : '')}
                                size='lg' onClick={() => {
                            if (configurator.programLayer.isServiceVM(this.state.VMID) == true) {
                                confirm("All post-launched VMs will be deleted together!!")
                                    .then((r) => {
                                        if (r) {
                                            configurator.programLayer.deleteVM(this.state.VMID)
                                            var len=scenarioData.vm.POST_LAUNCHED_VM.length
                                            for (var idx=len-1; idx>=0; idx--) {
                                                    let postvm = scenarioData.vm.POST_LAUNCHED_VM[idx]
                                                    configurator.programLayer.deleteVM(postvm['@id'])
                                            }
                                        }
                                    })
                            } else {
                                configurator.programLayer.deleteVM(this.state.VMID)
                            }
                        }}>
                            <FontAwesomeIcon icon={faMinus} color="white" size="lg"/> Delete VM
                        </Button>
                    </div>
                    <div className="tab-content p-3" id="myTabContent"
                         style={{border: '1px solid #373A77', boxSizing: 'border-box', borderRadius: '0 5px 5px 5px'}}>
                        {this.tabsContent('Basic', this.state.VMID)}
                        {this.tabsContent('Advanced', this.state.VMID)}
                    </div>

                </div>
            </Accordion.Body>
        </Accordion.Item>)
    }
}
ConfigureSettingsForScenario.contextType = ACRNContext