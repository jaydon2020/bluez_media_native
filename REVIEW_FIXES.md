Review fixes for `lib/`, `native/`, and `hook/`, completed on 2026-09-08 against baseline `3f134e8`.

Each functional issue has its own commit. Follow-up races found while exercising the fixes also have separate commits. The implementation retains the layered C ABI / generated FFI / Dart proxy architecture and the synchronous C status codes.

| Issue | Resolution | Commit |
| --- | --- | --- |
| Native clients survive isolate-group exit; cached Variants can crash during process shutdown | Native finalization, managed cleanup queue, and explicit static destruction ordering | `9e828b2` |
| Removed proxies accept commands or late refresh replies | Disposed guards across all five proxy types | `57fa17d` |
| Raw registration races asynchronous registration | Lock the shared local-player registry | `47211a0` |
| Property changes can be missed during initial discovery | Install the service-wide property subscription before discovery | `d39d310` |
| Invalidated properties become fabricated defaults | Asynchronously fetch replacement properties | `3d4ee03` |
| Daemon restart leaves stale objects and registrations | Observe owner changes, invalidate old state, resynchronize, expose availability | `8b3ed7c` |
| Remote calls block event delivery; cover art blocks unrelated operations | Asynchronous D-Bus replies on worker queues and a separate cover-art queue | `f3d3b27` |
| RegisterPlayer sends the wrong initial property dictionary | Supply MPRIS properties | `7ae9d82` |
| Local registration advertises playback it cannot perform | Explicitly scope it as experimental and inert; return NotSupported | `1fa5746` |
| Cover-art timeout leaves partial files/transfers | RAII cleanup and bounded cancellation/session removal | `a895698` |
| Build hook can mislabel host binaries and miss vendor edits | Validate Linux/host architecture and track vendored build inputs | `282c5ef` |
| Native library resolution depends on checkout layout | Resolve through the declared Native Assets ID | `5df36ad` |
| Native implementation sources compile twice | Reuse one CMake object target | `e7b9bb3` |
| Unchecked test decoder ships in production headers | Move it into native test support | `773f8ce` |
| Acquired descriptors can leak before Dart takes ownership | Native ownership tokens, claim protocol, and finalizers | `33e352d` |
| close() completes before native cleanup | Await the native teardown completion; join creation workers | `3dd7f62` |
| Resync snapshots overwrite concurrent signals | Replay buffered updates after the snapshot | `6a1cc02` |
| Callers can mutate decoded snapshots | Return unmodifiable decoded collections | `62a07e3` |
| An unrelated property update permanently discards an invalidation refresh | Retry superseded refreshes | `d6fe6a9` |
| Client creation loses D-Bus error details | Preserve the original name, message, and path in the exception model | `595a526` |
| Lifecycle/concurrency behavior lacks native integration coverage | Add private-bus native/Dart tests and ASAN/UBSan CI coverage | `158856b` |
| Descriptor entry points can unwind through the C ABI | Contain C++ exceptions at those boundaries | `22ed79c` |
| New code fails formatting/static checks | Apply formatter and analyzer corrections | `d305cd5` |
| Packaging tests do not exercise compiled consumers | Build and relocate an independent CLI bundle in CI | `15bef60` |
| Acquired wrapper lifetime does not protect its descriptor owner | Mark the wrapper Finalizable and document ownership | `882315f` |
| An explicit Dart refresh overwrites a newer event | Check proxy revisions before applying replies | `f8a6185` |

Validation completed locally on Linux x64 with Dart 3.11.5, GCC 15, and Clang 21:

- `dart analyze --fatal-infos`: clean.
- `dart test`: 88 passed; the 14 private-bus tests are skipped here and run separately.
- `scripts/test_dbus.sh`: 14 Dart integration tests and both native CTest executables passed.
- ASAN + UBSan: both native executables passed against the private D-Bus fixture, including concurrent registration, dropped acquisition delivery, and static teardown.
- ThreadSanitizer: both native executables passed against the private fixture using a separate instrumented build.
- `scripts/test_asset.sh`: a separate consumer compiled, its complete bundle was relocated, and native loading succeeded outside the checkout.
- Dart formatting, native formatting, and clang-tidy: passed.
- `dart pub publish --dry-run`: zero warnings; nothing was published.

The fixture exercises real D-Bus traffic, the compiled C ABI, actual transferred file descriptors, and Dart isolates. It does not replace testing a physical adapter, real BlueZ/obexd versions, audio playback, or Flutter widget behavior. Sanitizer coverage is limited to the native scenarios described above.

Local player registration still has no Dart playback-command or metadata bridge. It is deliberately documented as an inert experimental facility rather than presenting simulated playback as working audio integration. Cross-compilation is rejected until a target toolchain/sysroot is supported.

Low-level consumers of asynchronous Acquire/TryAcquire must handle the new ownership-token response documented in `native/include/bluez_media_native.h`; the synchronous C acquisition payload is unchanged. Dart callers should retain the acquired owner while using its descriptor and explicitly close it. Decoded collections are now read-only, and `client.close()` waits for queued work and native cleanup. For standalone Dart executables on the tested SDK, use `dart build cli` and distribute the entire bundle: plain `dart compile exe` omitted the native asset.
