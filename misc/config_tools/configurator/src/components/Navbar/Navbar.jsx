import React from "react";
import {Container} from "react-bootstrap";
import {Navbar as BootstrapNavbar} from "react-bootstrap";
import logo from "./images/ACRN_Logo.svg";
import {faWindowMaximize} from "@fortawesome/free-regular-svg-icons";
import {faClose, faMinus} from "@fortawesome/free-solid-svg-icons"
import {FontAwesomeIcon} from "@fortawesome/react-fontawesome";

import {windowHelper} from "../../lib/platform/tauri/tauri";


export class Navbar extends React.Component {
    render() {
        return (
            <BootstrapNavbar bg="navbar" variant="dark" height="80">
                <Container fluid data-tauri-drag-region>
                    <BootstrapNavbar.Brand>
                        <img
                            alt="ACRN" src={logo} height="38" data-tauri-drag-region="true"
                            className="d-inline-block align-self-center mx-3"
                        />
                        <div className="d-inline align-bottom logo-text" data-tauri-drag-region="true">
                            Configurator
                        </div>
                    </BootstrapNavbar.Brand>
                    <div className="controlButtons d-flex justify-content-between align-items-center"
                         data-tauri-drag-region={true}>
                        <FontAwesomeIcon className="wmb" icon={faMinus} color="white" size="lg"
                                         onClick={windowHelper.minimal}/>
                        <FontAwesomeIcon className="wmb" icon={faWindowMaximize} color="white" size="lg"
                                         onClick={windowHelper.maxmal}/>
                        <FontAwesomeIcon className="wmb" icon={faClose} color="white" size="lg"
                                         onClick={windowHelper.close}/>
                    </div>
                </Container>
            </BootstrapNavbar>
        );
    }
}
