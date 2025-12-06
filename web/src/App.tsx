import { useModuleCommunication } from "./contexts/ModuleCommunication";
import Control from "./Control";
import Splash from "./Splash";

export default function Application() {
    const { isConnected } = useModuleCommunication();
    
    return (
        isConnected 
            ? <Control />
            : <Splash />
    );
}