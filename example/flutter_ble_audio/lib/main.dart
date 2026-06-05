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
          seedColor: const Color(0xff0f766e),
          brightness: Brightness.light,
        ),
        useMaterial3: true,
        visualDensity: VisualDensity.compact,
      ),
      home: const MediaProxyDashboard(),
    );
  }
}

class MediaProxyDashboard extends StatefulWidget {
  const MediaProxyDashboard({super.key});

  @override
  State<MediaProxyDashboard> createState() => _MediaProxyDashboardState();
}

class _MediaProxyDashboardState extends State<MediaProxyDashboard> {
  late final BluezMediaClient _client;
  final _subscriptions = <StreamSubscription<List<String>>>[];
  final _messages = <String>[];

  var _loading = true;
  String? _error;

  List<BluezMediaPlayer> _players = const [];
  List<BluezMediaControl> _controls = const [];
  List<BluezMediaTransport> _transports = const [];

  String? _selectedDevicePath;
  double? _transportVolumeDraft;

  List<_MediaDevice> get _devices =>
      _mediaDevices(_players, _controls, _transports);

  _MediaDevice? get _selectedDevice {
    final devices = _devices;
    if (devices.isEmpty) return null;
    final selectedPath = _selectedDevicePath;
    if (selectedPath == null) return devices.first;
    return devices
            .where((device) => device.devicePath == selectedPath)
            .firstOrNull ??
        devices.first;
  }

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
      final transports = objects.transports.map(_client.transport).toList();

      for (final subscription in _subscriptions) {
        await subscription.cancel();
      }
      _subscriptions
        ..clear()
        ..addAll([
          for (final player in players)
            player.propertiesChanged.listen((_) => _refreshView()),
          for (final control in controls)
            control.propertiesChanged.listen((_) => _refreshView()),
          for (final transport in transports)
            transport.propertiesChanged.listen((_) => _refreshView()),
        ]);

