# Schema identification

Status: Draft 0.1

The CAN and LoRa interfaces each carry an independent 16-bit `schemaId`.
The identifier detects incompatible interface definitions; it is not a
security feature and does not replace normal message validation.

## Algorithm

1. The source of truth is the corresponding JSON manifest in `schema`.
2. Parse the JSON and serialize it as UTF-8 JSON with object keys sorted,
   no insignificant whitespace, and no ASCII escaping.
3. Calculate CRC-16/CCITT-FALSE over those bytes:
   - polynomial: `0x1021`
   - initial value: `0xFFFF`
   - input reflection: false
   - output reflection: false
   - final XOR: `0x0000`
4. Represent the result as four uppercase hexadecimal digits.

The manifest contains only wire-significant facts. Prose, spelling, diagrams,
implementation details, and test procedures do not affect the identifier.

Any change to field order, size, scaling, signedness, identifier assignment,
enumeration value, timing contract, or framing rule requires a manifest change
and therefore a new `schemaId`.

Because a 16-bit hash can collide, released identifiers shall be retained in
the schema history. If a new manifest collides with a different released
manifest, increment its `schemaSalt` until the identifier is unique.

Run the calculator from the repository root:

```powershell
python tools/calculateSchemaId.py schema/canSchema.json
python tools/calculateSchemaId.py schema/loraSchema.json
python tools/calculateSchemaId.py schema/webApiSchema.json
```
