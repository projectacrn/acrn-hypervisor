import React from "react";
import {Col, Container, Row} from "react-bootstrap";

import './Welcome.css'

import Banner from "../../components/Banner";

import StartNewConfigurationPanel from "./StartNewConfigurationPanel";
import UseAnExistingConfigurationPanel from "./UseAnExistingConfigurationPanel";
import Footer from "../../components/Footer";

class Welcome extends React.Component {
    render() {
        return (
            <div>
                <Container fluid>
                    <Banner>
                        <div className="banner-text ms-4 ps-3 py-4">
                            ACRN Configurator helps you set up and customize your ACRN hypervisor and VMs.
                        </div>
                    </Banner>
                    <p className="text-center py-3 mb-0">
                        <div className="text-danger p-0 m-0">This is a preview version, please be careful.</div>
                        If you find a bug, please report it to &nbsp;
                        <a href="https://github.com/Weiyi-Feng/acrn-hypervisor" target="_blank">
                            https://github.com/Weiyi-Feng/acrn-hypervisor
                        </a>.
                    </p>
                    <Container fluid="xxl">
                        <Row className="py-4">
                            <Col className="d-flex justify-content-center border-end-sm" sm>
                                <div className="py-3 py-sm-0 px-3" style={{width: "100%"}}>
                                    <StartNewConfigurationPanel/>
                                </div>
                            </Col>

                            <Col className="d-flex justify-content-center" sm>
                                <div className="py-3 py-sm-0 px-3" style={{width: "100%"}}>
                                    <UseAnExistingConfigurationPanel/>
                                </div>
                            </Col>

                        </Row>

                    </Container>
                    <Footer/>
                </Container>
            </div>
        )
    }
}

export default Welcome
