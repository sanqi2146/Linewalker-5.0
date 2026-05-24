package com.smartcar.linewalker;

import android.app.Activity;
import android.app.WallpaperManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.util.Log;
import android.webkit.JavascriptInterface;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;
import org.json.JSONArray;
import org.json.JSONException;

/**
 * BTBridge — 安卓蓝牙SPP桥接 + 壁纸取色，暴露给WebView的JS调用
 */
public class BTBridge {

    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private static final String TAG = "BTBridge";

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private BluetoothSocket socket;
    private OutputStream outStream;
    private InputStream inStream;
    private volatile boolean running = false;
    private volatile boolean connecting = false;
    private Activity activity;
    private android.webkit.WebView webView;

    public BTBridge(android.webkit.WebView wv, Activity act) {
        this.webView = wv;
        this.activity = act;
    }

    // ==================== JS 蓝牙接口 ====================

    @JavascriptInterface
    public void send(String data) {
        OutputStream os = outStream; // 本地快照，防止被disconnect置null
        if (os == null) return;
        try {
            os.write(data.getBytes("UTF-8"));
            os.flush();
        } catch (Exception e) {
            Log.e(TAG, "send error", e);
        }
    }

    @JavascriptInterface
    public synchronized void connect(String deviceName) {
        if (connecting) { evalJS("log('正在连接中...','info')"); return; }
        if (running) { disconnect(); } // 重连先断旧连接
        connecting = true;
        new Thread(() -> doConnect(deviceName)).start();
    }

    @JavascriptInterface
    public synchronized void disconnect() {
        connecting = false;
        running = false;
        closeSocket();
        evalJS("btConnected=false; if(window.onBtDisconnected)window.onBtDisconnected();");
    }

