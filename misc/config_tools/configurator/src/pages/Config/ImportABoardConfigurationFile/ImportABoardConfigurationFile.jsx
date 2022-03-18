import React, {Component} from "react";
import {Accordion, Button, Col, Form, Row} from "react-bootstrap";
import Banner from "../../../components/Banner";
import {dialog} from "@tauri-apps/api";
import {ACRNContext} from "../../../ACRNContext";

export default class ImportABoardConfigurationFile extends Component {
    constructor(props) {
        super(props);
        this.boardXMLSelect = React.createRef()
        this.boardXMLFileInput = React.createRef()
        this.state = {
            disableImport: true,
            boardFiles: [],
            boardName: '',
            boardXML: '',
            BIOS_INFO: '',
            BASE_BOARD_INFO: ''
        }
    }

    componentDidMount() {
        let {configurator} = this.context
        configurator.getHistory('board').then((boardFiles) => {
            let disableImport = boardFiles.length === 0
            this.setState({boardFiles, disableImport})
        })
    }


    openFileDialog = () => {
        //this.boardXMLFileInput.current.click()
        dialog.open({directory: false, multiple: false}).then((filepath) => {
            this.boardChange(filepath)
        })
    };

    boardChange = (filepath) => {
        let {configurator} = this.context
        configurator.addHistory('board', filepath).then(() => {
            return configurator.getHistory('board')
        }).then((boardFiles) => {
            console.log(boardFiles)
            this.setState({
                disableImport: false,
                boardFiles
            })
        }).then(() => {
            this.boardXMLSelect.current.value = filepath
        })
    }

    importBoard = () => {
        let {configurator} = this.context

        if (!this.state.boardXML) {
            configurator.loadBoard(this.boardXMLSelect.current.value, this.updateBoardInfo)
        } else {
            this.openFileDialog()
        }
    }

    getNodeTextContent(boardXML, nodeSelector) {
        return boardXML.querySelector(nodeSelector).textContent;
    }

    updateBoardInfo = (boardName, boardXMLText) => {
        const boardXML = (new DOMParser()).parseFromString(boardXMLText, 'text/xml')
        let BIOS_INFO = this.getNodeTextContent(boardXML, "BIOS_INFO").replace(/\s+BIOS Information\s+/, '')
        let BASE_BOARD_INFO = this.getNodeTextContent(boardXML, "BASE_BOARD_INFO").replace(/\s+Base Board Information\s+/, '')
        this.setState({
            boardName: boardName,
            boardXML: boardXML,
            BIOS_INFO: BIOS_INFO,
            BASE_BOARD_INFO: BASE_BOARD_INFO
        })
        this.boardXMLSelect.current.disabled = true
    }

    render() {
        let boardHistorySelect = this.state.boardFiles.map((optionValue, index) => {
            return (
                <option key={index} value={optionValue}>{optionValue}</option>
            )
        })
        return (
            <Accordion.Item eventKey="0">
                <Accordion.Header>
                    <div className="p-1 fs-4">
                        1. Import a board configuration file
                    </div>
                </Accordion.Header>
                <Accordion.Body>

                    <Row className="px-3 py-2">
                        <Col className="border-end-sm py-1" sm>

                            <Form>
                                <b className="py-3 d-block" style={{"letterSpacing": "0.49px"}}>
                                    {this.state.boardXML ? 'Current board file:' : 'Recently used board files:'}
                                </b>
                                <table style={{width: "100%"}}>
                                    <tbody>
                                    <tr>
                                        <td>
                                            <Form.Select className="d-inline" ref={this.boardXMLSelect}>
                                                {boardHistorySelect}
                                            </Form.Select>
                                            <input type="file" style={{display: "none"}} ref={this.boardXMLFileInput}
                                                   onChange={this.boardChange} onBlur={this.boardChange}/>
                                        </td>
                                        <td>
                                            <a className="ps-3 text-nowrap" style={{cursor: "pointer"}}
                                               onClick={this.openFileDialog}>
                                                Browse for file…
                                            </a>
                                        </td>
                                    </tr>
                                    <tr>
                                        <td>
                                            <div className="py-4 text-right">
                                                <Button
                                                    className="wel-btn" size="lg" onClick={this.importBoard}
                                                    disabled={this.state.disableImport}
                                                >
                                                    {this.state.boardXML ? 'Use a Different Board…' : 'Import Board File'}
                                                </Button>
                                            </div>
                                        </td>
                                        <td/>
                                    </tr>
                                    </tbody>
                                </table>


                            </Form>

                        </Col>

                        <Col className="py-1 ps-sm-5" sm>
                            <div className="card">
                                <div className="card-header">
                                    {this.state.boardXML ? "Current Board: " + this.state.boardName : "No board information has been imported yet."}
                                </div>
                                <div className="card-body">
                                    <div className="card-text text-pre-line">
                                        <Row className="px-3 py-2">
                                            <Col className="py-1" sm>
                                                {this.state.BASE_BOARD_INFO}
                                            </Col>
                                            <Col className="py-1" sm>
                                                {this.state.BIOS_INFO}
                                            </Col>
                                        </Row>
                                    </div>
                                </div>
                            </div>
                        </Col>
                    </Row>
                </Accordion.Body>
            </Accordion.Item>
        )
    }
}
ImportABoardConfigurationFile.contextType = ACRNContext