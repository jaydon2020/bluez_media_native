import 'package:code_assets/code_assets.dart';
import 'package:test/test.dart';
import '../hook/build.dart' as hook;

void main() {
  test('Linux host and target are accepted', () {
    expect(
      () => hook.validateOperatingSystems(OS.linux, OS.linux),
      returnsNormally,
    );
  });
  test('unsupported host OS is rejected before starting CMake', () {
    expect(
      () => hook.validateOperatingSystems(OS.windows, OS.linux),
      throwsUnsupportedError,
    );
  });
  test('unsupported target OS is rejected before starting CMake', () {
    expect(
      () => hook.validateOperatingSystems(OS.linux, OS.windows),
      throwsUnsupportedError,
    );
  });
}
