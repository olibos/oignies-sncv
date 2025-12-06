import { useEffect, useState } from 'react'
import { Sun, Moon, Sunrise, Lightbulb, Wind, AlertCircle, Monitor } from 'lucide-react';
import './Control.css'

const labels = { day: 'jour', night: 'nuit', sunrise: 'aube', sunset: 'crépuscule' };
type LabelKeys = keyof typeof labels;
type LightMode = keyof typeof labels | 'auto' | 'manual';
type Theme = 'light' | 'dark' | 'auto';
export default function Control() {
  const [theme, setTheme] = useState<Theme>('auto');
  const [lightMode, setLightMode] = useState<LightMode>('auto');
  const [timeOfDay, setTimeOfDay] = useState<LabelKeys>('day');
  const [manualColor, setManualColor] = useState('#FFD700');
  const [brightness, setBrightness] = useState(80);
  const [wcDoorOpen, setWcDoorOpen] = useState(false);
  const [trainSpeed, setTrainSpeed] = useState(0);
  const [forwardDirection, setForwardDirection] = useState(true);
  const [_autoTime, setAutoTime] = useState(0);
  const [systemPrefersDark, setSystemPrefersDark] = useState(false);

  useEffect(() => {
    const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
    const controller = new AbortController();
    setSystemPrefersDark(mediaQuery.matches);
    mediaQuery.addEventListener('change', (e) => setSystemPrefersDark(e.matches), {signal: controller.signal});
    return () => controller.abort();
  }, []);

  // Determine actual theme to use
  const actualTheme = theme === 'auto' ? (systemPrefersDark ? 'dark' : 'light') : theme;
  const isDark = actualTheme === 'dark';

  // Auto day-night cycle
  useEffect(() => {
    if (lightMode === 'auto') {
      const interval = setInterval(() => {
        setAutoTime(prev => {
          const newTime = (prev + 1) % 240;
          if (newTime < 60) setTimeOfDay('sunrise');
          else if (newTime < 120) setTimeOfDay('day');
          else if (newTime < 180) setTimeOfDay('sunset');
          else setTimeOfDay('night');
          return newTime;
        });
      }, 100);
      return () => clearInterval(interval);
    }
  }, [lightMode]);

  const getLightColor = () => {
    if (lightMode === 'manual') return manualColor;
    
    const colors = {
      day: '#FFE87C',
      night: '#1a1a2e',
      sunrise: '#FF6B35',
      sunset: '#FF8C42'
    } as const;
    return colors[lightMode === 'auto' ? timeOfDay : lightMode];
  };

  const toggleWCDoor = () => {
    setWcDoorOpen(!wcDoorOpen);
  };

  const getTimeOfDayText = (tod: LabelKeys) => {
    
    return labels[tod] || tod;
  };

  const getLightModeText = () => {
    switch (lightMode){
      case 'auto': return `Auto (${getTimeOfDayText(timeOfDay)})`;
      case 'manual': return 'manuel';
      case 'day': return 'jour';
      case 'night': return 'nuit';
      default: return 'aube';
    }
  };

  return (
    <div className={`app ${isDark ? 'dark' : 'light'}`}>
      <div className="container">
        <div className="header">
          <div className="header-top">
            <div className="header-spacer"></div>
            <h1 className="title">🚂 Oignies SNCV</h1>
            <div className="header-spacer" style={{ display: 'flex', justifyContent: 'flex-end' }}>
              <div className="theme-toggle">
                <button
                  onClick={() => setTheme('light')}
                  className={`theme-btn ${theme === 'light' ? `active ${isDark ? 'dark' : 'light'}` : ''}`}
                  title="Thème Clair"
                >
                  <Sun size={16} />
                </button>
                <button
                  onClick={() => setTheme('dark')}
                  className={`theme-btn ${theme === 'dark' ? `active ${isDark ? 'dark' : 'light'}` : ''}`}
                  title="Thème Sombre"
                >
                  <Moon size={16} />
                </button>
                <button
                  onClick={() => setTheme('auto')}
                  className={`theme-btn ${theme === 'auto' ? `active ${isDark ? 'dark' : 'light'}` : ''}`}
                  title="Thème Auto"
                >
                  <Monitor size={16} />
                </button>
              </div>
            </div>
          </div>
        </div>

        {/* Lighting Control */}
        <div className="card">
          <h2 className="card-title">
            <Lightbulb size={20} />
            Éclairage
          </h2>
          
          <div 
            className="light-preview"
            style={{ 
              backgroundColor: getLightColor(),
              opacity: lightMode === 'manual' ? brightness / 100 : 0.9
            }}
          >
            <span>
              {lightMode === 'auto' ? timeOfDay.toUpperCase() : lightMode.toUpperCase()}
            </span>
          </div>

          <div className="grid-2">
            <button
              onClick={() => setLightMode('auto')}
              className={`btn ${lightMode === 'auto' ? 'btn-primary' : 'btn-default'}`}
            >
              <div className="btn-icon">
                <Wind size={20} />
                <span>Cycle Auto</span>
              </div>
            </button>
            <button
              onClick={() => setLightMode('manual')}
              className={`btn ${lightMode === 'manual' ? 'btn-primary' : 'btn-default'}`}
            >
              <div className="btn-icon">
                <AlertCircle size={20} />
                <span>Manuel</span>
              </div>
            </button>
          </div>

          {lightMode !== 'manual' && lightMode !== 'auto' && (
            <div className="grid-3">
              <button
                onClick={() => setLightMode('day')}
                className={`btn btn-small ${lightMode === 'day' ? 'btn-yellow' : 'btn-default'}`}
              >
                <div className="btn-icon">
                  <Sun size={16} />
                  <span>Jour</span>
                </div>
              </button>
              <button
                onClick={() => setLightMode('night')}
                className={`btn btn-small ${lightMode === 'night' ? 'btn-indigo' : 'btn-default'}`}
              >
                <div className="btn-icon">
                  <Moon size={16} />
                  <span>Nuit</span>
                </div>
              </button>
              <button
                onClick={() => setLightMode('sunrise')}
                className={`btn btn-small ${lightMode === 'sunrise' || lightMode === 'sunset' ? 'btn-orange' : 'btn-default'}`}
              >
                <div className="btn-icon">
                  <Sunrise size={16} />
                  <span>Aube</span>
                </div>
              </button>
            </div>
          )}

          {lightMode === 'auto' && (
            <div className="grid-3">
              <button onClick={() => setLightMode('day')} className="btn btn-small btn-default">
                <div className="btn-icon">
                  <Sun size={16} />
                  <span>Jour</span>
                </div>
              </button>
              <button onClick={() => setLightMode('night')} className="btn btn-small btn-default">
                <div className="btn-icon">
                  <Moon size={16} />
                  <span>Nuit</span>
                </div>
              </button>
              <button onClick={() => setLightMode('sunrise')} className="btn btn-small btn-default">
                <div className="btn-icon">
                  <Sunrise size={16} />
                  <span>Aube</span>
                </div>
              </button>
            </div>
          )}

          {lightMode === 'manual' && (
            <div className="space-y">
              <div>
                <label className="label">Couleur</label>
                <input
                  type="color"
                  value={manualColor}
                  onChange={(e) => setManualColor(e.target.value)}
                  className="input-color"
                />
              </div>
              <div>
                <label className="label">Luminosité : {brightness}%</label>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={brightness}
                  onChange={(e) => setBrightness(Number(e.target.value))}
                  className="input-range"
                />
              </div>
            </div>
          )}
        </div>

        {/* WC Door Control */}
        <div className="card">
          <h2 className="card-title">🚽 Sanitaires</h2>
          <button
            onClick={toggleWCDoor}
            className={`wc-btn ${wcDoorOpen ? 'open' : 'closed'}`}
          >
            <div className="wc-content">
              <span className="wc-emoji">{wcDoorOpen ? '💩' : '🚪'}</span>
              <span>{wcDoorOpen ? 'FERMER' : 'OUVRIR'}</span>
              <span className="wc-emoji">{wcDoorOpen ? '😱' : '😌'}</span>
            </div>
          </button>
          {wcDoorOpen && (
            <p className="wc-warning">
              🚨 Alerte : niveau de gaz critique ! 🚨
            </p>
          )}
        </div>

        {/* Train Control */}
        <div className="card">
          <h2 className="card-title">🚂 Vitesse</h2>
          
          <div style={{ marginBottom: '1rem' }}>
            <label className="label">Direction</label>
            <div className="grid-2">
              <button
                onClick={() => setForwardDirection(true)}
                className={`btn ${forwardDirection ? 'btn-green' : 'btn-default'}`}
              >
                ➡️ Avant
              </button>
              <button
                onClick={() => setForwardDirection(false)}
                className={`btn ${!forwardDirection ? 'btn-purple' : 'btn-default'}`}
              >
                ⬅️ Arrière
              </button>
            </div>
          </div>

          <div>
            <label className="label">Vitesse : {trainSpeed}%</label>
            <input
              type="range"
              min="0"
              max="100"
              value={trainSpeed}
              onChange={(e) => setTrainSpeed(Number(e.target.value))}
              className="input-range"
            />
            <div className="range-labels">
              <span>Arrêt</span>
              <span>Moyenne</span>
              <span>Vitesse Max</span>
            </div>
          </div>

          <div className="speed-indicator">
            <div className="speed-box">
              <div className="speed-value">
                {forwardDirection ? '→' : '←'} {trainSpeed}
              </div>
              <div className="speed-label">
                {trainSpeed === 0 ? 'ARRÊTÉ' : trainSpeed < 30 ? 'LENT' : trainSpeed < 70 ? 'MOYEN' : 'RAPIDE'}
              </div>
            </div>
          </div>
        </div>

        {/* Status Bar */}
        <div className="status-bar">
          <div className="status-content">
            <span>💡 {getLightModeText()}</span>
            <span>🚪 WC : {wcDoorOpen ? 'OUVERT 💩' : 'FERMÉ'}</span>
            <span>🚂 {trainSpeed}% {forwardDirection ? '→' : '←'}</span>
          </div>
        </div>
      </div>
    </div>
  );
}
