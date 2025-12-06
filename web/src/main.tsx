import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './styles/main.css';

import App from './App.tsx'
import { ModuleCommunicationProvider } from './contexts/ModuleCommunication.tsx';

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ModuleCommunicationProvider>
      <App />
    </ModuleCommunicationProvider>
  </StrictMode>,
)
