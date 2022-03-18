import {Component} from "react";
import {getVersion} from "@tauri-apps/api/app";


export default class Footer extends Component {
    constructor(props) {
        super(props);
        this.state = {
            version: "0.1.0"
        }
    }

    componentDidMount = () => {
        getVersion().then((version) => {
            this.setState({version: version})
        })
    }

    render = () => {
        return <div className="pt-3">
            <p className="text-center text-secondary">© Copyright Project ACRN™, a Series of LF Projects, LLC.
                - Version {this.state.version}</p>
        </div>
    }
}