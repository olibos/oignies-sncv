<script lang="ts">
	let connected = $state(false);
	let led = $state(false);

	let device: BluetoothDevice;
	let characteristic: BluetoothRemoteGATTCharacteristic;

	const SERVICE_UUID = "1234abcd-0000-0000-0000-000000000000";
	const CHAR_UUID = "1234abcd-0000-0000-0000-000000000001";

	async function connect() {
		try {
			device = (await navigator.bluetooth.getDevices())[0];
			if (!device){
				device = await navigator.bluetooth.requestDevice({
					optionalServices: [SERVICE_UUID],
					filters:[
						{namePrefix:"OIGNIES-SNCV-"}
					],
				});
			}

			const server = await device.gatt!.connect();
			const service = await server.getPrimaryService(SERVICE_UUID);
			characteristic = await service.getCharacteristic(CHAR_UUID);

			connected = true;
		} catch (err) {
			console.error("BLE error:", err);
		}
	}

	async function toggleLed() {
		if (!characteristic) return;
		const value = led ? 1 : 0;
		await characteristic.writeValue(Uint8Array.of(value));
	}
</script>

{#if !connected}
	<button onclick={connect}>Connect to ESP32</button>
{:else}
	<label>
		<input type="checkbox" bind:checked={led} onchange={toggleLed} />
		Built-in LED
	</label>
{/if}
