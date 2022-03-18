import React, {Component} from "react";
import './Tab.css'

export default class Tab extends Component {
    constructor(props) {
        super(props);
    }

    setActive = () => {
        this.props.active(this.props.VMID)
    }

    render() {
        let active = this.props.active() === this.props.VMID ? "Active" : ''
        return (
            <div className={"d-inline-block p-3 pt-2 Tab " + active} onClick={this.setActive}>
                <div style={{fontSize: "22px"}}>{this.props.title}</div>
                <div>{this.props.desc}</div>
            </div>
        )
    }
}