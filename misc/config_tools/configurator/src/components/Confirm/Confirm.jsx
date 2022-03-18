import {Button, Modal} from "react-bootstrap";
import {useState} from "react";

export default function Confirm(props) {
    const [show, setShow] = useState(false);

    const handleClose = (choice) => {
        setShow(false);
        props.callback(choice)
    }

    const handleShow = (e) => {
        setShow(true);
        e.preventDefault()
    }

    return (
        <Modal show={show} onHide={() => handleClose('cancel')} size="lg">
            <Modal.Header closeButton closeVariant="white" className="bg-primary text-white">
                <Modal.Title>{props.title}</Modal.Title>
            </Modal.Header>
            <Modal.Body>
                <div className="p-4">
                    <b>{props.content}</b>
                </div>
            </Modal.Body>
            <Modal.Footer className="px-5 py-4 my-2">
                <Button className="me-sm-2" variant="outline-primary" onClick={() => handleShow('cancel')}
                        size="lg" style={{width: '137px'}}>
                    Cancel
                </Button>
                <Button className="me-sm-2" variant="outline-primary" onClick={() => handleShow('no')} size="lg"
                        style={{width: '137px'}}>
                    {props.no ? props.no : "No"}
                </Button>
                <Button variant="primary" onClick={() => {
                    props.callback('yes')
                }} size="lg" style={{width: '137px'}}>
                    {props.yes ? props.yes : "Yes"}
                </Button>
            </Modal.Footer>
        </Modal>
    );
}

