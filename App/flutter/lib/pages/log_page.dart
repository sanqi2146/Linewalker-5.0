import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';

class LogPage extends StatefulWidget {
  const LogPage({super.key});
  @override
  State<LogPage> createState() => _LogPageState();
}

class _LogPageState extends State<LogPage> {
  final ScrollController _scrollCtrl = ScrollController();
  final GlobalKey _listKey = GlobalKey();
  bool _autoScroll = true;
  bool _selectMode = false;
  final Set<int> _selected = {};
  int? _dragAnchor;
  late Set<int> _dragBackup;
  static const double _kItemH = 13.5;

  Color _stateColor(String line, ColorScheme cs) {
    if (line.contains('锁')) return const Color(0xFF9E9E9E);
    if (line.contains('[直线]') || line.contains('[直行]')) return cs.primary;
    if (line.contains('[偏左]') || line.contains('[偏右]')) return cs.secondary;
    if (line.contains('弧弯')) return cs.tertiary;
    if (line.contains('直角')) return cs.error;
    if (line.contains('十字')) return cs.errorContainer;
    if (line.contains('丢线')) return const Color(0xFFFF9800);
    if (line.contains('终点')) return const Color(0xFF4CAF50);
    if (line.contains('避障')) return cs.error;
    if (line.contains('[CONN]')) return cs.error;
    if (line.contains(' > ')) return cs.secondary;
    if (line.contains('S=')) return cs.tertiary;
    if (line.contains('E=') || line.contains('C=')) return cs.primary;
    if (line.contains('VER=')) return const Color(0xFF4CAF50);
    if (line.contains('断开') || line.contains('失败')) return cs.error;
    if (line.contains('已连') || line.contains('扫描')) return cs.primary;
    return cs.onSurfaceVariant;
  }

  int _posToIndex(double globalY, int maxLen) {
    final box = _listKey.currentContext?.findRenderObject() as RenderBox?;
    if (box == null) return -1;
    final localY = box.globalToLocal(Offset(0, globalY)).dy;
    final scroll = _scrollCtrl.hasClients ? _scrollCtrl.offset : 0.0;
    final idx = ((localY + scroll - 4) / _kItemH).floor();
    return idx.clamp(0, maxLen - 1);
  }

  void _exitSelect() {
    setState(() {
      _selectMode = false;
      _selected.clear();
      _dragAnchor = null;
    });
  }

