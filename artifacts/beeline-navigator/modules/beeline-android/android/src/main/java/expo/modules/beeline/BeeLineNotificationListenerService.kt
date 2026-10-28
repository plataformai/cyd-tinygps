package expo.modules.beeline

import android.content.Intent
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import org.json.JSONArray
import org.json.JSONObject

class BeeLineNotificationListenerService : NotificationListenerService() {
  companion object {
    const val ACTION_NOTIFICATION_UPDATED = "expo.modules.beeline.MAPS_NOTIFICATION_UPDATED"
    const val EXTRA_NOTIFICATION = "notification"
    private const val PREFS_NAME = "beeline_notifications"
    private const val LATEST_NOTIFICATION_KEY = "latest_maps_notification"
    private const val MAPS_PACKAGE = "com.google.android.apps.maps"
  }

  override fun onNotificationPosted(sbn: StatusBarNotification) {
    if (sbn.packageName != MAPS_PACKAGE) return
    val extras = sbn.notification.extras
    val lines = extras.getCharSequenceArray(android.app.Notification.EXTRA_TEXT_LINES)
      ?.joinToString(" · ") { it.toString() }
      .orEmpty()
    val notification = JSONObject().apply {
      put("packageName", sbn.packageName)
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

  override fun onNotificationRemoved(sbn: StatusBarNotification) {
    if (sbn.packageName != MAPS_PACKAGE) return
    getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
      .edit()
      .remove(LATEST_NOTIFICATION_KEY)
      .apply()
  }
}
