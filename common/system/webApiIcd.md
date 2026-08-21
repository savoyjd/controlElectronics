# sensor family web API interface control document

Status: Draft 0.2  
Schema ID: `0x208B`

The machine-readable source of truth is `schema/webApiSchema.json`. This ICD
defines a common read-only web interface for both the vehicle sensorRF and the
paddock LoRa receiver. JSON property names use camelCase.

## Purpose and host roles

Both hosts serve the same page and API, but they represent different evidence:

| `hostRole` | Meaning |
|---|---|
| `vehicleMirror` | Locally acquired vehicle data formatted as LoRa telemetry. It does not prove that a radio packet reached the paddock. |
| `paddockReceiver` | Telemetry decoded from packets actually received over LoRa. It exposes radio reception age and link measurements. |

Every identity and snapshot response shall include `hostRole`, `hostDeviceId`,
and schema identifiers. The HTML page shall display the role prominently above
all telemetry and shall label vehicle data **LOCAL — NOT PROOF OF LORA
DELIVERY**. The paddock page shall display **PADDOCK — RECEIVED LORA DATA**.
Color alone shall not communicate this distinction.

## Wi-Fi topology

The vehicle `vehicleMirror` host shall operate as a Wi-Fi SoftAP. It shall not
depend on paddock infrastructure, Internet access, or a previously provisioned
station network. This provides a direct local diagnostic path when personnel
are near the vehicle.

The `paddockReceiver` host shall use simultaneous SoftAP and station mode. Its
HTTP service shall be reachable through both interfaces when the station link
is available. Failure to associate with, obtain an address from, or retain the
configured paddock network shall not disable or restart the SoftAP or web
service. Recovery attempts on the station interface shall be bounded and shall
not block LoRa reception.

Both SoftAPs shall use WPA2 or stronger protection and unique provisioned
credentials. Production firmware shall not use a universal default password.
SSID, credentials, channel, IP subnet, station provisioning, and recovery
mechanisms remain configuration details for the next iteration.

## HTTP behavior

The initial interface is read-only:

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/` | Small self-contained telemetry page |
| `GET` | `/api/v1/identity` | Host role, device, firmware, and schema identity |
| `GET` | `/api/v1/snapshot` | Latest complete telemetry snapshot |
| `GET` | `/api/v1/schema` | The canonical web API schema document |

Successful JSON responses use `Content-Type: application/json; charset=utf-8`.
Unknown routes return 404. An unavailable snapshot returns 503 with a JSON
error body; individual missing telemetry groups do not make the whole snapshot
unavailable. Responses shall include `Cache-Control: no-store`.

API versioning and binary interface schemas are independent. Breaking HTTP or
JSON changes use a new `/api/vN` path. Changes to CAN or LoRa encoding update
their schema identifiers without necessarily changing the API version.

## Identity response

`GET /api/v1/identity` returns at least:

```json
{
  "apiVersion": 1,
  "webApiSchemaId": 8331,
  "hostRole": "vehicleMirror",
  "hostDeviceId": 305419896,
  "firmwareVersion": "0.1.0",
  "canSchemaId": 6773,
  "loraSchemaId": 32821
}
```

Numeric schema IDs are unsigned JSON integers. User interfaces should also
render them as four-digit hexadecimal values.

## Snapshot response

`GET /api/v1/snapshot` returns one coherent copy of the most recently available
state. The implementation shall capture or lock the snapshot before JSON
serialization so fields from different updates are not accidentally combined.

Required top-level properties are:

```json
{
  "apiVersion": 1,
  "webApiSchemaId": 8331,
  "hostRole": "vehicleMirror",
  "hostDeviceId": 305419896,
  "generatedAt": {
    "monotonicMs": 1234567,
    "utc": null
  },
  "telemetrySourceNode": 1,
  "loraSchemaId": 32821,
  "canSchemaId": 6773,
  "navigation": {},
  "dynamicTelemetry": {},
  "thermalTelemetry": {},
  "statusTelemetry": {},
  "transport": {}
}
```

The four telemetry objects mirror the decoded packet payloads defined in
`loraIcd.md`, using the same field names, engineering units, unavailable-value
semantics, and flag meanings. The API shall expose decoded engineering values,
not only packed integers. An unavailable measurement is JSON `null`; its
validity and age fields remain present. NaN and infinity are prohibited JSON
values.

`generatedAt.monotonicMs` is the host monotonic time at snapshot capture.
`generatedAt.utc` is an ISO 8601 UTC string when a trustworthy UTC source is
available and otherwise `null`.

## Transport provenance

The `transport` object contains at least:

```json
{
  "mode": "localPreTransmit",
  "lastPacketSequence": 42,
  "lastPacketTimestamp": 1234000,
  "transportAgeMs": 567,
  "packetValid": true,
  "rssiDbm": null,
  "snrDb": null
}
```

For `vehicleMirror`, `mode` is `localPreTransmit`; sequence and timestamp refer
to the most recently constructed LoRa packet. `transportAgeMs` is its age, and
RSSI/SNR are `null`. This role shall never describe the packet as received.

For `paddockReceiver`, `mode` is `receivedLora`; sequence and timestamp come
from the most recently accepted LoRa frame, `transportAgeMs` is time since its
reception, and RSSI/SNR contain receiver measurements when supported. Failed
CRC, schema, authentication, or length validation shall not update the exposed
telemetry snapshot or `lastPacketSequence`.

Each telemetry group shall additionally expose its own update age because the
LoRa packet classes have different rates. `transportAgeMs` is not a substitute
for CAN-source ages already carried inside telemetry payloads.

## Built-in page

The page served at `/` shall use only local assets so it remains useful without
Internet access. It shall poll `/api/v1/snapshot` no faster than once every five
seconds, display the time of its last successful refresh, and visibly mark
stale, invalid, or unavailable groups. A failed poll shall leave the last data
visible but mark it stale; it shall not replace values with plausible zeros.

The first page is diagnostic rather than a race dashboard. It should favor a
clear table of navigation, speeds, suspension extrema, acceleration extrema,
thermocouples, system health, schema IDs, and link provenance.

## Deferred decisions

- SSID provisioning, WPA credentials, and recovery behavior.
- Cross-origin policy and phone-app discovery.
- HTTPS feasibility and telemetry confidentiality.
- Command API endpoints, authorization, and mapping to authenticated LoRa
  commands.
- Multiple vehicle/source selection at a paddock receiver.
