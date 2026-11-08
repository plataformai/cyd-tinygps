package expo.modules.beeline

import android.app.Notification
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import android.os.Build
import android.service.notification.StatusBarNotification
import android.util.Base64
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream

/** On-demand, local diagnostic only. Never infers a maneuver from an icon. */
object MapsNotificationDiagnostic {
  fun capture(context: Context, sbn: StatusBarNotification): JSONObject {
    require(sbn.packageName == "com.google.android.apps.maps")
    val notification = sbn.notification
    val images = JSONArray()
    val views = JSONArray()
    val errors = JSONArray()
    val fields = JSONObject()
    val keys = JSONArray()
    val extras = notification.extras
    for (key in extras.keySet().sorted().take(80)) {
      keys.put(JSONObject().put("key", key).put("type", try {
        @Suppress("DEPRECATION")
        extras.get(key)?.javaClass?.simpleName ?: "null"
      } catch (_: Exception) { "unavailable" }))
    }
    for (key in listOf(Notification.EXTRA_TITLE, Notification.EXTRA_TEXT,
        Notification.EXTRA_BIG_TEXT, Notification.EXTRA_SUB_TEXT,
        Notification.EXTRA_TITLE_BIG, Notification.EXTRA_SUMMARY_TEXT,
        Notification.EXTRA_INFO_TEXT)) {
      fields.put(key, extras.getCharSequence(key)?.toString()?.take(2000).orEmpty())
    }
    fields.put(Notification.EXTRA_TEXT_LINES,
      JSONArray(extras.getCharSequenceArray(Notification.EXTRA_TEXT_LINES)
        ?.take(20)?.map { it.toString().take(500) }.orEmpty()))

    fun image(source: String, drawable: Drawable?) {
      if (drawable == null || images.length() >= 8) return
      try {
        val bitmap = Bitmap.createBitmap(96, 96, Bitmap.Config.ARGB_8888)
        try {
          val previous = android.graphics.Rect(drawable.bounds)
          val width = drawable.intrinsicWidth.coerceAtLeast(1)
          val height = drawable.intrinsicHeight.coerceAtLeast(1)
          val scale = 88f / maxOf(width, height)
          val w = (width * scale).toInt().coerceAtLeast(1)
          val h = (height * scale).toInt().coerceAtLeast(1)
          drawable.setBounds((96 - w) / 2, (96 - h) / 2, (96 + w) / 2, (96 + h) / 2)
          try { drawable.draw(Canvas(bitmap)) } finally { drawable.bounds = previous }
          val bytes = ByteArrayOutputStream()
          bitmap.compress(Bitmap.CompressFormat.PNG, 100, bytes)
          if (bytes.size() <= 48000) {
            images.put(JSONObject().put("source", source).put("uri",
              "data:image/png;base64," + Base64.encodeToString(bytes.toByteArray(), Base64.NO_WRAP)))
          } else errors.put("$source: imagem excedeu limite")
        } finally { bitmap.recycle() }
      } catch (error: Exception) {
        errors.put("$source: ${error.javaClass.simpleName}")
      }
    }
    fun icon(source: String, load: () -> Drawable?) {
      try { image(source, load()) }
      catch (error: Exception) { errors.put("$source: ${error.javaClass.simpleName}") }
    }
    icon("smallIcon (pode ser apenas o logo Maps)") { notification.smallIcon?.loadDrawable(context) }
    icon("largeIcon") { notification.getLargeIcon()?.loadDrawable(context) }
    for (key in listOf("android.largeIcon.big", "android.picture", "android.pictureIcon")) {
      try {
        @Suppress("DEPRECATION")
        when (val value = extras.get(key)) {
          is android.graphics.drawable.Icon -> icon(key) { value.loadDrawable(context) }
          is Bitmap -> image(key, android.graphics.drawable.BitmapDrawable(context.resources, value))
        }
      } catch (error: Exception) {
        errors.put("$key: ${error.javaClass.simpleName}")
      }
    }

    // Public RemoteViews API only. No reflection, accessibility service or screen capture.
    @Suppress("DEPRECATION")
    val layouts = listOf("contentView" to notification.contentView,
      "bigContentView" to notification.bigContentView,
      "headsUpContentView" to notification.headsUpContentView)
    for ((name, layout) in layouts) {
      if (layout == null) {
        views.put(JSONObject().put("source", name).put("available", false))
        continue
      }
      try {
        val root = layout.apply(context, null)
        var visited = 0
        fun inspect(view: View, path: String, depth: Int) {
          if (++visited > 160 || depth > 12) return
          val description = view.contentDescription?.toString()?.take(500).orEmpty()
          val text = (view as? TextView)?.text?.toString()?.take(1000).orEmpty()
          if (description.isNotBlank() || text.isNotBlank() || view is ImageView) {
            views.put(JSONObject().put("source", path).put("class", view.javaClass.simpleName)
              .put("description", description).put("text", text))
          }
          if (view is ImageView) image(path, view.drawable)
          if (view is ViewGroup) {
            for (index in 0 until minOf(view.childCount, 40)) {
              inspect(view.getChildAt(index), "$path/$index", depth + 1)
            }
          }
        }
        inspect(root, name, 0)
      } catch (error: Exception) {
        errors.put("$name: ${error.javaClass.simpleName}")
      }
    }
    val mapsVersion = try {
      @Suppress("DEPRECATION")
      context.packageManager.getPackageInfo(sbn.packageName, 0).versionName
    } catch (_: Exception) { "unavailable" }
    return JSONObject()
      .put("schemaVersion", 1)
      .put("capturedAt", System.currentTimeMillis())
      .put("postedAt", sbn.postTime)
      .put("androidApi", Build.VERSION.SDK_INT)
      .put("mapsVersion", mapsVersion)
      .put("category", notification.category)
      .put("groupSummary", notification.flags and Notification.FLAG_GROUP_SUMMARY != 0)
      .put("fields", fields).put("extraKeys", keys)
      .put("images", images).put("views", views).put("errors", errors)
      .put("notice", "Diagnóstico gráfico, não reconhecimento de manobra. Pode conter vias/destinos.")
  }
}
