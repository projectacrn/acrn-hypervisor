import React, {createContext} from 'react'
import {Helper, TauriLocalFSBackend} from "./lib/helper";
import {Configurator} from "./lib/acrn";

// 1. Use React createContext API to create ACRN Context
export const ACRNContext = createContext({
    helper: () => {
    },
    configurator: () => {
    }
})

// 2. Create Context Provider
export class ACRNProvider extends React.Component {
    constructor(props) {
        super(props);
        let fsBackend = new TauriLocalFSBackend()
        let helper = new Helper(fsBackend, fsBackend)
        let configurator = new Configurator(helper)
        this.state = {
            helper: helper,
            configurator: configurator
        }
    }

    render() {
        console.log(this.state)
        return (
            <ACRNContext.Provider value={this.state}>
                {this.props.children}
            </ACRNContext.Provider>
        )
    }
}

// 3. export Consumer
export const ACRNConsumer = ACRNContext.Consumer
