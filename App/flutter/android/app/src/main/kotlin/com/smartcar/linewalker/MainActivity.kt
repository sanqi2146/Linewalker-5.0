package com.smartcar.linewalker

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

class MainActivity : FlutterActivity() {
    companion object {
        private const val CHANNEL = "com.smartcar.linewalker/bt"
        private val SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
        private const val TAG = "LW-BT"
        private const val REQ_BT = 1001
    }

    private var socket: BluetoothSocket? = null
    private var outStream: OutputStream? = null
    private var inStream: InputStream? = null
    @Volatile private var running = false
    private var channel: MethodChannel? = null
    private var pendingResult: MethodChannel.Result? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    private val discoveredDevices = mutableMapOf<String, String>()
    private var discoveryReceiver: BroadcastReceiver? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        Log.d(TAG, "init")
        channel = MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL)
        channel!!.setMethodCallHandler { call, result ->
            when (call.method) {
                "scan" -> {
                    if (!hasBtPermission()) { pendingResult = result; requestBtPermission(); return@setMethodCallHandler }
                    doScan(result)
                }
                "connect" -> {
                    val name = call.argument<String>("name") ?: ""
                    if (!hasBtPermission()) { result.error("BT", "No permission", null); return@setMethodCallHandler }
                    Thread { doConnect(name, result) }.start()
                }
                "send" -> {
                    val data = call.argument<String>("data") ?: ""
                    Log.d(TAG, "send: ${data.replace("\r","\\r").replace("\n","\\n")}")
                    try { outStream?.write(data.toByteArray()); outStream?.flush(); Log.d(TAG, "send OK") } catch (e: Exception) { Log.e(TAG, "send err", e) }
                    result.success(null)
                }
                "disconnect" -> { disconnect(); result.success(null) }
                else -> result.notImplemented()
            }
        }
    }

    private fun hasBtPermission() =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        else true

    private fun requestBtPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN), REQ_BT)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQ_BT) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) pendingResult?.let { doScan(it) }
            else pendingResult?.error("BT", "Permission denied", null)
            pendingResult = null
        }
    }

    @SuppressLint("MissingPermission")
    private fun doScan(result: MethodChannel.Result) {
        val adapter = BluetoothAdapter.getDefaultAdapter()
        if (adapter == null) { result.success(emptyList<String>()); return }

        discoveredDevices.clear()
        for (d in adapter.bondedDevices) {
            val nm = d.name ?: d.address
            discoveredDevices[nm] = d.address
            Log.d(TAG, "bonded: $nm (${d.address})")
        }

        // 立即检查是否有HC-05, 有就直接返回(不等待完整发现)
        val hc05 = discoveredDevices.entries.firstOrNull {
            it.key.contains("HC-05", ignoreCase = true) || it.key.contains("HC05", ignoreCase = true)
        }
        if (hc05 != null) {
            Log.d(TAG, "HC-05 found in bonded, return immediately")
            val list = discoveredDevices.map { "${it.key}||${it.value}" }
            result.success(list)
            return
        }

        val filter = IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
        }
        discoveryReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                when (intent?.action) {
                    BluetoothDevice.ACTION_FOUND -> {
                        val dev: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                        dev?.let {
                            val mac = it.address ?: return
                            val nm = it.name ?: mac
                            if (!discoveredDevices.containsKey(nm)) {
                                discoveredDevices[nm] = mac
                                Log.d(TAG, "found: $nm ($mac)")
                                // 发现HC-05立即停止并返回
                                if (nm.contains("HC-05", ignoreCase = true) || nm.contains("HC05", ignoreCase = true)) {
                                    stopDiscovery()
                                    val list = discoveredDevices.map { "${it.key}||${it.value}" }
                                    Log.d(TAG, "HC-05 discovered, return now")
                                    mainHandler.post { result.success(list) }
                                }
                            }
                        }
                    }
                    BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                        stopDiscovery()
                        val list = discoveredDevices.map { "${it.key}||${it.value}" }
                        Log.d(TAG, "done, total=${list.size}")
                        mainHandler.post { result.success(list) }
                    }
                }
            }
        }
        registerReceiver(discoveryReceiver, filter)
        if (adapter.isDiscovering) adapter.cancelDiscovery()
        adapter.startDiscovery()

        mainHandler.postDelayed({
            if (discoveryReceiver != null) {
                stopDiscovery()
                val list = discoveredDevices.map { "${it.key}||${it.value}" }
                result.success(list)
            }
        }, 15000)
    }

    @SuppressLint("MissingPermission")
    private fun stopDiscovery() {
        try { discoveryReceiver?.let { unregisterReceiver(it) } } catch (_: Exception) {}
        discoveryReceiver = null
        try { BluetoothAdapter.getDefaultAdapter()?.cancelDiscovery() } catch (_: Exception) {}
    }

    @SuppressLint("MissingPermission")
    private fun doConnect(input: String, result: MethodChannel.Result) {
        try {
            val adapter = BluetoothAdapter.getDefaultAdapter()
            if (adapter == null) { result.error("BT", "No adapter", null); return }
            cancelDiscoverySafe()
            disconnect()

            val dev = findDevice(adapter, input)
            if (dev == null) {
                result.error("BT", "未找到: $input", null)
                return
            }

            Log.d(TAG, "Connecting ${dev.name ?: dev.address} (${dev.address})")

            // 方法1: 标准 SPP UUID 连接 (推荐, 会做SDP查询)
            var sock: BluetoothSocket? = null
            try {
                sock = dev.createRfcommSocketToServiceRecord(SPP_UUID)
                adapter.cancelDiscovery()
                sock?.connect()
                Log.d(TAG, "SPP socket connected OK")
            } catch (e1: Exception) {
                Log.w(TAG, "SPP failed: ${e1.message}, trying reflection channel 1")
                // 方法2: 反射直连 channel 1 (绕过SDP, 适用于部分固件)
                try {
                    val m = dev.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType)
                    sock = m.invoke(dev, 1) as BluetoothSocket
                    adapter.cancelDiscovery()
                    sock?.connect()
                    Log.d(TAG, "Reflection socket connected (channel 1)")
                } catch (e2: Exception) {
                    Log.e(TAG, "Both methods failed: ${e2.message}")
                    mainHandler.post { result.error("BT", "无法连接: ${e2.message}\n请确认HC-05指示灯是双闪(已连接+透传模式)而非慢闪(AT模式)", null) }
                    return
                }
            }

            socket = sock
            outStream = socket?.outputStream
            inStream = socket?.inputStream
            running = true
            startReadLoop()
            Log.d(TAG, "Connected OK")
            mainHandler.post { result.success(true) }
        } catch (e: Exception) {
            Log.e(TAG, "connect err: ${e.message}")
            mainHandler.post { result.error("BT", "连接失败: ${e.message}\n请尝试在手机蓝牙设置中取消配对HC-05后重新配对(密码1234)", null) }
        }
    }

    @SuppressLint("MissingPermission")
    private fun findDevice(adapter: BluetoothAdapter, input: String): BluetoothDevice? {
        val trimmed = input.trim()

        if (trimmed.matches(Regex("[0-9A-Fa-f:]{17}"))) {
            val mac = trimmed.uppercase()
            adapter.bondedDevices.firstOrNull { it.address.equals(mac, ignoreCase = true) }?.let { return it }
            discoveredDevices.entries.firstOrNull { it.value.equals(mac, ignoreCase = true) }?.let {
                return adapter.getRemoteDevice(it.value)
            }
            try { return adapter.getRemoteDevice(mac) } catch (_: Exception) {}
        }

        if (trimmed.contains("||"))
            return findDevice(adapter, trimmed.substringAfterLast("||").trim())

        return adapter.bondedDevices.firstOrNull {
            (it.name ?: "").contains(trimmed, ignoreCase = true)
        }
    }

    private fun cancelDiscoverySafe() {
        try { BluetoothAdapter.getDefaultAdapter()?.cancelDiscovery() } catch (_: Exception) {}
    }

    private fun disconnect() {
        running = false
        try { inStream?.close() } catch (_: Exception) {}
        try { outStream?.close() } catch (_: Exception) {}
        try { socket?.close() } catch (_: Exception) {}
        inStream = null; outStream = null; socket = null
    }

    private val lineBuf = StringBuilder()
    private fun startReadLoop() {
        lineBuf.clear()
        Thread {
            val buf = ByteArray(256)
            try {
                while (running) {
                    val len = inStream?.read(buf) ?: -1
                    if (len > 0) {
                        lineBuf.append(String(buf, 0, len, Charsets.UTF_8))
                        while (true) {
                            val crlf = lineBuf.indexOf("\r\n")
                            val lf = lineBuf.indexOf("\n")
                            val ix = when { crlf >= 0 && lf >= 0 -> minOf(crlf, lf); crlf >= 0 -> crlf; lf >= 0 -> lf; else -> -1 }
                            if (ix < 0) break
                            val skip = if (crlf >= 0 && lf == crlf + 1) 2 else 1
                            val line = lineBuf.substring(0, ix).trim()
                            lineBuf.delete(0, ix + skip)
                            if (line.isNotEmpty()) {
                                val safe = line.replace("\\", "\\\\").replace("'", "\\'")
                                Log.d(TAG, "recv: $line")
                                mainHandler.post { channel?.invokeMethod("onData", safe) }
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                if (running) { Log.e(TAG, "read err: ${e.message}"); mainHandler.post { channel?.invokeMethod("onDisconnected", null) } }
            }
        }.apply { start() }
    }

    override fun onDestroy() { stopDiscovery(); disconnect(); super.onDestroy() }
}
