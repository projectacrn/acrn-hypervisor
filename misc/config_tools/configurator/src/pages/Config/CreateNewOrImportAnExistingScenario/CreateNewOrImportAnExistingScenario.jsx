import React, {Component} from "react";
import {Accordion, Button, Col, Form, Row} from "react-bootstrap";

import CreateScenarioModal from "./CreateScenarioModal/CreateScenarioModal";
import {dialog} from "@tauri-apps/api";
import {ACRNContext} from "../../../ACRNContext";
import {invoke} from "@tauri-apps/api/tauri";
import _ from "lodash/fp";

export default class CreateNewOrImportAnExistingScenario extends Component {
    constructor(props) {
        super(props);
        this.scenarioXMLSelect = React.createRef()
        this.scenarioXMLFileInput = React.createRef()
        this.state = {
            scenarioConfigFiles: [],
            stage: props.stage,
            scenarioName: '',
            scenarioConfig: {},
            selected: null
        }
    }

    componentDidMount() {
        let {configurator} = this.context
        this.scenarioHistoryUpdate()
        invoke('fs_read_dir', {
            path: configurator.WorkingFolder,
            recursive: false
        }).then((files) => {
            for (let i = 0; i < files.length; i++) {
                let isScenario = _.endsWith("/scenario.xml", files[i].path.replace(/\\/g, '/'));
                console.log("files", isScenario, files[i].path)
                if (isScenario) {
                    this.scenarioChange(files[i].path).then(() => {
                        this.importScenario()
                    })
                }
            }
        })
    }

    scenarioHistoryUpdate() {
        let {configurator} = this.context
        return configurator.getHistory('scenario').then((scenarioHistory) => {
            this.setState({scenarioConfigFiles: scenarioHistory})
        })
    }


    openFileDialog = () => {
        dialog.open({directory: false, multiple: false}).then(this.scenarioChange)
    }

    scenarioChange = (filepath) => {
        console.log(filepath)
        let {configurator} = this.context
        return configurator.addHistory('scenario', filepath).then(() => {
            this.scenarioHistoryUpdate().then(() => {
                this.scenarioXMLSelect.current.value = filepath
            })
        })
    }

    importScenario = () => {
        let {configurator} = this.context
        return configurator.programLayer.loadScenario(this.scenarioXMLSelect.current.value)
            .then(() => {
                this.setState({selected: this.getScenarioPath()})
            }).then(() => {
                let tabButton = document.querySelectorAll(".accordion-button")[2];
                if (tabButton.className.indexOf('collapsed') >= 0) {
                    tabButton.click()
                }
            })
            .catch((reason) => {
                console.log(reason)
                alert(reason)
            })
    };

    getScenarioPath = () => {
        let {configurator} = this.context
        let printPath = configurator.WorkingFolder;
        if (_.endsWith("/", configurator.WorkingFolder) || _.endsWith("\\", configurator.WorkingFolder)) {
            printPath = printPath + 'scenario.xml'
        } else {
            printPath = printPath + (configurator.WorkingFolder[1] === ":" ? "\\" : '/') + 'scenario.xml'
        }
        return printPath;
    }

    render = () => {
        let {configurator} = this.context
        let scenarioHistorySelect = this.state.scenarioConfigFiles.map((optionValue, index) => {
            return (<option key={index} value={optionValue}>{optionValue}</option>)
        })
        return (
            <Accordion.Item eventKey="1">
                <Accordion.Header>
                    <div className="p-1 fs-4">
                        2. Create new or import an existing scenario
                    </div>
                </Accordion.Header>
                <Accordion.Body>
                    <Row className="px-3 py-2">
                        <Col className="border-end-sm py-1" sm>
                            <p className="py-2" style={{"letterSpacing": "0.49px"}}>Current scenario:
                                {this.state.selected ? this.state.selected : "none selected"}</p>
                            <div className="py-4 text-center">
                                <CreateScenarioModal cb={() => {
                                    this.setState({selected: this.getScenarioPath()})
                                }}/>
                            </div>
                        </Col>

                        <Col className="py-1 ps-sm-5" sm>
                            <Form>
                                <b className="d-block pb-3" style={{"letterSpacing": "-0.29px"}}>
                                    Recently used scenarios:
                                </b>
                                <table>
                                    <tbody>
                                    <tr>
                                        <td>
                                            <Form.Select className="d-inline" ref={this.scenarioXMLSelect}>
                                                {scenarioHistorySelect}
                                            </Form.Select>
                                            <input type="file" style={{display: 'none'}} ref={this.scenarioXMLFileInput}
                                                   onChange={this.scenarioChange} onBlur={this.scenarioChange}/>
                                        </td>
                                        <td>
                                            <a className="ps-3 text-nowrap" style={{cursor: "pointer"}}
                                               onClick={this.openFileDialog}>
                                                Browse for scenario file…
                                            </a>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>
                                            <div className="py-4 text-right">
                                                <Button className="wel-btn" size="lg" onClick={this.importScenario}>Import
                                                    Scenario</Button>
                                            </div>
                                        </td>
                                        <td/>
                                    </tr>
                                    </tbody>
                                </table>
                            </Form>
                        </Col>
                    </Row>
                </Accordion.Body>
            </Accordion.Item>
        )
    }
}
CreateNewOrImportAnExistingScenario.contextType = ACRNContext