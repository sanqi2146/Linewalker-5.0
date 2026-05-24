import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';
import '../utils/vib.dart';

class TunePage extends StatefulWidget {
  const TunePage({super.key});
  @override
  State<TunePage> createState() => _TunePageState();
}

class _TunePageState extends State<TunePage> {
  bool _pidOpen = true, _turnOpen = true;
  final Map<String, FixedExtentScrollController> _scrollers = {};
  double _innerVal = 25, _outerVal = 50, _sharpRotateVal = 45;
  double _lockVal = 10;
  bool _wasCfgReceived = false;

  void _syncFromBle(BleService ble) {
    if (!ble.cfgReceived) return;
    _innerVal = ble.turnInner.toDouble();
    _outerVal = ble.turnOuter.toDouble();
    _sharpRotateVal = ble.sharpRotateSpeed.toDouble();
    _lockVal = ble.turnLock.toDouble();
    _jumpScroller('Kp 刚度', ble.kp, 0, 300);
    _jumpScroller('Ki.float', ble.ki, 0, 5.00);
    _jumpScroller('Kd 阻尼', ble.kd, 0, 300);
    _jumpScroller('SP 车速', ble.sp.toDouble(), 20, 99);
    _jumpScroller('内轮速', _innerVal, 0, 99);
    _jumpScroller('外轮速', _outerVal, 0, 99);
    _jumpScroller('转弯速度', _sharpRotateVal, 10, 60);
    _jumpScroller('锁存轮数', _lockVal, 0, 500);
  }

  void _jumpScroller(String id, double value, double min, double max) {
    final c = _scrollers[id];
    if (c != null && c.hasClients) {
      final idx = ((value - min).toInt()).clamp(0, (max - min).toInt());
      c.jumpToItem(idx);
    }
  }

  FixedExtentScrollController _ctrl(
    String id,
    double value,
    double min,
    double max,
  ) {
    if (!_scrollers.containsKey(id)) {
      final count = (max - min).toInt() + 1;
      final idx = (value - min).toInt().clamp(0, count - 1);
      _scrollers[id] = FixedExtentScrollController(initialItem: idx);
    }
    return _scrollers[id]!;
  }

  int _val(String id, FixedExtentScrollController c, double min) {
    if (!c.hasClients) return min.toInt();
    return (c.selectedItem + min.toInt()).clamp(
      min.toInt(),
      (min + 100).toInt(),
    );
  }

  void _onWheel(
    String id,
    double min,
    double max,
    String cmd, {
    bool isInt = true,
  }) {
    final c = _scrollers[id]!;
    final v = _val(id, c, min);
    final ble = context.read<BleService>();
    HapticFeedback.selectionClick();
    if (isInt)
      ble.sendCmd('$cmd=$v');
    else
      ble.sendCmd('$cmd=${v.toDouble()}');
  }

  Widget _roller(
    String label,
    double currentVal,
    double min,
    double max,
    String cmd,
  ) {
    final cs = Theme.of(context).colorScheme;
    final count = (max - min).toInt() + 1;
    final items = List.generate(count, (i) => '${min.toInt() + i}');
    final ctrl = _ctrl(label, currentVal, min, max);
    return Column(
      children: [
        Text(
          label,
          style: TextStyle(
            fontSize: 11,
            fontWeight: FontWeight.w700,
            color: cs.primary,
          ),
        ),
        const SizedBox(height: 2),
        SizedBox(
          height: 88,
          child: ListWheelScrollView.useDelegate(
            controller: ctrl,
            itemExtent: 32,
            diameterRatio: 2.5,
            perspective: 0.003,
            onSelectedItemChanged: (_) => _onWheel(label, min, max, cmd),
            childDelegate: ListWheelChildBuilderDelegate(
              builder: (context, i) {
                final isCenter = ctrl.hasClients && i == ctrl.selectedItem;
                return Center(
                  child: Text(
                    items[i],
                    style: TextStyle(
                      fontSize: isCenter ? 20 : 14,
                      fontWeight: isCenter ? FontWeight.w800 : FontWeight.w400,
                      color:
                          isCenter
                              ? cs.primary
                              : cs.onSurfaceVariant.withValues(
                                alpha: isCenter ? 1 : 0.35,
                              ),
                    ),
                  ),
                );
              },
              childCount: count,
            ),
          ),
        ),
      ],
    );
  }

