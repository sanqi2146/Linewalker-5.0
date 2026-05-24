import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';

class SensorPage extends StatelessWidget {
  const SensorPage({super.key});

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final cs = Theme.of(context).colorScheme;
    final s = ble.sensors.padRight(10, '0'); // V4: 10位

    // 八路传感器 (车头朝前, 从左到右 L1~L8)
    final labels = ['L1', 'L2', 'L3', 'L4', 'L5', 'L6', 'L7', 'L8'];
    final weights = ['-7', '-5', '-3', '-1', '+1', '+3', '+5', '+7'];
    final colors = [
      0xFF4CAF50,
      0xFF66BB6A,
      0xFFFF9800,
      0xFFFF5722,
      0xFFFF5722,
      0xFFFF9800,
      0xFF66BB6A,
      0xFF4CAF50,
    ];
    final obstacleSensors = ['TL', 'TR'];
    final obstacleColor = 0xFF2196F3;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(12),
      child: Column(
        children: [
          // ===== 八路巡线传感器 =====
          Card(
            child: Padding(
              padding: const EdgeInsets.all(14),
              child: Column(
                children: [
                  Row(
                    children: [
                      Icon(Icons.track_changes, size: 16, color: cs.primary),
                      const SizedBox(width: 6),
                      Text(
                        '八路巡线 (I2C)',
                        style: TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w600,
                          color: cs.primary,
                        ),
                      ),
                      const Spacer(),
                      Text(
                        'PB6/PB7',
                        style: TextStyle(
                          fontSize: 10,
                          color: cs.onSurfaceVariant.withValues(alpha: 0.5),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  // 上排: X1~X4
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: List.generate(4, (i) {
                      final on = s[i] == '0'; // 0=黑线
                      final c = Color(colors[i]);
                      return _SensorDot(
                        label: labels[i],
                        weight: weights[i],
                        on: on,
                        color: c,
                        cs: cs,
                      );
                    }),
                  ),
                  const SizedBox(height: 6),
                  // 下排: X5~X8
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: List.generate(4, (i) {
                      final idx = i + 4;
                      final on = s[idx] == '0';
                      final c = Color(colors[idx]);
                      return _SensorDot(
                        label: labels[idx],
                        weight: weights[idx],
                        on: on,
                        color: c,
                        cs: cs,
                      );
                    }),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 8),

          // ===== 前方红外避障 =====
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  Row(
                    children: [
                      Icon(Icons.sensors, size: 16, color: cs.secondary),
                      const SizedBox(width: 6),
                      Text(
                        '前方红外避障',
                        style: TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w600,
                          color: cs.secondary,
                        ),
                      ),
                      const Spacer(),
                      Text(
                        'PC14/PC15',
                        style: TextStyle(
                          fontSize: 10,
                          color: cs.onSurfaceVariant.withValues(alpha: 0.5),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: List.generate(2, (i) {
                      final idx = i + 8; // 避障在S=的8,9位
                      final on = s.length > idx && s[idx] == '0'; // 0=障碍
                      final c = Color(obstacleColor);
                      return Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 24),
                        child: Column(
                          children: [
                            Container(
                              width: 40,
                              height: 40,
                              decoration: BoxDecoration(
                                shape: BoxShape.circle,
                                color: on ? c : cs.surfaceContainerHighest,
                                border: Border.all(
                                  color:
                                      on
                                          ? c.withValues(alpha: 0.5)
                                          : cs.outlineVariant,
                                  width: 2,
                                ),
                              ),
                              child: Center(
                                child: Text(
                                  obstacleSensors[i],
                                  style: TextStyle(
                                    fontSize: 11,
                                    color:
                                        on ? Colors.white : cs.onSurfaceVariant,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ),
                            ),
                            const SizedBox(height: 4),
                            Text(
                              on ? '障碍' : '安全',
                              style: TextStyle(
                                fontSize: 10,
                                color: on ? c : cs.onSurfaceVariant,
                              ),
                            ),
                          ],
                        ),
                      );
                    }),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 8),

          // ===== 红外编码器 =====
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  Row(
                    children: [
                      Icon(Icons.speed, size: 16, color: cs.tertiary),
                      const SizedBox(width: 6),
                      Text(
                        '红外编码器',
                        style: TextStyle(
                          fontSize: 14,
                          fontWeight: FontWeight.w600,
                          color: cs.tertiary,
                        ),
                      ),
                      const Spacer(),
                      Text(
                        'PA4/PA5',
                        style: TextStyle(
                          fontSize: 10,
                          color: cs.onSurfaceVariant.withValues(alpha: 0.5),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      _EncCircle(
                        label: 'EL',
                        color: cs.tertiaryContainer,
                        onCol: cs.onTertiaryContainer,
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          '${ble.leftDtMs}ms / 4格 → ${(ble.leftSpeed / 10.0).toStringAsFixed(1)} cm/s',
                          style: TextStyle(
                            fontSize: 13,
                            fontFamily: 'monospace',
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      _EncCircle(
                        label: 'ER',
                        color: cs.errorContainer,
                        onCol: cs.onErrorContainer,
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          '${ble.rightDtMs}ms / 4格 → ${(ble.rightSpeed / 10.0).toStringAsFixed(1)} cm/s',
                          style: TextStyle(
                            fontSize: 13,
                            fontFamily: 'monospace',
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const Divider(height: 20),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceAround,
                    children: [
                      _Metric(
                        label: '均速',
                        value:
                            '${(ble.avgSpeed / 10.0).toStringAsFixed(1)} cm/s',
                        color: cs.tertiary,
                      ),
                      Container(width: 1, height: 32, color: cs.outlineVariant),
                      _Metric(
                        label: '均里程',
                        value:
                            '${(ble.avgMileage / 10.0).toStringAsFixed(1)} cm',
                        color: cs.primary,
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 8),

          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  _Metric(
                    label: '状态',
                    value: ble.logStateCN,
                    color: cs.secondary,
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// 单个传感器圆点
class _SensorDot extends StatelessWidget {
  final String label, weight;
  final bool on;
  final Color color;
  final ColorScheme cs;
  const _SensorDot({
    required this.label,
    required this.weight,
    required this.on,
    required this.color,
    required this.cs,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Container(
          width: 42,
          height: 42,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: on ? color : cs.surfaceContainerHighest,
            border: Border.all(
              color: on ? color.withValues(alpha: 0.4) : cs.outlineVariant,
              width: 2,
            ),
          ),
          child: Center(
            child: Text(
              label,
              style: TextStyle(
                fontSize: 10,
                color: on ? Colors.white : cs.onSurfaceVariant,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
        ),
        const SizedBox(height: 2),
        Text(
          on ? '黑线' : '白底',
          style: TextStyle(
            fontSize: 9,
            color: on ? color : cs.onSurfaceVariant,
          ),
        ),
        Text(
          weight,
          style: TextStyle(
            fontSize: 8,
            color: cs.onSurfaceVariant.withValues(alpha: 0.4),
          ),
        ),
      ],
    );
  }
}

// 编码器圆点
class _EncCircle extends StatelessWidget {
  final String label;
  final Color color, onCol;
  const _EncCircle({
    required this.label,
    required this.color,
    required this.onCol,
  });
  @override
  Widget build(BuildContext context) {
    return Container(
      width: 32,
      height: 32,
      decoration: BoxDecoration(shape: BoxShape.circle, color: color),
      child: Center(
        child: Text(
          label,
          style: TextStyle(
            fontSize: 10,
            fontWeight: FontWeight.w700,
            color: onCol,
          ),
        ),
      ),
    );
  }
}

// 度量卡片
class _Metric extends StatelessWidget {
  final String label, value;
  final Color color;
  const _Metric({
    required this.label,
    required this.value,
    required this.color,
  });
  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Column(
      children: [
        Text(
          value,
          style: TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.w700,
            color: color,
          ),
        ),
        const SizedBox(height: 2),
        Text(label, style: TextStyle(fontSize: 11, color: cs.onSurfaceVariant)),
      ],
    );
  }
}
