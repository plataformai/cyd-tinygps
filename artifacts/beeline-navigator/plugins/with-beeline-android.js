const { withAndroidManifest } = require('expo/config-plugins');

module.exports = function withBeeLineAndroid(config) {
  return withAndroidManifest(config, (modConfig) => {
    const manifest = modConfig.modResults.manifest;
    const permissions = manifest['uses-permission'] ?? [];
    const required = [
      ['android.permission.BLUETOOTH', { 'android:maxSdkVersion': '30' }],
      ['android.permission.BLUETOOTH_ADMIN', { 'android:maxSdkVersion': '30' }],
      ['android.permission.BLUETOOTH_SCAN', {}],
      ['android.permission.BLUETOOTH_CONNECT', {}],
    ];
    for (const [name, attributes] of required) {
      const existing = permissions.find((permission) => permission.$['android:name'] === name);
      if (existing) {
        Object.assign(existing.$, attributes);
      } else {
        permissions.push({ $: { 'android:name': name, ...attributes } });
      }
    }
    manifest['uses-permission'] = permissions;

    const application = manifest.application?.[0];
    if (!application) throw new Error('BeeLine exige uma tag <application> no AndroidManifest.');
    application.service = application.service ?? [];
    const serviceName = 'expo.modules.beeline.BeeLineNotificationListenerService';
    if (!application.service.some((service) => service.$?.['android:name'] === serviceName)) {
      application.service.push({
        $: {
          'android:name': serviceName,
          'android:label': 'Acesso às notificações do BeeLine',
          'android:permission': 'android.permission.BIND_NOTIFICATION_LISTENER_SERVICE',
          'android:exported': 'true',
        },
        'intent-filter': [{ action: [{ $: { 'android:name': 'android.service.notification.NotificationListenerService' } }] }],
      });
    }
    return modConfig;
  });
};
