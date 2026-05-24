import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:dynamic_color/dynamic_color.dart';
import 'services/ble_service.dart';
import 'services/app_settings.dart';
import 'utils/vib.dart';
import 'pages/sensor_page.dart';
import 'pages/tune_page.dart';
import 'pages/motion_page.dart' as motion;
import 'pages/log_page.dart';
import 'pages/settings_page.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => BleService()),
        ChangeNotifierProvider(create: (_) => AppSettings()..load()),
      ],
      child: const LineWalkerApp(),
    ),
  );
}

class LineWalkerApp extends StatelessWidget {
  const LineWalkerApp({super.key});
  @override
  Widget build(BuildContext context) {
    final s = context.watch<AppSettings>();
    return DynamicColorBuilder(
      builder: (l, d) {
        final ls =
            (s.useWallpaperColor && l != null)
                ? l.harmonized()
                : ColorScheme.fromSeed(
                  seedColor: s.seedColor,
                  brightness: Brightness.light,
                );
        final ds =
            (s.useWallpaperColor && d != null)
                ? d.harmonized()
                : ColorScheme.fromSeed(
                  seedColor: s.seedColor,
                  brightness: Brightness.dark,
                );
        return MaterialApp(
          title: 'LW v5.0',
          debugShowCheckedModeBanner: false,
          theme: ThemeData(useMaterial3: true, colorScheme: ls),
          darkTheme: ThemeData(useMaterial3: true, colorScheme: ds),
          themeMode: s.themeMode,
          home: const HomePage(),
        );
      },
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  int _tab = 0;

  static const _nav = [
    NavigationDestination(
      icon: Icon(Icons.sensors_outlined),
      selectedIcon: Icon(Icons.sensors),
      label: '传感器',
    ),
    NavigationDestination(
      icon: Icon(Icons.tune_outlined),
      selectedIcon: Icon(Icons.tune),
      label: '调试',
    ),
    NavigationDestination(
      icon: Icon(Icons.sports_esports_outlined),
      selectedIcon: Icon(Icons.sports_esports),
      label: '控制',
    ),
    NavigationDestination(
      icon: Icon(Icons.article_outlined),
      selectedIcon: Icon(Icons.article),
      label: '日志',
    ),
  ];
  static const _rail = [
    NavigationRailDestination(
      icon: Icon(Icons.sensors_outlined),
      selectedIcon: Icon(Icons.sensors),
      label: Text('传感器'),
    ),
    NavigationRailDestination(
      icon: Icon(Icons.tune_outlined),
      selectedIcon: Icon(Icons.tune),
      label: Text('调试'),
    ),
    NavigationRailDestination(
      icon: Icon(Icons.sports_esports_outlined),
      selectedIcon: Icon(Icons.sports_esports),
      label: Text('控制'),
    ),
    NavigationRailDestination(
      icon: Icon(Icons.article_outlined),
      selectedIcon: Icon(Icons.article),
      label: Text('日志'),
    ),
  ];
  static final _pages = const [
    SensorPage(),
    TunePage(),
    motion.ControlPage(),
    LogPage(),
  ];

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _autoConnect());
  }

  Future<void> _autoConnect() async {
    final ble = context.read<BleService>();
    if (ble.connected) return;
    await ble.scan();
    if (!ble.connected && ble.deviceList.isNotEmpty) {
      final t = ble.deviceList.firstWhere(
        (d) =>
            d.contains('HC-05') ||
            d.contains('HC05') ||
            d.toUpperCase().contains('20:26:01:00:21:4F'),
        orElse: () => ble.deviceList.first,
      );
      ble.connect(t);
    }
  }

  void _sw(int i) {
    Vib.light();
    if (i == _tab) return;
    setState(() => _tab = i);
  }

  String _fmt(int c) {
    final m = c ~/ 6000, s = (c % 6000) ~/ 100, cs = c % 100;
    return '${m.toString().padLeft(2, '0')}:${s.toString().padLeft(2, '0')}.${cs.toString().padLeft(2, '0')}';
  }

  void _startMotion() {
    final ble = context.read<BleService>();
    ble.lwRunning = true;
    if (!ble.timerDisabled) ble.startTimer();
    ble.sendRaw('G');
  }

  void _stopMotion() {
    final ble = context.read<BleService>();
    ble.lwRunning = false;
    ble.stopTimer();
    ble.sendRaw('B');
  }

  void _tp() {
    Vib.heavy();
    if (context.read<BleService>().timerRunning)
      _stopMotion();
    else
      _startMotion();
  }

  void _tl() {
    Vib.heavy();
    final ble = context.read<BleService>();
    ble.resetTimer();
    ble.sendRaw('B');
    ble.lwRunning = false;
  }

