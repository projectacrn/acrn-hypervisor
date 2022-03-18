import React from "react";

export class Banner extends React.Component {
    render() {
        return (
            <div className="banner">{this.props.children}</div>
        );
    }
}
