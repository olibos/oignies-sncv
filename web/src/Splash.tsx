import Classes from './Splash.module.css';
import { Bluetooth, Loader, AlertCircle, CheckCircle } from 'lucide-react';
import { useModuleCommunication } from './contexts/ModuleCommunication';

export default function ConnectionScreen() {
  const {connect, isConnecting, isConnected, error} = useModuleCommunication();

  return (
    <div className={Classes.container}>
      <div className={Classes["connection-container"]}>
        <h1 className={Classes.title}>Oignies SNCV</h1>

        {error && (
          <div className={`${Classes["status-message"]} ${Classes["status-error"]}`}>
            <AlertCircle size={24} />
            <span>{error}</span>
          </div>
        )}

        {isConnected && (
          <div className={`${Classes["status-message"]} ${Classes["status-success"]}`}>
            <CheckCircle size={24} />
            <span>Connexion réussie !</span>
          </div>
        )}

        <button
          className={`${Classes["connect-button"]} ${isConnecting ? Classes.connecting : ''}`}
          onClick={connect}
          disabled={isConnecting || isConnected}
        >
          {isConnecting ? (
            <>
              <Loader className={`${Classes["button-icon"]} ${Classes.spinner}`} />
              <span>Connexion...</span>
            </>
          ) : isConnected ? (
            <>
              <CheckCircle className={Classes["button-icon"]} />
              <span>Connecté !</span>
            </>
          ) : (
            <>
              <Bluetooth className={`${Classes["button-icon"]} ${Classes["bluetooth-icon-animated"]}`} />
              <span>Connecter</span>
            </>
          )}
        </button>

        <p className={Classes.footer}>
          Assurez-vous que le Bluetooth est activé sur votre appareil
        </p>
      </div>
    </div>
  );
}