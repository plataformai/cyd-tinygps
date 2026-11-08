import { useState } from 'react';
import { ActivityIndicator, Alert, Image, Modal, Pressable, ScrollView, Share, StyleSheet, Text, View } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { BeeLineAndroid } from '@/modules/beeline-android';
import { useColors } from '@/hooks/useColors';

type Diagnostic = {
  schemaVersion: number;
  capturedAt: number;
  postedAt: number;
  androidApi: number;
  mapsVersion: string;
  fields: Record<string, string | string[]>;
  images: { source: string; uri: string }[];
  views: { source: string; description?: string; text?: string; available?: boolean }[];
  errors: string[];
};

export function MapsDiagnostic() {
  const colors = useColors();
  const [open, setOpen] = useState(false);
  const [busy, setBusy] = useState(false);
  const [report, setReport] = useState<Diagnostic | null>(null);
  const [error, setError] = useState('');

  async function capture() {
    setBusy(true);
    setError('');
    setReport(null);
    try {
      const next: Diagnostic = JSON.parse(await BeeLineAndroid.captureMapsDiagnostic());
      setReport(next);
    } catch (failure) {
      setError(failure instanceof Error ? failure.message : 'Falha ao capturar diagnóstico.');
    } finally {
      setBusy(false);
    }
  }

  function share() {
    if (!report) return;
    Alert.alert('Compartilhar diagnóstico?',
      'O relatório inclui texto, imagens e possíveis vias/destinos da notificação do Maps. Só será enviado ao aplicativo que você escolher.',
      [{ text: 'Cancelar', style: 'cancel' }, {
        text: 'Compartilhar',
        onPress: () => {
          void Share.share({ title: 'Diagnóstico PetitGPS — Maps', message: JSON.stringify(report, null, 2) })
            .catch(() => setError('Não foi possível abrir o compartilhamento. Você também pode enviar capturas desta tela.'));
        },
      }]);
  }

  return <>
    <Pressable testID="maps-diagnostic-open" accessibilityRole="button"
      onPress={() => { setOpen(true); }} style={styles.link}>
      <Text style={{ color: colors.primary }}>Diagnóstico da seta do Maps</Text>
    </Pressable>
    <Modal visible={open} animationType="slide" onRequestClose={() => setOpen(false)}>
      <SafeAreaView style={[styles.screen, { backgroundColor: colors.background }]}>
        <View style={styles.header}>
          <Text style={[styles.title, { color: colors.foreground }]}>Diagnóstico do Maps</Text>
          <Pressable testID="maps-diagnostic-close" accessibilityRole="button" onPress={() => setOpen(false)}>
            <Text style={{ color: colors.primary }}>Fechar</Text>
          </Pressable>
        </View>
        <ScrollView contentContainerStyle={styles.content}>
          <Text style={{ color: colors.foreground }}>
            Com uma navegação ativa, capture os campos e ícones da notificação atual.
            Use somente com o veículo parado.
          </Text>
          <Text style={{ color: colors.mutedForeground }}>
            As imagens são candidatas, não manobras reconhecidas. Podem incluir o logo do Maps.
            Nada deste diagnóstico é enviado à ESP32 ou a um servidor.
            Apenas a última captura fica na memória desta tela, sem histórico em disco.
          </Text>
          <Pressable testID="maps-diagnostic-capture" accessibilityRole="button"
            disabled={busy || !BeeLineAndroid.available} onPress={() => void capture()}
            style={[styles.button, { backgroundColor: colors.secondary }]}>
            <Text style={{ color: colors.foreground }}>{busy ? 'Capturando…' : 'Capturar notificação atual'}</Text>
          </Pressable>
          {!BeeLineAndroid.available && <Text style={{ color: colors.mutedForeground }}>
            Requer o APK Android atualizado. Não disponível no preview web ou Expo Go.
          </Text>}
          {busy && <ActivityIndicator color={colors.primary} />}
          {!!error && <Text selectable style={{ color: colors.destructive }}>{error}</Text>}
          {report && <>
            <Text selectable style={{ color: colors.mutedForeground }}>
              Capturado: {new Date(report.capturedAt).toLocaleString()}{'\n'}
              Notificação: {new Date(report.postedAt).toLocaleString()}{'\n'}
              Android API {report.androidApi} · Maps {report.mapsVersion}
            </Text>
            <Text style={[styles.title, { color: colors.foreground }]}>Campos de texto</Text>
            {Object.entries(report.fields).map(([key, value]) => <Text key={key} selectable style={{ color: colors.foreground }}>
              {key}: {Array.isArray(value) ? value.join(' · ') : value || '(vazio)'}
            </Text>)}
            <Text style={[styles.title, { color: colors.foreground }]}>Imagens candidatas ({report.images.length})</Text>
            {!report.images.length && <Text style={{ color: colors.mutedForeground }}>
              Nenhuma imagem extraível pelas interfaces públicas consultadas. Isso não prova que a notificação não tenha uma seta.
            </Text>}
            {report.images.map((image, index) => <View key={`${index}:${image.source}`} style={styles.imageRow}>
              <View style={[styles.imageBox, { backgroundColor: colors.secondary, borderColor: colors.border }]}>
                <Image source={{ uri: image.uri }} style={styles.image} accessibilityLabel={`Candidato ${index + 1}`} />
              </View>
              <Text selectable style={[styles.imageLabel, { color: colors.foreground }]}>{index + 1}. {image.source}</Text>
            </View>)}
            <Text style={[styles.title, { color: colors.foreground }]}>Layout e descrições</Text>
            {report.views.map((view, index) => <Text selectable key={index} style={{ color: colors.foreground }}>
              {view.source}: {view.available === false ? 'layout não fornecido' : view.description || view.text || '(imagem sem descrição)'}
            </Text>)}
            {!!report.errors.length && <Text selectable style={{ color: colors.destructive }}>{report.errors.join('\n')}</Text>}
            <Pressable testID="maps-diagnostic-share" accessibilityRole="button" onPress={share}
              style={[styles.button, { backgroundColor: colors.secondary }]}>
              <Text style={{ color: colors.foreground }}>Compartilhar relatório JSON</Text>
            </Pressable>
            <Text style={{ color: colors.mutedForeground }}>Você também pode tirar prints das imagens e seus números nesta tela.</Text>
          </>}
          <Pressable testID="maps-diagnostic-clear" accessibilityRole="button" onPress={() => { setReport(null); setError(''); }}>
            <Text style={{ color: colors.primary }}>Apagar captura desta tela</Text>
          </Pressable>
        </ScrollView>
      </SafeAreaView>
    </Modal>
  </>;
}

const styles = StyleSheet.create({
  screen: { flex: 1 },
  header: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', padding: 20, gap: 12 },
  title: { fontSize: 18, fontWeight: '600' },
  content: { padding: 20, gap: 16, paddingBottom: 40 },
  link: { paddingVertical: 14, marginBottom: 10 },
  button: { padding: 16, borderRadius: 12, alignItems: 'center' },
  imageRow: { flexDirection: 'row', gap: 12, alignItems: 'center' },
  imageBox: { borderWidth: 1, borderRadius: 8 },
  image: { width: 96, height: 96 },
  imageLabel: { flex: 1 },
});
