# xGRIB 0.2.5.2 developer-bundle integration

The bundle pins public xGRIB `f5e1ea1019f37af4d8d8e951f43213e8d122f96d` (0.2.5.2),
with generator 0.1.8 at `bf650d8960423461f607f9d96edb257e1092a7b9`.
It applies `xgrib-0.2.5.2-provider.patch` after checking it with `git apply --check`.

The patch preserves the previous bundle's external environmental acquisition
provider, originally from `446814bafe6edfeaf23fadc3f478864f56d81362`.
The previous 0.2.5.1 integration patch applies cleanly to 0.2.5.2; this patch is
regenerated against the new immutable source, including its added provider files.
Live size estimates, measured totals and comparison reporting remain from 0.2.5.2.
No public plugin CI or publishing configuration is changed.

Both architectures must pass the complete xGRIB tests and the bundle's clean
installation, provider registration and API smoke checks before publication.
