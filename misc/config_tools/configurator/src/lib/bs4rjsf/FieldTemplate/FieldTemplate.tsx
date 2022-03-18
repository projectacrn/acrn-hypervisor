import React from "react";

import {FieldTemplateProps} from "@rjsf/core";

import Form from "react-bootstrap/Form";
import ListGroup from "react-bootstrap/ListGroup";

import WrapIfAdditional from "./WrapIfAdditional";

import {OverlayTrigger, Popover} from "react-bootstrap";
import {FontAwesomeIcon} from "@fortawesome/react-fontawesome";
import {faCircleInfo, faCircleExclamation} from "@fortawesome/free-solid-svg-icons";
import _ from "lodash";

const FieldTemplate = (
    {
        id,
        children,
        displayLabel,
        rawErrors = [],
        rawHelp,
        rawDescription,
        classNames,
        disabled,
        label,
        onDropPropertyClick,
        onKeyChange,
        readonly,
        required,
        schema,
        uiSchema
    }: FieldTemplateProps) => {

    let descLabel = (uiSchema.hasOwnProperty("ui:descLabel") && uiSchema["ui:descLabel"] === true)
    let descWithChildren
    let showLabel = _.endsWith(id, 'IVSHMEM_VM_0_VBDF') || _.endsWith(id, 'IVSHMEM_VM_0_VM_NAME') || (
        id.indexOf('vuart_connection') > 0 && (_.endsWith(id, 'vm_name') || _.endsWith(id, 'io_port'))
    )
    let dlva = uiSchema.hasOwnProperty("ui:descLabelAli") && uiSchema["ui:descLabelAli"] === 'V'
    if (displayLabel && rawDescription) {
        let desc
        const icon = rawErrors.length > 0 ? faCircleExclamation : faCircleInfo;
        if (descLabel) {
            if (dlva) {
                desc = <OverlayTrigger
                    trigger={["hover", "focus"]}
                    key="top"
                    placement="top"
                    overlay={
                        <Popover id={`popover-positioned-top`}>
                            <Popover.Body>
                                <Form.Text className={rawErrors.length > 0 ? "text-danger" : "text-muted"}
                                           dangerouslySetInnerHTML={{__html: rawDescription}}/>
                            </Popover.Body>
                        </Popover>
                    }>
                    <div className="mx-2 py-2 row">
                        <Form.Label
                            className={(showLabel ? 'col-12 ps-4' : 'd-none') + " col-form-label " + (rawErrors.length > 0 ? "text-danger" : "")}>
                            {uiSchema["ui:title"] || schema.title || label}
                            {(label || uiSchema["ui:title"] || schema.title) && required ? "*" : null}
                        </Form.Label>
                    </div>
                </OverlayTrigger>
                descWithChildren = <div className="row">
                    {desc}
                    <div className="col-12">
                        {children}
                    </div>
                </div>
            } else {
                desc = <OverlayTrigger
                    trigger={["hover", "focus"]}
                    key="top"
                    placement="top"
                    overlay={
                        <Popover id={`popover-positioned-top`}>
                            <Popover.Body>
                                <Form.Text className={rawErrors.length > 0 ? "text-danger" : "text-muted"}
                                           dangerouslySetInnerHTML={{__html: rawDescription}}/>
                            </Popover.Body>
                        </Popover>
                    }>
                    <div className="col-4"
                         style={{marginTop: (uiSchema.hasOwnProperty("ui:descLabelMT") && uiSchema["ui:descLabelMT"] ? '54px' : 'auto')}}>
                        <Form.Label
                            className={(showLabel ? 'col-12 ps-4' : 'd-none') + " col-form-label " + (rawErrors.length > 0 ? "text-danger" : "")}>
                            {uiSchema["ui:title"] || schema.title || label}
                            {(label || uiSchema["ui:title"] || schema.title) && required ? "*" : null}
                        </Form.Label>
                    </div>
                </OverlayTrigger>
                descWithChildren = <div className="row">
                    {desc}
                    <div className="col-8">
                        {children}
                    </div>
                </div>
            }
        } else {
            desc = <OverlayTrigger
                trigger={["hover", "focus"]}
                key="top"
                placement="top"
                overlay={
                    <Popover id={`popover-positioned-top`}>
                        <Popover.Body>
                            <Form.Text className={rawErrors.length > 0 ? "text-danger" : "text-muted"}
                                       dangerouslySetInnerHTML={{__html: rawDescription}}/>
                        </Popover.Body>
                    </Popover>
                }>
                <div className="mx-2 py-2 descInfoBtn">
                    <FontAwesomeIcon
                        icon={icon}
                        color={rawErrors.length > 0 ? "red" : ""}
                    />
                </div>
            </OverlayTrigger>
            descWithChildren = <div className="d-flex">
                {desc}
                <div className="w-100">
                    {children}
                </div>
            </div>
        }


    } else {
        descWithChildren = children
    }
    return (
        <WrapIfAdditional
            classNames={classNames}
            disabled={disabled}
            id={id}
            label={label}
            onDropPropertyClick={onDropPropertyClick}
            onKeyChange={onKeyChange}
            readonly={readonly}
            required={required}
            schema={schema}
        >
            <Form.Group style={uiSchema.hasOwnProperty('ui:style') ? uiSchema['ui:style'] : {}}>
                {descWithChildren}
                {rawErrors.length > 0 && (
                    <ListGroup as="ul" className={descLabel ? (dlva ? 'ps-4' : ' col-8 offset-4') : ''}>
                        {rawErrors.map((error: string) => {
                            return (
                                <ListGroup.Item as="li" key={error} className="border-0 m-0 p-0">
                                    <small className="m-0 text-danger">
                                        {error}
                                    </small>
                                </ListGroup.Item>
                            );
                        })}
                    </ListGroup>
                )}
                {rawHelp && (
                    <Form.Text
                        className={rawErrors.length > 0 ? "text-danger" : "text-muted"}
                        id={id}>
                        {rawHelp}
                    </Form.Text>
                )}
            </Form.Group>
        </WrapIfAdditional>
    );
};

export default FieldTemplate;
