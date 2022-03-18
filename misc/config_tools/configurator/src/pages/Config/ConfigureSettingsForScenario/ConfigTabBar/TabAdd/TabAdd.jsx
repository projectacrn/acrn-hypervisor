import React, {Component} from "react";
import addIcon from './assets/Plus.svg'
import './TabAdd.css'

export default class TabAdd extends Component {
    render() {
        return (<div className="d-inline-block p-3 pt-2 TabAdd" onClick={this.props.addVM}>
            <div style={{fontSize: "22px"}}><img src={addIcon} alt={"Add " + this.props.desc}/></div>
            <div>{this.props.desc}</div>
        </div>)
    }
}