import expo.modules.beeline.RouteSummary

/** Run with kotlinc against the actual production helper; no Android runtime needed. */
fun main() {
  fun checkSummary(expected: Long?, vararg fields: String) {
    check(RouteSummary.parse(fields.toList()) == expected) { fields.joinToString(" | ") }
  }
  checkSummary(3900, "0 m", "em direção a uma via", "10min · 3,9 km · Chegada: 17:49")
  checkSummary(3900, "0 m · em direção a uma via · 10min · 3,9 km · Chegada: 17:49")
  checkSummary(1200, "12 min • 1.2 km • ETA 15:30")
  checkSummary(900, "900 m · 2 min")
  checkSummary(0, "0 m · Chegada: 15:30")
  checkSummary(3900, "Distância restante: 3,9 km")
  checkSummary(3900, "3.9 km remaining")
  checkSummary(900, "Faltam 900 metros")
  checkSummary(18000, "1 h 20 min · 18 km · Chegada: 15:00")
  checkSummary(null, "Vire à direita em 300 m")
  checkSummary(null, "0 m · em direção a uma via")
  checkSummary(null, "300 m", "Vire à direita", "Chegada às 15:30 · 12 min")
  checkSummary(null, "Você chegou ao destino · 2 min · 900 m")
  checkSummary(null, "Navegação encerrada · 3,9 km restantes")
  checkSummary(null, "You have arrived · 2 min · 900 m")
  checkSummary(null, "")
  checkSummary(null, "12 min · 999999999 km")
  check(RouteSummary.ttl(1000, 1000) == 30000L)
  check(RouteSummary.ttl(1000, 120999) == 1L)
  check(RouteSummary.ttl(1000, 121000) == 0L)
  check(RouteSummary.ttl(2001, 1000) == 0L)
  check(RouteSummary.ttl(0, 1000) == 0L)
  println("PASS: real Kotlin route parser, summary vs maneuver, locale, lifecycle and source expiry")
}
