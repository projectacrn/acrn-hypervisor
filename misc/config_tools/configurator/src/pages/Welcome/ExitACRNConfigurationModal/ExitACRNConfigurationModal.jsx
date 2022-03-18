import {CloseButton, Button, Modal} from "react-bootstrap";
import {useState} from "react";

export default function ExitACRNConfigurationModal(props) {
    const [show, setShow] = useState(false);

    const handleClose = () => setShow(false);
    const handleShow = (e) => {
        setShow(true);
        e.preventDefault()
    }

    return (
        <>
            <CloseButton className="text-right me-3" variant="white" onClick={handleShow}/>

            <Modal show={show} onHide={handleClose} size="lg">
                <Modal.Header closeButton closeVariant="white" className="bg-primary text-white">
                    <Modal.Title>Exit ACRN Configuration</Modal.Title>
                </Modal.Header>
                <Modal.Body>
                    <div className="p-4">
                        <b>Do you want exit ACRN Configuration?</b>
                    </div>
                </Modal.Body>
                <Modal.Footer className="px-5 py-4 my-2">
                    <Button className="me-sm-2" variant="outline-primary" onClick={handleClose} size="lg"
                            style={{width: '137px'}}>
                        Cancel
                    </Button>
                    <Button variant="primary" onClick={() => {
                        window.close()
                    }} size="lg" style={{width: '137px'}}>
                        Yes
                    </Button>
                </Modal.Footer>
            </Modal>
        </>
    );
}

