import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';
import '../utils/vib.dart';

class ControlPage extends StatefulWidget {
  const ControlPage({super.key});
  @override
  State<ControlPage> createState() => _ControlPageState();
}

class _ControlPageState extends State<ControlPage> {
  bool _rotateDir = true;

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final cs = Theme.of(context).colorScheme;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(12),
      child: Column(
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: FilledButton.icon(
                icon: const Icon(Icons.track_changes, size: 18),
                label: const Text('循迹'),
                onPressed:
                    ble.connected
                        ? () {
                          Vib.light();
                          ble.lwRunning = true;
                          ble.startTimer();
                          ble.sendRaw('G');
                          ble.sendCmd('LOG=1');
                        }
                        : null,
                style: FilledButton.styleFrom(
                  backgroundColor: cs.primary,
                  foregroundColor: cs.onPrimary,
                  minimumSize: const Size(double.infinity, 48),
                ),
              ),
            ),
          ),
          const SizedBox(height: 8),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  Row(
                    children: [
                      const SizedBox(width: 4),
                      Icon(Icons.gamepad, size: 14, color: cs.primary),
                      const SizedBox(width: 6),
                      Text(
                        '方向控制',
                        style: TextStyle(
                          fontSize: 13,
                          fontWeight: FontWeight.w600,
                          color: cs.primary,
                        ),
                      ),
                      const Spacer(),
                    ],
                  ),
                  const SizedBox(height: 12),
                  SizedBox(
                    width: 220,
                    height: 220,
                    child: Stack(
                      alignment: Alignment.center,
                      children: [
                        Positioned(
                          top: 0,
                          child: _DB(Icons.keyboard_arrow_up, '@', ble),
                        ),
                        Positioned(
                          bottom: 0,
                          child: _DB(Icons.keyboard_arrow_down, 'A', ble),
                        ),
                        Positioned(
                          left: 0,
                          child: _DB(Icons.keyboard_arrow_left, 'C', ble),
                        ),
                        Positioned(
                          right: 0,
                          child: _DB(Icons.keyboard_arrow_right, 'D', ble),
                        ),
                        Positioned(
                          child: IconButton.filled(
                            icon: Icon(
                              Icons.cached,
                              size: 28,
                              color: cs.onTertiaryContainer,
                            ),
                            style: IconButton.styleFrom(
                              backgroundColor: cs.tertiaryContainer,
                              minimumSize: const Size(64, 64),
                            ),
                            onPressed:
                                ble.connected
                                    ? () {
                                      Vib.light();
                                      _rotateDir = !_rotateDir;
                                      ble.sendRaw(_rotateDir ? 'E' : 'F');
                                    }
                                    : null,
                          ),
                        ),
                      ],
                    ),
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

class _DB extends StatelessWidget {
  final IconData icon;
  final String cmd;
  final BleService ble;
  const _DB(this.icon, this.cmd, this.ble);
  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return IconButton.filled(
      icon: Icon(icon, size: 32),
      style: IconButton.styleFrom(
        backgroundColor: cs.surfaceContainerHighest,
        foregroundColor:
            ble.connected
                ? cs.primary
                : cs.onSurfaceVariant.withValues(alpha: 0.3),
        minimumSize: const Size(56, 56),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
      ),
      onPressed:
          ble.connected
              ? () {
                Vib.light();
                ble.sendRaw(cmd);
              }
              : null,
    );
  }
}
