package expo.modules.beeline

import android.content.Intent
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import org.json.JSONObject

class BeeLineNotificationListenerService : NotificationListenerService() {
  companion object {
    const val ACTION_NOTIFICATION_UPDATED = "expo.modules.beeline.MAPS_NOTIFICATION_UPDATED"
    const val EXTRA_NOTIFICATION = "notification"
    private const val PREFS_NAME = "beeline_notifications"
    private const val LATEST_NOTIFICATION_KEY = "latest_maps_notification"
    private const val MAPS_PACKAGE = "com.google.android.apps.maps"
  }

  override fun onListenerConnected() {
    super.onListenerConnected()
    refreshLatestMapsNotification()
  }

  override fun onListenerDisconnected() {
    clearLatest()
    super.onListenerDisconnected()
  }

  override fun onNotificationPosted(sbn: StatusBarNotification) {
    if (sbn.packageName != MAPS_PACKAGE) return
    saveMapsNotification(sbn)
  }

  override fun onNotificationRemoved(sbn: StatusBarNotification) {
    if (sbn.packageName != MAPS_PACKAGE) return
    val preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
    val current = preferences.getString(LATEST_NOTIFICATION_KEY, null)
    val removedCurrent = try {
      current != null && JSONObject(current).optString("notificationKey") == sbn.key
    } catch (_: Exception) {
      false
    }
    if (!removedCurrent) return

    val replacement = activeMapsNotifications()
      .filterNot { it.key == sbn.key }
      .maxByOrNull { it.postTime }
    if (replacement != null) saveMapsNotification(replacement)
    else clearLatest()
  }

  private fun refreshLatestMapsNotification() {
    val latest = activeMapsNotifications().maxByOrNull { it.postTime }
    val preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
    if (latest == null) {
      clearLatest()
    } else {
      saveMapsNotification(latest)
    }
  }

  private fun clearLatest() {
    getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().remove(LATEST_NOTIFICATION_KEY).apply()
    // Explicit lifecycle event: clients must not retain a previous route summary.
    sendBroadcast(Intent(ACTION_NOTIFICATION_UPDATED).setPackage(packageName)
      .putExtra(EXTRA_NOTIFICATION, "{}"))
  }

  private fun activeMapsNotifications(): List<StatusBarNotification> {
    return try {
      activeNotifications
        ?.filter { it.packageName == MAPS_PACKAGE }
        .orEmpty()
    } catch (error: SecurityException) {
      Log.w("BeeLineNotifications", "Não foi possível consultar notificações ativas. Confira o acesso às notificações.", error)
      emptyList()
    }
  }

  private fun saveMapsNotification(sbn: StatusBarNotification) {
    val extras = sbn.notification.extras
    val lines = extras.getCharSequenceArray(android.app.Notification.EXTRA_TEXT_LINES)
      ?.joinToString(" · ") { it.toString() }
      .orEmpty()
    val notification = JSONObject().apply {
      put("packageName", sbn.packageName)
      put("notificationKey", sbn.key)
      put("title", extras.getCharSequence(android.app.Notification.EXTRA_TITLE)?.toString().orEmpty())
      put("text", extras.getCharSequence(android.app.Notification.EXTRA_TEXT)?.toString().orEmpty())
      put("bigText", extras.getCharSequence(android.app.Notification.EXTRA_BIG_TEXT)?.toString().orEmpty())
      put("subText", extras.getCharSequence(android.app.Notification.EXTRA_SUB_TEXT)?.toString().orEmpty())
      put("textLines", lines)
      put("postedAt", sbn.postTime)
    }
    val serialized = notification.toString()
    getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
      .edit()
      .putString(LATEST_NOTIFICATION_KEY, serialized)
      .apply()
    sendBroadcast(
      Intent(ACTION_NOTIFICATION_UPDATED)
        .setPackage(packageName)
        .putExtra(EXTRA_NOTIFICATION, serialized),
    )
  }
}
