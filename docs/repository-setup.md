# Repository setup

## Firmware release signing key

Generate the production signing key before installing firmware on production keypads. Use an offline or otherwise tightly controlled machine, and keep the private key outside this repository:

```sh
umask 077
mkdir -p /secure/offline/location/mindflayer-signing
cd /secure/offline/location/mindflayer-signing

openssl genpkey \
  -algorithm RSA \
  -pkeyopt rsa_keygen_bits:2048 \
  -out signing-private.pem

openssl pkey \
  -in signing-private.pem \
  -pubout \
  -out signing-public.pem

chmod 600 signing-private.pem
chmod 644 signing-public.pem
```

The GitHub release workflow is non-interactive, so the private key must not require a passphrase. Compensate by strictly limiting access to the key and keeping an encrypted offline backup.

### Configure the repository public key

Copy only `signing-public.pem` to `keys/firmware-signing-public.pem` and commit it. This is the authoritative public-key file used by release signing. Replace `FIRMWARE_SIGNING_PUBLIC_KEY_PEM` in `include/HardwareConfig.h` with the exact same public PEM, represented as the existing newline-terminated C string. Do not create another public PEM under `scripts/`.

Verify that the distributable public key and firmware trust anchor are identical:

```sh
./scripts/verify-signing-key.py
```

Test a complete signed release locally without placing the private key contents in an environment variable:

```sh
FIRMWARE_SIGNING_PRIVATE_KEY_FILE=/secure/offline/location/mindflayer-signing/signing-private.pem \
  ./scripts/build-release.sh 1.0.0
```

The generated release archive is written under `dist/`. Verify the first production archive before distributing or importing it into the server firmware repository.

### Configure the GitHub Actions secret

In GitHub, open **Repository settings → Secrets and variables → Actions**, create a repository secret, and use this exact name:

```text
FIRMWARE_SIGNING_PRIVATE_KEY
```

Paste the complete private PEM as a multiline value, including its boundary lines:

```text
-----BEGIN PRIVATE KEY-----
...
-----END PRIVATE KEY-----
```

Do not base64-encode the value. `scripts/build-release.sh` writes the secret directly to a temporary mode-0600 file and removes that file when the release build exits.

Pay particular attention to the following:

- Never commit the private key, copy it into the server repository or container, include it in release artifacts, or print it in workflow logs.
- Keep at least one encrypted offline backup. Losing the private key prevents production of updates accepted by existing keypads.
- Restrict repository administration and workflow-editing permissions. A person able to change and run the release workflow on `main` may be able to misuse the secret.
- Consider placing the key in a protected GitHub Environment with required reviewers if every production release should have a human approval gate. Update the release job to use that environment before moving the secret there.
- GitHub does not provide repository secrets to workflows triggered from forks. The release job uses the key only for pushes to `main`.

### Key rotation warning

Firmware-signing key rotation is not currently implemented. Every keypad trusts the public key compiled into its installed firmware, and the build scripts require a release private key that matches the compiled public key. Replacing the key after devices have been deployed therefore requires a deliberately designed transition and cannot be performed by merely changing the GitHub secret.

Do not delete or replace the active private key while deployed keypads depend on it. If the private key may have been exposed, stop releases and design a recovery or reprovisioning plan before changing either public-key copy.

For disposable local development only, `scripts/generate-test-signing-key.sh` can create a test pair. Do not treat its default repository-local output directory as suitable storage for a production private key.
