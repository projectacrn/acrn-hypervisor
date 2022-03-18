import React from "react";
import {Accordion, Container} from "react-bootstrap";

import "./Config.css"

import Banner from "../../components/Banner";

import backArrow from '../../assets/images/back_arrow_icon.svg'


// import ExitACRNConfigurationModal from "./ExitACRNConfigurationModal/ExitACRNConfigurationModal";
import ImportABoardConfigurationFile from "./ImportABoardConfigurationFile";
import CreateNewOrImportAnExistingScenario from "./CreateNewOrImportAnExistingScenario";
import ConfigureSettingsForScenario from "./ConfigureSettingsForScenario";

import Footer from "../../components/Footer";
import {ACRNContext} from "../../ACRNContext";

class Config extends React.Component {
    constructor(props, context) {
        super(props);
        let {configurator} = context
        this.state = {
            WorkingFolder: configurator.WorkingFolder
        }
    }

    render = () => {
        return (<div>
            <Container fluid className="configBody">
                <Banner>
                    <div className="ms-4 ps-3 py-4 text-white d-flex align-items-center">
                        <img src={backArrow} className="pe-2" style={{cursor: 'pointer'}} alt="back to welcome page"
                             onClick={() => {
                                 document.location.href = '#'
                             }}/> Working folder: {this.state.WorkingFolder}
                    </div>
                </Banner>


                <Accordion defaultActiveKey={['0']} alwaysOpen>

                    {/*<!-- stage 1 -->*/}

                    <ImportABoardConfigurationFile/>
                    <Banner/>

                    {/*/!*<!-- stage 2 -->*!/*/}

                    <CreateNewOrImportAnExistingScenario/>
                    <Banner/>

                    {/*<!-- stage 3 -->*/}
                    <ConfigureSettingsForScenario/>
                </Accordion>

                <Footer/>
            </Container>
        </div>)
    };


}

Config.contextType = ACRNContext

export default Config
