// UUID Bluetooth
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let mochiCharacteristic = null;
let connectedDevice = null;

let mochiSettings = {
    timezone: "Europe/Rome",
    // Aggiungi qui altre preferenze in futuro
};

// Riferimenti UI
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
const statusText = document.getElementById('status-text');
const cmdButtons = document.querySelectorAll('.cmd-btn');
const versionLabel = document.getElementById('version-label');

// Riferimenti Sidebar
const btnSettings = document.getElementById('btn-settings');
const settingsPanel = document.getElementById('settings-panel');
const settingsBackdrop = document.getElementById('settings-backdrop');
const btnSaveSettings = document.getElementById('btn-save-settings');
const btnCloseSettings = document.getElementById('btn-close-settings');
const tzSelector = document.getElementById('tz-selector');

// 1. Caricamento versione
fetch('manifest.json')
    .then(r => r.json())
    .then(data => { if(versionLabel) versionLabel.innerText = `(v${data.version})`; })
    .catch(() => console.log("Versione non trovata"));

// 2. Logica Sidebar
function openSettings() {
    settingsPanel.classList.add('open');
    settingsBackdrop.classList.add('visible');
    loadTimezones();
}

function closeSettings() {
    settingsPanel.classList.remove('open');
    settingsBackdrop.classList.remove('visible');
}

btnSettings.onclick = openSettings;
btnCloseSettings.onclick = closeSettings;
settingsBackdrop.onclick = closeSettings;

btnSaveSettings.onclick = () => {
    localStorage.setItem('selectedTimezone', tzSelector.value);
    closeSettings();
    if (mochiCharacteristic) syncMochiTime();
};

// 3. API Timezones & Sync
async function loadTimezones() {
    try {
        const response = await fetch('https://timeapi.io/api/TimeZone/AvailableTimeZones');
        const zones = await response.json();
        const savedTz = localStorage.getItem('selectedTimezone') || 'Europe/Rome';
        
        tzSelector.innerHTML = '';
        zones.forEach(zone => {
            const opt = document.createElement('option');
            opt.value = zone;
            opt.innerText = zone;
            if (zone === savedTz) opt.selected = true;
            tzSelector.appendChild(opt);
        });
    } catch (e) {
        tzSelector.innerHTML = '<option value="Europe/Rome">Europe/Rome (Default)</option>';
    }
}

async function syncMochiTime() {
    try {
        const tz = localStorage.getItem('selectedTimezone') || 'Europe/Rome';
        const response = await fetch(`https://timeapi.io/api/Time/current/zone?timeZone=${tz}`);
        const data = await response.json();
        
        // Creiamo la data esplicita dai campi dell'API per evitare errori di fuso del browser
        const localDate = Date.UTC(data.year, data.month - 1, data.day, data.hour, data.minute, data.seconds);
        const unixTimestamp = Math.floor(localDate / 1000);
        
        console.log(`Sincronizzazione orario (${tz}): ${data.time}`);
        
        if (mochiCharacteristic) {
            await sendCmd(`unix:${unixTimestamp}`);
        }
    } catch (e) {
        // Fallback locale in caso di errore API
		console.warn(`[SYNC] API Fallita, uso orario locale del browser per ${tz}`);
        
        // Calcoliamo l'ora locale basandoci sul fuso scelto, senza API
        const oraLocaleBrowser = new Date();
        
        // Formattiamo la data locale come stringa per estrarre i numeri "piatti"
        const localeString = oraLocaleBrowser.toLocaleString('en-US', { timeZone: tz, hour12: false });
        const d = new Date(localeString);

        // Creiamo lo stesso tipo di timestamp "sfasato" usato nel blocco try
        const fallbackTimestamp = Math.floor(Date.UTC(
            d.getFullYear(), d.getMonth(), d.getDate(), 
            d.getHours(), d.getMinutes(), d.getSeconds()
        ) / 1000);

        if (mochiCharacteristic) {
            await sendCmd(`unix:${fallbackTimestamp}`);
        }
    }
}

