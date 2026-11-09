package expo.modules.beeline

import android.app.Notification
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import android.service.notification.StatusBarNotification
import android.util.Base64
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream

/**
 * In-memory collection of distinct Maps navigation large icons seen during a
 * route, used to build maneuver templates. Collects evidence only; it never
 * assigns a maneuver and is never sent over BLE or persisted to disk.
 */
object ArrowSamples {
  const val MASK_SIZE = 24
  private const val MAX_SAMPLES = 30
  private const val MERGE_DISTANCE = 6
  private val samples = LinkedHashMap<String, JSONObject>()

  private fun render(drawable: Drawable, size: Int): Bitmap {
    val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
    val previous = android.graphics.Rect(drawable.bounds)
    drawable.setBounds(0, 0, size, size)
    try { drawable.draw(Canvas(bitmap)) } finally { drawable.bounds = previous }
    return bitmap
  }

  /** Bright, opaque pixels: Maps draws a light arrow on a transparent icon. */
  private fun mask(bitmap: Bitmap): BooleanArray {
    val pixels = IntArray(bitmap.width * bitmap.height)
    bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
    return BooleanArray(pixels.size) { index ->
      val color = pixels[index]
      val alpha = color ushr 24 and 0xff
      val bright = maxOf(color shr 16 and 0xff, color shr 8 and 0xff, color and 0xff)
      alpha * bright / 255 > 96
    }
  }

  private fun toHex(bits: BooleanArray): String {
    val builder = StringBuilder()
    for (start in bits.indices step 4) {
      var nibble = 0
      for (offset in 0 until 4) {
        nibble = nibble shl 1
        if (start + offset < bits.size && bits[start + offset]) nibble = nibble or 1
      }
      builder.append("0123456789abcdef"[nibble])
    }
    return builder.toString()
  }

  private fun distance(a: String, b: String): Int {
    if (a.length != b.length) return Int.MAX_VALUE
    var total = 0
    for (index in a.indices) {
      total += Integer.bitCount(Character.digit(a[index], 16) xor Character.digit(b[index], 16))
    }
    return total
  }

  @Synchronized
  fun record(context: Context, sbn: StatusBarNotification) {
    try {
      val drawable = sbn.notification.getLargeIcon()?.loadDrawable(context) ?: return
      val small = render(drawable, MASK_SIZE)
      val key = try { toHex(mask(small)) } finally { small.recycle() }
      if (key.all { it == '0' }) return
      val now = System.currentTimeMillis()
      val extras = sbn.notification.extras
      val text = listOf(Notification.EXTRA_TITLE, Notification.EXTRA_TEXT)
        .mapNotNull { extras.getCharSequence(it)?.toString() }
        .filter { it.isNotBlank() }.joinToString(" · ").take(160)
      val existing = samples[key] ?: samples.entries
        .firstOrNull { distance(it.key, key) <= MERGE_DISTANCE }?.value
      if (existing != null) {
        existing.put("count", existing.optInt("count") + 1).put("lastSeen", now).put("lastText", text)
        return
      }
      if (samples.size >= MAX_SAMPLES) return
      val preview = render(drawable, 48)
      val png = try {
        ByteArrayOutputStream().also { preview.compress(Bitmap.CompressFormat.PNG, 100, it) }.toByteArray()
      } finally { preview.recycle() }
      samples[key] = JSONObject()
        .put("id", samples.size + 1)
        .put("mask", key)
        .put("maskSize", MASK_SIZE)
        .put("uri", "data:image/png;base64," + Base64.encodeToString(png, Base64.NO_WRAP))
        .put("count", 1).put("firstSeen", now).put("lastSeen", now).put("lastText", text)
    } catch (error: Exception) {
      android.util.Log.w("PetitGPSArrows", "Falha ao registrar ícone do Maps", error)
    }
  }

  @Synchronized
  fun export(): String = JSONObject()
    .put("schemaVersion", 1)
    .put("exportedAt", System.currentTimeMillis())
    .put("limit", MAX_SAMPLES)
    .put("samples", JSONArray(samples.values.map { JSONObject(it.toString()) }))
    .toString()

  @Synchronized
  fun clear() = samples.clear()
}
