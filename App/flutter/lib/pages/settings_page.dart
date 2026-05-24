import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../services/app_settings.dart';
import '../services/ble_service.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});
  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  static const _seedColors = [
    0xFF6750A4,
    0xFFBA1A1A,
    0xFF0B6B4E,
    0xFF3F51B5,
    0xFF00796B,
    0xFFE65100,
    0xFF5C6BC0,
    0xFF2E7D32,
  ];

  @override
  Widget build(BuildContext context) {
    final settings = context.watch<AppSettings>();
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(title: const Text('设置')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Text(
            '主题',
            style: TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: cs.primary,
            ),
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              _ThemeBtn(
                '跟随系统',
                ThemeMode.system,
                settings.themeMode,
                () => settings.setThemeMode(ThemeMode.system),
              ),
              const SizedBox(width: 8),
              _ThemeBtn(
                '浅色',
                ThemeMode.light,
                settings.themeMode,
                () => settings.setThemeMode(ThemeMode.light),
              ),
              const SizedBox(width: 8),
              _ThemeBtn(
                '深色',
                ThemeMode.dark,
                settings.themeMode,
                () => settings.setThemeMode(ThemeMode.dark),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Text(
            '主题色',
            style: TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: cs.primary,
            ),
          ),
          const SizedBox(height: 4),
          Row(
            children: [
              Text(
                '提取壁纸颜色',
                style: TextStyle(fontSize: 13, color: cs.onSurfaceVariant),
              ),
              const Spacer(),
              Switch(
                value: settings.useWallpaperColor,
                onChanged: (v) {
                  HapticFeedback.lightImpact();
                  settings.setWallpaperColor(v);
                },
              ),
            ],
          ),
          if (!settings.useWallpaperColor) ...[
            const SizedBox(height: 8),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children:
                  _seedColors.map((c) {
                    final sel = c == settings.seedColor.value;
                    return GestureDetector(
                      onTap: () {
                        HapticFeedback.lightImpact();
                        settings.setSeedColor(Color(c));
                      },
                      child: Container(
                        width: 36,
                        height: 36,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: Color(c),
                          border:
                              sel
                                  ? Border.all(color: cs.onSurface, width: 3)
                                  : null,
                        ),
                        child:
                            sel
                                ? const Icon(
                                  Icons.check,
                                  size: 16,
                                  color: Colors.white,
                                )
                                : null,
                      ),
                    );
                  }).toList(),
            ),
          ],
          const SizedBox(height: 16),
          Text(
            '震动反馈',
            style: TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: cs.primary,
            ),
          ),
          const SizedBox(height: 4),
          Row(
            children: [
              const Text('强度'),
              Expanded(
                child: Slider(
                  value: settings.vibStrength.toDouble(),
                  min: 0,
                  max: 100,
                  divisions: 10,
                  label: '${settings.vibStrength}%',
                  onChanged: (v) {
                    HapticFeedback.selectionClick();
                    settings.setVibStrength(v.toInt());
                  },
                ),
              ),
              Text(
                '${settings.vibStrength}%',
                style: TextStyle(fontSize: 12, color: cs.onSurfaceVariant),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Text(
            '引脚对照',
            style: TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: cs.primary,
            ),
          ),
          const SizedBox(height: 8),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: _pinTable(cs),
            ),
          ),
          const SizedBox(height: 24),
          Text(
            '关于',
            style: TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: cs.primary,
            ),
          ),
          const SizedBox(height: 8),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Builder(
                    builder: (context) {
                      final fw = context.read<BleService>().fw;
                      return Text(
                        fw.isNotEmpty ? '固件 $fw' : 'LineWalker v5.0',
                        style: const TextStyle(fontWeight: FontWeight.w600),
                      );
                    },
                  ),
                  const SizedBox(height: 4),
                  Text(
                    'Flutter + C (STM32)',
                    style: TextStyle(fontSize: 12, color: cs.onSurfaceVariant),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    'Material You 3, 蓝牙SPP (HC-05)',
                    style: TextStyle(fontSize: 12, color: cs.onSurfaceVariant),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _pinTable(ColorScheme cs) {
    final rows = [
      ['左电机 IN1', 'PB14', '右电机 IN1', 'PA6'],
      ['左电机 IN2', 'PB15', '右电机 IN2', 'PA7'],
      ['左电机 PWM', 'PA1', '右电机 PWM', 'PA2'],
      ['8路巡线 SCL', 'PB6', '8路巡线 SDA', 'PB7'],
      ['TL 避障', 'PC15', 'TR 避障', 'PC14'],
      ['左编码器', 'PA4', '右编码器', 'PA5'],
      ['蜂鸣器', 'PA8', '蓝牙 TX', 'PA9'],
      ['蓝牙 RX', 'PA10', 'OLED SCL', 'PB8'],
      ['OLED SDA', 'PB9', '', ''],
    ];
    return Table(
      columnWidths: const {
        0: FlexColumnWidth(2),
        1: FlexColumnWidth(1),
        2: FlexColumnWidth(2),
        3: FlexColumnWidth(1),
      },
      defaultVerticalAlignment: TableCellVerticalAlignment.middle,
      children: [
        TableRow(
          decoration: BoxDecoration(
            border: Border(bottom: BorderSide(color: cs.outlineVariant)),
          ),
          children: [
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: Text(
                '左/前',
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                  color: cs.primary,
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: Text(
                '引脚',
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                  color: cs.primary,
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: Text(
                '右/后',
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                  color: cs.primary,
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: Text(
                '引脚',
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                  color: cs.primary,
                ),
              ),
            ),
          ],
        ),
        ...rows.map(
          (r) => TableRow(
            decoration: BoxDecoration(
              border: Border(
                bottom: BorderSide(
                  color: cs.outlineVariant.withValues(alpha: 0.3),
                ),
              ),
            ),
            children: [
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Text(
                  r[0],
                  style: TextStyle(fontSize: 11, color: cs.onSurface),
                ),
              ),
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Text(
                  r[1],
                  style: TextStyle(
                    fontSize: 11,
                    fontFamily: 'monospace',
                    fontWeight: FontWeight.w600,
                    color: cs.tertiary,
                  ),
                ),
              ),
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Text(
                  r[2],
                  style: TextStyle(fontSize: 11, color: cs.onSurface),
                ),
              ),
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Text(
                  r[3],
                  style: TextStyle(
                    fontSize: 11,
                    fontFamily: 'monospace',
                    fontWeight: FontWeight.w600,
                    color: cs.tertiary,
                  ),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _ThemeBtn extends StatelessWidget {
  final String label;
  final ThemeMode mode;
  final ThemeMode current;
  final VoidCallback onTap;
  const _ThemeBtn(this.label, this.mode, this.current, this.onTap);

  @override
  Widget build(BuildContext context) {
    final sel = mode == current;
    return Expanded(
      child: FilterChip(
        label: Text(label, style: const TextStyle(fontSize: 12)),
        selected: sel,
        onSelected: (_) {
          HapticFeedback.lightImpact();
          onTap();
        },
        showCheckmark: false,
      ),
    );
  }
}
