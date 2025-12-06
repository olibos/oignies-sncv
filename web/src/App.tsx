import { useRegisterSW } from "virtual:pwa-register/react";
import { useModuleCommunication } from "./contexts/ModuleCommunication";
import Control from "./Control";
import Splash from "./Splash";

export default function Application() {
    const { isConnected } = useModuleCommunication();
    useRegisterSW();
    
    return (
        isConnected 
            ? <Control />
            : <Splash />
    );
}