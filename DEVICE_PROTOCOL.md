# Mind Flayer Device Protocol v1

The direct keypad endpoint is `WSS /device/v1`. It accepts binary WebSocket frames only; Foundry continues to use JSON text frames on `/ws`. Every device frame is one definite-length RFC 8949 CBOR array and is rejected before QCBOR when its size is zero or exceeds 512 bytes.

The first array item is an unsigned message type. Integer encodings are enums or bounded values, binary security values are byte strings, and text is used only for textual identifiers. Exact arity, order, type, range, length, complete consumption, and authorization state are mandatory.

| Type | Name | Exact array schema |
|---:|---|---|
| 0 | auth challenge | `[0, 1, challenge:bstr .size 32]` |
| 1 | auth response | `[1, deviceId:tstr .size (1..64), hmac:bstr .size 32]` |
| 2 | auth result | `[2, status:uint 0..1, deviceId:tstr .size (0..64)]`; status 0 requires the authenticated ID |
| 3 | registration | `[3, firmware:tstr .size (1..47), hardware:tstr .size (1..64)]`; controller ID is the authenticated session ID |
| 4 | key event | `[4, key:uint 0..10, action:uint 0..1]` |
| 5 | LED configuration | `[5, r1:uint8, g1:uint8, b1:uint8, r2:uint8, g2:uint8, b2:uint8]` |
| 6 | update available | `[6, version:tstr .size (1..47), size:uint32 .gt 0, sha256:bstr .size 32, path:tstr .size (1..191), token:bstr .size 32]` |

Auth status is `0=OK`, `1=FAILED`. Action is `0=UP`, `1=DOWN`. Key codes are `0=Q, 1=W, 2=E, 3=A, 4=S, 5=D, 6=Z, 7=X, 8=C, 9=SHI, 10=SPC`.

The profile rejects indefinite strings, arrays and maps; tags; floats; decimal fractions and big floats; nested containers where a scalar is required; unknown types; extra or missing fields; negative/out-of-range integers; oversized strings; malformed lengths; invalid UTF-8; and trailing objects. The keypad codec uses caller-owned output and typed structures, not a CBOR DOM or parser-owned allocation. QCBOR is pinned at v1.6.1 commit `930708bb86481e88879eb1d87fd4d664f1d69503`; unused indefinite-length, tag, exponent/mantissa, and floating-point features are compiled out.

## Canonical fixtures

These hexadecimal fixtures are normative and tested independently in Node and QCBOR:

- challenge with 32 bytes `11`: `8300015820` + `11` × 32
- response for `controller1` with HMAC bytes `22`: `83016b636f6e74726f6c6c6572315820` + `22` × 32
- auth OK for `controller1`: `8302006b636f6e74726f6c6c657231`
- registration `1.2.3`: `830365312e322e33746d696e64666c617965722d6b65797061642d7631`
- W down: `83040101`
- LED channels 1..6: `8705010203040506`
- update `1.2.3`, size 123: `860665312e322e33187b5820` + 32 zero bytes + `712f6669726d776172652f612f312e322e335820` + `33` × 32

The HMAC input remains independent of CBOR: three unsigned-32-bit-big-endian-length-prefixed fields containing `mindflayer-device-auth-v1`, device ID UTF-8 bytes, and the raw 32-byte challenge.

## Representative frame sizes

These measured payload sizes compare the prior compact JSON representation with the v1 CBOR fixtures; WebSocket framing is excluded. The restricted format is chosen for bounded parsing rather than compression, but it is also smaller in every case.

| Message | JSON bytes | CBOR bytes |
|---|---:|---:|
| challenge | 116 | 37 |
| auth response | 124 | 49 |
| registration | 144 | 30 |
| key down | 75 | 4 |
| key up | 73 | 4 |
| LED configuration | 78 | 8 |
| update available | 212 | 96 |

## Resource measurements

The Phase 1 baseline used 31,632 bytes static RAM and 462,888 bytes flash. The final generic Core 3.1.2 build uses 38,052/81,920 bytes static RAM and 442,351/1,044,464 bytes application flash: RAM is +6,420 bytes (+20.30%), while used flash is -20,537 bytes (-4.44%). The RAM increase was investigated and is primarily the deliberately fixed, caller-owned provisioning envelope, record I/O, decoded settings, redundant-record selection, and QCBOR decode work buffers. The linked symbols attributable by name to QCBOR total about 3,010 bytes; ArduinoJson was header-only, so its isolated old contribution cannot be measured from the retained baseline artifact, but removing it plus the old JSON codec yields the measured net flash reduction.

Real ESP8266 telemetry after the bounded serial RX allocation recorded 32,928 bytes free immediately after boot, 31,512 after Wi-Fi, and 10,128 after pinned WSS plus HMAC authentication. Frame decoding itself owns no heap allocation; the TLS and WebSocket libraries account for the large runtime drop.