// 4. Bluetooth Core
async function connectToMochi() {
    try {
        statusText.innerHTML = `<span class="status-dot"></span> Ricerca...`;
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ namePrefix: 'MOCHI-' }],
            optionalServices: [SERVICE_UUID]
        });
        connectedDevice = device;
        device.addEventListener('gattserverdisconnected', onDisconnected);
        
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        mochiCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);
        
        onConnected(device.name);
    } catch (e) {
        statusText.innerHTML = `<span class="status-dot"></span> Errore`;
    }
}

async function onConnected(name) {
	
	await downloadSettings();
	
    statusText.innerHTML = `<span class="status-dot online"></span> Connesso: ${name}`;
	
	await mochiCharacteristic.startNotifications();
	mochiCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
		const value = new TextDecoder().decode(event.targer.value);
		console.log("[BLE RECEIVE] <-", value);
		
		if (value.startsWith('{')) {
			try {
				mochiSettings = JSON.parse(value);
				if(mochiSettings.timezone) tzSelector.value = mochiSettings.timezone;
				console.log("updated settings from Mochi");
			} catch (e) { console.error("JSON Parse Error", e); }
		}
	});
	
    btnConnect.style.display = "none";
    btnDisconnect.style.display = "block";
    cmdButtons.forEach(b => b.removeAttribute('disabled'));
    
    // Attesa per stabilità BLE prima della sync
    await new Promise(r => setTimeout(r, 1000));
    syncMochiTime();
}

// Per salvare
async function uploadSettings(settingsObj) {
    const jsonString = JSON.stringify(settingsObj);
    await sendCmd(`set_json:${jsonString}`);
}

// Per caricare
async function downloadSettings() {
    await sendCmd("get_json");
    // Aspetta un attimo che il Mochi aggiorni il valore
    await new Promise(r => setTimeout(r, 200)); 
    const value = await mochiCharacteristic.readValue();
    const jsonString = new TextDecoder().decode(value);
    mochiSettings = JSON.parse(jsonString);
    
    // Aggiorna la UI (es. il selettore fuso orario)
    if(mochiSettings.timezone) {
        tzSelector.value = mochiSettings.timezone;
    }
}

function onDisconnected() {
    statusText.innerHTML = `<span class="status-dot"></span> Disconnesso`;
    mochiCharacteristic = null;
    btnConnect.style.display = "block";
    btnDisconnect.style.display = "none";
    cmdButtons.forEach(b => b.setAttribute('disabled', 'true'));
}

async function disconnectMochi() {
    if (connectedDevice) connectedDevice.gatt.disconnect();
}

async function sendCmd(action) {
    if (!mochiCharacteristic) return;
    try {
		// Logga esattamente cosa stiamo per mandare via BLE
        console.log(`[BLE SEND] -> ${action}`);
        await mochiCharacteristic.writeValue(new TextEncoder().encode(action));
    } catch (e) { console.error("Errore invio:", e); }
}

async function saveAndUploadSettings() {
    // 1. Preleva i valori dalla UI
    mochiSettings.timezone = tzSelector.value;
    // mochiSettings.brightness = sliderBrightness.value;

    // 2. Salva localmente (per sicurezza)
    localStorage.setItem('selectedTimezone', mochiSettings.timezone);

    // 3. Invia al Mochi via BLE
    // Usiamo un prefisso "set:" per far capire all'ESP32 che deve salvare
    const payload = `set_json:${JSON.stringify(mochiSettings)}`;
    await sendCmd(payload);
    
    console.log("Impostazioni inviate al Mochi!");
    closeSettings(); // Chiudi la sidebar
}

// 5. Event Listeners
btnConnect.onclick = connectToMochi;
btnDisconnect.onclick = disconnectMochi;

cmdButtons.forEach(btn => {
    btn.onclick = () => {
        const cmd = btn.getAttribute('data-cmd');
        if (cmd) sendCmd(cmd);
    };
});