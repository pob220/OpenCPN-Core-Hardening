# Environmental GRIB catalogue port

The in-tree development UI now invokes the native `environmental-grib` helper
using the versioned JSON job protocol. Python is not part of this path.

## Runtime boundary

- The plugin writes a schema-versioned job file.
- Copernicus passwords are passed only in the child environment.
- The native helper writes an atomic result file and JSON Lines progress.
- The plugin validates the generated GRIB before requesting that GRIB opens it.
- The helper remains a separate process so provider/decoder failures do not
  terminate OpenCPN or introduce its dependency symbols into OpenCPN.

For source builds, `ENVIRONMENTAL_GRIB_HELPER` identifies the helper copied
beside the OpenCPN executable. Installed development builds place it below
`share/opencpn/plugins/environmental_grib_pi/bin`.

## Catalogue packaging still required

The catalogue package must build the helper from a pinned generator revision
and bundle its private dependency closure, ecCodes definitions/samples, and
PROJ data. It must not rely on the development machine's shared libraries.
Platform-specific RPATH/install-name handling and Windows DLL deployment must
be validated in the standalone `environmental_grib_pi` frontend2 repository.

The first catalogue release is desktop-only. Android remains unsupported until
the dependency size, storage permissions, and background execution model are
resolved.
