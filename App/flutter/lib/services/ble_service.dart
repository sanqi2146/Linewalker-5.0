import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../utils/vib.dart';

class BleService extends ChangeNotifier {
  static const _channel = MethodChannel('com.smartcar.linewalker/bt');
  static const _cmdMap = {
    '@': '前进',
    'A': '后退',
    'B': '停止',
    'C': '左转',
    'D': '右转',
    'E': '顺转',
    'F': '逆转',
    'G': '循迹',
  };

  static const _stCN = {
    'LINE': '直行',
    'R_ANGLE': '右直角',
    'L_ANGLE': '左直角',
    'R_SHARP': '右锐角',
    'L_SHARP': '左锐角',
    'ROT_CW': '右旋转',
    'ROT_CCW': '左旋转',
    'RECT': '找转角',
    'RECP': '找线',
    'LREC': '找线',
    'CROSS': '十字',
    'LOST': '丢线',
  };

  bool connected = false;
  bool timerDisabled = false;
  bool lwRunning = false;
  String fw = '';
  String? connectedDevName;
  List<String> deviceList = [];

  final List<String> _lines = [];
  List<String> get logLines => _lines;

  String _sensors = '0000000000'; // V4: X1~X8 + TL + TR (10位)
  double _error = 0, _correction = 0;
  double _kp = 18, _ki = 0.15, _kd = 8;
  int _sp = 45;
  String _logState = '';
  String _lapTime = '';

  int _obstacleEnable = 1;
  int _turnInner = 25;
  int _turnOuter = 50;
  int _sharpRotateSpeed = 45;
  int _turnLock = 10;
  int _pulseChunk = 4;
  int _logEnabled = 1;
  int _sensorEnabled = 1;
  int _traceEnabled = 0;
  bool _cfgReceived = false;

  int _leftSpeed = 0, _rightSpeed = 0;
  int _leftDtMs = 0, _rightDtMs = 0;
  double _leftMileage = 0, _rightMileage = 0;

  int get leftSpeed => _leftSpeed;
  int get rightSpeed => _rightSpeed;
  int get leftDtMs => _leftDtMs;
  int get rightDtMs => _rightDtMs;

  double get avgSpeed => (_leftSpeed + _rightSpeed) / 2.0;
  double get avgMileage => (_leftMileage + _rightMileage) / 2.0;

  String get sensors => _sensors;
  double get error => _error;
  double get correction => _correction;
  double get kp => _kp;
  double get ki => _ki;
  double get kd => _kd;
  int get sp => _sp;
  String get logStateCN {
    final s = _logState;
    if (s.isEmpty) return '--';
    return _stCN[s] ?? s;
  }

  int get obstacleEnable => _obstacleEnable;
  int get turnInner => _turnInner;
  int get turnOuter => _turnOuter;
  int get sharpRotateSpeed => _sharpRotateSpeed;
  int get turnLock => _turnLock;

  bool get cfgReceived => _cfgReceived;

  Timer? _stimer;
  Stopwatch _stopwatch = Stopwatch();
  int _cs = 0;
  int get cs => _cs;
  bool get timerRunning => _stimer != null;
  void startTimer() {
    _stopwatch.reset();
    _stopwatch.start();
    _stimer ??= Timer.periodic(const Duration(milliseconds: 50), (_) {
      _cs = _stopwatch.elapsedMilliseconds ~/ 10;
      notifyListeners();
    });
  }

  void stopTimer() {
    _stimer?.cancel();
    _stimer = null;
    _stopwatch.stop();
    _cs = _stopwatch.elapsedMilliseconds ~/ 10;
    notifyListeners();
  }

  void resetTimer() {
    stopTimer();
    _stopwatch.reset();
    _cs = 0;
    notifyListeners();
  }

  void _log(String msg) {
    final n = DateTime.now();
    final t =
        '${n.hour.toString().padLeft(2, '0')}:${n.minute.toString().padLeft(2, '0')}:${n.second.toString().padLeft(2, '0')}.${(n.millisecond ~/ 100)}';
    _lines.add('[$t] $msg');
    if (_lines.length > 500) _lines.removeRange(0, 200);
    notifyListeners();
  }

