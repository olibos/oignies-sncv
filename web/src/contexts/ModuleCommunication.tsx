import { createContext, useCallback, useContext, useEffect, useState, type PropsWithChildren } from "react";

// Types
interface TrainState {
  speed: number;
  direction: 'forward' | 'reverse';
  lightMode: 'auto' | 'manual' | 'day' | 'night' | 'sunrise' | 'sunset';
  lightColor: string;
  brightness: number;
  wcDoorOpen: boolean;
}

interface ModuleCommunicationContextType {
  isConnected: boolean;
  isConnecting: boolean;
  device?: BluetoothDevice;
  error?: string;
  trainState: TrainState;
  connect(): Promise<void>;
  disconnect(): void;
  setTrainSpeed(speed: number): Promise<void>;
  setTrainDirection(forward: boolean): Promise<void>;
  setLightMode(mode: TrainState['lightMode']): Promise<void>;
  setLightColor(color: string): Promise<void>;
  setLightBrightness(brightness: number): Promise<void>;
  toggleWCDoor(): Promise<void>;
}

// Bluetooth Service UUIDs (customize these for your train module)
const TRAIN_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TRAIN_CONTROL_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
type Command = {cmd: 'wc'|'mode'|'brightness'|'speed'|'train-speed'|'direction'|'status'|'led', value?: unknown};

// Create context
const ModuleCommunicationContext = createContext<ModuleCommunicationContextType | undefined>(undefined);

// Provider component
export const ModuleCommunicationProvider = ({ children }: PropsWithChildren) => {
  const [isConnected, setIsConnected] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [device, setDevice] = useState<BluetoothDevice>();
  const [server, setServer] = useState<BluetoothRemoteGATTServer>();
  const [error, setError] = useState<string>();
  
  const [trainState, setTrainState] = useState<TrainState>({
    speed: 0,
    direction: 'forward',
    lightMode: 'auto',
    lightColor: '#FFD700',
    brightness: 80,
    wcDoorOpen: false,
  });

  // Connect to Bluetooth device
  const connect = useCallback(async () => {
    if (!navigator.bluetooth) {
      setError('Bluetooth non supporté sur ce navigateur');
      return;
    }

    setIsConnecting(true);
    setError(undefined);

    try {
      // Request device
      const selectedDevice = await navigator.bluetooth.requestDevice({
        filters: [{ services: [TRAIN_SERVICE_UUID], namePrefix: "OIGNIES-SNCV-" }],
        optionalServices: [TRAIN_SERVICE_UUID]
      });

      setDevice(selectedDevice);

      // Connect to GATT Server
      const gattServer = await selectedDevice.gatt?.connect();
      if (!gattServer) throw new Error('Impossible de se connecter au serveur GATT');

      setServer(gattServer);
      setIsConnected(true);

      // Handle disconnection
      selectedDevice.addEventListener('gattserverdisconnected', () => {
        setIsConnected(false);
        setServer(undefined);
        console.log('Appareil déconnecté');
      }, { once: true });

      console.log('Connecté au train !');
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Erreur de connexion');
      console.info({err});
      console.error('Erreur Bluetooth:', err);
    } finally {
      setIsConnecting(false);
    }
  }, []);

  // Disconnect from device
  const disconnect = useCallback(() => {
    if (device?.gatt?.connected) {
      device.gatt.disconnect();
    }
    setDevice(undefined);
    setServer(undefined);
    setIsConnected(false);
  }, [device]);

  // Helper function to write data to characteristic
  const writeCharacteristic = useCallback(async (data: Command): Promise<void> => {
    if (!server || !isConnected) throw new Error('Non connecté au train');

    try {
      const service = await server.getPrimaryService(TRAIN_SERVICE_UUID);
      const characteristic = await service.getCharacteristic(TRAIN_CONTROL_CHARACTERISTIC_UUID);
      const buffer = new TextEncoder().encode(JSON.stringify(data));
      await characteristic.writeValue(buffer);
    } catch (err) {
      console.error('Erreur d\'écriture:', err);
      throw err;
    }
  }, [server, isConnected]);

  // Set train speed
  const setTrainSpeed = useCallback(async (speed: number) => {
    await writeCharacteristic({cmd:'train-speed', value: speed});
    setTrainState(prev => ({ ...prev, speed }));
  }, [writeCharacteristic]);

  // Set train direction
  const setTrainDirection = useCallback(async (forward: boolean) => {
    await writeCharacteristic({cmd:'direction', value: forward});
    setTrainState(prev => ({ ...prev, forward }));
  }, [writeCharacteristic]);

  // Set light mode
  const setLightMode = useCallback(async (mode: TrainState['lightMode']) => {
    const r = Number.parseInt(trainState.lightColor.slice(1, 3), 16);
    const g = Number.parseInt(trainState.lightColor.slice(3, 5), 16);
    const b = Number.parseInt(trainState.lightColor.slice(5, 7), 16);
    await writeCharacteristic({cmd:'mode', value: mode !== 'manual' ? mode : {r,g,b}});
    setTrainState(prev => ({ ...prev, lightMode: mode }));
  }, [writeCharacteristic]);

  // Set light color (for manual mode)
  const setLightColor = useCallback(async (color: string) => {
    // Convert hex color to RGB
    const r = Number.parseInt(color.slice(1, 3), 16);
    const g = Number.parseInt(color.slice(3, 5), 16);
    const b = Number.parseInt(color.slice(5, 7), 16);
    
    await writeCharacteristic({cmd:"mode", value:{r,g,b}});
    setTrainState(prev => ({ ...prev, lightColor: color }));
  }, [writeCharacteristic]);

  // Set light brightness
  const setLightBrightness = useCallback(async (brightness: number) => {
    await writeCharacteristic({cmd:"brightness", value: brightness});
    setTrainState(prev => ({ ...prev, brightness }));
  }, [writeCharacteristic]);

  // Toggle WC door
  const toggleWCDoor = useCallback(async () => {
    const newState = !trainState.wcDoorOpen;
    await writeCharacteristic({cmd:'wc', value: newState});
    setTrainState(prev => ({ ...prev, wcDoorOpen: newState }));
  }, [writeCharacteristic, trainState.wcDoorOpen]);

  // Auto-cleanup on unmount
  useEffect(() => {
    return () => disconnect();
  }, [disconnect]);

  const value: ModuleCommunicationContextType = {
    isConnected,
    isConnecting,
    device,
    error,
    trainState,
    connect,
    disconnect,
    setTrainSpeed,
    setTrainDirection,
    setLightMode,
    setLightColor,
    setLightBrightness,
    toggleWCDoor,
  };

  return (
    <ModuleCommunicationContext.Provider value={value}>
      {children}
    </ModuleCommunicationContext.Provider>
  );
};

// Custom hook to use the context
export const useModuleCommunication = (): ModuleCommunicationContextType => {
  const context = useContext(ModuleCommunicationContext);
  if (!context) {
    throw new Error('useBluetooth must be used inside BluetoothProvider');
  }
  return context;
};