      setState(() {
        _players = players;
        _controls = controls;
        _transports = transports;
        _selectedDevicePath = _keepDeviceSelection(
          _devices,
          _selectedDevicePath,
        );
        _transportVolumeDraft = null;
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

  void _refreshView() {
    if (mounted) setState(() {});
  }

  Future<void> _refreshSelected() async {
    final device = _selectedDevice;
    try {
      device?.player?.refresh();
      device?.control?.refresh();
      for (final transport
          in device?.transports ?? const <BluezMediaTransport>[]) {
        transport.refresh();
      }
      _pushMessage('Refreshed selected device');
      _refreshView();
    } catch (error) {
      _pushMessage('$error');
    }
  }

  void _runCommand(String label, VoidCallback command) {
    try {
      command();
      _pushMessage(label);
      _refreshView();
    } catch (error) {
      _pushMessage('$error');
    }
  }

  void _setRepeatMode(String repeat) {
    final player = _selectedDevice?.player;
    if (player == null) {
      _pushMessage('Selected device has no MediaPlayer1 support');
      return;
    }
    _runCommand('MediaPlayer1 Repeat: $repeat', () {
      player.setRepeat(repeat);
      player.refresh();
    });
  }

  void _setShuffleMode(String shuffle) {
    final player = _selectedDevice?.player;
    if (player == null) {
      _pushMessage('Selected device has no MediaPlayer1 support');
      return;
    }
    _runCommand('MediaPlayer1 Shuffle: $shuffle', () {
      player.setShuffle(shuffle);
      player.refresh();
    });
  }

  void _setTransportVolume(double value) {
    final transport = _selectedDevice?.transport;
    if (transport == null) return;
    _runCommand('MediaTransport1 Volume: ${value.round()}', () {
      transport.volume = value.round().clamp(0, 127);
    });
    setState(() => _transportVolumeDraft = null);
  }

  void _previewTransportVolume(double value) {
    setState(() => _transportVolumeDraft = value);
  }

  void _pushMessage(String message) {
    if (!mounted) return;
    setState(() {
      _messages.insert(0, message);
      if (_messages.length > 6) {
        _messages.removeLast();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('BlueZ Media Devices'),
        actions: [
          Tooltip(
            message: 'Refresh objects',
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
    if (_players.isEmpty && _controls.isEmpty && _transports.isEmpty) {
      return const _EmptyState(
        icon: Icons.bluetooth_disabled,
        title: 'No BlueZ media devices',
        subtitle: 'Pair and connect an AVRCP-capable audio device.',
      );
    }

    return LayoutBuilder(
      builder: (context, constraints) {
        final selectedDevice = _selectedDevice;
        final maxWidth = constraints.maxWidth >= 1040
            ? 1000.0
            : double.infinity;
        final dashboard = _ProxyPanels(
          device: selectedDevice,
          messages: _messages,
          onRefresh: _refreshSelected,
          onRepeatChanged: _setRepeatMode,
          onShuffleChanged: _setShuffleMode,
          transportVolumeDraft: _transportVolumeDraft,
          onTransportVolumePreview: _previewTransportVolume,
          onTransportVolumeChanged: _setTransportVolume,
          onCommand: _runCommand,
        );

        return ListView(
          padding: const EdgeInsets.all(16),
          children: [
            Center(
              child: ConstrainedBox(
                constraints: BoxConstraints(maxWidth: maxWidth),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    _DevicePicker(
                      devices: _devices,
                      value: selectedDevice,
                      onChanged: (device) {
                        setState(() {
                          _selectedDevicePath = device?.devicePath;
                          _transportVolumeDraft = null;
                        });
                        unawaited(_refreshSelected());
                      },
                    ),
                    const SizedBox(height: 16),
                    dashboard,
                  ],
                ),
              ),
            ),
          ],
        );
      },
    );
  }
}

class _DevicePicker extends StatelessWidget {
  final List<_MediaDevice> devices;
  final _MediaDevice? value;
  final ValueChanged<_MediaDevice?> onChanged;

  const _DevicePicker({
    required this.devices,
    required this.value,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return _SectionPanel(
      icon: Icons.bluetooth_connected,
      title: 'Device',
      trailing: Text('${devices.length}'),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          DropdownButtonFormField<_MediaDevice>(
            initialValue: value,
            isExpanded: true,
            decoration: const InputDecoration(
              border: OutlineInputBorder(),
              contentPadding: EdgeInsets.symmetric(
                horizontal: 12,
                vertical: 10,
              ),
            ),
            hint: const Text('No device'),
            items: [
              for (final device in devices)
                DropdownMenuItem<_MediaDevice>(
                  value: device,
                  child: Row(
                    children: [
                      Icon(
                        device.connected
                            ? Icons.bluetooth_connected
                            : Icons.bluetooth_disabled,
                        size: 18,
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          '${_deviceLabel(device)} - ${_connectionLabel(device)}',
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                      ),
                    ],
                  ),
                ),
            ],
            onChanged: devices.isEmpty ? null : onChanged,
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _SupportChip(
                icon: value?.connected ?? false
                    ? Icons.bluetooth_connected
                    : Icons.bluetooth_disabled,
                label: _connectionLabel(value),
                supported: value?.connected ?? false,
              ),
              _SupportChip(
                icon: Icons.queue_music,
                label: 'MediaPlayer1',
                supported: value?.player != null,
              ),
              _SupportChip(
                icon: Icons.settings_remote,
                label: 'MediaControl1',
                supported: value?.control != null,
              ),
              _SupportChip(
                icon: Icons.graphic_eq,
                label: 'MediaTransport1',
                supported: value?.transports.isNotEmpty ?? false,
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            value?.devicePath ?? 'No device selected',
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }
}

class _SupportChip extends StatelessWidget {
  final IconData icon;
  final String label;
  final bool supported;

  const _SupportChip({
    required this.icon,
    required this.label,
    required this.supported,
  });

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: supported ? colors.secondaryContainer : colors.surface,
        border: Border.all(color: colors.outlineVariant),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 18),
            const SizedBox(width: 8),
            Text(label),
            const SizedBox(width: 8),
            Icon(
              supported ? Icons.check_circle : Icons.cancel_outlined,
              size: 18,
              color: supported ? colors.primary : colors.outline,
            ),
          ],
        ),
      ),
    );
  }
}

class _ProxyPanels extends StatelessWidget {
  final _MediaDevice? device;
  final List<String> messages;
  final Future<void> Function() onRefresh;
  final ValueChanged<String> onRepeatChanged;
  final ValueChanged<String> onShuffleChanged;
  final double? transportVolumeDraft;
  final ValueChanged<double> onTransportVolumePreview;
  final ValueChanged<double> onTransportVolumeChanged;
  final void Function(String label, VoidCallback command) onCommand;

  const _ProxyPanels({
    required this.device,
    required this.messages,
    required this.onRefresh,
    required this.onRepeatChanged,
    required this.onShuffleChanged,
    required this.transportVolumeDraft,
    required this.onTransportVolumePreview,
    required this.onTransportVolumeChanged,
    required this.onCommand,
  });

  @override
  Widget build(BuildContext context) {
    final player = device?.player;
    final control = device?.control;
    final transport = device?.transport;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                'Device controls',
                style: Theme.of(context).textTheme.headlineSmall,
              ),
            ),
            Tooltip(
              message: 'Refresh selected device',
              child: IconButton.filledTonal(
                onPressed: device == null ? null : onRefresh,
                icon: const Icon(Icons.refresh),
              ),
            ),
          ],
        ),
        const SizedBox(height: 16),
        _PlayerProxyPanel(
          player: player,
          onRepeatChanged: onRepeatChanged,
          onShuffleChanged: onShuffleChanged,
          onCommand: onCommand,
        ),
        const SizedBox(height: 16),
        _ControlProxyPanel(control: control, onCommand: onCommand),
        const SizedBox(height: 16),
        _TransportProxyPanel(
          transport: transport,
          transportCount: device?.transports.length ?? 0,
          volumeDraft: transportVolumeDraft,
          onVolumePreview: onTransportVolumePreview,
          onVolumeChanged: onTransportVolumeChanged,
          onCommand: onCommand,
        ),
        if (messages.isNotEmpty) ...[
          const SizedBox(height: 16),
          _EventPanel(messages: messages),
        ],
      ],
    );
  }
}