  void _logRcv(String d) {
    final n = DateTime.now();
    final t =
        '${n.hour.toString().padLeft(2, '0')}:${n.minute.toString().padLeft(2, '0')}:${n.second.toString().padLeft(2, '0')}.${(n.millisecond ~/ 10).toString().padLeft(2, '0')}';
    _lines.add('$t $d');
    if (_lines.length > 500) _lines.removeRange(0, 200);
  }

  void clearLog() {
    _lines.clear();
    notifyListeners();
  }

  BleService() {
    _channel.setMethodCallHandler(_onMethodCall);
  }

  bool _autoReconnect = true;
  int _reconnectAttempts = 0;
  static const _maxReconnectAttempts = 3;
  static const _reconnectDelayMs = 1500;

  Future<dynamic> _onMethodCall(MethodCall call) async {
    switch (call.method) {
      case 'onData':
        _handleRawData(call.arguments as String? ?? '');
      case 'onDisconnected':
        connected = false;
        lwRunning = false;
        stopTimer();
        _log('断开');
        notifyListeners();
        if (_autoReconnect &&
            connectedDevName != null &&
            _reconnectAttempts < _maxReconnectAttempts) {
          _reconnectAttempts++;
          _log('重连 ${_reconnectAttempts}/$_maxReconnectAttempts');
          await Future.delayed(Duration(milliseconds: _reconnectDelayMs));
          try {
            await connect(connectedDevName!);
            if (connected) {
              _reconnectAttempts = 0;
              return;
            }
          } catch (_) {}
        }
        // 重连失败或次数耗尽: 重置状态, 允许用户手动重连
        _autoReconnect = true;
        _reconnectAttempts = 0;
        notifyListeners();
    }
  }

  Future<void> scan() async {
    _log('扫描...');
    notifyListeners();
    try {
      await [
        Permission.bluetoothConnect,
        Permission.bluetoothScan,
        Permission.location,
      ].request();
      final l = await _channel.invokeMethod<List<dynamic>>('scan');
      deviceList = l?.cast<String>() ?? [];
      _log('${deviceList.length}台');
      notifyListeners();
    } catch (e) {
      _log('失败: $e');
      notifyListeners();
    }
  }

  Future<void> connect(String name) async {
    _log('连接 $name...');
    notifyListeners();
    try {
      final ok = await _channel.invokeMethod<bool>('connect', {'name': name});
      if (ok == true) {
        connected = true;
        connectedDevName = name;
        _cfgReceived = false;
        _log('已连');
        Vib.heavy();
        (await SharedPreferences.getInstance()).setString('lastDevice', name);
        notifyListeners();
        Future.delayed(const Duration(milliseconds: 500), () {
          if (connected && !_cfgReceived) {
            requestConfig();
          }
        });
      } else {
        _log('失败');
        notifyListeners();
      }
    } catch (e) {
      _log('异常: $e');
      notifyListeners();
    }
  }

  void sendRaw(String d) {
    _log('> ${_cmdMap[d] ?? d}');
    try {
      _channel.invokeMethod('send', {'data': d});
    } catch (e) {
      _log('发送失败: $e');
    }
  }

  void sendCmd(String c) {
    final cn = _cmdCN(c);
    _log('> ${cn.isNotEmpty ? cn : c}');
    sendRaw('$c\r\n');
  }

  String _cmdCN(String c) {
    if (c.startsWith('RSP=')) return '转弯速度=${c.substring(4)}';
    if (c.startsWith('TL=')) return '锁存轮数=${c.substring(3)}';
    if (c.startsWith('LOG=')) return '日志=${c.substring(4) == '1' ? '开' : '关'}';
    if (c == 'PR1') return '预设1(保守)';
    if (c == 'PR2') return '预设2(默认)';
    if (c == 'PR3') return '预设3(激进)';
    if (c == 'SAVE') return '保存参数';
    if (c == 'SET?') return '查询配置';
    if (c == 'TURN_GO') return '模拟转弯';
    return '';
  }

