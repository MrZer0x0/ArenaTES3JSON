# Contributing

The lossless layer is the compatibility contract. New typed/semantic decoders must never discard or reorder unknown bytes.

Before a pull request:

1. Build the project.
2. Run `ctest --output-on-failure`.
3. Verify representative ESP/ESM files with `ArenaTES3JSON-cli --verify`.
4. Add a self-test for any new binary handling rule.

Prefer adding semantic JSON fields as optional views over `data_b64`, not replacements for the raw data.