  void _copySelected(List<String> lines) {
    final sel = _selected.toList()..sort();
    final text = sel.map((i) => lines[i]).join('\n');
    Clipboard.setData(ClipboardData(text: text));
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text('已复制 ${sel.length} 条日志'),
        duration: const Duration(seconds: 1),
      ),
    );
    _exitSelect();
  }

  @override
  void initState() {
    super.initState();
    _scrollCtrl.addListener(() {
      if (!_scrollCtrl.hasClients) return;
      final atBottom =
          _scrollCtrl.position.pixels >=
          _scrollCtrl.position.maxScrollExtent - 20;
      if (!atBottom && _autoScroll) setState(() => _autoScroll = false);
      if (atBottom && !_autoScroll) setState(() => _autoScroll = true);
    });
  }

  @override
  void dispose() {
    _scrollCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final cs = Theme.of(context).colorScheme;
    final lines = ble.logLines;

    if (_autoScroll && lines.isNotEmpty && !_selectMode) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (_scrollCtrl.hasClients)
          _scrollCtrl.jumpTo(_scrollCtrl.position.maxScrollExtent);
      });
    }

    return Column(
      children: [
        Expanded(
          child: ListView.builder(
            key: _listKey,
            controller: _scrollCtrl,
            physics: _selectMode ? const NeverScrollableScrollPhysics() : null,
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
            itemCount: lines.length,
            itemBuilder: (_, i) {
              final line = lines[i];
              final color = _stateColor(line, cs);
              final isBold =
                  line.contains('直角') ||
                  line.contains('终点') ||
                  line.contains('避障');
              final isSel = _selected.contains(i);
              return GestureDetector(
                onLongPress: () {
                  if (!_selectMode) {
                    setState(() {
                      _selectMode = true;
                      _autoScroll = false;
                      _dragAnchor = i;
                      _selected.add(i);
                    });
                    return;
                  }
                },
                onTap: () {
                  if (_selectMode) {
                    setState(() {
                      if (isSel) {
                        _selected.remove(i);
                        _dragAnchor = i;
                        if (_selected.isEmpty) _selectMode = false;
                      } else {
                        if (_dragAnchor != null &&
                            (_dragAnchor! - i).abs() > 1) {
                          final lo = _dragAnchor! < i ? _dragAnchor! : i;
                          final hi = _dragAnchor! > i ? _dragAnchor! : i;
                          for (int j = lo; j <= hi; j++) {
                            _selected.add(j);
                          }
                          _dragAnchor = null;
                        } else {
                          _selected.add(i);
                          _dragAnchor = i;
                        }
                      }
                    });
                  }
                },
                onVerticalDragStart:
                    _selectMode
                        ? (_) {
                          _dragBackup = Set.from(_selected);
                          _dragAnchor = i;
                          if (!_selected.contains(i)) {
                            setState(() => _selected.add(i));
                          }
                        }
                        : null,
                onVerticalDragUpdate:
                    _selectMode
                        ? (d) {
                          final idx = _posToIndex(
                            d.globalPosition.dy,
                            lines.length,
                          );
                          if (idx < 0) return;
                          setState(() {
                            _selected.clear();
                            _selected.addAll(_dragBackup);
                            final lo = _dragAnchor! < idx ? _dragAnchor! : idx;
                            final hi = _dragAnchor! > idx ? _dragAnchor! : idx;
                            for (int j = lo; j <= hi; j++) {
                              _selected.add(j);
                            }
                          });
                        }
                        : null,
                child: Container(
                  width: double.infinity,
                  color: isSel ? cs.primaryContainer.withAlpha(128) : null,
                  padding: const EdgeInsets.symmetric(vertical: 1),
                  child: Text(
                    line,
                    style: TextStyle(
                      fontSize: 11,
                      fontFamily: 'monospace',
                      color: isSel ? cs.onPrimaryContainer : color,
                      fontWeight: isBold ? FontWeight.w700 : FontWeight.w400,
                    ),
                  ),
                ),
              );
            },
          ),
        ),
        Padding(
          padding: const EdgeInsets.all(8),
          child:
              _selectMode
                  ? Row(
                    children: [
                      Text(
                        '已选 ${_selected.length} 条',
                        style: TextStyle(
                          color: cs.primary,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                      const Spacer(),
                      ActionChip(
                        avatar: const Icon(Icons.copy, size: 16),
                        label: const Text('复制'),
                        onPressed: () => _copySelected(lines),
                      ),
                      const SizedBox(width: 8),
                      ActionChip(
                        avatar: const Icon(Icons.close, size: 16),
                        label: const Text('取消'),
                        onPressed: _exitSelect,
                      ),
                    ],
                  )
                  : Row(
                    children: [
                      Expanded(
                        child: ActionChip(
                          avatar: const Icon(Icons.delete_outline, size: 16),
                          label: const Text('清空'),
                          onPressed: () => ble.clearLog(),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: ActionChip(
                          avatar: const Icon(Icons.copy, size: 16),
                          label: const Text('复制'),
                          onPressed: () {
                            final text = lines.join('\n');
                            Clipboard.setData(ClipboardData(text: text));
                            ScaffoldMessenger.of(context).showSnackBar(
                              const SnackBar(
                                content: Text('日志已复制到剪切板'),
                                duration: Duration(seconds: 1),
                              ),
                            );
                          },
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: ActionChip(
                          avatar: Icon(
                            _autoScroll ? Icons.lock_open : Icons.lock,
                            size: 16,
                          ),
                          label: Text(_autoScroll ? '自动滚' : '锁中'),
                          onPressed: () {
                            setState(() => _autoScroll = !_autoScroll);
                            if (_autoScroll && _scrollCtrl.hasClients) {
                              _scrollCtrl.jumpTo(
                                _scrollCtrl.position.maxScrollExtent,
                              );
                            }
                          },
                          backgroundColor:
                              _autoScroll
                                  ? cs.surfaceContainerHighest
                                  : cs.errorContainer,
                        ),
                      ),
                    ],
                  ),
        ),
      ],
    );
  }
}