  void _es() {
    Vib.heavy();
    if (context.read<BleService>().lwRunning)
      _stopMotion();
    else
      _startMotion();
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final cs = Theme.of(context).colorScheme;
    final isLandscape = MediaQuery.of(context).size.width > 600;
    return Scaffold(
      appBar: AppBar(
        titleSpacing: 4,
        title: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            GestureDetector(
              onTap: _es,
              child: Container(
                padding: const EdgeInsets.symmetric(
                  horizontal: 10,
                  vertical: 4,
                ),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(16),
                  color: ble.lwRunning ? cs.error : cs.errorContainer,
                ),
                child: Text(
                  ble.lwRunning ? 'STOP' : 'GO',
                  style: TextStyle(
                    fontSize: 13,
                    fontWeight: FontWeight.w800,
                    color: ble.lwRunning ? cs.onError : cs.onErrorContainer,
                  ),
                ),
              ),
            ),
            const SizedBox(width: 8),
            const Text(
              'LW v5.0',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.w600),
            ),
            const Spacer(),
            GestureDetector(
              onTap: ble.timerDisabled ? null : _tp,
              onLongPress: ble.timerDisabled ? null : _tl,
              child: Container(
                padding: const EdgeInsets.symmetric(
                  horizontal: 10,
                  vertical: 4,
                ),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(16),
                  color: cs.surfaceContainerHighest,
                ),
                child: Text(
                  ble.timerDisabled ? '---' : _fmt(ble.cs),
                  style: TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.w700,
                    color:
                        ble.timerDisabled
                            ? cs.onSurfaceVariant.withValues(alpha: 0.4)
                            : ble.timerRunning
                            ? cs.error
                            : cs.primary,
                  ),
                ),
              ),
            ),
          ],
        ),
        actions: [
          IconButton(
            icon: Icon(
              ble.connected
                  ? Icons.bluetooth_connected
                  : Icons.bluetooth_disabled,
              color: ble.connected ? Colors.green : cs.onSurfaceVariant,
            ),
            onPressed: () {
              if (ble.connected) {
                _picker(context);
              } else {
                _autoConnect();
              }
            },
          ),
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () {
              Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const SettingsPage()),
              );
            },
          ),
          const SizedBox(width: 4),
        ],
      ),
      body:
          isLandscape
              ? Row(
                children: [
                  NavigationRail(
                    selectedIndex: _tab,
                    onDestinationSelected: _sw,
                    labelType: NavigationRailLabelType.all,
                    destinations: _rail,
                  ),
                  const VerticalDivider(width: 1),
                  Expanded(child: IndexedStack(index: _tab, children: _pages)),
                ],
              )
              : IndexedStack(index: _tab, children: _pages),
      bottomNavigationBar:
          isLandscape
              ? null
              : NavigationBar(
                selectedIndex: _tab,
                onDestinationSelected: _sw,
                destinations: _nav,
              ),
    );
  }

  void _picker(BuildContext ctx) {
    final ble = context.read<BleService>();
    ble.scan();
    showModalBottomSheet(
      context: ctx,
      builder:
          (_) => DraggableScrollableSheet(
            initialChildSize: 0.45,
            minChildSize: 0.3,
            maxChildSize: 0.85,
            expand: false,
            builder:
                (c, sc) => Column(
                  children: [
                    Padding(
                      padding: const EdgeInsets.all(12),
                      child: Row(
                        children: [
                          const Text(
                            '蓝牙设备',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                          const Spacer(),
                          if (ble.connected)
                            FilledButton.tonalIcon(
                              icon: const Icon(Icons.link_off, size: 18),
                              label: const Text('断开'),
                              onPressed: () {
                                ble.disconnect();
                                Navigator.pop(c);
                              },
                            ),
                          const SizedBox(width: 8),
                          FilledButton.tonalIcon(
                            icon: const Icon(Icons.refresh, size: 18),
                            label: const Text('扫描'),
                            onPressed: () => ble.scan(),
                          ),
                        ],
                      ),
                    ),
                    const Divider(height: 1),
                    Expanded(
                      child: Consumer<BleService>(
                        builder:
                            (_, b, __) => ListView.builder(
                              controller: sc,
                              itemCount:
                                  b.deviceList.length + (b.connected ? 1 : 0),
                              itemBuilder: (_, i) {
                                if (b.connected && i == 0)
                                  return ListTile(
                                    leading: const Icon(
                                      Icons.bluetooth_connected,
                                      color: Colors.green,
                                    ),
                                    title: Text(b.connectedDevName ?? '已连接'),
                                    subtitle: Text(
                                      'FW: ${b.fw.isEmpty ? "--" : b.fw}',
                                    ),
                                    trailing: const Icon(
                                      Icons.check_circle,
                                      color: Colors.green,
                                    ),
                                  );
                                final d = b.deviceList[b.connected ? i - 1 : i];
                                final parts = d.split('||');
                                final name = parts.isNotEmpty ? parts[0] : d;
                                final mac = parts.length > 1 ? parts[1] : '';
                                return ListTile(
                                  leading: const Icon(
                                    Icons.devices,
                                    color: Color(0xFF1A73E8),
                                  ),
                                  title: Text(name),
                                  subtitle:
                                      mac.isNotEmpty && mac != name
                                          ? Text(
                                            mac,
                                            style: const TextStyle(
                                              fontSize: 11,
                                              fontFamily: 'monospace',
                                            ),
                                          )
                                          : null,
                                  onTap: () {
                                    ble.connect(d);
                                    Navigator.pop(c);
                                  },
                                );
                              },
                            ),
                      ),
                    ),
                  ],
                ),
          ),
    );
  }
}
