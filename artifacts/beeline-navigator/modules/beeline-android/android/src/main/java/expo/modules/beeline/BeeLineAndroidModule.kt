package expo.modules.beeline

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import androidx.core.content.ContextCompat
import expo.modules.kotlin.exception.Exceptions
import expo.modules.kotlin.functions.Coroutine
import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition
import kotlinx.coroutines.suspendCancellableCoroutine
import org.json.JSONObject
import java.util.UUID
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class BeeLineAndroidModule : Module() {
  companion object {
    private val SERVICE_UUID: UUID = UUID.fromString("7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001")
    private val COMMAND_UUID: UUID = UUID.fromString("7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001")
    private const val PREFS_NAME = "beeline_notifications"
    private const val LATEST_NOTIFICATION_KEY = "latest_maps_notification"
  }

  private val context: Context
    get() = appContext.reactContext?.applicationContext ?: throw Exceptions.ReactContextLost()

  private var bluetoothGatt: BluetoothGatt? = null
  private var commandCharacteristic: BluetoothGattCharacteristic? = null
  private var scanCallback: ScanCallback? = null
  private var pendingConnection: kotlinx.coroutines.CancellableContinuation<Unit>? = null
  private var pendingWrite: kotlinx.coroutines.CancellableContinuation<Unit>? = null
  private val mainHandler = Handler(Looper.getMainLooper())

  private val notificationReceiver = object : android.content.BroadcastReceiver() {
    override fun onReceive(context: Context?, intent: Intent?) {
      if (intent?.action != BeeLineNotificationListenerService.ACTION_NOTIFICATION_UPDATED) return
      val serialized = intent.getStringExtra(BeeLineNotificationListenerService.EXTRA_NOTIFICATION) ?: return
      val event = JSONObject(serialized)
      sendEvent(
        "onMapsNotification",
        mapOf(
          "packageName" to event.optString("packageName"),
          "title" to event.optString("title"),
          "text" to event.optString("text"),
          "bigText" to event.optString("bigText"),
          "subText" to event.optString("subText"),
          "textLines" to event.optString("textLines"),
          "postedAt" to event.optLong("postedAt"),
        ),
      )
    }
  }

  private val gattCallback = object : BluetoothGattCallback() {
    override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
      if (status != BluetoothGatt.GATT_SUCCESS) {
        emitConnection("error", "Falha na conexão BLE (código $status)")
        pendingConnection?.let {
          if (it.isActive) it.resumeWithException(IllegalStateException("Falha na conexão BLE (código $status)"))
        }
        pendingConnection = null
        gatt.close()
        bluetoothGatt = null
        commandCharacteristic = null
        return
      }
      if (newState == BluetoothProfile.STATE_CONNECTED) {
        bluetoothGatt = gatt
        emitConnection("connecting", "Conectado; localizando serviço BeeLine…")
        if (!gatt.requestMtu(247)) gatt.discoverServices()
      } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
        commandCharacteristic = null
        bluetoothGatt = null
        gatt.close()
        emitConnection("disconnected", "Desconectado")
      }
    }

    override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
      gatt.discoverServices()
    }

    override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
      if (status != BluetoothGatt.GATT_SUCCESS) {
        failPendingConnection("Não foi possível ler os serviços BLE.")
        return
      }
      val characteristic = gatt.getService(SERVICE_UUID)?.getCharacteristic(COMMAND_UUID)
      if (characteristic == null) {
        failPendingConnection("O serviço BeeLine não foi encontrado no ESP32.")
        return
      }
      commandCharacteristic = characteristic
      emitConnection("connected", "BeeLine-ESP32 conectado")
      pendingConnection?.let { if (it.isActive) it.resume(Unit) }
      pendingConnection = null
    }

    override fun onCharacteristicWrite(
      gatt: BluetoothGatt,
      characteristic: BluetoothGattCharacteristic,
      status: Int,
    ) {
      val continuation = pendingWrite ?: return
      pendingWrite = null
      if (continuation.isActive) {
        if (status == BluetoothGatt.GATT_SUCCESS) continuation.resume(Unit)
        else continuation.resumeWithException(IllegalStateException("O ESP32 recusou a escrita BLE (código $status)."))
      }
    }
  }

  override fun definition() = ModuleDefinition {
    Name("BeeLineAndroid")
    Events("onConnectionStateChange", "onMapsNotification")

    OnCreate {
      val filter = IntentFilter(BeeLineNotificationListenerService.ACTION_NOTIFICATION_UPDATED)
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        context.registerReceiver(notificationReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
      } else {
        @Suppress("DEPRECATION")
        context.registerReceiver(notificationReceiver, filter)
      }
    }

    OnDestroy {
      try {
        context.unregisterReceiver(notificationReceiver)
      } catch (_: IllegalArgumentException) {
        // The receiver may already have been removed with the Android process.
      }
      stopScan()
      bluetoothGatt?.disconnect()
      bluetoothGatt?.close()
      bluetoothGatt = null
      commandCharacteristic = null
    }

    AsyncFunction("isNotificationAccessEnabled") {
      val enabledListeners = Settings.Secure.getString(
        context.contentResolver,
        "enabled_notification_listeners",
      ).orEmpty()
      enabledListeners.contains(context.packageName)
    }

    AsyncFunction("openNotificationAccessSettings") {
      val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS).apply {
        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      }
      context.startActivity(intent)
    }

    AsyncFunction("getLatestNotification") {
      val serialized = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        .getString(LATEST_NOTIFICATION_KEY, null) ?: return@AsyncFunction null
      val notification = JSONObject(serialized)
      mapOf(
        "packageName" to notification.optString("packageName"),
        "title" to notification.optString("title"),
        "text" to notification.optString("text"),
        "bigText" to notification.optString("bigText"),
        "subText" to notification.optString("subText"),
        "textLines" to notification.optString("textLines"),
        "postedAt" to notification.optLong("postedAt"),
      )
    }

    AsyncFunction("scanAndConnect") Coroutine { ->
      scanAndConnect()
    }

    AsyncFunction("writeCommand") Coroutine { command: String ->
      writeCommand(command)
    }

    AsyncFunction("disconnect") {
      stopScan()
      bluetoothGatt?.disconnect()
      bluetoothGatt?.close()
      bluetoothGatt = null
      commandCharacteristic = null
      emitConnection("disconnected", "Desconectado")
    }
  }

  private fun emitConnection(state: String, message: String) {
    sendEvent("onConnectionStateChange", mapOf("state" to state, "message" to message))
  }

  private fun failPendingConnection(message: String) {
    stopScan()
    emitConnection("error", message)
    pendingConnection?.let { if (it.isActive) it.resumeWithException(IllegalStateException(message)) }
    pendingConnection = null
    bluetoothGatt?.close()
    bluetoothGatt = null
    commandCharacteristic = null
  }

  private fun requireBluetoothPermission(permission: String) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
      ContextCompat.checkSelfPermission(context, permission) != PackageManager.PERMISSION_GRANTED
    ) {
      throw SecurityException("Permissão Bluetooth necessária: $permission")
    }
  }

  private suspend fun scanAndConnect() = suspendCancellableCoroutine<Unit> { continuation ->
    if (commandCharacteristic != null) {
      continuation.resume(Unit)
      return@suspendCancellableCoroutine
    }
    if (pendingConnection?.isActive == true) {
      continuation.resumeWithException(IllegalStateException("Já existe uma busca ou conexão BeeLine em andamento."))
      return@suspendCancellableCoroutine
    }
    requireBluetoothPermission(Manifest.permission.BLUETOOTH_SCAN)
    requireBluetoothPermission(Manifest.permission.BLUETOOTH_CONNECT)
    val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
      ?: run {
        continuation.resumeWithException(IllegalStateException("Este telefone não oferece Bluetooth."))
        return@suspendCancellableCoroutine
      }
    if (!adapter.isEnabled) {
      continuation.resumeWithException(IllegalStateException("Ative o Bluetooth do telefone e tente novamente."))
      return@suspendCancellableCoroutine
    }
    val scanner = adapter.bluetoothLeScanner
    if (scanner == null) {
      continuation.resumeWithException(IllegalStateException("O scanner BLE não está disponível."))
      return@suspendCancellableCoroutine
    }

    emitConnection("searching", "Procurando BeeLine-ESP32…")
    pendingConnection = continuation
    var deviceFound = false
    val callback = object : ScanCallback() {
      override fun onScanResult(callbackType: Int, result: ScanResult) {
        if (scanCallback !== this || pendingConnection !== continuation) return
        val advertisedService = result.scanRecord?.serviceUuids?.any { it.uuid == SERVICE_UUID } == true
        val advertisedName = result.scanRecord?.deviceName
        val deviceName = advertisedName ?: try {
          result.device.name
        } catch (_: SecurityException) {
          null
        }
        if (!advertisedService && deviceName != "BeeLine-ESP32") return
        deviceFound = true
        stopScan()
        emitConnection("connecting", "Conectando à placa…")
        try {
          bluetoothGatt = result.device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
          if (bluetoothGatt == null) {
            failPendingConnection("A placa foi encontrada, mas o Android não iniciou a conexão BLE.")
          }
        } catch (error: SecurityException) {
          failPendingConnection("Permissão Bluetooth ausente.")
        } catch (error: Exception) {
          failPendingConnection(error.message ?: "Não foi possível iniciar a conexão BLE.")
        }
      }

      override fun onScanFailed(errorCode: Int) {
        if (scanCallback === this && pendingConnection === continuation) {
          failPendingConnection("A busca BLE falhou (código $errorCode).")
        }
      }
    }
    scanCallback = callback
    continuation.invokeOnCancellation {
      if (pendingConnection === continuation) {
        stopScan()
        pendingConnection = null
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        commandCharacteristic = null
      }
    }
    try {
      scanner.startScan(
        // Do not discard devices before the name/UUID checks in onScanResult.
        null,
        ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
        callback,
      )
      mainHandler.postDelayed({
        if (pendingConnection === continuation) {
          stopScan()
          failPendingConnection(
            if (deviceFound) "Placa encontrada, mas a conexão ou descoberta dos serviços BLE expirou."
            else "Nenhum anúncio BeeLine encontrado. Confira se a placa está ligada e anunciando.",
          )
        }
      }, 15_000)
    } catch (error: Exception) {
      failPendingConnection(error.message ?: "Não foi possível iniciar a busca BLE.")
    }
  }

  private fun stopScan() {
    val callback = scanCallback ?: return
    try {
      val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
      adapter?.bluetoothLeScanner?.stopScan(callback)
    } catch (_: SecurityException) {
      // Permissions may have been revoked while a scan was running.
    }
    scanCallback = null
  }

  private suspend fun writeCommand(command: String) = suspendCancellableCoroutine<Unit> { continuation ->
    requireBluetoothPermission(Manifest.permission.BLUETOOTH_CONNECT)
    val gatt = bluetoothGatt
    val characteristic = commandCharacteristic
    if (gatt == null || characteristic == null) {
      continuation.resumeWithException(IllegalStateException("Conecte ao BeeLine-ESP32 antes de enviar dados."))
      return@suspendCancellableCoroutine
    }
    val bytes = command.toByteArray(Charsets.UTF_8)
    if (bytes.isEmpty() || bytes.size > 180) {
      continuation.resumeWithException(IllegalArgumentException("A mensagem BLE deve ter entre 1 e 180 bytes."))
      return@suspendCancellableCoroutine
    }
    if (pendingWrite != null) {
      continuation.resumeWithException(IllegalStateException("Outra escrita BLE está em andamento."))
      return@suspendCancellableCoroutine
    }
    pendingWrite = continuation
    continuation.invokeOnCancellation { pendingWrite = null }
    try {
      val accepted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        gatt.writeCharacteristic(
          characteristic,
          bytes,
          BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
        ) == BluetoothStatusCodes.SUCCESS
      } else {
        @Suppress("DEPRECATION")
        characteristic.setValue(bytes)
        @Suppress("DEPRECATION")
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        @Suppress("DEPRECATION")
        gatt.writeCharacteristic(characteristic)
      }
      if (!accepted) {
        pendingWrite = null
        continuation.resumeWithException(IllegalStateException("Não foi possível iniciar a escrita BLE."))
      }
    } catch (error: Exception) {
      pendingWrite = null
      continuation.resumeWithException(error)
    }
  }
}
