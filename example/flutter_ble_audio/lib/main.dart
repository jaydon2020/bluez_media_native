import 'dart:async';

import 'package:bluez_media_native/bluez_media_native.dart';
import 'package:flutter/material.dart';

void main() => runApp(const BluezMediaExample());

class BluezMediaExample extends StatelessWidget {
  const BluezMediaExample({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BlueZ Media',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xff1f7a6f),
          brightness: Brightness.light,
        ),
        useMaterial3: true,
        visualDensity: VisualDensity.compact,
      ),
      home: const MediaControlPage(),
    );
  }
}

class MediaControlPage extends StatefulWidget {
  const MediaControlPage({super.key});

  @override
  State<MediaControlPage> createState() => _MediaControlPageState();
}

class _MediaControlPageState extends State<MediaControlPage> {
  late final BluezMediaClient _client;
  final _subscriptions = <StreamSubscription<List<String>>>[];
  final _messages = <String>[];

  var _loading = true;
  String? _error;
  List<BluezMediaPlayer> _players = const [];
  List<BluezMediaControl> _controls = const [];
  BluezMediaPlayer? _selectedPlayer;
  BluezMediaControl? _selectedControl;

  @override
  void initState() {
    super.initState();
    _client = BluezMediaClient.create();
    unawaited(_loadObjects());
  }

  @override
  void dispose() {
    for (final subscription in _subscriptions) {
      subscription.cancel();
    }
    _client.close();
    super.dispose();
  }

  Future<void> _loadObjects() async {
    setState(() {
      _loading = true;
      _error = null;
    });

    try {
      await _client.ready.timeout(const Duration(seconds: 2), onTimeout: () {});
      final objects = _client.getManagedObjects();
      final players = objects.players.map(_client.player).toList();
      final controls = objects.controls.map(_client.control).toList();

      for (final subscription in _subscriptions) {
        await subscription.cancel();
      }
      _subscriptions
        ..clear()
        ..addAll([
          for (final player in players)
            player.propertiesChanged.listen((_) {
              if (mounted) setState(() {});
            }),
          for (final control in controls)
            control.propertiesChanged.listen((_) {
              if (mounted) setState(() {});
            }),
        ]);

      final selectedPlayer = _selectedPlayer == null
          ? players.firstOrNull
          : players
                .where(
                  (player) => player.objectPath == _selectedPlayer!.objectPath,
                )
                .firstOrNull;
      final selectedControl = _selectedControl == null
          ? controls.firstOrNull
          : controls
                .where(
                  (control) =>
                      control.objectPath == _selectedControl!.objectPath,
                )
                .firstOrNull;

      setState(() {
        _players = players;
        _controls = controls;
        _selectedPlayer = selectedPlayer ?? players.firstOrNull;
        _selectedControl = selectedControl ?? controls.firstOrNull;
        _loading = false;
      });

      await _refreshSelected();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _error = '$error';
        _loading = false;
      });
    }
  }

  Future<void> _refreshSelected() async {
    try {
      _selectedPlayer?.refresh();
      _selectedControl?.refresh();
      _pushMessage('Refreshed');
      if (mounted) setState(() {});
    } catch (error) {
      _pushMessage('$error');
    }
  }

  void _runCommand(String label, VoidCallback command) {
    try {
      command();
      _pushMessage(label);
    } catch (error) {
      _pushMessage('$error');
    }
  }

  void _pushMessage(String message) {
    if (!mounted) return;
    setState(() {
      _messages.insert(0, message);
      if (_messages.length > 5) {
        _messages.removeLast();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('BlueZ Media'),
        actions: [
          Tooltip(
            message: 'Refresh',
            child: IconButton(
              onPressed: _loading ? null : _loadObjects,
              icon: const Icon(Icons.refresh),
            ),
          ),
        ],
      ),
      body: _buildBody(),
    );
  }

  Widget _buildBody() {
    if (_loading) {
      return const Center(child: CircularProgressIndicator());
    }
    if (_error != null) {
      return _EmptyState(
        icon: Icons.error_outline,
        title: 'Unable to load BlueZ media',
        subtitle: _error!,
      );
    }
    if (_players.isEmpty && _controls.isEmpty) {
      return const _EmptyState(
        icon: Icons.bluetooth_disabled,
        title: 'No BlueZ media objects',
        subtitle: 'Pair and connect an AVRCP-capable audio device.',
      );
    }

    return LayoutBuilder(
      builder: (context, constraints) {
        final narrow = constraints.maxWidth < 760;
        final content = [
          _ObjectPicker(
            players: _players,
            controls: _controls,
            selectedPlayer: _selectedPlayer,
            selectedControl: _selectedControl,
            onPlayerChanged: (player) {
              setState(() => _selectedPlayer = player);
              unawaited(_refreshSelected());
            },
            onControlChanged: (control) {
              setState(() => _selectedControl = control);
              unawaited(_refreshSelected());
            },
          ),
          _NowPlayingPanel(
            player: _selectedPlayer,
            control: _selectedControl,
            messages: _messages,
            onRefresh: _refreshSelected,
            onPlayerCommand: _runCommand,
            onControlCommand: _runCommand,
          ),
        ];

        if (narrow) {
          return ListView(
            padding: const EdgeInsets.all(16),
            children: [content[0], const SizedBox(height: 16), content[1]],
          );
        }

        return Row(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            SizedBox(
              width: 360,
              child: SingleChildScrollView(
                padding: const EdgeInsets.all(16),
                child: content[0],
              ),
            ),
            const VerticalDivider(width: 1),
            Expanded(
              child: SingleChildScrollView(
                padding: const EdgeInsets.all(24),
                child: content[1],
              ),
            ),
          ],
        );
      },
    );
  }
}

