import 'package:code_assets/code_assets.dart';
import 'package:test/test.dart';
import '../hook/build.dart' as hook;

void main() {
  test('native Linux target is accepted', () {
    expect(
      () => hook.validateTarget(OS.linux, Architecture.current),
      returnsNormally,
    );
  }, skip: OS.current != OS.linux);
  test('unsupported OS is rejected before starting CMake', () {
    expect(
      () => hook.validateTarget(OS.windows, Architecture.current),
      throwsUnsupportedError,
    );
  });
  test('cross-architecture build is rejected rather than mislabeled', () {
    final other = Architecture.current == Architecture.x64
        ? Architecture.arm64
        : Architecture.x64;
    expect(() => hook.validateTarget(OS.linux, other), throwsUnsupportedError);
  });
}
