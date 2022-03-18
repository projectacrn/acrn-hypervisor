import {useNavigate} from "react-router";
import React from "react";
import {Button, Form} from "react-bootstrap";
import {dialog} from "@tauri-apps/api";
import {ACRNContext} from "../../../ACRNContext";


class UseAnExistingConfigurationPanel extends React.Component {
    constructor(props) {
        super(props);
        const {navigate} = this.props;
        this.navigate = navigate
        this.pathSelect = React.createRef()
        this.state = {
            recentDirs: []
        }
    }

    componentDidMount() {
        this.updateHistory()
    }

    updateHistory = () => {
        let {configurator} = this.context
        return configurator.getHistory("recentlyWorkingFolders")
            .then((recentDirs) => {
                this.setState({recentDirs})
            })
    }

    recentDir = () => {
        let recent = this.state.recentDirs;
        let result = []
        for (let i = 0; i < recent.length; i++) {
            let dirPath = recent[i];
            result.push(<option key={i} value={dirPath}>
                {dirPath}
            </option>)
        }

        return result;
    }

    addRecentDir = (dirPath) => {
        let {config, configurator} = this.context
        return configurator.addHistory("recentlyWorkingFolders", dirPath)
    }

    nextPage = (WorkingFolder) => {
        let {configurator} = this.context
        this.addRecentDir(WorkingFolder).then(() => {
            let params = configurator.settingWorkingFolder(WorkingFolder)
            this.navigate(params);
        })
    }

    openDir = async () => {
        await dialog.open({
            title: 'Open Working Folder',
            directory: true,
            multiple: false
        }).then(async (existDir) => {
            await this.addRecentDir(existDir)
            await this.updateHistory()
            this.pathSelect.current.value = existDir
        }).catch()
    }

    useFolder = () => {
        let folderPath = this.pathSelect.current.value;
        if (!folderPath) {
            alert("Please select existing configuration folder!")
            return
        }
        this.nextPage(folderPath)
    }

    render() {
        let recent_dir = this.recentDir();
        return (
            <Form>
                <b className="py-2">Use an existing configuration</b>
                <p className="py-3 mb-0 mb-sm-4" style={{"maxWidth": "462px", "letterSpacing": "-0.3px"}}>
                    Open a working folder to retrieve an existing configuration.
                </p>
                <label className="d-block py-2" style={{"letterSpacing": "-0.29px"}}>
                    Select the working folder
                </label>
                <table>
                    <tbody>
                    <tr>
                        <td>
                            <Form.Select ref={this.pathSelect} className="d-inline">
                                {recent_dir}
                            </Form.Select>
                        </td>
                        <td>
                            <a className="ps-3 text-nowrap" href="#" onClick={this.openDir}>Browse for folder…</a>
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <div className="py-4 text-right">
                                <Button className="wel-btn" size="lg" onClick={this.useFolder}>Open Folder</Button>
                            </div>
                        </td>
                        <td/>
                    </tr>
                    </tbody>
                </table>

            </Form>
        )
    }
}

UseAnExistingConfigurationPanel.contextType = ACRNContext

export default function (props) {
    const navigate = useNavigate();

    return <UseAnExistingConfigurationPanel {...props} navigate={navigate}/>;
}