  void disconnect() {
    _autoReconnect = false;
    try {
      _channel.invokeMethod('disconnect');
    } catch (_) {}
    connected = false;
    connectedDevName = null;
    _log('断开');
    Vib.heavy();
    notifyListeners();
  }

  // V4.0: 与固件 ComputeDeviation 完全一致的本地计算
  // 返回 (偏差值, 状态名) — 用于每条 S= 日志追加
  // V4.6.0 纯偏差计算(不解析状态, 状态由固件上报)
  double _computeDevFromSensors(String s) {
    if (s.length < 8) return 0;
    int sumPos = 0, cnt = 0;
    for (int i = 0; i < 8; i++) {
      if (s[i] == '0') {
        sumPos += i;
        cnt++;
      }
    }
    if (cnt == 0) return 0;
    final centroid = sumPos / cnt;
    final rawDev = (centroid - 3.5) * 1.0;
    if (rawDev > -0.20 && rawDev < 0.20) return 0;
    return rawDev;
  }

  (double, String) _computeDeviation(String s) {
    if (s.length < 10) return (0, '--');
    // s[i]=='1'=白底(HIGH), s[i]=='0'=黑线(LOW)
    final w = [for (int i = 0; i < 10; i++) s[i] == '1'];

    // 全白 = 丢线
    bool allW = true;
    for (int i = 0; i < 8; i++) {
      if (!w[i]) {
        allW = false;
        break;
      }
    }
    if (allW) return (0, '丢线');

    // 全黑 = 十字
    bool allB = true;
    for (int i = 0; i < 8; i++) {
      if (w[i]) {
        allB = false;
        break;
      }
    }
    if (allB) return (0, '十字');

    // P1 锐角 (两段黑线分离, 中间有白隔开 → 状态机: 直行→旋转)
    // 同步固件V4.0+: 锐角检测到不转弯, 直行穿过分叉, dev=0
    int blackCnt = 0, lPos = -1, rPos = 0;
    for (int i = 0; i < 8; i++) {
      if (!w[i]) {
        blackCnt++;
        if (lPos < 0) lPos = i;
        rPos = i;
      }
    }
    if (blackCnt >= 2) {
      final gap = (rPos - lPos) - (blackCnt - 1);
      if (gap >= 1) {
        int leftB = 0, rightB = 0;
        for (int i = 0; i < 4; i++) {
          if (!w[i]) leftB++;
        }
        for (int i = 4; i < 8; i++) {
          if (!w[i]) rightB++;
        }
        final label =
            (leftB > rightB)
                ? '左锐角'
                : (rightB > leftB)
                ? '右锐角'
                : (lPos < 2 ? '左锐角' : '右锐角');
        return (0, label); // 固件直行穿过分叉, dev=0
      }
    }

    // P2 直角 ±4.5
    if ((!w[0] || !w[1]) && w[7]) return (-4.5, '左直角');
    if ((!w[6] || !w[7]) && w[0]) return (4.5, '右直角');

    // P3 中四黑
    if (w[0] && !w[2] && !w[3] && !w[4] && w[7]) return (0, '直行');

    // P4 质心法 (替代旧微偏8种+居中+加权和)
    // 计算黑色探头的位置重心, 中心=3.5, 缩放=1.0
    // 线宽2~3探头: 居中时 centroid≈3.5→dev≈0, 最小偏离0.5(移动一个探头)
    // 死区 |dev|<0.20 → 直行, 消除微小抖动
    int sumPos = 0, cnt = 0;
    for (int i = 0; i < 8; i++) {
      if (!w[i]) {
        sumPos += i;
        cnt++;
      }
    }
    if (cnt > 0) {
      final centroid = sumPos / cnt;
      final rawDev = (centroid - 3.5) * 1.0;
      if (rawDev > -0.20 && rawDev < 0.20) return (0, '直行');
      if (rawDev > 0) return (rawDev, '偏右');
      return (rawDev, '偏左');
    }
    return (0, '直行');
  }

