import type { MapsNotification } from '../modules/beeline-android';

export type Turn = {
  maneuver: 'straight' | 'slight_left' | 'left' | 'slight_right' | 'right' | 'uturn';
  label: string;
  distanceMeters: number;
  road: string;
};

const maneuverLabels: Record<Turn['maneuver'], string> = {
  straight: 'Siga em frente',
  slight_left: 'Mantenha-se à esquerda',
  left: 'Vire à esquerda',
  slight_right: 'Mantenha-se à direita',
  right: 'Vire à direita',
  uturn: 'Faça o retorno',
};

function normalized(value: string): string {
  return value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLocaleLowerCase('pt-BR');
}

export function parseMapsInstruction(notification: MapsNotification & { textLines?: string }): Turn | null {
  if (notification.packageName !== 'com.google.android.apps.maps') return null;
  const source = [
    notification.title,
    notification.text,
    notification.bigText,
    notification.subText,
    notification.textLines ?? '',
  ].filter(Boolean).join(' · ').trim();
  if (!source) return null;

  const text = normalized(source);
  let maneuver: Turn['maneuver'] | null = null;
  if (/\b(retorno|retorne|retornar|faca meia-volta|faca o retorno|u-turn|make a u-turn)\b/.test(text)) {
    maneuver = 'uturn';
  } else if (/\b(mantenha-se|mantenha|keep)\s+(?:a|na|to the)?\s*(?:faixa\s+)?(?:esquerda|left)\b/.test(text)) {
    maneuver = 'slight_left';
  } else if (/\b(mantenha-se|mantenha|keep)\s+(?:a|na|to the)?\s*(?:faixa\s+)?(?:direita|right)\b/.test(text)) {
    maneuver = 'slight_right';
  } else if (/\b(esquerda|left)\b/.test(text)) {
    maneuver = 'left';
  } else if (/\b(direita|right)\b/.test(text)) {
    maneuver = 'right';
  } else if (/\b(siga em frente|continue|straight|head north|head south|head east|head west)\b/.test(text)) {
    maneuver = 'straight';
  }
  if (!maneuver) return null;

  const distanceMatch = text.match(/\b(\d+(?:[.,]\d+)?)\s*(km|quil[oô]metros?|m|metros?)\b/i);
  const distanceValue = distanceMatch ? Number(distanceMatch[1].replace(',', '.')) : 0;
  const distanceMeters = distanceMatch
    ? Math.min(65535, Math.max(0, Math.round(distanceValue * (/^(km|quil)/i.test(distanceMatch[2]) ? 1000 : 1))))
    : 0;

  const roadMatch = source.match(/(?:\bna\b|\bno\b|\bpela\b|\bpelo\b|\bpara(?:\s+(?:a|o))?\b|\bonto\b|\bon)\s+([^,·]+?)(?=\s+(?:em|in)\s+\d|[,·]|$)/i);
  const road = roadMatch?.[1]?.trim().replace(/[.!?]+$/, '').slice(0, 48) ?? '';

  return { maneuver, label: maneuverLabels[maneuver], distanceMeters, road };
}

export function makeNavigationCommand(turn: Turn, timestamp = Date.now()): string {
  const command = JSON.stringify({
    type: 'navigation',
    maneuver: turn.maneuver,
    distanceMeters: turn.distanceMeters,
    road: turn.road,
    timestamp,
  });
  const utf8Bytes = Array.from(command).reduce((total, character) => {
    const codePoint = character.codePointAt(0) ?? 0;
    return total + (codePoint <= 0x7f ? 1 : codePoint <= 0x7ff ? 2 : codePoint <= 0xffff ? 3 : 4);
  }, 0);
  if (utf8Bytes > 180) {
    throw new Error('A instrução não cabe no limite de 180 bytes do firmware.');
  }
  return command;
}

export const diagnosticTurn: Turn = {
  maneuver: 'slight_right',
  label: 'Vire levemente à direita',
  distanceMeters: 350,
  road: 'Rua Haddock Lobo',
};
