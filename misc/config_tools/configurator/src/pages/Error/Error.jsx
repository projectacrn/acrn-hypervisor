import React from 'react';

export class Error extends React.Component {
    render() {
        return (
            <div>
                There's nothing here!<br/>
                Your URL: {document.location.href}
            </div>
        );
    }
}


export default Error;