  Widget _rollerFloat2(
    String label,
    double currentVal,
    double min,
    double max,
    String cmd,
  ) {
    final cs = Theme.of(context).colorScheme;
    final steps = ((max - min) * 100).toInt() + 1;
    final items = List.generate(
      steps,
      (i) => (min + i * 0.01).toStringAsFixed(2),
    );
    final key = '$label.float';
    if (!_scrollers.containsKey(key)) {
      final idx = ((currentVal - min) * 100).toInt().clamp(0, steps - 1);
      _scrollers[key] = FixedExtentScrollController(initialItem: idx);
    }
    final ctrl = _scrollers[key]!;
    return _buildRoller(label, items, ctrl, cmd, min, max, 0.01, cs);
  }

  Widget _buildRoller(
    String label,
    List<String> items,
    FixedExtentScrollController ctrl,
    String cmd,
    double min,
    double max,
    double step,
    ColorScheme cs,
  ) {
    return Column(
      children: [
        Text(
          label,
          style: TextStyle(
            fontSize: 11,
            fontWeight: FontWeight.w700,
            color: cs.primary,
          ),
        ),
        const SizedBox(height: 2),
        SizedBox(
          height: 88,
          child: ListWheelScrollView.useDelegate(
            controller: ctrl,
            itemExtent: 32,
            diameterRatio: 2.5,
            perspective: 0.003,
            onSelectedItemChanged: (_) {
              if (!ctrl.hasClients) return;
              final v = (min + ctrl.selectedItem * step).clamp(min, max);
              HapticFeedback.selectionClick();
              final ble = context.read<BleService>();
              final s =
                  step > 0.05 ? v.toStringAsFixed(1) : v.toStringAsFixed(2);
              ble.sendCmd('$cmd=$s');
            },
            childDelegate: ListWheelChildBuilderDelegate(
              builder: (context, i) {
                final isCenter = ctrl.hasClients && i == ctrl.selectedItem;
                return Center(
                  child: Text(
                    items[i],
                    style: TextStyle(
                      fontSize: isCenter ? 20 : 14,
                      fontWeight: isCenter ? FontWeight.w800 : FontWeight.w400,
                      color:
                          isCenter
                              ? cs.primary
                              : cs.onSurfaceVariant.withValues(
                                alpha: isCenter ? 1 : 0.35,
                              ),
                    ),
                  ),
                );
              },
              childCount: items.length,
            ),
          ),
        ),
      ],
    );
  }

  @override
  void dispose() {
    for (final c in _scrollers.values) {
      c.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final cs = Theme.of(context).colorScheme;

    if (ble.cfgReceived && !_wasCfgReceived) {
      _wasCfgReceived = true;
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _syncFromBle(ble);
      });
    }

