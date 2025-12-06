<script lang="ts">
	import module from './assets/module.avif';
	let connected = $state(false);
	let led = $state(false);
	let brightness = $state(200);

	let characteristic: BluetoothRemoteGATTCharacteristic;

	const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
	const CONTROL_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
	const STATUS_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

	function handleNotification(
		event: Event & { target: BluetoothRemoteGATTCharacteristic },
	) {
		const value = new TextDecoder().decode(event.target.value);
		console.log("Notification reçue:", value);

		try {
			const status = JSON.parse(value);
			cycle.style.setProperty('--progress', status.progress);
			//updateUI(status);
			console.info({ status });
		} catch (e) {
			console.log("Message non-JSON:", value);
		}
	}

	async function getServer() {
		let device = (await navigator.bluetooth.getDevices())[0];
		if (device) {
			try {
				const server = await device.gatt?.connect();
				if (server) return server;
			} catch {
				/* try get device */
			}
		}

		device = await navigator.bluetooth.requestDevice({
			optionalServices: [SERVICE_UUID],
			filters: [{ namePrefix: "OIGNIES-SNCV-" }],
		});
		return device.gatt?.connect();
	}
	async function connect() {
		try {
			const server = await getServer();
			if (!server) return;

			console.log("Récupération du service...");
			const service = await server.getPrimaryService(SERVICE_UUID);

			console.log("Récupération de la caractéristique...");
			characteristic = await service.getCharacteristic(CONTROL_CHAR_UUID);
			const notificationCharacteristic =
				await service.getCharacteristic(STATUS_CHAR_UUID);

			// Activer les notifications
			await notificationCharacteristic.startNotifications();
			notificationCharacteristic.addEventListener(
				"characteristicvaluechanged",
				handleNotification as any,
			);
			connected = true;
		} catch (err) {
			console.error("BLE error:", err);
		}
	}

	async function toggleLed() {
		if (!characteristic) return;
		const json = JSON.stringify({ cmd: "LED", value: led });
		const encoder = new TextEncoder();
		await characteristic.writeValue(encoder.encode(json));
	}

	async function handleMode(e: Event & {currentTarget: HTMLButtonElement}){
		const json = JSON.stringify({ cmd: "MODE", value: e.currentTarget.name });
		const encoder = new TextEncoder();
		await characteristic.writeValue(encoder.encode(json));
	}

	async function handleBrightness(e:Event & { currentTarget:HTMLInputElement}) {
		const json = JSON.stringify({ cmd: "BRIGHTNESS", value: e.currentTarget.valueAsNumber });
		const encoder = new TextEncoder();
		await characteristic.writeValue(encoder.encode(json));
	}

	let cycle: HTMLDivElement;
</script>