class _ObjectPicker extends StatelessWidget {
  final List<BluezMediaPlayer> players;
  final List<BluezMediaControl> controls;
  final BluezMediaPlayer? selectedPlayer;
  final BluezMediaControl? selectedControl;
  final ValueChanged<BluezMediaPlayer?> onPlayerChanged;
  final ValueChanged<BluezMediaControl?> onControlChanged;

  const _ObjectPicker({
    required this.players,
    required this.controls,
    required this.selectedPlayer,
    required this.selectedControl,
    required this.onPlayerChanged,
    required this.onControlChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text('Objects', style: Theme.of(context).textTheme.titleLarge),
        const SizedBox(height: 12),
        _PathDropdown<BluezMediaPlayer>(
          label: 'MediaPlayer1',
          value: selectedPlayer,
          values: players,
          pathFor: (player) => player.objectPath,
          onChanged: onPlayerChanged,
        ),
        const SizedBox(height: 12),
        _PathDropdown<BluezMediaControl>(
          label: 'MediaControl1',
          value: selectedControl,
          values: controls,
          pathFor: (control) => control.objectPath,
          onChanged: onControlChanged,
        ),
      ],
    );
  }
}

class _PathDropdown<T> extends StatelessWidget {
  final String label;
  final T? value;
  final List<T> values;
  final String Function(T value) pathFor;
  final ValueChanged<T?> onChanged;

  const _PathDropdown({
    required this.label,
    required this.value,
    required this.values,
    required this.pathFor,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return DropdownButtonFormField<T>(
      initialValue: value,
      isExpanded: true,
      decoration: InputDecoration(
        labelText: label,
        border: const OutlineInputBorder(),
      ),
      items: [
        for (final item in values)
          DropdownMenuItem<T>(
            value: item,
            child: Text(
              pathFor(item),
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
          ),
      ],
      onChanged: values.isEmpty ? null : onChanged,
    );
  }
}

class _NowPlayingPanel extends StatelessWidget {
  final BluezMediaPlayer? player;
  final BluezMediaControl? control;
  final List<String> messages;
  final Future<void> Function() onRefresh;
  final void Function(String label, VoidCallback command) onPlayerCommand;
  final void Function(String label, VoidCallback command) onControlCommand;

  const _NowPlayingPanel({
    required this.player,
    required this.control,
    required this.messages,
    required this.onRefresh,
    required this.onPlayerCommand,
    required this.onControlCommand,
  });

  @override
  Widget build(BuildContext context) {
    final title = _trackValue(player, const ['Title', 'xesam:title']);
    final artist = _trackValue(player, const ['Artist', 'xesam:artist']);
    final album = _trackValue(player, const ['Album', 'xesam:album']);
    final duration = _trackValue(player, const ['Duration', 'mpris:length']);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    title.isEmpty ? 'No track title' : title,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.headlineMedium,
                  ),
                  const SizedBox(height: 4),
                  Text(
                    artist.isEmpty ? 'Unknown artist' : artist,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ],
              ),
            ),
            Tooltip(
              message: 'Refresh',
              child: IconButton.filledTonal(
                onPressed: onRefresh,
                icon: const Icon(Icons.refresh),
              ),
            ),
          ],
        ),
        const SizedBox(height: 24),
        Wrap(
          spacing: 12,
          runSpacing: 12,
          children: [
            _InfoChip(
              icon: Icons.play_circle_outline,
              label: 'Status',
              value: player?.status.isEmpty ?? true
                  ? 'unknown'
                  : player!.status,
            ),
            _InfoChip(
              icon: Icons.timer_outlined,
              label: 'Position',
              value: _formatMilliseconds(player?.position ?? 0),
            ),
            _InfoChip(
              icon: Icons.album_outlined,
              label: 'Album',
              value: album.isEmpty ? 'unknown' : album,
            ),
            _InfoChip(
              icon: Icons.hourglass_empty,
              label: 'Duration',
              value: duration.isEmpty ? 'unknown' : duration,
            ),
            _InfoChip(
              icon: control?.connected ?? false
                  ? Icons.bluetooth_connected
                  : Icons.bluetooth,
              label: 'Connected',
              value: control?.connected ?? false ? 'yes' : 'no',
            ),
          ],
        ),
        const SizedBox(height: 28),
        _TransportButtons(
          player: player,
          control: control,
          onPlayerCommand: onPlayerCommand,
          onControlCommand: onControlCommand,
        ),
        const SizedBox(height: 28),
        Text('Track Metadata', style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 8),
        _MetadataTable(properties: player?.track ?? const []),
        if (messages.isNotEmpty) ...[
          const SizedBox(height: 24),
          Text('Recent Events', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          for (final message in messages)
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              child: Text(
                message,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
              ),
            ),
        ],
      ],
    );
  }
}