class _PlayerProxyPanel extends StatelessWidget {
  final BluezMediaPlayer? player;
  final ValueChanged<String> onRepeatChanged;
  final ValueChanged<String> onShuffleChanged;
  final void Function(String label, VoidCallback command) onCommand;

  const _PlayerProxyPanel({
    required this.player,
    required this.onRepeatChanged,
    required this.onShuffleChanged,
    required this.onCommand,
  });

  @override
  Widget build(BuildContext context) {
    final title = _trackValue(player, const ['Title', 'xesam:title']);
    final artist = _trackValue(player, const ['Artist', 'xesam:artist']);
    final album = _trackValue(player, const ['Album', 'xesam:album']);

    return _SectionPanel(
      icon: Icons.queue_music,
      title: 'MediaPlayer1',
      subtitle: player?.objectPath ?? 'No player support for selected device',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            title.isEmpty ? 'No track title' : title,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: Theme.of(context).textTheme.titleLarge,
          ),
          const SizedBox(height: 4),
          Text(
            artist.isEmpty ? 'Unknown artist' : artist,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: Theme.of(context).textTheme.bodyLarge,
          ),
          const SizedBox(height: 14),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _MetricTile(
                icon: Icons.play_circle_outline,
                label: 'Status',
                value: _display(player?.status),
              ),
              _MetricTile(
                icon: Icons.timer_outlined,
                label: 'Position',
                value: _formatMilliseconds(player?.position ?? 0),
              ),
              _MetricTile(
                icon: Icons.album_outlined,
                label: 'Album',
                value: _display(album),
              ),
              _MetricTile(
                icon: Icons.devices,
                label: 'Device',
                value: _display(player?.device),
              ),
              _MetricTile(
                icon: Icons.folder_open,
                label: 'Browsable',
                value: player?.browsable ?? false ? 'yes' : 'no',
              ),
              _MetricTile(
                icon: Icons.search,
                label: 'Searchable',
                value: player?.searchable ?? false ? 'yes' : 'no',
              ),
            ],
          ),
          const SizedBox(height: 16),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            crossAxisAlignment: WrapCrossAlignment.center,
            children: [
              _CommandButton(
                tooltip: 'Previous',
                icon: Icons.skip_previous,
                onPressed: player == null
                    ? null
                    : () =>
                          onCommand('MediaPlayer1 Previous', player!.previous),
              ),
              _CommandButton(
                tooltip: 'Play',
                icon: Icons.play_arrow,
                onPressed: player == null
                    ? null
                    : () => onCommand('MediaPlayer1 Play', player!.play),
              ),
              _CommandButton(
                tooltip: 'Pause',
                icon: Icons.pause,
                onPressed: player == null
                    ? null
                    : () => onCommand('MediaPlayer1 Pause', player!.pause),
              ),
              _CommandButton(
                tooltip: 'Stop',
                icon: Icons.stop,
                onPressed: player == null
                    ? null
                    : () => onCommand('MediaPlayer1 Stop', player!.stop),
              ),
              _CommandButton(
                tooltip: 'Next',
                icon: Icons.skip_next,
                onPressed: player == null
                    ? null
                    : () => onCommand('MediaPlayer1 Next', player!.next),
              ),
              _RepeatModeControl(
                repeat: player?.repeat ?? 'off',
                onChanged: player == null ? null : onRepeatChanged,
              ),
              _ShuffleModeControl(
                shuffle: player?.shuffle ?? 'off',
                onChanged: player == null ? null : onShuffleChanged,
              ),
            ],
          ),
          const SizedBox(height: 16),
          Text(
            'Track metadata',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 8),
          _MetadataTable(properties: player?.track ?? const []),
        ],
      ),
    );
  }
}

