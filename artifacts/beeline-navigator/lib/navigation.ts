import type { MapsNotification } from '../modules/beeline-android';

export type Turn = {
  maneuver: 'straight' | 'slight_left' | 'left' | 'slight_right' | 'right' | 'uturn' | 'text';
  label: string;
  distanceMeters: number | null;
  road: string;
};

const maneuverLabels: Record<Turn['maneuver'], string> = {
  straight: 'Siga em frente',
  slight_left: 'Mantenha-se à esquerda',
  left: 'Vire à esquerda',
  slight_right: 'Mantenha-se à direita',
  right: 'Vire à direita',
  uturn: 'Faça o retorno',
  text: 'Orientação do Maps',
};

function normalized(value: string): string {
  return value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLocaleLowerCase('pt-BR');
}

type DistanceMatch = RegExpMatchArray;

function metersFromMatch(match: DistanceMatch): number | null {
  const value = Number(match[1].replace(',', '.'));
  if (!Number.isFinite(value)) return null;
  return Math.min(65535, Math.max(0, Math.round(
    value * (/^(km|quil)/i.test(match[2]) ? 1000 : 1),
  )));
}

export function getMapsNotificationText(notification: MapsNotification): string {
  return [
    notification.title,
    notification.text,
    notification.bigText,
    notification.subText,
    notification.textLines ?? '',
  ].filter(Boolean).join(' · ').trim();
}

export function parseMapsInstruction(notification: MapsNotification): Turn | null {
  if (notification.packageName !== 'com.google.android.apps.maps') return null;
  const source = getMapsNotificationText(notification);
  if (!source) return null;

  // A destination/street name is not a turn. Keep this contextual instruction
  // separate from maneuver matching (e.g. "em direção a Rua Direita").
  const parts = source.split(/[·\n\r]+/).map(part => part.trim()).filter(Boolean);
  const isContext = (part: string) =>
    /^(?:\d+(?:[.,]\d+)?\s*(?:m|metros?|km|quilometros?)\s*[,–-]?\s*)?em direcao (?:a|ao|aos|as)\s+\S/.test(normalized(part));
  const contextIndex = parts.findIndex(isContext);
  const contextPart = contextIndex >= 0 ? parts[contextIndex] : undefined;
  const contextPrefixDistance = contextPart?.match(
    /^\s*(\d+(?:[.,]\d+)?)\s*(km|quil[oô]metros?|m|metros?)\s*[,–-]?\s*em dire[cç][aã]o\b/i,
  );
  const contextSuffixDistance = contextPart?.match(
    /em dire[cç][aã]o\b.*?[,–-]?\s*(\d+(?:[.,]\d+)?)\s*(km|quil[oô]metros?|m|metros?)\s*$/i,
  );
  const parseStandaloneDistance = (part: string | undefined) =>
    part?.match(/^\s*(\d+(?:[.,]\d+)?)\s*(km|quil[oô]metros?|m|metros?)\s*[.!]?\s*$/i) ?? null;
  const summaryMarkerNear = (index: number) =>
    [parts[index - 1], parts[index + 1]].some(part =>
      part !== undefined && /\b(?:\d+\s*(?:min(?:utos?)?|h(?:oras?)?)|chegada|arrival)\b/.test(normalized(part)),
    );
  const previousDistance = contextIndex > 0
    ? parseStandaloneDistance(parts[contextIndex - 1])
    : null;
  const nextDistance = contextIndex >= 0
    ? parseStandaloneDistance(parts[contextIndex + 1])
    : null;
  const safePreviousDistance = previousDistance &&
      !(/^(km|quil)/i.test(previousDistance[2]) && summaryMarkerNear(contextIndex - 1))
    ? previousDistance : null;
  const safeNextDistance = nextDistance &&
      !(/^(km|quil)/i.test(nextDistance[2]) && summaryMarkerNear(contextIndex + 1))
    ? nextDistance : null;
  const directDistances = [contextPrefixDistance, contextSuffixDistance].filter(
    (match): match is DistanceMatch => match !== null && match !== undefined,
  );
  const adjacentDistances = [safePreviousDistance, safeNextDistance].filter(
    (match): match is DistanceMatch => match !== null,
  );
  const selectedDistance =
    directDistances.find(match => metersFromMatch(match) !== null && metersFromMatch(match)! > 0) ??
    adjacentDistances.find(match => metersFromMatch(match) !== null && metersFromMatch(match)! > 0) ??
    directDistances[0] ??
    adjacentDistances[0] ??
    null;
  const contextDistanceMeters = selectedDistance ? metersFromMatch(selectedDistance) : null;
  const context = contextPart
    ?.replace(/^\d+(?:[.,]\d+)?\s*(?:km|quil[oô]metros?|m|metros?)\s*[,–-]?\s*/i, '')
    ?.replace(contextSuffixDistance ? /[,–-]?\s*\d+(?:[.,]\d+)?\s*(?:km|quil[oô]metros?|m|metros?)\s*$/i : /$^/, '')
    ?.replace(/\s+\d+\s*(?:min(?:utos?)?|h(?:oras?)?)\b.*$/i, '')
    .replace(/\s+chegada\b.*$/i, '').trim();
  const text = normalized(parts.filter(part => !isContext(part)).join(' · '));
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
  } else if (/\b(siga em frente|continue|straight|head (?:north|south)(?:east|west)?|head east|head west|siga (?:na direcao|em direcao|em direcao ao|em direcao a) (?:norte|sul|leste|oeste|nordeste|noroeste|sudeste|sudoeste))\b/.test(text)) {
    maneuver = 'straight';
  }
  if (!maneuver) {
    if (!context) return null;
    return {
      maneuver: 'text',
      label: maneuverLabels.text,
      distanceMeters: contextDistanceMeters,
      road: context,
    };
  }

  const distanceMatch = text.match(/\b(\d+(?:[.,]\d+)?)\s*(km|quil[oô]metros?|m|metros?)\b/i);
  const distanceValue = distanceMatch ? Number(distanceMatch[1].replace(',', '.')) : 0;
  const distanceMeters = distanceMatch
    ? Math.min(65535, Math.max(0, Math.round(distanceValue * (/^(km|quil)/i.test(distanceMatch[2]) ? 1000 : 1))))
    : 0;

  const roadSource = source.replace(/\bsiga\s+(?:na|em)\s+dire[cç][aã]o(?:\s+(?:a|ao))?\s+(?:norte|sul|leste|oeste|nordeste|noroeste|sudeste|sudoeste)\b/i, '');
  const roadMatch = roadSource.match(/(?:\bna\b|\bno\b|\bpela\b|\bpelo\b|\bpara(?:\s+(?:a|o))?\b|\bonto\b|\bon)\s+([^,·]+?)(?=\s+(?:em|in)\s+\d|[,·]|$)/i);
  const road = roadMatch?.[1]?.trim().replace(/[.!?]+$/, '').slice(0, 48) ?? '';

  return { maneuver, label: maneuverLabels[maneuver], distanceMeters, road };
}