{#if !connected}
	<button onclick={connect}>Connect to ESP32</button>
{:else}
	<label>
		<input type="checkbox" bind:checked={led} onchange={toggleLed} />
		Built-in LED
	</label>
	<button name="NIGHT" onclick={handleMode}>NIGHT</button>
	<button name="DAY" onclick={handleMode}>DAY</button>
	<button name="AUTO" onclick={handleMode}>AUTO</button>
	<input type="range" min="0" max="255" bind:value={brightness} onchange={handleBrightness} />
{/if}
<div class="container" style={`--image: url('${module}')`}>
        <div class="cycle-display" bind:this={cycle} data-phase="day" >
            <!-- Soleil -->
            <div class="sun"></div>
            
            <!-- Lune -->
            <div class="moon"></div>
            
            <!-- Étoiles -->
            <div class="stars-container" id="starsContainer"></div>
        </div>

    </div>

<style>
	* {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            width: 100%;
            max-width: 600px;
			position: relative;
        }

		.container:after{
			content: '';
			background-image: var(--image);
  			background-size: cover;
			position: absolute;
			inset: 0;
			z-index: 100;
		}

        .cycle-display {
            width: 100%;
            height: 300px;
            border-radius: 20px;
            position: relative;
            overflow: hidden;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
            
            /* Variable CSS pour la progression (0-100) */
            --progress: 0;
        }

        /* Fond du ciel - change selon la progression */
        .sky-background {
            position: absolute;
            width: 100%;
            height: 100%;
            transition: background 0.5s ease;
        }

        /* Calculer la couleur du ciel selon --progress */
        .cycle-display {
            background: 
                linear-gradient(
                    180deg,
                    /* Jour (0-25%) */
                    rgb(135, 206, 250) 0%,
                    rgb(135, 206, 250) calc(var(--progress) * 1%),
                    
                    /* Coucher de soleil (25-37.5%) */
                    rgb(255, 100, 50) calc(max(25%, min(37.5%, var(--progress) * 1%))),
                    
                    /* Nuit (37.5-62.5%) */
                    rgb(0, 0, 30) calc(max(37.5%, min(62.5%, var(--progress) * 1%))),
                    
                    /* Lever de soleil (62.5-75%) */
                    rgb(255, 150, 80) calc(max(62.5%, min(75%, var(--progress) * 1%))),
                    
                    /* Jour (75-100%) */
                    rgb(135, 206, 250) calc(max(75%, var(--progress) * 1%))
                );
        }

        /* Étoiles */
        .stars-container {
            position: absolute;
            width: 100%;
            height: 100%;
            opacity: 0;
            transition: opacity 0.5s ease;
        }

        /* Les étoiles apparaissent entre 37.5% et 62.5% */
        .cycle-display[data-phase="night"] .stars-container,
        .cycle-display[data-phase="sunset"] .stars-container {
            opacity: 1;
        }

        .star {
            position: absolute;
            background: white;
            border-radius: 50%;
            animation: twinkle 2s ease-in-out infinite;
        }

        @keyframes twinkle {
            0%, 100% { opacity: 0.5; transform: scale(1); }
            50% { opacity: 1; transform: scale(1.2); }
        }

        /* Soleil */
        .sun {
            position: absolute;
            width: 60px;
            height: 60px;
            background: radial-gradient(circle, #FFD700, #FFA500);
            border-radius: 50%;
            box-shadow: 0 0 40px rgba(255, 215, 0, 0.8);
            transition: all 0.5s ease;
            
            /* Position verticale selon la progression */
            top: calc(70% - (var(--progress) * 0.4%));
            left: 50%;
            transform: translateX(-50%);
            
            /* Visible uniquement pendant le jour et transitions */
            opacity: 0;
        }

        .cycle-display[data-phase="day"] .sun,
        .cycle-display[data-phase="sunset"] .sun,
        .cycle-display[data-phase="sunrise"] .sun {
            opacity: 1;
        }

        /* Lune */
        .moon {
            position: absolute;
            width: 50px;
            height: 50px;
            background: #f4f4f4;
            border-radius: 50%;
            box-shadow: 0 0 30px rgba(244, 244, 244, 0.5);
            top: 20%;
            right: 15%;
            opacity: 0;
            transition: opacity 0.5s ease;
        }

        .cycle-display[data-phase="night"] .moon {
            opacity: 1;
        }

        /* Informations */
        .info-panel {
            margin-top: 30px;
            padding: 20px;
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            color: white;
        }

        .info-row {
            display: flex;
            justify-content: space-between;
            margin-bottom: 15px;
            font-size: 16px;
        }

        .phase-label {
            font-size: 24px;
            font-weight: bold;
            text-align: center;
            margin-bottom: 20px;
        }

        .progress-bar-container {
            width: 100%;
            height: 8px;
            background: rgba(255, 255, 255, 0.2);
            border-radius: 4px;
            overflow: hidden;
            margin-top: 15px;
        }

        .progress-bar-fill {
            height: 100%;
            background: linear-gradient(90deg, #FFD700, #87CEEB);
            width: calc(var(--progress) * 1%);
            transition: width 0.3s ease;
        }

        /* Contrôles de test */
        .controls {
            margin-top: 20px;
            padding: 20px;
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
        }

        .slider-container {
            margin-top: 10px;
        }

        input[type="range"] {
            width: 100%;
            height: 6px;
            border-radius: 3px;
            background: rgba(255, 255, 255, 0.3);
            outline: none;
            -webkit-appearance: none;
        }

        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: white;
            cursor: pointer;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
        }

        input[type="range"]::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: white;
            cursor: pointer;
            border: none;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
        }

        label {
            color: white;
            font-weight: 600;
            display: block;
            margin-bottom: 10px;
        }
</style>