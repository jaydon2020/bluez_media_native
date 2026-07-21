import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    if (Platform.environment.containsKey('SKIP_NATIVE_BUILD')) return;

    final packageRoot = input.packageRoot.toFilePath();
    final nativeRoot = '${packageRoot}native';
    final buildDirectory = input.outputDirectory.resolve('cmake/').toFilePath();
    await Directory(buildDirectory).create(recursive: true);

    if (!File('${buildDirectory}CMakeCache.txt').existsSync()) {
      await _run('cmake', [
        '-S',
        nativeRoot,
        '-B',
        buildDirectory,
        '-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=OFF',
        if (await _hasExecutable('ninja')) ...['-G', 'Ninja'],
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
      for (final entity in directory.listSync(recursive: true)) {
        if (entity is File &&
            const [
              '.c',
              '.cc',
              '.cpp',
              '.h',
              '.hpp',
            ].any(entity.path.endsWith)) {
          output.dependencies.add(entity.uri);
        }
      }
    }
    output.dependencies.add(Uri.file('$nativeRoot/CMakeLists.txt'));
  });
}

Future<void> _run(String executable, List<String> arguments) async {
  final process = await Process.start(
    executable,
    arguments,
    mode: ProcessStartMode.inheritStdio,
  );
  final exitCode = await process.exitCode;
  if (exitCode != 0) {
    throw ProcessException(
      executable,
      arguments,
      'exit code $exitCode',
      exitCode,
    );
  }
}

Future<bool> _hasExecutable(String executable) async =>
    (await Process.run('which', [executable])).exitCode == 0;
