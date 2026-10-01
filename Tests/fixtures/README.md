# Quad Cortex USB test captures

These `.hex` files are real USB-HID reports (one 129-byte report per line, hex) captured between
Cortex Control 4.0.1 and a Quad Cortex on CorOS 4.0.1. They come from the
[pyquadcortex](https://github.com/stokes-audio/pyquadcortex) test fixtures (MIT licence, see
`LICENSE-pyquadcortex.txt`) and are used by `Tests/CueModelTests.cpp` to check PedalCues' read-only
USB framing and decoding (`Source/QcUsb.*`).

| file | what it is |
|---|---|
| `qc_version_read.hex` | host to QC: Version READ |
| `qc_version_reply.hex` | QC to host: Version reply over 3 reports (CorOS "4.0.1") |
| `qc_file_reply_plain.hex` | QC to host: an empty folder push ("cloud-0-1", Downloads) |
| `qc_file_reply_gzip.hex` | QC to host: a gzipped folder push over 33 reports (the impulse-response folder, 588 files) |
| `qc_license_encrypted.hex` | QC to host: an encrypted License reply (must be skipped) |