  // V3.5 旧版4路状态推断 (保留兼容)
  String _sensorState4(String s) {
    if (s.length < 6) return '--';
    bool b(int i) => s[i] == '1';
    bool w(int i) => s[i] == '0';
    final bl = b(0),
        b1 = b(1),
        b2 = b(2),
        br = b(3),
        bt = s[4] == '0',
        btr = s[5] == '0';
    final wl = w(0), w1 = w(1), w2 = w(2), wr = w(3);
    if (bt && btr) return '避障停车';
    if (bl && b1 && b2 && br) return '十字';
    if (wl && w1 && w2 && wr) return '丢线';
    if (bl && b1 && b2 && wr) return '左直角';
    if (wl && b1 && b2 && br) return '右直角';
    if (bl && b1 && w2) return '左弧弯';
    if (w1 && b2 && br) return '右弧弯';
    if (wl && b1 && b2 && wr) return '直行';
    if (bl || b1) return '偏右';
    if (b2 || br) return '偏左';
    return '直行';
  }

  void _parseConfig(String cfg) {
    final parts = cfg.split(',');
    for (final p in parts) {
      if (p.startsWith('KP')) {
        _kp = double.tryParse(p.substring(2)) ?? _kp;
      } else if (p.startsWith('KI')) {
        _ki = double.tryParse(p.substring(2)) ?? _ki;
      } else if (p.startsWith('KD')) {
        _kd = double.tryParse(p.substring(2)) ?? _kd;
      } else if (p.startsWith('SP')) {
        _sp = int.tryParse(p.substring(2)) ?? _sp;
      } else if (p.startsWith('O')) {
        _obstacleEnable = int.tryParse(p.substring(1)) ?? _obstacleEnable;
      } else if (p.startsWith('TIN')) {
        _turnInner = int.tryParse(p.substring(3)) ?? _turnInner;
      } else if (p.startsWith('TOUT')) {
        _turnOuter = int.tryParse(p.substring(4)) ?? _turnOuter;
      } else if (p.startsWith('RSP')) {
        _sharpRotateSpeed = int.tryParse(p.substring(3)) ?? _sharpRotateSpeed;
      } else if (p.startsWith('TL')) {
        _turnLock = int.tryParse(p.substring(2)) ?? _turnLock;
      } else if (p.startsWith('CH')) {
        _pulseChunk = int.tryParse(p.substring(2)) ?? _pulseChunk;
      } else if (p.startsWith('LOG')) {
        _logEnabled = int.tryParse(p.substring(3)) ?? _logEnabled;
      } else if (p.startsWith('SENS')) {
        _sensorEnabled = int.tryParse(p.substring(4)) ?? _sensorEnabled;
      } else if (p.startsWith('TRC')) {
        _traceEnabled = int.tryParse(p.substring(3)) ?? _traceEnabled;
      } else if (p.startsWith('VER')) {
        final ver = p.substring(4);
        if (ver.isNotEmpty && ver != fw) fw = ver;
      }
    }
    _cfgReceived = true;
    _log(
      '配置同步: 固件=$fw KP=$_kp KI=$_ki KD=$_kd SP=$_sp 内轮=$_turnInner 外轮=$_turnOuter 转弯速度=$_sharpRotateSpeed 锁存=$_turnLock',
    );
    notifyListeners();
  }

  void requestConfig() {
    sendCmd('SET?');
  }

