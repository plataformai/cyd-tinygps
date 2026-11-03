package expo.modules.beeline

import java.text.Normalizer
import kotlin.math.roundToLong

/** Public notification text only. A maneuver distance alone is never a route summary. */
object RouteSummary {
  const val SOURCE_MAX_AGE_MS = 120_000L
  const val FIRMWARE_TTL_MS = 30_000L
  private const val MAX_METERS = 40_000_000L
  private val duration = Regex("""\b\d+\s*(?:h|hr|hours?|horas?|mins?|minutos?)\b""")
  private val eta = Regex("""\b(?:chegada|arrival|eta)\b""")
  private val distance = Regex("""^(\d+(?:[.,]\d+)?)\s*(km|m|quilometros?|metros?)$""")
  private val explicit = Regex("""(?:distancia restante|remaining distance|faltam)\s*[:=]?\s*(\d+(?:[.,]\d+)?)\s*(km|m|quilometros?|metros?)\b|(\d+(?:[.,]\d+)?)\s*(km|m|quilometros?|metros?)\s+(?:restantes|remaining)\b""")
  private val finished = Regex("""\b(?:voce chegou|chegou ao destino|you have arrived|navigation ended|navegacao encerrada|navegacao finalizada)\b""")

  fun ttl(postedAt: Long, now: Long): Long {
    val age = now - postedAt
    return if (postedAt <= 0 || age < -1000 || age >= SOURCE_MAX_AGE_MS) 0
      else minOf(FIRMWARE_TTL_MS, SOURCE_MAX_AGE_MS - maxOf(0, age))
  }

  fun parse(fields: List<String>): Long? {
    val text = Normalizer.normalize(fields.joinToString(" · "), Normalizer.Form.NFD)
      .replace(Regex("\\p{M}+"), "").lowercase(java.util.Locale.ROOT)
    if (finished.containsMatchIn(text)) return null
    explicit.find(text)?.let {
      val offset = if (it.groupValues[1].isNotEmpty()) 1 else 3
      return meters(it.groupValues[offset], it.groupValues[offset + 1])
    }
    val parts = text.split(Regex("""[·•|\n]""")).map { it.trim() }
    // Require an unambiguous summary neighbor, not merely any distance in the text.
    return parts.indices.reversed().firstNotNullOfOrNull { index ->
      val match = distance.matchEntire(parts[index]) ?: return@firstNotNullOfOrNull null
      val before = parts.getOrNull(index - 1).orEmpty()
      val after = parts.getOrNull(index + 1).orEmpty()
      if (!duration.containsMatchIn(before) && !duration.containsMatchIn(after) &&
          !eta.containsMatchIn(after)) return@firstNotNullOfOrNull null
      meters(match.groupValues[1], match.groupValues[2])
    }
  }

  private fun meters(number: String, unit: String): Long? {
    val value = number.replace(',', '.').toDoubleOrNull() ?: return null
    val result = value * if (unit == "km" || unit.startsWith("quil")) 1000 else 1
    if (!result.isFinite() || result < 0 || result > MAX_METERS) return null
    return result.roundToLong()
  }
}