class _TransportButtons extends StatelessWidget {
  final BluezMediaPlayer? player;
  final BluezMediaControl? control;
  final void Function(String label, VoidCallback command) onPlayerCommand;
  final void Function(String label, VoidCallback command) onControlCommand;

  const _TransportButtons({
    required this.player,
    required this.control,
    required this.onPlayerCommand,
    required this.onControlCommand,
  });

  @override
  Widget build(BuildContext context) {
    return Wrap(
      spacing: 10,
      runSpacing: 10,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        _CommandButton(
          tooltip: 'Previous',
          icon: Icons.skip_previous,
          onPressed: player == null
              ? null
              : () => onPlayerCommand('Previous', player!.previous),
        ),
        _CommandButton(
          tooltip: 'Play',
          icon: Icons.play_arrow,
          onPressed: player == null
              ? null
              : () => onPlayerCommand('Play', player!.play),
        ),
        _CommandButton(
          tooltip: 'Pause',
          icon: Icons.pause,
          onPressed: player == null
              ? null
              : () => onPlayerCommand('Pause', player!.pause),
        ),
        _CommandButton(
          tooltip: 'Stop',
          icon: Icons.stop,
          onPressed: player == null
              ? null
              : () => onPlayerCommand('Stop', player!.stop),
        ),
        _CommandButton(
          tooltip: 'Next',
          icon: Icons.skip_next,
          onPressed: player == null
              ? null
              : () => onPlayerCommand('Next', player!.next),
        ),
        const SizedBox(width: 8),
        _CommandButton(
          tooltip: 'Rewind',
          icon: Icons.fast_rewind,
          onPressed: control == null
              ? null
              : () => onControlCommand('Rewind', control!.rewind),
        ),
        _CommandButton(
          tooltip: 'Fast Forward',
          icon: Icons.fast_forward,
          onPressed: control == null
              ? null
              : () => onControlCommand('Fast forward', control!.fastForward),
        ),
        _CommandButton(
          tooltip: 'Volume Down',
          icon: Icons.volume_down,
          onPressed: control == null
              ? null
              : () => onControlCommand('Volume down', control!.volumeDown),
        ),
        _CommandButton(
          tooltip: 'Volume Up',
          icon: Icons.volume_up,
          onPressed: control == null
              ? null
              : () => onControlCommand('Volume up', control!.volumeUp),
        ),
      ],
    );
  }
}

class _CommandButton extends StatelessWidget {
  final String tooltip;
  final IconData icon;
  final VoidCallback? onPressed;

  const _CommandButton({
    required this.tooltip,
    required this.icon,
    required this.onPressed,
  });

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tooltip,
      child: SizedBox.square(
        dimension: 48,
        child: IconButton.filledTonal(onPressed: onPressed, icon: Icon(icon)),
      ),
    );
  }
}

class _InfoChip extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;

  const _InfoChip({
    required this.icon,
    required this.label,
    required this.value,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 180,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Row(
        children: [
          Icon(icon, size: 20),
          const SizedBox(width: 8),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(label, style: Theme.of(context).textTheme.labelMedium),
                Text(
                  value,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _MetadataTable extends StatelessWidget {
  final List<BlueZMediaProperty> properties;

  const _MetadataTable({required this.properties});

  @override
  Widget build(BuildContext context) {
    if (properties.isEmpty) {
      return const Text('No metadata');
    }

    return Table(
      columnWidths: const {0: FixedColumnWidth(150), 1: FlexColumnWidth()},
      defaultVerticalAlignment: TableCellVerticalAlignment.middle,
      children: [
        for (final property in properties)
          TableRow(
            children: [
              Padding(
                padding: const EdgeInsets.only(right: 12, bottom: 8),
                child: Text(
                  property.key,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.labelLarge,
                ),
              ),
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: Text(
                  property.value,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ],
          ),
      ],
    );
  }
}

class _EmptyState extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;

  const _EmptyState({
    required this.icon,
    required this.title,
    required this.subtitle,
  });

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 44),
            const SizedBox(height: 12),
            Text(title, style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 6),
            Text(
              subtitle,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ],
        ),
      ),
    );
  }
}

String _trackValue(BluezMediaPlayer? player, List<String> keys) {
  if (player == null) return '';
  for (final key in keys) {
    for (final property in player.track) {
      if (property.key == key) return property.value;
    }
  }
  return '';
}

String _formatMilliseconds(int milliseconds) {
  final duration = Duration(milliseconds: milliseconds);
  final minutes = duration.inMinutes.remainder(60).toString().padLeft(2, '0');
  final seconds = duration.inSeconds.remainder(60).toString().padLeft(2, '0');
  final hours = duration.inHours;
  if (hours > 0) {
    return '$hours:$minutes:$seconds';
  }
  return '$minutes:$seconds';
}
