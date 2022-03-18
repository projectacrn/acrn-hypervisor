import React from "react";

import Form from "react-bootstrap/Form";

import {WidgetProps} from "@rjsf/core";
import _ from "lodash";

const TextWidget = (
    {
        id,
        placeholder,
        required,
        readonly,
        disabled,
        type,
        label,
        value,
        onChange,
        onBlur,
        onFocus,
        autofocus,
        options,
        schema,
        rawErrors = [],
        uiSchema,
    }: WidgetProps) => {
    const _onChange = ({
                           target: {value},
                       }: React.ChangeEvent<HTMLInputElement>) =>
        onChange(value === "" ? options.emptyValue : value);
    const _onBlur = ({target: {value}}: React.FocusEvent<HTMLInputElement>) =>
        onBlur(id, value);
    const _onFocus = ({
                          target: {value},
                      }: React.FocusEvent<HTMLInputElement>) => onFocus(id, value);
    const inputType = (type || schema.type) === "string" ? "text" : `${type || schema.type}`;

    // const classNames = [rawErrors.length > 0 ? "is-invalid" : "", type === 'file' ? 'custom-file-label': ""]
    // let isFirst = _.endsWith(id, 'IVSHMEM_VM_0_VBDF')
    return (
        <Form.Group className="mb-0 row ps-4">
            <div className="col-sm-12">
                <Form.Control
                    id={id}
                    placeholder={placeholder}
                    autoFocus={autofocus}
                    required={required}
                    disabled={disabled}
                    readOnly={readonly}
                    className={rawErrors.length > 0 ? "is-invalid" : ""}
                    list={schema.examples ? `examples_${id}` : undefined}
                    type={inputType}
                    value={value || value === 0 ? value : ""}
                    onChange={_onChange}
                    onBlur={_onBlur}
                    onFocus={_onFocus}
                />
            </div>
            {schema.examples ? (
                <datalist id={`examples_${id}`}>
                    {(schema.examples as string[])
                        .concat(schema.default ? ([schema.default] as string[]) : [])
                        .map((example: any) => {
                            return <option key={example} value={example}/>;
                        })}
                </datalist>
            ) : null}
        </Form.Group>
    );
};

export default TextWidget;
