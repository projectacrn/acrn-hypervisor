import {Button, Form, Modal} from "react-bootstrap";
import {Component} from "react";
import "./CreateScenarioModal.css"
import {ACRNContext} from "../../../../ACRNContext";

export default class CreateScenarioModal extends Component {
    constructor(props) {
        super(props);
        this.state = {
            show: false,
            mode: 'POST_LAUNCHED_VM'
        }
    }


    handelScenarioCreateEvent = () => {
        function getVal(id) {
            // @ts-ignore
            return document.querySelector('#' + id + 'Inp').value
        }

        function getInt(id) {
            return parseInt(getVal(id))
        }

        const vmNum = {
            PRE_LAUNCHED_VM: this.state.mode === 'POST_LAUNCHED_VM' ? 0 : getInt('PRE_LAUNCHED_VM'),
            SERVICE_VM: this.state.mode === 'PRE_LAUNCHED_VM' ? 0 : getInt('SERVICE_VM'),
            POST_LAUNCHED_VM: this.state.mode === 'PRE_LAUNCHED_VM' ? 0 : getInt('POST_LAUNCHED_VM'),
        }
        let {configurator} = this.context

        configurator.programLayer.newScenario(vmNum.PRE_LAUNCHED_VM, vmNum.SERVICE_VM, vmNum.POST_LAUNCHED_VM)
        this.props.cb()
        this.setState({show: false})
    }


    render = () => {

        const handleClose = () => this.setState({show: false});
        const handleShow = () => this.setState({show: true});
        const handleScenarioModeChange = (event) => this.setState({mode: event.target.id})

        return (
            <>
                <Button size="lg" variant="primary" onClick={handleShow}>
                    Create Scenario…
                </Button>

                <Modal show={this.state.show} onHide={handleClose} size="lg">
                    <Modal.Header closeButton closeVariant="white" className="bg-primary text-white">
                        <Modal.Title>Create a new Scenario</Modal.Title>
                    </Modal.Header>
                    <Modal.Body>
                        <Form className="p-4">
                            <div className="form-group p-4 bg-gray" style={{border: "1px solid #D7D7D7"}}>
                                <div className="form-group pb-4" onChange={handleScenarioModeChange}>
                                    <p className="d-flex justify-content-between">
                                        <b>Choose a scenario type:</b>
                                        <a className="fs-6" style={{cursor: "pointer"}}>
                                            Learn about scenarios
                                        </a>
                                    </p>
                                    <Form.Check
                                        type="radio" className="mb-3" name="scenarioMode" id="POST_LAUNCHED_VM"
                                        label={<>Shared <i>(Post-launched VMs only)</i></>}
                                        defaultChecked={this.state.mode === 'POST_LAUNCHED_VM'}
                                    />

                                    <Form.Check
                                        type="radio" className="mb-3" name="scenarioMode" id="PRE_LAUNCHED_VM"
                                        label={<>Partitioned <i>(Pre-launched VMs only)</i></>}
                                        defaultChecked={this.state.mode === 'PRE_LAUNCHED_VM'}
                                    />

                                    <Form.Check
                                        type="radio" className="mb-3" name="scenarioMode" id="hybrid"
                                        label={<>Hybrid <i>(Both pre-launched and post-launched VMs)</i></>}
                                        defaultChecked={this.state.mode === 'hybrid'}
                                    />
                                </div>
                                <b>VM Configuration</b>
                                <p className="mt-3">This system’s board supports a maximum of 8 VMs.</p>
                                <p> How many of each VM type do you want in your scenario? (You can change these
                                    later.)</p>
                                <div style={{maxWidth: '332px'}} className={[
                                    'px-4', 'ms-2', 'vmNum', 'd-flex', 'justify-content-between',
                                    'align-items-center', 'flex-wrap', 'align-content-between'
                                ].join(' ')}>
                                    <b className={this.state.mode === 'POST_LAUNCHED_VM' ? 'd-none' : ''}>Pre-launch
                                        VMs:</b>
                                    <Form.Control
                                        type="number" min="0" max="8" defaultValue="1" id="PRE_LAUNCHED_VMInp"
                                        className={this.state.mode === 'POST_LAUNCHED_VM' ? 'd-none' : ''}
                                    />
                                    <b className={this.state.mode === 'PRE_LAUNCHED_VM' ? 'd-none' : ''}>Service VM:</b>
                                    <Form.Control
                                        disabled type="number" min="0" max="8" id="SERVICE_VMInp" defaultValue="1"
                                        className={this.state.mode === 'PRE_LAUNCHED_VM' ? 'd-none' : ''}
                                    />
                                    <b className={this.state.mode === 'PRE_LAUNCHED_VM' ? 'd-none' : ''}>Post-launch
                                        VMs:</b>
                                    <Form.Control
                                        type="number" min="0" max="8" defaultValue="1" id="POST_LAUNCHED_VMInp"
                                        className={this.state.mode === 'PRE_LAUNCHED_VM' ? 'd-none' : ''}
                                    />
                                </div>
                            </div>
                        </Form>
                    </Modal.Body>
                    <Modal.Footer className="px-5 py-4">
                        <Button className="me-sm-2" variant="outline-primary" onClick={handleClose} size="lg"
                                style={{width: '137px'}}>
                            Cancel
                        </Button>
                        <Button variant="primary" onClick={this.handelScenarioCreateEvent} size="lg"
                                style={{width: '137px'}}>
                            Create
                        </Button>
                    </Modal.Footer>
                </Modal>
            </>
        )
    }
}

CreateScenarioModal.contextType = ACRNContext