import { Feather, MaterialCommunityIcons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { useState } from 'react';
import {
  Alert,
  Linking,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useColors } from '@/hooks/useColors';

type Turn = {
  maneuver: 'straight' | 'slight_left' | 'left' | 'slight_right' | 'right' | 'uturn';
  label: string;
  distanceMeters: number;
  road: string;
};

const sampleTurns: Turn[] = [
  { maneuver: 'straight', label: 'Siga em frente', distanceMeters: 120, road: 'Av. Paulista' },
  { maneuver: 'slight_right', label: 'Vire levemente à direita', distanceMeters: 350, road: 'Rua Haddock Lobo' },
  { maneuver: 'left', label: 'Vire à esquerda', distanceMeters: 80, road: 'Al. Santos' },
];

function turnIcon(maneuver: Turn['maneuver']) {
  const icons: Record<Turn['maneuver'], keyof typeof MaterialCommunityIcons.glyphMap> = {
    straight: 'arrow-up',
    slight_left: 'arrow-top-left',
    left: 'arrow-left',
    slight_right: 'arrow-top-right',
    right: 'arrow-right',
    uturn: 'keyboard-return',
  };
  return icons[maneuver];
}

export default function HomeScreen() {
  const colors = useColors();
  const [turnIndex, setTurnIndex] = useState(0);
  const [showProtocol, setShowProtocol] = useState(false);
  const turn = sampleTurns[turnIndex];
  const message = JSON.stringify({
    type: 'navigation',
    maneuver: turn.maneuver,
    distanceMeters: turn.distanceMeters,
    road: turn.road,
    timestamp: 1761600000000,
  });

  const showNotificationSteps = () => {
    Alert.alert(
      'Acesso às notificações',
      'No Android, abra Configurações → Acesso a notificações e permita o BeeLine Navigator. O Maps precisa estar navegando para publicar as instruções.',
      [
        { text: 'Entendi', style: 'cancel' },
        ...(Platform.OS !== 'web'
          ? [{ text: 'Configurações do app', onPress: () => Linking.openSettings().catch(() => undefined) }]
          : []),
      ],
    );
  };

  const advanceTurn = async (step: number) => {
    await Haptics.selectionAsync();
    setTurnIndex((current) => (current + step + sampleTurns.length) % sampleTurns.length);
  };

  return (
    <SafeAreaView style={[styles.safe, { backgroundColor: colors.background }]}>
      <ScrollView
        contentContainerStyle={styles.content}
        showsVerticalScrollIndicator={false}
        style={Platform.OS === 'web' ? styles.webScroll : undefined}
      >
        <View style={styles.topRow}>
          <View style={styles.brand}>
            <View style={[styles.brandMark, { backgroundColor: colors.accent }]}>
              <Feather name="navigation" size={20} color={colors.accentForeground} />
            </View>
            <View>
              <Text style={[styles.brandName, { color: colors.foreground }]}>beeline</Text>
              <Text style={[styles.brandCaption, { color: colors.mutedForeground }]}>NAVIGADOR ESP32</Text>
            </View>
          </View>
          <View style={[styles.previewPill, { backgroundColor: colors.accent }]}>
            <View style={[styles.statusDot, { backgroundColor: colors.primary }]} />
            <Text style={[styles.previewText, { color: colors.accentForeground }]}>PRÉVIA</Text>
          </View>
        </View>

        <View style={styles.headingBlock}>
          <Text style={[styles.eyebrow, { color: colors.mutedForeground }]}>NAVEGAÇÃO SEM DESVIAR O OLHAR</Text>
          <Text style={[styles.heading, { color: colors.foreground }]}>A próxima curva,{'\n'}de relance.</Text>
          <Text style={[styles.intro, { color: colors.mutedForeground }]}>
            O objetivo é levar as instruções do Maps até a seta do seu ESP32 por Bluetooth.
          </Text>
        </View>

        <View style={[styles.connectionCard, { backgroundColor: colors.card, borderColor: colors.border }]}>
          <View style={styles.connectionTop}>
            <View style={[styles.bluetoothIcon, { backgroundColor: colors.secondary }]}>
              <Feather name="bluetooth" size={19} color={colors.mutedForeground} />
            </View>
            <View style={styles.connectionCopy}>
              <Text style={[styles.cardEyebrow, { color: colors.mutedForeground }]}>DISPOSITIVO</Text>
              <Text style={[styles.connectionTitle, { color: colors.foreground }]}>ESP32 não conectado</Text>
            </View>
            <View style={[styles.offlinePill, { backgroundColor: colors.secondary }]}>
              <Text style={[styles.offlineText, { color: colors.mutedForeground }]}>OFFLINE</Text>
            </View>
          </View>
          <View style={[styles.divider, { backgroundColor: colors.border }]} />
          <Text style={[styles.helperText, { color: colors.mutedForeground }]}>
            A conexão BLE Android ainda precisa ser integrada. O preview web não acessa o Bluetooth do telefone.
          </Text>
        </View>

        <View style={styles.turnHeader}>
          <Text style={[styles.sectionLabel, { color: colors.mutedForeground }]}>PRÓXIMA INSTRUÇÃO</Text>
          <View style={[styles.examplePill, { borderColor: colors.border }]}>
            <Text style={[styles.exampleText, { color: colors.mutedForeground }]}>EXEMPLO</Text>
          </View>
        </View>

        <View style={[styles.turnCard, { backgroundColor: colors.foreground }]}>
          <View style={styles.turnTopLine}>
            <View style={styles.turnInstruction}>
              <MaterialCommunityIcons name={turnIcon(turn.maneuver)} size={40} color={colors.primary} />
              <Text style={[styles.turnLabel, { color: colors.card }]}>{turn.label}</Text>
            </View>
            <View style={styles.distanceBlock}>
              <Text style={[styles.distance, { color: colors.card }]}>{turn.distanceMeters}</Text>
              <Text style={[styles.distanceUnit, { color: colors.mutedForeground }]}>METROS</Text>
            </View>
          </View>
          <View style={[styles.roadRow, { borderTopColor: 'rgba(255,255,255,0.14)' }]}>
            <Feather name="corner-up-right" size={15} color={colors.primary} />
            <Text style={[styles.roadText, { color: colors.card }]}>{turn.road}</Text>
            <View style={styles.turnControls}>
              <Pressable
                accessibilityLabel="Instrução anterior"
                onPress={() => advanceTurn(-1)}
                style={({ pressed }) => [styles.controlButton, { opacity: pressed ? 0.55 : 1 }]}
                testID="previous-turn"
              >
                <Feather name="chevron-left" size={18} color={colors.card} />
              </Pressable>
              <Pressable
                accessibilityLabel="Próxima instrução"
                onPress={() => advanceTurn(1)}
                style={({ pressed }) => [styles.controlButton, { opacity: pressed ? 0.55 : 1 }]}
                testID="next-turn"
              >
                <Feather name="chevron-right" size={18} color={colors.card} />
              </Pressable>
            </View>
          </View>
        </View>
        <Text style={[styles.demoNote, { color: colors.mutedForeground }]}>
          Toque nas setas para pré-visualizar instruções. Estes dados são apenas de demonstração.
        </Text>

        <View style={styles.sectionHeader}>
          <Text style={[styles.sectionLabel, { color: colors.mutedForeground }]}>PREPARAÇÃO</Text>
          <Text style={[styles.progressText, { color: colors.mutedForeground }]}>1 DE 2</Text>
        </View>

        <Pressable
          accessibilityRole="button"
          onPress={showNotificationSteps}
          style={({ pressed }) => [
            styles.setupRow,
            { backgroundColor: colors.card, borderColor: colors.border, opacity: pressed ? 0.72 : 1 },
          ]}
          testID="notification-access"
        >
          <View style={[styles.setupIcon, { backgroundColor: colors.accent }]}>
            <Feather name="bell" size={18} color={colors.accentForeground} />
          </View>
          <View style={styles.setupCopy}>
            <Text style={[styles.setupTitle, { color: colors.foreground }]}>Acesso ao Google Maps</Text>
            <Text style={[styles.setupDetail, { color: colors.mutedForeground }]}>Ative o acesso às notificações no Android</Text>
          </View>
          <Feather name="chevron-right" size={19} color={colors.mutedForeground} />
        </Pressable>

        <Pressable
          accessibilityRole="button"
          onPress={() => setShowProtocol((visible) => !visible)}
          style={({ pressed }) => [
            styles.setupRow,
            styles.protocolToggle,
            { backgroundColor: colors.card, borderColor: colors.border, opacity: pressed ? 0.72 : 1 },
          ]}
          testID="protocol-toggle"
        >
          <View style={[styles.setupIcon, { backgroundColor: colors.secondary }]}>
            <Feather name="code" size={18} color={colors.secondaryForeground} />
          </View>
          <View style={styles.setupCopy}>
            <Text style={[styles.setupTitle, { color: colors.foreground }]}>Protocolo BLE</Text>
            <Text style={[styles.setupDetail, { color: colors.mutedForeground }]}>JSON por característica GATT</Text>
          </View>
          <Feather name={showProtocol ? 'chevron-up' : 'chevron-down'} size={19} color={colors.mutedForeground} />
        </Pressable>

        {showProtocol && (
          <View style={[styles.protocolCard, { backgroundColor: colors.secondary }]}>
            <Text style={[styles.protocolLabel, { color: colors.mutedForeground }]}>MENSAGEM DE EXEMPLO</Text>
            <Text selectable style={[styles.code, { color: colors.secondaryForeground }]}>{message}</Text>
            <Text style={[styles.protocolHint, { color: colors.mutedForeground }]}>
              Serviço: 7b1e0001 · característica de escrita: 7b1e0002
            </Text>
          </View>
        )}

        <View style={[styles.footer, { borderTopColor: colors.border }]}>
          <Feather name="shield" size={14} color={colors.mutedForeground} />
          <Text style={[styles.footerText, { color: colors.mutedForeground }]}>
            A navegação real só será mostrada quando o Maps enviar uma notificação compatível.
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe: { flex: 1 },
  webScroll: { flex: 1, paddingTop: 67, paddingBottom: 34 },
  content: { width: '100%', maxWidth: 520, alignSelf: 'center', paddingHorizontal: 22, paddingTop: 12, paddingBottom: 34 },
  topRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', marginBottom: 35 },
  brand: { flexDirection: 'row', alignItems: 'center', gap: 10 },
  brandMark: { width: 40, height: 40, borderRadius: 15, alignItems: 'center', justifyContent: 'center' },
  brandName: { fontSize: 20, lineHeight: 22, fontFamily: 'Inter_700Bold', letterSpacing: -0.8 },
  brandCaption: { fontSize: 9, lineHeight: 13, letterSpacing: 1.4, fontFamily: 'Inter_600SemiBold' },
  previewPill: { flexDirection: 'row', alignItems: 'center', gap: 7, borderRadius: 30, paddingHorizontal: 11, paddingVertical: 7 },
  statusDot: { width: 6, height: 6, borderRadius: 4 },
  previewText: { fontSize: 10, fontFamily: 'Inter_700Bold', letterSpacing: 0.9 },
  headingBlock: { marginBottom: 23 },
  eyebrow: { fontSize: 10, fontFamily: 'Inter_600SemiBold', letterSpacing: 1.2, marginBottom: 11 },
  heading: { fontSize: 35, lineHeight: 39, letterSpacing: -1.4, fontFamily: 'Inter_700Bold', marginBottom: 11 },
  intro: { fontSize: 14, lineHeight: 21, fontFamily: 'Inter_400Regular', maxWidth: 400 },
  connectionCard: { borderWidth: 1, borderRadius: 20, padding: 16, marginBottom: 26 },
  connectionTop: { flexDirection: 'row', alignItems: 'center', gap: 11 },
  bluetoothIcon: { width: 39, height: 39, alignItems: 'center', justifyContent: 'center', borderRadius: 13 },
  connectionCopy: { flex: 1, gap: 3 },
  cardEyebrow: { fontSize: 9, fontFamily: 'Inter_600SemiBold', letterSpacing: 1 },
  connectionTitle: { fontSize: 14, fontFamily: 'Inter_600SemiBold' },
  offlinePill: { borderRadius: 20, paddingHorizontal: 9, paddingVertical: 5 },
  offlineText: { fontSize: 9, fontFamily: 'Inter_700Bold', letterSpacing: 0.8 },
  divider: { height: 1, marginVertical: 13 },
  helperText: { fontSize: 12, lineHeight: 18, fontFamily: 'Inter_400Regular' },
  turnHeader: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', marginBottom: 10 },
  sectionLabel: { fontSize: 10, fontFamily: 'Inter_700Bold', letterSpacing: 1.25 },
  examplePill: { borderWidth: 1, borderRadius: 20, paddingHorizontal: 9, paddingVertical: 4 },
  exampleText: { fontSize: 9, fontFamily: 'Inter_600SemiBold', letterSpacing: 0.8 },
  turnCard: { borderRadius: 22, padding: 19, paddingBottom: 0, overflow: 'hidden' },
  turnTopLine: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', minHeight: 98 },
  turnInstruction: { flex: 1, gap: 9, paddingRight: 8 },
  turnLabel: { fontSize: 17, lineHeight: 22, fontFamily: 'Inter_600SemiBold' },
  distanceBlock: { alignItems: 'flex-end', justifyContent: 'center' },
  distance: { fontSize: 44, lineHeight: 49, letterSpacing: -2, fontFamily: 'Inter_700Bold' },
  distanceUnit: { fontSize: 9, letterSpacing: 1.2, fontFamily: 'Inter_600SemiBold', marginTop: 1 },
  roadRow: { minHeight: 52, borderTopWidth: 1, flexDirection: 'row', alignItems: 'center', gap: 8, marginTop: 17 },
  roadText: { flex: 1, fontSize: 12, fontFamily: 'Inter_500Medium' },
  turnControls: { flexDirection: 'row', gap: 2 },
  controlButton: { width: 34, height: 38, alignItems: 'center', justifyContent: 'center' },
  demoNote: { fontSize: 11, lineHeight: 16, marginTop: 8, marginBottom: 24 },
  sectionHeader: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: 9 },
  progressText: { fontSize: 10, fontFamily: 'Inter_500Medium', letterSpacing: 0.7 },
  setupRow: { flexDirection: 'row', alignItems: 'center', borderWidth: 1, borderRadius: 17, padding: 13, gap: 11 },
  setupIcon: { width: 37, height: 37, borderRadius: 12, alignItems: 'center', justifyContent: 'center' },
  setupCopy: { flex: 1, gap: 3 },
  setupTitle: { fontSize: 13, fontFamily: 'Inter_600SemiBold' },
  setupDetail: { fontSize: 11, lineHeight: 15, fontFamily: 'Inter_400Regular' },
  protocolToggle: { marginTop: 8 },
  protocolCard: { borderRadius: 15, padding: 14, marginTop: 8 },
  protocolLabel: { fontSize: 9, fontFamily: 'Inter_700Bold', letterSpacing: 1.1, marginBottom: 9 },
  code: { fontSize: 11, lineHeight: 17, fontFamily: 'monospace' },
  protocolHint: { fontSize: 10, lineHeight: 14, marginTop: 9, fontFamily: 'Inter_400Regular' },
  footer: { flexDirection: 'row', alignItems: 'flex-start', gap: 8, borderTopWidth: 1, paddingTop: 14, marginTop: 20 },
  footerText: { flex: 1, fontSize: 10, lineHeight: 15, fontFamily: 'Inter_400Regular' },
});
