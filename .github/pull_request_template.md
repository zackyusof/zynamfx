## Summary

<!-- What changes for the user, and why. Keep it small: no speculative fallbacks, no dead code. -->

## Checks

- [ ] The app builds and the changed screen or chain path was exercised locally
- [ ] `./script/validate-plugin.sh` passes if the processor, editor or plug-in wrappers changed
- [ ] State format is unchanged, or the schema version is bumped and old state is handled
- [ ] AU parameter hints, the LV2 URI and the CLAP ID are unchanged
- [ ] README updated if public behaviour or setup changed

## Compatibility

These are contracts from the first public release; leave unchecked only if this PR does not touch that surface.
