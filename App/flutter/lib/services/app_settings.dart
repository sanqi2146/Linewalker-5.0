import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:dynamic_color/dynamic_color.dart';

class AppSettings extends ChangeNotifier {
  ThemeMode _themeMode = ThemeMode.system;
  bool _useWallpaperColor = true;
  Color _seedColor = const Color(0xFF6750A4);
  int _vibStrength = 80;

  ThemeMode get themeMode => _themeMode;
  bool get useWallpaperColor => _useWallpaperColor;
  Color get seedColor => _seedColor;
  int get vibStrength => _vibStrength;

  Future<void> load() async {
    final prefs = await SharedPreferences.getInstance();
    _themeMode = ThemeMode.values[prefs.getInt('themeMode') ?? 0];
    _useWallpaperColor = prefs.getBool('useWallpaperColor') ?? true;
    _seedColor = Color(prefs.getInt('seedColor') ?? 0xFF6750A4);
    _vibStrength = prefs.getInt('vibStrength') ?? 80;
    notifyListeners();
  }

  Future<void> setThemeMode(ThemeMode m) async {
    _themeMode = m;
    (await SharedPreferences.getInstance()).setInt('themeMode', m.index);
    notifyListeners();
  }

  Future<void> setSeedColor(Color c) async {
    _seedColor = c;
    _useWallpaperColor = false;
    final prefs = await SharedPreferences.getInstance();
    prefs.setInt('seedColor', c.value);
    prefs.setBool('useWallpaperColor', false);
    notifyListeners();
  }

  Future<void> setWallpaperColor(bool v) async {
    _useWallpaperColor = v;
    (await SharedPreferences.getInstance()).setBool('useWallpaperColor', v);
    notifyListeners();
  }

  Future<void> setVibStrength(int v) async {
    _vibStrength = v.clamp(0, 100);
    (await SharedPreferences.getInstance()).setInt('vibStrength', _vibStrength);
    notifyListeners();
  }
}
