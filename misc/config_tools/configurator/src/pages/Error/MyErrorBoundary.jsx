import {Component} from "react";

export default class MyErrorBoundary extends Component {
    state = {
        hasError: false,
    };

    static getDerivedStateFromError(error) {
        return {hasError: true};
    };

    componentDidCatch(error, errorInfo) {
        // A custom error logging function
        console.log(error, errorInfo);
    };

    render() {
        return this.state.hasError ? <>
            Error detect, you can see error log in vconsole at right bottom.
            Please report error log to me, https://github.com/Weiyi-Feng/acrn-hypervisor/issue .
        </> : this.props.children;
    }
}