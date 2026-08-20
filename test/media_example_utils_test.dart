// SPDX-License-Identifier: Apache-2.0

import 'package:test/test.dart';

import '../example/media_example_utils.dart';

void main() {
  group('optionValue', () {
    test('returns an option value', () {
      expect(
        optionValue(['--adapter', '/org/bluez/hci1'], '--adapter'),
        '/org/bluez/hci1',
      );
    });

    test('uses a fallback for a missing option', () {
      expect(
        optionValue(const [], '--adapter', fallback: 'default'),
        'default',
      );
    });

    test('rejects an option without a value', () {
      expect(
        () => optionValue(['--adapter'], '--adapter'),
        throwsA(isA<FormatException>()),
      );
    });
  });

  test('flags and list commands are parsed exactly', () {
    expect(hasFlag(['--verbose'], '--verbose'), isTrue);
    expect(isListCommand(['LIST']), isTrue);
    expect(isListCommand(['list', '--verbose']), isFalse);
  });
}
