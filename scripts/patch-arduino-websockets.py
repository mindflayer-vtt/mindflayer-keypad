"""Make ArduinoWebsockets 0.5.4 yields safe in ESP8266 SDK callbacks.

The ESP8266 Arduino core aborts when yield() is called from the SDK (SYS)
context.  Its optimistic_yield() API is deliberately safe in both SYS and the
normal Arduino continuation context.  Keep this patch local to PlatformIO's
per-project dependency copy and reject an unexpected upstream source shape.
"""

from pathlib import Path

Import("env")

header = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / (
    "ArduinoWebsockets/src/tiny_websockets/network/generic_esp/"
    "generic_esp_clients.hpp"
)

def patch_yields(target, source, env):
    del target, source, env
    if not header.is_file():
        raise RuntimeError(f"pinned ArduinoWebsockets header is missing: {header}")
    source = header.read_text(encoding="utf-8")
    unsafe = source.count("yield();")
    safe = source.count("optimistic_yield(1000);")
    if unsafe == 10 and safe == 0:
        header.write_text(
            source.replace("yield();", "optimistic_yield(1000);"), encoding="utf-8"
        )
    elif unsafe != 0 or safe != 10:
        raise RuntimeError(
            "ArduinoWebsockets 0.5.4 generic ESP client changed; refusing to apply "
            f"yield safety patch (yield={unsafe}, optimistic_yield={safe})"
        )


env.AddPreAction("$BUILD_DIR/src/main.cpp.o", patch_yields)
