# xGRIB 0.3.0.0 developer-bundle integration

The bundle pins public xGRIB `c0ca9d10640562b560be97727b897766889a6b55` (0.3.0.0),
with Generator 0.3.0 at `0cca0871c04f273579b4c69d6cb8fe26e8596677`.
It applies `xgrib-0.3.0.0-provider.patch` after checking it with `git apply --check`.

The patch preserves the previous bundle's external environmental acquisition
provider, originally from `446814bafe6edfeaf23fadc3f478864f56d81362`.
The provider patch applies cleanly to the pinned 0.3.0.0 source. It adds the
external-control provider only to this isolated developer bundle; the ordinary
published xGRIB package is unchanged. The 0.3 generator and size estimates are
retained.
No public plugin CI or publishing configuration is changed.

Both architectures must pass the complete xGRIB tests and the bundle's clean
installation, provider registration and API smoke checks before publication.