    final kp = ble.kp, ki = ble.ki, kd = ble.kd;
    final sp = ble.sp;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(12),
      child: Column(
        children: [
          _card(
            'PID 调参',
            Icons.tune,
            _pidOpen,
            () => setState(() => _pidOpen = !_pidOpen),
            cs,
            children: [
              Row(
                children: [
                  Expanded(child: _roller('Kp 刚度', kp, 0, 300, 'KP')),
                  const SizedBox(width: 8),
                  Expanded(child: _rollerFloat2('Ki 惯性', ki, 0, 5.00, 'KI')),
                  const SizedBox(width: 8),
                  Expanded(child: _roller('Kd 阻尼', kd, 0, 300, 'KD')),
                  const SizedBox(width: 8),
                  Expanded(
                    child: _roller('SP 车速', sp.toDouble(), 20, 99, 'SP'),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  _chip('保守', () {
                    _preset(1, 18, 0.30, 12, 45, 45, 7, ble);
                  }, cs),
                  _chip('默认', () {
                    _preset(2, 25, 0.15, 30, 55, 55, 6, ble);
                  }, cs),
                  _chip('激进', () {
                    _preset(3, 32, 0.20, 14, 60, 60, 5, ble);
                  }, cs),
                  const Spacer(),
                  FilledButton.tonalIcon(
                    icon: const Icon(Icons.save, size: 16),
                    label: const Text('保存'),
                    onPressed: () {
                      Vib.medium();
                      ble.sendCmd('SAVE');
                    },
                  ),
                ],
              ),
            ],
          ),
          const SizedBox(height: 8),
          _card(
            '转弯调节',
            Icons.tune,
            _turnOpen,
            () => setState(() => _turnOpen = !_turnOpen),
            cs,
            children: [
              Row(
                children: [
                  Expanded(child: _roller('内轮速', _innerVal, 0, 99, 'TURN_IN')),
                  const SizedBox(width: 8),
                  Expanded(child: _roller('外轮速', _outerVal, 0, 99, 'TURN_OUT')),
                ],
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: _roller('转弯速度', _sharpRotateVal, 10, 60, 'RSP'),
                  ),
                  const SizedBox(width: 8),
                  Expanded(child: _roller('锁存轮数', _lockVal, 0, 500, 'TL')),
                ],
              ),
              const SizedBox(height: 8),
              FilledButton.tonalIcon(
                icon: const Icon(Icons.play_arrow, size: 18),
                label: const Text('模拟转弯'),
                onPressed:
                    ble.connected
                        ? () {
                          Vib.medium();
                          ble.lwRunning = true;
                          ble.sendCmd('TURN_GO');
                        }
                        : null,
              ),
            ],
          ),
        ],
      ),
    );
  }

  void _preset(
    int n,
    double kp,
    double ki,
    double kd,
    int sp,
    int rsp,
    int tl,
    BleService ble,
  ) {
    Vib.medium();
    ble.sendCmd('PR$n');
    ble.sendCmd('RSP=$rsp');
    ble.sendCmd('TL=$tl');
    _scrollers['Kp 刚度']?.jumpToItem(kp.toInt());
    _scrollers['Kd 阻尼']?.jumpToItem(kd.toInt());
    _scrollers['SP 车速']?.jumpToItem(sp - 20);
    _scrollers['Ki.float']?.jumpToItem((ki * 100).toInt());
    _scrollers['转弯速度']?.jumpToItem(rsp - 10);
    _scrollers['锁存轮数']?.jumpToItem(tl);
  }

  Widget _card(
    String title,
    IconData icon,
    bool open,
    VoidCallback toggle,
    ColorScheme cs, {
    required List<Widget> children,
  }) {
    return Card(
      child: ExpansionTile(
        initiallyExpanded: open,
        onExpansionChanged: (_) => toggle(),
        title: Row(
          children: [
            Icon(icon, size: 16, color: cs.primary),
            const SizedBox(width: 6),
            Text(
              title,
              style: TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w600,
                color: cs.primary,
              ),
            ),
          ],
        ),
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(12, 0, 12, 12),
            child: Column(children: children),
          ),
        ],
      ),
    );
  }

  Widget _chip(String label, VoidCallback onTap, ColorScheme cs) {
    return Padding(
      padding: const EdgeInsets.only(right: 6),
      child: ActionChip(
        label: Text(label, style: const TextStyle(fontSize: 12)),
        onPressed: onTap,
        backgroundColor: cs.surfaceContainerHighest,
      ),
    );
  }
}
