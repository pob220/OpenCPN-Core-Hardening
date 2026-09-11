# xGRIB 0.2.5.1 developer-bundle integration

The public xGRIB release and its CircleCI/Cloudsmith configuration are unchanged.
This bundle builds immutable xGRIB source
`b4fd23d75df24e6c8bb6c3ac2ac502dd8e15b637` (0.2.5.1) and applies the adjacent
`xgrib-0.2.5.1-provider.patch` with `git apply --check` first.

The patch preserves the external environment provider from the previous bundle's
`446814bafe6edfeaf23fadc3f478864f56d81362`. It was produced by merging the two
pinned source trees in a disposable checkout. Version/translation/documentation
conflicts retain the newer release; both environmental configuration and external
provider contract tests are retained. The resulting delta against 0.2.5.1 is
additive and contains no CI/publication changes. Generator remains 0.1.7 at
`6156c997191ffbfa9fba39385e84fa06a2fed353`.

Both architectures must pass the complete xGRIB tests and the bundle's clean
installation, provider registration and API smoke checks before publication.
Do not substitute the unpatched main-branch plugin: it lacks the bundle's
unattended environmental acquisition provider.