export function makeNavigationCommand(turn: Turn, timestamp = Date.now()): string {
  const payload = {
    type: 'navigation',
    maneuver: turn.maneuver,
    // Textual guidance includes distance only when it is attached to that
    // guidance field, never from the route-summary distance to destination.
    distanceMeters: turn.distanceMeters === null ? undefined : turn.distanceMeters,
    // Built-in TFT fonts: send ASCII for this wrapped textual mode only.
    road: turn.maneuver === 'text'
      ? turn.road.normalize('NFD').replace(/[\u0300-\u036f]/g, '')
        .replace(/[^\x20-\x7e]/g, ' ').replace(/\s+/g, ' ').trim()
      : turn.road,
    timestamp,
  };
  const byteLength = (value: string) => Array.from(value).reduce((total, character) => {
    const codePoint = character.codePointAt(0) ?? 0;
    return total + (codePoint <= 0x7f ? 1 : codePoint <= 0x7ff ? 2 : codePoint <= 0xffff ? 3 : 4);
  }, 0);
  let command = JSON.stringify(payload);
  if (turn.maneuver === 'text' && byteLength(command) > 180) {
    const characters = Array.from(payload.road);
    do {
      characters.pop();
      payload.road = characters.join('').trimEnd() + '...';
      command = JSON.stringify(payload);
    } while (characters.length > 0 && byteLength(command) > 180);
  }
  if (byteLength(command) > 180) {
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