    @JavascriptInterface
    public String scan() {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) return "[]";
        Set<BluetoothDevice> devices = adapter.getBondedDevices();
        JSONArray arr = new JSONArray();
        for (BluetoothDevice d : devices) {
            String name = d.getName();
            arr.put(name != null ? name : "");
        }
        return arr.toString();
    }

    // ==================== JS 壁纸取色接口 ====================

    /**
     * 获取系统壁纸的主色调（Material You 取色）
     * 调用后通过 window.onWallpaperColor(hex) 回调给 JS
     * 需要 API 27+ (Android 8.1+), 否则回退到默认 #6750A4
     */
    @JavascriptInterface
    public void getWallpaperColor() {
        mainHandler.post(() -> {
            String colorHex = "#6750A4"; // 默认紫色
            try {
                if (activity == null) {
                    evalJS("if(window.onWallpaperColor)window.onWallpaperColor('#6750A4')");
                    return;
                }
                WallpaperManager wm = WallpaperManager.getInstance(activity);
                if (wm == null) {
                    evalJS("if(window.onWallpaperColor)window.onWallpaperColor('#6750A4')");
                    return;
                }
                // API 27+ WallpaperColors
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
                    android.app.WallpaperColors colors = wm.getWallpaperColors(
                        WallpaperManager.FLAG_SYSTEM);
                    if (colors != null) {
                        Color c = colors.getPrimaryColor();
                        if (c != null) {
                            int argb = c.toArgb();
                            colorHex = String.format("#%06X", (0xFFFFFF & argb));
                        }
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "wallpaper color error", e);
            }
            evalJS("if(window.onWallpaperColor)window.onWallpaperColor('" + colorHex + "')");
        });
    }

    // ==================== JS 振动接口 ====================

    @JavascriptInterface
    public void vibrate(int durationMs) {
        try {
            if (activity == null) return;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                VibratorManager vm = (VibratorManager) activity.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                if (vm != null) {
                    Vibrator v = vm.getDefaultVibrator();
                    if (v != null) {
                        v.vibrate(VibrationEffect.createOneShot(durationMs, VibrationEffect.DEFAULT_AMPLITUDE));
                    }
                }
            } else {
                Vibrator v = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
                if (v != null) {
                    v.vibrate(VibrationEffect.createOneShot(durationMs, VibrationEffect.DEFAULT_AMPLITUDE));
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "vibrate error", e);
        }
    }

    @JavascriptInterface
    public void vibratePattern(String pattern) {
        try {
            if (activity == null) return;
            String[] parts = pattern.split(",");
            long[] timings = new long[parts.length];
            int[] amplitudes = new int[parts.length];
            for (int i = 0; i < parts.length; i++) {
                timings[i] = Long.parseLong(parts[i].trim());
                amplitudes[i] = (i % 2 == 0) ? VibrationEffect.DEFAULT_AMPLITUDE : 0;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                VibratorManager vm = (VibratorManager) activity.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                if (vm != null) {
                    Vibrator v = vm.getDefaultVibrator();
                    if (v != null) {
                        v.vibrate(VibrationEffect.createWaveform(timings, amplitudes, -1));
                    }
                }
            } else {
                Vibrator v = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
                if (v != null) {
                    v.vibrate(VibrationEffect.createWaveform(timings, amplitudes, -1));
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "vibratePattern error", e);
        }
    }

    @JavascriptInterface
    public void hapticClick() {
        try {
            if (activity == null) return;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                VibrationEffect effect = VibrationEffect.createPredefined(VibrationEffect.EFFECT_CLICK);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    VibratorManager vm = (VibratorManager) activity.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                    if (vm != null) {
                        Vibrator v = vm.getDefaultVibrator();
                        if (v != null && v.hasAmplitudeControl()) {
                            v.vibrate(effect);
                            return;
                        }
                    }
                }
                Vibrator v = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
                if (v != null) v.vibrate(effect);
            } else {
                vibrate(10);
            }
        } catch (Exception e) {
            Log.e(TAG, "hapticClick error", e);
        }
    }

    @JavascriptInterface
    public void hapticHeavy() {
        try {
            if (activity == null) return;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                VibrationEffect effect = VibrationEffect.createPredefined(VibrationEffect.EFFECT_HEAVY_CLICK);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    VibratorManager vm = (VibratorManager) activity.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                    if (vm != null) {
                        Vibrator v = vm.getDefaultVibrator();
                        if (v != null && v.hasAmplitudeControl()) {
                            v.vibrate(effect);
                            return;
                        }
                    }
                }
                Vibrator v = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
                if (v != null) v.vibrate(effect);
            } else {
                vibrate(200);
            }
        } catch (Exception e) {
            Log.e(TAG, "hapticHeavy error", e);
        }
    }

    // ==================== 内部 ====================

    private void doConnect(String deviceName) {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) { evalJS("log('蓝牙不可用','')"); return; }

        BluetoothDevice target = null;
        for (BluetoothDevice d : adapter.getBondedDevices()) {
            if (d.getName() != null && d.getName().contains(deviceName)) {
                target = d; break;
            }
        }
        if (target == null) { evalJS("log('未找到设备: "+deviceName+"','')"); return; }

        try {
            socket = target.createRfcommSocketToServiceRecord(SPP_UUID);
            adapter.cancelDiscovery();
            socket.connect();
            outStream = socket.getOutputStream();
            inStream  = socket.getInputStream();
            running   = true;
            evalJS("btConnected=true; if(window.onBtConnected)window.onBtConnected();");
            readLoop();
        } catch (Exception e) {
            Log.e(TAG, "connect failed", e);
            evalJS("log('连接失败: "+e.getMessage()+"','')");
            closeSocket();
        } finally {
            connecting = false;
        }
    }

    private void readLoop() {
        new Thread(() -> {
            byte[] buf = new byte[256];
            int len;
            try {
                while (running && inStream != null) {
                    len = inStream.read(buf);
                    if (len > 0) {
                        String data = new String(buf, 0, len, "UTF-8");
                        final String safe = data.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\r", "\\r");
                        mainHandler.post(() -> evalJS("if(window.onBtData)window.onBtData('"+safe+"')"));
                    }
                }
            } catch (Exception e) {
                if (running) {
                    Log.e(TAG, "read error", e);
                    mainHandler.post(() -> { running = false; connecting = false; evalJS("btConnected=false; if(window.onBtDisconnected)window.onBtDisconnected();"); });
                }
            }
        }).start();
    }

    private void closeSocket() {
        try { if (inStream != null) { inStream.close(); inStream = null; } } catch (Exception e) {}
        try { if (outStream != null) { outStream.close(); outStream = null; } } catch (Exception e) {}
        try { if (socket != null) { socket.close(); socket = null; } } catch (Exception e) {}
    }

    public void destroy() {
        disconnect();
        activity = null;
        webView = null;
    }

    private void evalJS(String code) {
        if (webView != null) {
            webView.post(() -> {
                if (webView != null) webView.evaluateJavascript(code, null);
            });
        }
    }
}


