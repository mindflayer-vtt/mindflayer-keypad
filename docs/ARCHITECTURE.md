# Firmware application structure

`src/main.cpp` contains only the Arduino `setup()` and `loop()` entry points. They delegate to the
application orchestrator, while implementation is grouped by action domain:

| Module               | Responsibility                                                                    |
| -------------------- | --------------------------------------------------------------------------------- |
| `Application`        | boot sequencing, provisioned Wi-Fi startup, and top-level health/rollback loop    |
| `ApplicationState`   | shared runtime state and heap diagnostics                                         |
| `SerialProvisioning` | serial envelope reception and double-reset recovery markers                       |
| `DeviceConnection`   | pinned WebSocket lifecycle, authentication, protocol dispatch, and key events     |
| `FirmwareUpdate`     | HTTPS download, signature verification, inactive-slot writing, and temporary boot |
| `LedController`      | NeoPixel construction and color updates                                           |

Reusable protocol, persistence, boot, and hardware primitives remain under `lib/`. rBoot failure
simulation remains isolated in `lib/RBootTestHooks` so the application modules contain only neutral
hook calls.