class _ControlProxyPanel extends StatelessWidget {
  final BluezMediaControl? control;
  final void Function(String label, VoidCallback command) onCommand;

  const _ControlProxyPanel({required this.control, required this.onCommand});

  @override
  Widget build(BuildContext context) {
    return _SectionPanel(
      icon: Icons.settings_remote,
      title: 'MediaControl1',
      subtitle:
          control?.objectPath ?? 'No controller support for selected device',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _MetricTile(
                icon: control?.connected ?? false
                    ? Icons.bluetooth_connected
                    : Icons.bluetooth,
                label: 'Connected',
                value: control?.connected ?? false ? 'yes' : 'no',
              ),
              _MetricTile(
                icon: Icons.queue_music,
                label: 'Active player',
                value: _display(control?.playerPath),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _CommandButton(
                tooltip: 'Previous',
                icon: Icons.skip_previous,
                onPressed: control == null
                    ? null
                    : () => onCommand(
                        'MediaControl1 Previous',
                        control!.previous,
                      ),
              ),
              _CommandButton(
                tooltip: 'Play',
                icon: Icons.play_arrow,
                onPressed: control == null
                    ? null
                    : () => onCommand('MediaControl1 Play', control!.play),
              ),
              _CommandButton(
                tooltip: 'Pause',
                icon: Icons.pause,
                onPressed: control == null
                    ? null
                    : () => onCommand('MediaControl1 Pause', control!.pause),
              ),
              _CommandButton(
                tooltip: 'Stop',
                icon: Icons.stop,
                onPressed: control == null
                    ? null
                    : () => onCommand('MediaControl1 Stop', control!.stop),
              ),
              _CommandButton(
                tooltip: 'Next',
                icon: Icons.skip_next,
                onPressed: control == null
                    ? null
                    : () => onCommand('MediaControl1 Next', control!.next),
              ),
              _CommandButton(
                tooltip: 'Rewind',
                icon: Icons.fast_rewind,
                onPressed: control == null
                    ? null
                    : () => onCommand('MediaControl1 Rewind', control!.rewind),
              ),
              _CommandButton(
                tooltip: 'Fast Forward',
                icon: Icons.fast_forward,
                onPressed: control == null
                    ? null
                    : () => onCommand(
                        'MediaControl1 Fast forward',
                        control!.fastForward,
                      ),
              ),
              _CommandButton(
                tooltip: 'Volume Down',
                icon: Icons.volume_down,
                onPressed: control == null
                    ? null
                    : () => onCommand(
                        'MediaControl1 Volume down',
                        control!.volumeDown,
                      ),
              ),
              _CommandButton(
                tooltip: 'Volume Up',
                icon: Icons.volume_up,
                onPressed: control == null
                    ? null
                    : () => onCommand(
                        'MediaControl1 Volume up',
                        control!.volumeUp,
                      ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _TransportProxyPanel extends StatelessWidget {
  final BluezMediaTransport? transport;
  final int transportCount;
  final double? volumeDraft;
  final ValueChanged<double> onVolumePreview;
  final ValueChanged<double> onVolumeChanged;
  final void Function(String label, VoidCallback command) onCommand;

  const _TransportProxyPanel({
    required this.transport,
    required this.transportCount,
    required this.volumeDraft,
    required this.onVolumePreview,
    required this.onVolumeChanged,
    required this.onCommand,
  });

  @override
  Widget build(BuildContext context) {
    final volume = (volumeDraft ?? transport?.volume ?? 0)
        .clamp(0, 127)
        .toDouble();

    return _SectionPanel(
      icon: Icons.graphic_eq,
      title: 'MediaTransport1',
      subtitle: transport == null
          ? 'No transport support for selected device'
          : '$transportCount transport${transportCount == 1 ? '' : 's'} available',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _MetricTile(
                icon: Icons.radio_button_checked,
                label: 'State',
                value: _display(transport?.state),
              ),
              _MetricTile(
                icon: Icons.memory,
                label: 'Codec',
                value: '${transport?.codec ?? 0}',
              ),
              _MetricTile(
                icon: Icons.timer_outlined,
                label: 'Delay',
                value: '${transport?.delay ?? 0}',
              ),
              _MetricTile(
                icon: Icons.devices,
                label: 'Device',
                value: _display(transport?.device),
              ),
              _MetricTile(
                icon: Icons.link,
                label: 'UUID',
                value: _display(transport?.uuid),
              ),
              _MetricTile(
                icon: Icons.output,
                label: 'Endpoint',
                value: _display(transport?.endpoint),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              const Icon(Icons.volume_up),
              const SizedBox(width: 10),
              Expanded(
                child: Slider(
                  value: volume,
                  min: 0,
                  max: 127,
                  divisions: 127,
                  label: '${volume.round()}',
                  onChanged: transport == null ? null : onVolumePreview,
                  onChangeEnd: transport == null ? null : onVolumeChanged,
                ),
              ),
              SizedBox(
                width: 44,
                child: Text(
                  '${volume.round()}',
                  textAlign: TextAlign.end,
                  style: Theme.of(context).textTheme.labelLarge,
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _CommandButton(
                tooltip: 'Refresh transport',
                icon: Icons.sync,
                onPressed: transport == null
                    ? null
                    : () => onCommand(
                        'MediaTransport1 Refresh',
                        transport!.refresh,
                      ),
              ),
              _CommandButton(
                tooltip: 'Try acquire and close',
                icon: Icons.output,
                onPressed: transport == null
                    ? null
                    : () => onCommand('MediaTransport1 TryAcquire', () {
                        final acquired = transport!.tryAcquire();
                        acquired.close();
                      }),
              ),
              _CommandButton(
                tooltip: 'Release transport',
                icon: Icons.close,
                onPressed: transport == null
                    ? null
                    : () => onCommand(
                        'MediaTransport1 Release',
                        transport!.release,
                      ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _EventPanel extends StatelessWidget {
  final List<String> messages;

  const _EventPanel({required this.messages});

  @override
  Widget build(BuildContext context) {
    return _SectionPanel(
      icon: Icons.history,
      title: 'Events',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
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
      ),
    );
  }
}

class _SectionPanel extends StatelessWidget {
  final IconData icon;
  final String title;
  final String? subtitle;
  final Widget? trailing;
  final Widget child;

  const _SectionPanel({
    required this.icon,
    required this.title,
    this.subtitle,
    this.trailing,
    required this.child,
  });

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: colors.surface,
        border: Border.all(color: colors.outlineVariant),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Icon(icon, size: 22),
              const SizedBox(width: 10),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title, style: Theme.of(context).textTheme.titleMedium),
                    if (subtitle != null) ...[
                      const SizedBox(height: 2),
                      Text(
                        subtitle!,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ],
                ),
              ),
              ?trailing,
            ],
          ),
          const SizedBox(height: 14),
          child,
        ],
      ),
    );
  }
}

class _MetricTile extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;

  const _MetricTile({
    required this.icon,
    required this.label,
    required this.value,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 188,
      child: DecoratedBox(
        decoration: BoxDecoration(
          border: Border.all(
            color: Theme.of(context).colorScheme.outlineVariant,
          ),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          child: Row(
            children: [
              Icon(icon, size: 20),
              const SizedBox(width: 8),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(label, style: Theme.of(context).textTheme.labelMedium),
                    Text(value, maxLines: 1, overflow: TextOverflow.ellipsis),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _RepeatModeControl extends StatelessWidget {
  final String repeat;
  final ValueChanged<String>? onChanged;

  const _RepeatModeControl({required this.repeat, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    final enabled = repeat == 'singletrack';

    return _ToggleCommandButton(
      tooltip: enabled ? 'Repeat off' : 'Repeat single track',
      icon: enabled ? Icons.repeat_one : Icons.repeat,
      selected: enabled,
      onPressed: onChanged == null
          ? null
          : () => onChanged!(enabled ? 'off' : 'singletrack'),
    );
  }
}

class _ShuffleModeControl extends StatelessWidget {
  final String shuffle;
  final ValueChanged<String>? onChanged;

  const _ShuffleModeControl({required this.shuffle, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    final enabled = shuffle == 'alltracks';

    return _ToggleCommandButton(
      tooltip: enabled ? 'Shuffle off' : 'Shuffle all',
      icon: Icons.shuffle,
      selected: enabled,
      onPressed: onChanged == null
          ? null
          : () => onChanged!(enabled ? 'off' : 'alltracks'),
    );
  }
}

class _ToggleCommandButton extends StatelessWidget {
  final String tooltip;
  final IconData icon;
  final bool selected;
  final VoidCallback? onPressed;

  const _ToggleCommandButton({
    required this.tooltip,
    required this.icon,
    required this.selected,
    required this.onPressed,
  });

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Tooltip(
      message: tooltip,
      child: SizedBox.square(
        dimension: 48,
        child: IconButton.filledTonal(
          onPressed: onPressed,
          icon: Icon(icon, color: selected ? colors.primary : null),
        ),
      ),
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

class _MediaDevice {
  final String devicePath;
  final BluezMediaPlayer? player;
  final BluezMediaControl? control;
  final List<BluezMediaTransport> transports;

  const _MediaDevice({
    required this.devicePath,
    required this.player,
    required this.control,
    required this.transports,
  });

  BluezMediaTransport? get transport => transports.firstOrNull;
  bool get connected => control?.connected ?? false;

  @override
  bool operator ==(Object other) =>
      other is _MediaDevice && other.devicePath == devicePath;

  @override
  int get hashCode => devicePath.hashCode;
}

List<_MediaDevice> _mediaDevices(
  List<BluezMediaPlayer> players,
  List<BluezMediaControl> controls,
  List<BluezMediaTransport> transports,
) {
  final devicePaths = <String>{};
  for (final player in players) {
    devicePaths.add(_devicePathForPlayer(player));
  }
  for (final control in controls) {
    devicePaths.add(control.objectPath);
  }
  for (final transport in transports) {
    devicePaths.add(_devicePathForTransport(transport));
  }

  return [
    for (final devicePath in devicePaths.where((path) => path.isNotEmpty))
      _MediaDevice(
        devicePath: devicePath,
        player: players
            .where((player) => _devicePathForPlayer(player) == devicePath)
            .firstOrNull,
        control: controls
            .where((control) => control.objectPath == devicePath)
            .firstOrNull,
        transports: transports
            .where(
              (transport) => _devicePathForTransport(transport) == devicePath,
            )
            .toList(),
      ),
  ]..sort((left, right) => left.devicePath.compareTo(right.devicePath));
}

String? _keepDeviceSelection(List<_MediaDevice> devices, String? selectedPath) {
  if (devices.isEmpty) return null;
  if (selectedPath == null) return devices.first.devicePath;
  return devices
          .where((device) => device.devicePath == selectedPath)
          .firstOrNull
          ?.devicePath ??
      devices.first.devicePath;
}

String _devicePathForPlayer(BluezMediaPlayer player) {
  if (player.device.isNotEmpty) return player.device;
  return _devicePathFromObjectPath(player.objectPath);
}

String _devicePathForTransport(BluezMediaTransport transport) {
  if (transport.device.isNotEmpty) return transport.device;
  return _devicePathFromObjectPath(transport.objectPath);
}

String _devicePathFromObjectPath(String objectPath) {
  final marker = objectPath.indexOf('/dev_');
  if (marker < 0) return objectPath;
  final nextSlash = objectPath.indexOf('/', marker + 1);
  if (nextSlash < 0) return objectPath;
  return objectPath.substring(0, nextSlash);
}

String _deviceLabel(_MediaDevice device) {
  return device.devicePath.split('/').last.replaceAll('_', ':');
}

String _connectionLabel(_MediaDevice? device) {
  if (device == null) return 'Not connected';
  if (device.control == null) return 'Connection unknown';
  return device.connected ? 'Connected' : 'Not connected';
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

String _display(String? value) {
  if (value == null || value.isEmpty) return 'unknown';
  return value;
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