  void _handleRawData(String raw) {
    for (final line in raw.split('\n')) {
      final d = line.replaceAll('\r', '').trim();
      if (d.isEmpty) continue;
      if (d == 'HB=0') {
        lwRunning = false;
        notifyListeners();
        continue;
      }
      if (d == 'HB=1') {
        lwRunning = true;
        notifyListeners();
        continue;
      }
      if (d.startsWith('S=')) {
        final v = d.substring(2);
        final parts = v.split('|');
        // V4.6.0 新格式: S=1110011111|STATE|LOCK|L|R
        if (parts.length >= 3) {
          final rawState = parts[1];
          _sensors = parts[0];
          _logState = rawState;
          final dev = _computeDevFromSensors(_sensors);
          final sign = dev >= 0 ? '+' : '';
          final cn = _stCN[rawState] ?? rawState;
          final lockVal = parts.length >= 3 ? (int.tryParse(parts[2]) ?? 0) : 0;
          String lockTag = '';
          if (lockVal == 1)
            lockTag = ' 锁';
          else if (lockVal == 2)
            lockTag = '·到期';
          else if (lockVal == 3)
            lockTag = '·破锁';
          _logRcv(
            'S=${_sensors.length >= 8 ? _sensors.substring(0, 8) : _sensors}  $cn$lockTag  L=${parts.length >= 4 ? parts[3] : "?"} R=${parts.length >= 5 ? parts[4] : "?"}',
          );
        } else if (v.length >= 10) {
          // 兼容旧固件10位格式
          final newSensors = v.substring(0, 10);
          if (newSensors != _sensors) {
            _sensors = newSensors;
            final (dev, state) = _computeDeviation(_sensors);
            if (state != _logState) _logState = state;
            final corr = _kp * dev;
            final int l = (_sp + corr).round().clamp(-99, 99);
            final int r = (_sp - corr).round().clamp(-99, 99);
            final signD = dev >= 0 ? '+' : '';
            final signC = corr >= 0 ? '+' : '';
            _logRcv(
              '$d  $state  dev=$signD${dev.toStringAsFixed(2)} corr=$signC${corr.toStringAsFixed(1)} L=$l R=$r',
            );
          }
        } else if (v.length >= 6) {
          final newSensors = v.substring(0, 6);
          if (newSensors != _sensors) {
            _sensors = newSensors;
            final newState = _sensorState4(_sensors);
            if (newState != _logState) _logState = newState;
            _logRcv('$d  $newState');
          }
        }
        notifyListeners();
        continue;
      }
      if (d.startsWith('SPD=')) {
        final m = RegExp(
          r'SPD=L(\d+),(\d+),(\d+),R(\d+),(\d+),(\d+)',
        ).firstMatch(d);
        if (m != null) {
          _leftSpeed = int.tryParse(m.group(1)!) ?? 0;
          _leftDtMs = int.tryParse(m.group(2)!) ?? 0;
          _leftMileage = (int.tryParse(m.group(3)!) ?? 0).toDouble();
          _rightSpeed = int.tryParse(m.group(4)!) ?? 0;
          _rightDtMs = int.tryParse(m.group(5)!) ?? 0;
          _rightMileage = (int.tryParse(m.group(6)!) ?? 0).toDouble();
        }
        notifyListeners();
        continue;
      }
      if (d == 'STA=1') {
        lwRunning = true;
        _log('模式=开');
        notifyListeners();
        continue;
      }
      if (d == 'STA=0') {
        lwRunning = false;
        _log('模式=关');
        notifyListeners();
        continue;
      }
      final em = RegExp(r'E=([+\-\d.]+),C=([+\-\d.]+)').firstMatch(d);
      if (em != null) {
        _error = double.tryParse(em.group(1)!) ?? 0;
        _correction = double.tryParse(em.group(2)!) ?? 0;
        notifyListeners();
        continue;
      }
      _logRcv(d);
      if (d.startsWith('VER=')) {
        fw = d.substring(4);
        _log('固件: $fw');
        continue;
      }
      if (d.startsWith('SET=')) {
        _parseConfig(d.substring(4));
        continue;
      }
      if (d.startsWith('T=')) {
        _lapTime = d.substring(2);
        _log('圈速=${d.substring(2)}');
        notifyListeners();
        continue;
      }
      notifyListeners();
    }
  }

  @override
  void dispose() {
    _channel.invokeMethod('disconnect');
    super.dispose();
  }
}
