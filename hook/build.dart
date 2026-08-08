// Native assets build hook for bluez_media_native.
//
// Builds libbluez_media_native.so with CMake and exposes it as a bundled
// CodeAsset. Patterned after jwinarske/bluez_native.

import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;

    if (Platform.environment.containsKey('SKIP_NATIVE_BUILD')) {
      stderr.writeln('SKIP_NATIVE_BUILD set — skipping native build.');
      return;
    }

    final packageRoot = input.packageRoot.toFilePath();
    final nativeRoot = '${packageRoot}native';
    final buildDirectory = input.outputDirectory.resolve('cmake/').toFilePath();

    await Directory(buildDirectory).create(recursive: true);

    // Git dependencies do not automatically populate submodules.
    final sdbus = File('$nativeRoot/third_party/sdbus-cpp/CMakeLists.txt');
    if (!sdbus.existsSync()) {
      if (!Directory('$packageRoot.git').existsSync() &&
          !File('$packageRoot.git').existsSync()) {
        throw StateError(
          'native/third_party/sdbus-cpp is missing and $packageRoot has no '
          '.git to restore it from. Clone with --recurse-submodules.',
        );
      }
      stderr.writeln('sdbus-cpp submodule missing; initializing');
      await _run('git', [
        '-C',
        packageRoot,
        'submodule',
        'update',
        '--init',
        '--recursive',
      ]);
    }

    final hasNinja = await _which('ninja');

    if (!File('${buildDirectory}CMakeCache.txt').existsSync()) {
      await _run('cmake', [
        '-S',
        nativeRoot,
        '-B',
        buildDirectory,
        '-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=OFF',
        if (hasNinja) ...['-G', 'Ninja'],
      ]);
    }

    await _run('cmake', ['--build', buildDirectory, '--parallel']);

    final library = File('${buildDirectory}libbluez_media_native.so');
    if (!library.existsSync()) {
      throw StateError('Native library not found at ${library.path}');
    }
    output.assets.code.add(
      CodeAsset(
        package: input.packageName,
        name: 'src/ffi/bluez_media_native_asset.dart',
        linkMode: DynamicLoadingBundled(),
        file: library.uri,
      ),
    );

    for (final directoryName in ['src', 'include']) {
      final directory = Directory('$nativeRoot/$directoryName');
      if (!directory.existsSync()) continue;
      for (final entity in directory.listSync(recursive: true)) {
        if (entity is! File) continue;
        final path = entity.path;
        if (path.endsWith('.c') ||
            path.endsWith('.cc') ||
            path.endsWith('.cpp') ||
            path.endsWith('.h') ||
            path.endsWith('.hpp')) {
          output.dependencies.add(entity.uri);
        }
      }
    }
    output.dependencies.add(Uri.file('$nativeRoot/CMakeLists.txt'));

    stderr.writeln('libbluez_media_native built: ${library.path}');
  });
}

Future<void> _run(String executable, List<String> arguments) async {
  final process = await Process.start(
    executable,
    arguments,
    mode: ProcessStartMode.inheritStdio,
  );
  final code = await process.exitCode;
  if (code != 0) {
    throw ProcessException(executable, arguments, 'exit code $code', code);
  }
}

Future<bool> _which(String executable) async {
  final result = await Process.run('which', [executable]);
  return result.exitCode == 0;
}
