import { requireNativeModule, type EventSubscription } from 'expo';

export type MapsNotification = {
  packageName: string;
  title: string;
  text: string;
  bigText: string;
  subText: string;
  textLines?: string;
  postedAt: number;
};

type BeeLineNativeModule = {
  isNotificationAccessEnabled(): Promise<boolean>;
  openNotificationAccessSettings(): Promise<void>;
  getLatestNotification(): Promise<MapsNotification | null>;
  scanAndConnect(): Promise<void>;
  writeCommand(command: string): Promise<void>;
  disconnect(): Promise<void>;
  addListener(eventName: 'onConnectionStateChange', listener: (event: ConnectionEvent) => void): EventSubscription;
  addListener(eventName: 'onMapsNotification', listener: (event: MapsNotification) => void): EventSubscription;
};

export type ConnectionEvent = { state: string; message: string };
let nativeModule: BeeLineNativeModule | undefined;
try {
  nativeModule = requireNativeModule<BeeLineNativeModule>('BeeLineAndroid');
} catch {
  // Web, Expo Go, and non-custom Android builds do not include this native module.
}

function requireAndroidModule(): BeeLineNativeModule {
  if (!nativeModule) {
    throw new Error('A integração BeeLine requer uma build nativa Android; ela não funciona no preview web ou Expo Go.');
  }
  return nativeModule;
}

export const BeeLineAndroid = {
  available: Boolean(nativeModule),
  isNotificationAccessEnabled: () => requireAndroidModule().isNotificationAccessEnabled(),
  openNotificationAccessSettings: () => requireAndroidModule().openNotificationAccessSettings(),
  getLatestNotification: () => requireAndroidModule().getLatestNotification(),
  scanAndConnect: () => requireAndroidModule().scanAndConnect(),
  writeCommand: (command: string) => requireAndroidModule().writeCommand(command),
  disconnect: () => requireAndroidModule().disconnect(),
  addConnectionListener: (listener: (event: ConnectionEvent) => void) => {
    requireAndroidModule();
    return nativeModule!.addListener('onConnectionStateChange', listener);
  },
  addMapsNotificationListener: (listener: (event: MapsNotification) => void) => {
    return requireAndroidModule().addListener('onMapsNotification', listener);
  },
};
