# Serial-install release bundle

The release build now prepares `mindflayer-keypad-VERSION-serial-install.tar.gz`
in addition to the existing OTA archive. The semantic-release configuration uploads
both when a release is actually published. This code change does not publish one.

Format 1 contains rBoot, metadata A/B, the unsigned initial application,
`install-rboot.py`, `LICENSE`, `manifest.json` and `manifest.sig`. The manifest
records hardware, version, exact flash size, file sizes/hashes and flash addresses.
It also declares `deviceProtocol: 3`; an installer must check server capabilities
before flashing. Older servers cannot authenticate this new firmware, while the
updated server continues to support older v1/v2 keypads.
Its exact bytes are signed with RSA-2048/SHA-256 using the firmware signing key;
the builder verifies against the repository's public trust anchor before publishing
the archive locally. Consumers must pin that key independently, not trust a key
from an uploaded/downloaded archive.

The fixed ESP8266/4 MiB layout writes rBoot at `0`, metadata at `0x1000` and
`0x100000`, and the unsigned boot2 application at `0x2000`. Provisioning sectors
`0x3f9000` and `0x3fa000` must be backed up and preserved. The existing installer's
image-format validation is run by the packager and must run again before flashing;
signatures alone do not replace chip/flash-size checks or provisioning backup.

`python3 -m unittest discover -s test -p 'test_serial_bundle.py'` tests packaging
with synthetic images and temporary keys, including wrong-key and signed-OTA
rejection without replacing a previously generated bundle. It does not flash
hardware or use the production private key.

Dependabot covers Actions, npm release tooling and PlatformIO Core through
`requirements.txt`. PlatformIO library/platform versions, rBoot/esptool2 source
pins and signing trust anchors still need explicit firmware compatibility review.
