import { Feather, MaterialCommunityIcons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { useCallback, useEffect, useRef, useState } from 'react';
import {
  Alert,
  AppState,
  Linking,
  PermissionsAndroid,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import * as Location from 'expo-location';
import { useColors } from '@/hooks/useColors';
import { BeeLineAndroid } from '@/modules/beeline-android';
import { diagnosticTurn, getMapsNotificationText, makeNavigationCommand, parseMapsInstruction, type Turn } from '@/lib/navigation';
import { makeSpeedCommand } from '@/lib/speed';
import { SPEED_TASK_NAME } from '@/lib/speed-task';

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
  const [showProtocol, setShowProtocol] = useState(false);
  const [connectionState, setConnectionState] = useState('disconnected');
  const [connectionMessage, setConnectionMessage] = useState('Desconectado');
  const [notificationEnabled, setNotificationEnabled] = useState(false);
  const [liveTurn, setLiveTurn] = useState<Turn | null>(null);
  const [notificationStatus, setNotificationStatus] = useState('Ainda não recebi uma notificação do Google Maps.');
  const [mapsReadState, setMapsReadState] = useState<'waiting' | 'disabled' | 'missing' | 'unsupported' | 'ready' | 'error'>('waiting');
  const [capturedNotificationText, setCapturedNotificationText] = useState('');
  const [operationError, setOperationError] = useState('');
  const [sendingTest, setSendingTest] = useState(false);
  const [speedKmh, setSpeedKmh] = useState<number | null>(null);
  const [speedTracking, setSpeedTracking] = useState(false);
  const [speedError, setSpeedError] = useState('');
  const lastNotificationRef = useRef('');
  const sentInstructionRef = useRef('');
  const lastSpeedFixAtRef = useRef(0);
  const isConnected = connectionState === 'connected';
  const message = makeNavigationCommand(diagnosticTurn, 1761600000000);
  const notificationNotParsed = mapsReadState === 'unsupported';
  const notificationNotReceived = mapsReadState === 'missing';

  const refreshNotifications = useCallback(async () => {
    if (!BeeLineAndroid.available) return;
    try {
      const enabled = await BeeLineAndroid.isNotificationAccessEnabled();
      setNotificationEnabled(enabled);
      if (!enabled) {
        lastNotificationRef.current = '';
        setCapturedNotificationText('');
        setMapsReadState('disabled');
        setLiveTurn(null);
        setNotificationStatus('Ative o acesso às notificações para receber instruções.');
        return;
      }
      const notification = await BeeLineAndroid.getLatestNotification();
      if (!notification) {
        lastNotificationRef.current = '';
        setCapturedNotificationText('');
        setMapsReadState('missing');
        setLiveTurn(null);
        setNotificationStatus('Nenhuma notificação do Maps recebida. Com uma rota ativa, confira a central de notificações do celular.');
        return;
      }
      const key = JSON.stringify([
        notification.postedAt, notification.title, notification.text,
        notification.bigText, notification.subText, notification.textLines,
      ]);
      if (key === lastNotificationRef.current) return;
      lastNotificationRef.current = key;
      const parsed = parseMapsInstruction(notification);
      setCapturedNotificationText(parsed ? '' : getMapsNotificationText(notification));
      setMapsReadState(parsed ? 'ready' : 'unsupported');
      setLiveTurn(parsed);
      setNotificationStatus(parsed
        ? `Instrução recebida do Maps às ${new Date(notification.postedAt).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}.`
        : 'O Maps publicou uma notificação sem manobra compatível.');
    } catch (error) {
      setMapsReadState('error');
      setOperationError(error instanceof Error ? error.message : 'Falha ao ler notificações do Android.');
    }
  }, []);

  useEffect(() => {
    const subscription = BeeLineAndroid.available
      ? BeeLineAndroid.addConnectionListener((event) => {
          setConnectionState(event.state);
          setConnectionMessage(event.message);
          if (event.state === 'error') setOperationError(event.message);
          else setOperationError('');
        })
      : undefined;
    const mapsSubscription = BeeLineAndroid.available
      ? BeeLineAndroid.addMapsNotificationListener(() => void refreshNotifications())
      : undefined;
    void refreshNotifications();
    const interval = setInterval(() => void refreshNotifications(), 1500);
    const appStateSubscription = AppState.addEventListener('change', (state) => {
      if (state === 'active') void refreshNotifications();
    });
    return () => {
      clearInterval(interval);
      appStateSubscription.remove();
      subscription?.remove();
      mapsSubscription?.remove();
    };
  }, [refreshNotifications]);

  useEffect(() => {
    if (!isConnected || !liveTurn || !BeeLineAndroid.available) return;
    const key = `${liveTurn.maneuver}:${liveTurn.distanceMeters}:${liveTurn.road}`;
    if (sentInstructionRef.current === key) return;
    sentInstructionRef.current = key;
    BeeLineAndroid.writeCommand(makeNavigationCommand(liveTurn))
      .then(() => setNotificationStatus('Instrução do Maps enviada ao ESP32.'))
      .catch((error: unknown) => {
        sentInstructionRef.current = '';
        setOperationError(error instanceof Error ? error.message : 'Falha ao enviar instrução BLE.');
      });
  }, [isConnected, liveTurn]);

  useEffect(() => {
    let mounted = true;
    if (Platform.OS !== 'android') return () => { mounted = false; };

    Location.hasStartedLocationUpdatesAsync(SPEED_TASK_NAME)
      .then((started) => {
        if (mounted) setSpeedTracking(started);
      })
      .catch(() => undefined);

    let subscription: Location.LocationSubscription | undefined;
    if (speedTracking) {
      const staleCheck = setInterval(() => {
        if (Date.now() - lastSpeedFixAtRef.current > 10_000) setSpeedKmh(null);
      }, 1000);
      Location.watchPositionAsync(
        { accuracy: Location.Accuracy.High, timeInterval: 1000, distanceInterval: 0 },
        (location) => {
          const fresh = Date.now() - location.timestamp <= 10_000;
          const metersPerSecond = location.coords.speed;
          const valid = fresh && metersPerSecond !== null && metersPerSecond >= 0;
          lastSpeedFixAtRef.current = valid ? location.timestamp : 0;
          setSpeedKmh(valid ? metersPerSecond * 3.6 : null);
        },
      ).then((watcher) => {
        if (mounted) subscription = watcher;
        else watcher.remove();
      }).catch((error: unknown) => {
        setSpeedError(error instanceof Error ? error.message : 'Não foi possível ler a velocidade do GPS.');
      });
      return () => {
        mounted = false;
        clearInterval(staleCheck);
        subscription?.remove();
      };
    }

    return () => {
      mounted = false;
      subscription?.remove();
    };
  }, [speedTracking]);

  const toggleSpeedTracking = async () => {
    if (Platform.OS !== 'android' || !BeeLineAndroid.available) {
      setSpeedError('A captura GPS e o envio BLE precisam de uma build nativa Android.');
      return;
    }
    setSpeedError('');
    try {
      if (speedTracking) {
        if (await Location.hasStartedLocationUpdatesAsync(SPEED_TASK_NAME)) {
          await Location.stopLocationUpdatesAsync(SPEED_TASK_NAME);
        }
        setSpeedTracking(false);
        setSpeedKmh(null);
        lastSpeedFixAtRef.current = 0;
        await BeeLineAndroid.writeCommand(makeSpeedCommand(null));
        return;
      }
      if (!isConnected) {
        setSpeedError('Conecte ao BeeLine-ESP32 antes de ativar a transmissão de velocidade.');
        return;
      }

      const foreground = await Location.requestForegroundPermissionsAsync();
      if (!foreground.granted) {
        setSpeedError('Permita o acesso à localização durante o uso para ler a velocidade.');
        if (!foreground.canAskAgain) {
          Alert.alert(
            'Localização bloqueada',
            'Habilite a permissão de localização nas configurações do BeeLine.',
            [
              { text: 'Agora não', style: 'cancel' },
              { text: 'Abrir configurações', onPress: () => Linking.openSettings().catch(() => undefined) },
            ],
          );
        }
        return;
      }

      const background = await Location.requestBackgroundPermissionsAsync();
      if (!background.granted) {
        Alert.alert(
          'Permissão de localização necessária',
          'Para manter a velocidade atualizada enquanto o Google Maps estiver aberto, permita localização em segundo plano nas configurações do Android.',
          [
            { text: 'Agora não', style: 'cancel' },
            { text: 'Abrir configurações', onPress: () => Linking.openSettings().catch(() => undefined) },
          ],
        );
        setSpeedError('A localização em segundo plano não foi autorizada.');
        return;
      }

      if (Number(Platform.Version) >= 33) {
        const notificationPermission = await PermissionsAndroid.request(
          PermissionsAndroid.PERMISSIONS.POST_NOTIFICATIONS,
        );
        if (notificationPermission !== PermissionsAndroid.RESULTS.GRANTED) {
          setSpeedError('Permita notificações para o Android manter o serviço GPS visível.');
          Alert.alert(
            'Notificação necessária',
            'Permita as notificações do BeeLine para manter visível o serviço de localização em segundo plano.',
            [
              { text: 'Agora não', style: 'cancel' },
              { text: 'Abrir configurações', onPress: () => Linking.openSettings().catch(() => undefined) },
            ],
          );
          return;
        }
      }

      const alreadyStarted = await Location.hasStartedLocationUpdatesAsync(SPEED_TASK_NAME);
      if (!alreadyStarted) {
        await Location.startLocationUpdatesAsync(SPEED_TASK_NAME, {
          accuracy: Location.Accuracy.High,
          timeInterval: 1000,
          distanceInterval: 0,
          foregroundService: {
            notificationTitle: 'BeeLine atualizando velocidade',
            notificationBody: 'A velocidade GPS está sendo enviada à sua placa.',
            killServiceOnDestroy: false,
          },
        });
      }
      setSpeedTracking(true);
    } catch (error) {
      setSpeedError(error instanceof Error ? error.message : 'Não foi possível iniciar a captura de velocidade.');
    }
  };

  const showNotificationSteps = async () => {
    if (!BeeLineAndroid.available) {
      Alert.alert('Build Android necessária', 'O acesso às notificações requer o app Android nativo. Expo Go e o preview web não incluem esse serviço.');
      return;
    }
    if (notificationEnabled) {
      Alert.alert('Acesso ativo', 'BeeLine pode ler o texto das notificações publicadas pelo Maps. O Android não expõe dados internos da rota.');
      return;
    }
    Alert.alert(
      'Permita o acesso às notificações',
      'O Android solicitará autorização especial para o BeeLine. O listener lê apenas o texto publicado nas notificações do Google Maps; não acessa dados privados nem a rota interna.',
      [
        { text: 'Agora não', style: 'cancel' },
        {
          text: 'Abrir ajustes',
          onPress: async () => {
            try {
              await BeeLineAndroid.openNotificationAccessSettings();
            } catch (error) {
              setOperationError(error instanceof Error ? error.message : 'Não foi possível abrir os ajustes.');
            }
          },
        },
      ],
    );
  };

  const connectDevice = async () => {
    if (!BeeLineAndroid.available) {
      Alert.alert('Build Android necessária', 'A conexão BLE requer uma build nativa Android com o módulo BeeLine. Não funciona no preview web ou Expo Go.');
      return;
    }
    setOperationError('');
    try {
      const permissions = Number(Platform.Version) >= 31
        ? [PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN, PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT]
        : [PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION];
      const result = await PermissionsAndroid.requestMultiple(permissions);
      if (permissions.some((permission) => result[permission] !== PermissionsAndroid.RESULTS.GRANTED)) {
        setOperationError('Permissão Bluetooth negada. Conceda a permissão e tente novamente.');
        Alert.alert('Permissão necessária', 'Permita Bluetooth (e localização em Android 11 ou anterior) nas permissões do app.', [
          { text: 'Cancelar', style: 'cancel' },
          { text: 'Abrir ajustes', onPress: () => Linking.openSettings().catch(() => undefined) },
        ]);
        return;
      }
      await Haptics.selectionAsync();
      await BeeLineAndroid.scanAndConnect();
    } catch (error) {
      const detail = error instanceof Error ? error.message : 'Falha ao conectar ao ESP32.';
      setOperationError(detail);
    }
  };

  const sendDiagnostic = async () => {
    if (!isConnected || !BeeLineAndroid.available) {
      setOperationError('Conecte ao BeeLine-ESP32 antes de enviar a instrução de teste.');
      return;
    }
    setSendingTest(true);
    setOperationError('');
    try {
      await BeeLineAndroid.writeCommand(makeNavigationCommand(diagnosticTurn));
      setNotificationStatus('Instrução de teste enviada; confira seta, distância e via no TFT.');
    } catch (error) {
      setOperationError(error instanceof Error ? error.message : 'Falha ao enviar instrução de teste.');
    } finally {
      setSendingTest(false);
    }
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
            <Text style={[styles.previewText, { color: colors.accentForeground }]}>
              {BeeLineAndroid.available ? 'ANDROID' : 'BUILD NATIVA'}
            </Text>
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
              <Text style={[styles.connectionTitle, { color: colors.foreground }]}>{connectionMessage}</Text>
            </View>
            <View style={[styles.offlinePill, { backgroundColor: isConnected ? colors.accent : colors.secondary }]}>
              <Text style={[styles.offlineText, { color: isConnected ? colors.accentForeground : colors.mutedForeground }]}>
                {isConnected ? 'CONECTADO' : connectionState === 'searching' || connectionState === 'connecting' ? 'BUSCANDO' : 'OFFLINE'}
              </Text>
            </View>
          </View>
          <View style={[styles.divider, { backgroundColor: colors.border }]} />
          <Text style={[styles.helperText, { color: colors.mutedForeground }]}>
            {BeeLineAndroid.available
              ? 'BLE ativo. Mantenha a placa ligada e próxima do telefone.'
              : 'O preview web não acessa Bluetooth. Use uma build nativa Android, não o Expo Go.'}
          </Text>
          <Pressable
            accessibilityRole="button"
            disabled={connectionState === 'searching' || connectionState === 'connecting'}
            onPress={() => {
              if (isConnected) {
                void BeeLineAndroid.disconnect();
              } else {
                void connectDevice();
              }
            }}
            style={({ pressed }) => [
              styles.primaryAction,
              { backgroundColor: colors.accent, opacity: pressed ? 0.75 : 1 },
            ]}
            testID="ble-connect"
          >
            <Feather name={isConnected ? 'bluetooth' : 'search'} size={16} color={colors.accentForeground} />
            <Text style={[styles.primaryActionText, { color: colors.accentForeground }]}>
              {isConnected ? 'Desconectar ESP32' : connectionState === 'searching' || connectionState === 'connecting' ? 'Procurando BeeLine…' : 'Conectar BeeLine-ESP32'}
            </Text>
          </Pressable>
          {operationError ? (
            <Text accessibilityRole="alert" style={[styles.errorText, { color: colors.destructive }]}>{operationError}</Text>
          ) : null}
        </View>

        <View style={styles.turnHeader}>
          <Text style={[styles.sectionLabel, { color: colors.mutedForeground }]}>PRÓXIMA INSTRUÇÃO</Text>
          <View style={[styles.examplePill, { borderColor: colors.border }]}>
            <Text style={[styles.exampleText, { color: colors.mutedForeground }]}>{liveTurn ? 'MAPS' : 'AGUARDANDO'}</Text>
          </View>
        </View>

        <View style={[styles.turnCard, { backgroundColor: colors.foreground }]}>
          {liveTurn ? (
            <>
              <View style={styles.turnTopLine}>
                <View style={styles.turnInstruction}>
                  <MaterialCommunityIcons name={turnIcon(liveTurn.maneuver)} size={40} color={colors.primary} />
                  <Text style={[styles.turnLabel, { color: colors.card }]}>{liveTurn.label}</Text>
                </View>
                <View style={styles.distanceBlock}>
                  <Text style={[styles.distance, { color: colors.card }]}>{liveTurn.distanceMeters || '—'}</Text>
                  <Text style={[styles.distanceUnit, { color: colors.mutedForeground }]}>METROS</Text>
                </View>
              </View>
              <View style={[styles.roadRow, { borderTopColor: 'rgba(255,255,255,0.14)' }]}>
                <Feather name="corner-up-right" size={15} color={colors.primary} />
                <Text style={[styles.roadText, { color: colors.card }]}>{liveTurn.road || 'Via não informada'}</Text>
              </View>
            </>
          ) : (
            <View style={styles.emptyInstruction}>
              <MaterialCommunityIcons name="map-marker-path" size={34} color={colors.primary} />
              <Text style={[styles.turnLabel, { color: colors.card }]}>
                {notificationNotParsed
                  ? 'Notificação do Maps não reconhecida'
                  : notificationNotReceived
                    ? 'Aguardando instrução do Maps'
                    : 'Inicie uma rota no Google Maps'}
              </Text>
              <Text style={[styles.emptyHint, { color: colors.mutedForeground }]}>
                {notificationNotParsed
                  ? 'O Android entregou uma notificação, mas o app não identificou a manobra.'
                  : notificationNotReceived
                    ? 'Com uma rota ativa, confira se o Maps publicou uma notificação na central do celular.'
                    : 'A próxima manobra compatível aparecerá aqui.'}
              </Text>
            </View>
          )}
        </View>
        <Text style={[styles.demoNote, { color: colors.mutedForeground }]}>
          {notificationStatus}{!liveTurn ? ' Use “Enviar instrução de teste” para verificar a placa sem uma rota.' : ''}
        </Text>
        {notificationNotParsed && (
          <Pressable
            accessibilityRole="button"
            onPress={() => Alert.alert(
              'Texto recebido do Maps',
              `${capturedNotificationText.slice(0, 1200) || 'A notificação não contém texto nos campos acessíveis.'}\n\nEste texto é exibido somente neste celular. Pode conter nomes de vias ou destinos; o app não o envia a um servidor.`,
            )}
          >
            <Text style={[styles.notificationDetails, { color: colors.primary }]}>Ver texto recebido do Maps</Text>
          </Pressable>
        )}

        <View style={styles.sectionHeader}>
          <Text style={[styles.sectionLabel, { color: colors.mutedForeground }]}>PREPARAÇÃO</Text>
          <Text style={[styles.progressText, { color: colors.mutedForeground }]}>{notificationEnabled ? '1 DE 2 · ATIVO' : '1 DE 2'}</Text>
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
            <Text style={[styles.setupDetail, { color: colors.mutedForeground }]}>
              {notificationEnabled ? 'Acesso às notificações autorizado' : 'Autorize o listener nas configurações Android'}
            </Text>
          </View>
          <Feather name={notificationEnabled ? 'check' : 'chevron-right'} size={19} color={colors.mutedForeground} />
        </Pressable>

        <Pressable
          accessibilityRole="button"
          disabled={!isConnected || sendingTest}
          onPress={() => void sendDiagnostic()}
          style={({ pressed }) => [
            styles.setupRow,
            styles.protocolToggle,
            { backgroundColor: colors.card, borderColor: colors.border, opacity: pressed || !isConnected ? 0.55 : 1 },
          ]}
          testID="send-diagnostic"
        >
          <View style={[styles.setupIcon, { backgroundColor: colors.accent }]}>
            <Feather name="activity" size={18} color={colors.accentForeground} />
          </View>
          <View style={styles.setupCopy}>
            <Text style={[styles.setupTitle, { color: colors.foreground }]}>Enviar instrução de teste</Text>
            <Text style={[styles.setupDetail, { color: colors.mutedForeground }]}>350 m · direita · Rua Haddock Lobo</Text>
          </View>
          <Feather name="send" size={17} color={colors.mutedForeground} />
        </Pressable>

        <View style={[styles.speedCard, { backgroundColor: colors.card, borderColor: colors.border }]}>
          <View style={styles.speedReadout}>
            <View>
              <Text style={[styles.cardEyebrow, { color: colors.mutedForeground }]}>VELOCIDADE GPS</Text>
              <Text style={[styles.speedValue, { color: colors.foreground }]}>
                {speedKmh === null ? '—' : Math.round(speedKmh)}
                <Text style={[styles.speedUnit, { color: colors.mutedForeground }]}> km/h</Text>
              </Text>
            </View>
            <Feather name="activity" size={22} color={speedTracking ? colors.accent : colors.mutedForeground} />
          </View>
          <Text style={[styles.speedStatus, { color: speedError ? colors.destructive : colors.mutedForeground }]}>
            {speedError || (speedTracking
              ? 'GPS ativo; transmissão contínua para a placa.'
              : 'Ative para enviar a velocidade enquanto navega.')}
          </Text>
          <Pressable
            accessibilityRole="button"
            onPress={() => void toggleSpeedTracking()}
            style={({ pressed }) => [
              styles.speedAction,
              { backgroundColor: colors.accent, opacity: pressed ? 0.76 : 1 },
            ]}
            testID="speed-tracking-toggle"
          >
            <Feather name={speedTracking ? 'pause' : 'map-pin'} size={15} color={colors.accentForeground} />
            <Text style={[styles.primaryActionText, { color: colors.accentForeground }]}>
              {speedTracking ? 'Parar velocidade GPS' : 'Ativar velocidade GPS'}
            </Text>
          </Pressable>
        </View>

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
            O app recebe somente o conteúdo que o Android publica nas notificações do Maps. Algumas instruções podem não incluir manobra, distância ou via.
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
  primaryAction: { flexDirection: 'row', alignItems: 'center', justifyContent: 'center', gap: 9, borderRadius: 13, minHeight: 46, paddingHorizontal: 14, marginTop: 14 },
  primaryActionText: { fontSize: 13, fontFamily: 'Inter_600SemiBold' },
  errorText: { fontSize: 11, lineHeight: 16, fontFamily: 'Inter_500Medium', marginTop: 10 },
  turnHeader: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', marginBottom: 10 },
  sectionLabel: { fontSize: 10, fontFamily: 'Inter_700Bold', letterSpacing: 1.25 },
  examplePill: { borderWidth: 1, borderRadius: 20, paddingHorizontal: 9, paddingVertical: 4 },
  exampleText: { fontSize: 9, fontFamily: 'Inter_600SemiBold', letterSpacing: 0.8 },
  turnCard: { borderRadius: 22, padding: 19, paddingBottom: 0, overflow: 'hidden' },
  turnTopLine: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', minHeight: 98 },
  turnInstruction: { flex: 1, gap: 9, paddingRight: 8 },
  emptyInstruction: { minHeight: 158, alignItems: 'center', justifyContent: 'center', gap: 10, paddingVertical: 22 },
  emptyHint: { fontSize: 11, lineHeight: 16, fontFamily: 'Inter_400Regular', textAlign: 'center' },
  turnLabel: { fontSize: 17, lineHeight: 22, fontFamily: 'Inter_600SemiBold' },
  distanceBlock: { alignItems: 'flex-end', justifyContent: 'center' },
  distance: { fontSize: 44, lineHeight: 49, letterSpacing: -2, fontFamily: 'Inter_700Bold' },
  distanceUnit: { fontSize: 9, letterSpacing: 1.2, fontFamily: 'Inter_600SemiBold', marginTop: 1 },
  roadRow: { minHeight: 52, borderTopWidth: 1, flexDirection: 'row', alignItems: 'center', gap: 8, marginTop: 17 },
  roadText: { flex: 1, fontSize: 12, fontFamily: 'Inter_500Medium' },
  demoNote: { fontSize: 11, lineHeight: 16, marginTop: 8, marginBottom: 24 },
  notificationDetails: { fontSize: 12, lineHeight: 17, fontFamily: 'Inter_500Medium', textDecorationLine: 'underline', marginTop: -14, marginBottom: 24 },
  sectionHeader: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: 9 },
  progressText: { fontSize: 10, fontFamily: 'Inter_500Medium', letterSpacing: 0.7 },
  setupRow: { flexDirection: 'row', alignItems: 'center', borderWidth: 1, borderRadius: 17, padding: 13, gap: 11 },
  setupIcon: { width: 37, height: 37, borderRadius: 12, alignItems: 'center', justifyContent: 'center' },
  setupCopy: { flex: 1, gap: 3 },
  setupTitle: { fontSize: 13, fontFamily: 'Inter_600SemiBold' },
  setupDetail: { fontSize: 11, lineHeight: 15, fontFamily: 'Inter_400Regular' },
  protocolToggle: { marginTop: 8 },
  speedCard: { borderWidth: 1, borderRadius: 17, padding: 14, marginTop: 8 },
  speedReadout: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between' },
  speedValue: { fontSize: 29, lineHeight: 35, fontFamily: 'Inter_700Bold', letterSpacing: -0.7, marginTop: 3 },
  speedUnit: { fontSize: 13, fontFamily: 'Inter_500Medium', letterSpacing: 0 },
  speedStatus: { fontSize: 11, lineHeight: 16, fontFamily: 'Inter_400Regular', marginTop: 6 },
  speedAction: { flexDirection: 'row', alignItems: 'center', justifyContent: 'center', gap: 8, borderRadius: 12, minHeight: 42, paddingHorizontal: 12, marginTop: 11 },
  protocolCard: { borderRadius: 15, padding: 14, marginTop: 8 },
  protocolLabel: { fontSize: 9, fontFamily: 'Inter_700Bold', letterSpacing: 1.1, marginBottom: 9 },
  code: { fontSize: 11, lineHeight: 17, fontFamily: 'monospace' },
  protocolHint: { fontSize: 10, lineHeight: 14, marginTop: 9, fontFamily: 'Inter_400Regular' },
  footer: { flexDirection: 'row', alignItems: 'flex-start', gap: 8, borderTopWidth: 1, paddingTop: 14, marginTop: 20 },
  footerText: { flex: 1, fontSize: 10, lineHeight: 15, fontFamily: 'Inter_400Regular' },
});
