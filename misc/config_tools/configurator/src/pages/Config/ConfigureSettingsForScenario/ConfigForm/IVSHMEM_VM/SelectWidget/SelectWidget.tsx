import React from "react";

import Form from "react-bootstrap/Form";

import {WidgetProps} from "@rjsf/core";
import {utils} from "@rjsf/core";
import _ from "lodash";

const {asNumber, guessType} = utils;

const nums = new Set(["number", "integer"]);

/**
 * This is a silly limitation in the DOM where option change event values are
 * always retrieved as strings.
 */
const processValue = (schema: any, value: any) => {
    // "enum" is a reserved word, so only "type" and "items" can be destructured
    const {type, items} = schema;
    if (value === "") {
        return undefined;
    } else if (type === "array" && items && nums.has(items.type)) {
        return value.map(asNumber);
    } else if (type === "boolean") {
        return value === "true";
    } else if (type === "number") {
        return asNumber(value);
    }

    // If type is undefined, but an enum is present, try and infer the type from
    // the enum values
    if (schema.enum) {
        if (schema.enum.every((x: any) => guessType(x) === "number")) {
            return asNumber(value);
        } else if (schema.enum.every((x: any) => guessType(x) === "boolean")) {
            return value === "true";
        }
    }

    return value;
};

const SelectWidget = (
    {
        schema,
        id,
        options,
        label,
        required,
        disabled,
        readonly,
        value,
        multiple,
        autofocus,
        onChange,
        onBlur,
        onFocus,
        placeholder,
        rawErrors = [],
    }: WidgetProps) => {
    const {enumOptions, enumDisabled} = options;

    const emptyValue = multiple ? [] : "";

    function getValue(
        event: React.FocusEvent | React.ChangeEvent | any,
        multiple: Boolean,
    ) {
        if (multiple) {
            return [].slice
                .call(event.target.options as any)
                .filter((o: any) => o.selected)
                .map((o: any) => o.value);
        } else {
            return event.target.value;
        }
    }

    let isFirst = _.endsWith(id, 'IVSHMEM_VM_0_VM_NAME')
    return (
        <Form.Group className="row" style={{marginTop: isFirst ? '54px' : '16px'}}>
            <div className={"col-sm-12"}>
                <Form.Control
                    as="select"
                    id={id}
                    value={typeof value === "undefined" ? emptyValue : value}
                    required={required}
                    multiple={multiple}
                    disabled={disabled}
                    readOnly={readonly}
                    autoFocus={autofocus}
                    className={"form-select " + (rawErrors.length > 0 ? "is-invalid" : "")}
                    onBlur={
                        onBlur &&
                        ((event: React.FocusEvent) => {
                            const newValue = getValue(event, multiple);
                            onBlur(id, processValue(schema, newValue));
                        })
                    }
                    onFocus={
                        onFocus &&
                        ((event: React.FocusEvent) => {
                            const newValue = getValue(event, multiple);
                            onFocus(id, processValue(schema, newValue));
                        })
                    }
                    onChange={(event: React.ChangeEvent) => {
                        const newValue = getValue(event, multiple);
                        onChange(processValue(schema, newValue));
                    }}>
                    {!multiple && schema.default === undefined && (
                        <option value="">{placeholder}</option>
                    )}
                    {(enumOptions as any).map(({value, label}: any, i: number) => {
                        const disabled: any =
                            Array.isArray(enumDisabled) &&
                            (enumDisabled as any).indexOf(value) != -1;
                        return (
                            <option key={i} id={label} value={value} disabled={disabled}>
                                {label}
                            </option>
                        );
                    })}
                </Form.Control>
            </div>
        </Form.Group>
    );
};

export default SelectWidget;
