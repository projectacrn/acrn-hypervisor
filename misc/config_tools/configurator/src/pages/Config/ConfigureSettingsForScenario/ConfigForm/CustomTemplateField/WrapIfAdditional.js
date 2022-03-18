import React from "react";
import { utils } from "@rjsf/core";
import Row from "react-bootstrap/Row";
import Col from "react-bootstrap/Col";
import Form from "react-bootstrap/Form";
import IconButton from "@rjsf/bootstrap-4";
const { ADDITIONAL_PROPERTY_FLAG } = utils;
const WrapIfAdditional = ({ children, disabled, id, label, onDropPropertyClick, onKeyChange, readonly, required, schema, }) => {
    const keyLabel = `${label} Key`; // i18n ?
    const additional = schema.hasOwnProperty(ADDITIONAL_PROPERTY_FLAG);
    if (!additional) {
        return children;
    }
    const handleBlur = ({ target }) => onKeyChange(target.value);
    // @ts-ignore
    return (React.createElement(Row, { key: `${id}-key` },
        React.createElement(Col, { xs: 5 },
            React.createElement(Form.Group, null,
                React.createElement(Form.Label, null, keyLabel),
                React.createElement(Form.Control, { required: required, defaultValue: label, disabled: disabled || readonly, id: `${id}-key`, name: `${id}-key`, onBlur: !readonly ? handleBlur : undefined, type: "text" }))),
        React.createElement(Col, { xs: 5 }, children),
        React.createElement(Col, { xs: 2, className: "py-4" },
            React.createElement(IconButton
            // @ts-ignore
            , { 
                // @ts-ignore
                block: true, className: "w-100", variant: "danger", icon: "remove", tabIndex: -1, disabled: disabled || readonly, onClick: onDropPropertyClick(label) }))));
};
export default WrapIfAdditional;
//# sourceMappingURL=WrapIfAdditional.js.map