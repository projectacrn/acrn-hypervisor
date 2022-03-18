import React from "react";

import Row from "react-bootstrap/Row";
import Col from "react-bootstrap/Col";
import Container from "react-bootstrap/Container";

import {ObjectFieldTemplateProps} from "@rjsf/core";
import {utils} from "@rjsf/core";

import AddButton from "../AddButton/AddButton";

const {canExpand} = utils;

const ObjectFieldTemplate = (
    {
        DescriptionField,
        description,
        TitleField,
        title,
        properties,
        required,
        uiSchema,
        idSchema,
        schema,
        formData,
        onAddClick,
        disabled,
        readonly,
    }: ObjectFieldTemplateProps) => {
    let content = properties.map((element: any, index: number) => {
        return (
            <Col
                key={index}
                style={{marginBottom: "10px"}}
                className={element.hidden ? "d-none" : undefined}
                xs={(uiSchema[element.name] && uiSchema[element.name]['ui:grid']) ?
                    uiSchema[element.name]['ui:grid'] :
                    12
                }>
                {element.content}
            </Col>
        );
    })
    let expand = canExpand(schema, uiSchema, formData) ? (
        <Row>
            <Col xs={{offset: 9, span: 3}} className="py-4">
                <AddButton
                    onClick={onAddClick(schema)}
                    disabled={disabled || readonly}
                    className="object-property-expand"
                />
            </Col>
        </Row>
    ) : null

    let container = <Container fluid className="p-0">
        <Row>{content}</Row>
        {expand}
    </Container>

    return (
        <>
            {(uiSchema["ui:title"] || title) && (
                <TitleField
                    id={`${idSchema.$id}-title`}
                    title={uiSchema["ui:title"] || title}
                    required={required}
                />
            )}
            {description && (
                <DescriptionField
                    id={`${idSchema.$id}-description`}
                    description={description}
                />
            )}
            {container}
        </>
    );
};

export default ObjectFieldTemplate;
