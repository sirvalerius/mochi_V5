// UUID Bluetooth
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let mochiCharacteristic = null;
let connectedDevice = null;

let mochiSettings = {
    timezone: "Europe/Rome",
	bgTop: "#FFA0C8",    
    bgBottom: "#82F0FF",
    // Aggiungi qui altre preferenze in futuro
};

// Riferimenti UI
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
const statusText = document.getElementById('status-text');
const cmdButtons = document.querySelectorAll('.cmd-btn');
const versionLabel = document.getElementById('version-label');

// Riferimenti Amici
const nearbyListEl = document.getElementById('nearby-list');
const friendsListEl = document.getElementById('friends-list');
const btnRefreshSocial = document.getElementById('btn-refresh-social');
let lastArrayRequest = null;   // 'nearby' | 'friends' (disambigua gli array vuoti)
let socialPollTimer = null;

// Riferimenti Sidebar
const btnSettings = document.getElementById('btn-settings');
const settingsPanel = document.getElementById('settings-panel');
const settingsBackdrop = document.getElementById('settings-backdrop');
const btnSaveSettings = document.getElementById('btn-save-settings');
const btnCloseSettings = document.getElementById('btn-close-settings');
const tzSelector = document.getElementById('tz-selector');
const colorTopPicker = document.getElementById('color-top');       
const colorBottomPicker = document.getElementById('color-bottom');
const sliderBrightness = document.getElementById('slider-brightness');
const brightnessVal = document.getElementById('brightness-val');

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
    saveAndUploadSettings();
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
		
		await mochiCharacteristic.startNotifications();
		mochiCharacteristic.addEventListener('characteristicvaluechanged', handleNotifications);
        
        onConnected(device.name);
    } catch (e) {
        statusText.innerHTML = `<span class="status-dot"></span> Errore`;
    }
}

function updateStatsUI(data) {
    for (const key of ['str', 'spd', 'int', 'chr']) {
        if (data[key] !== undefined) {
            const val = data[key];
            document.getElementById(`bar-${key}`).style.width = `${(val / 99) * 100}%`;
            document.getElementById(`val-${key}`).innerText = val;
        }
    }
}

async function downloadState() {
    await sendCmd("get_state");
    // handleNotifications receives the response and calls updateStatsUI
}

async function onConnected(name) {

    await downloadSettings();
    await downloadState();

    statusText.innerHTML = `<span class="status-dot online"></span> Connesso: ${name}`;
	
	await mochiCharacteristic.startNotifications();
	mochiCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
		const value = new TextDecoder().decode(event.target.value);
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

    // Avvia il polling di vicini e amici
    pollSocial();
    if (socialPollTimer) clearInterval(socialPollTimer);
    socialPollTimer = setInterval(pollSocial, 5000);
}

sliderBrightness.addEventListener('input', (e) => {
    const val = e.target.value;
    const percentage = Math.round((val / 255) * 100);
    brightnessVal.innerText = percentage;
});

// Per salvare
async function uploadSettings(settingsObj) {
    const jsonString = JSON.stringify(settingsObj);
    await sendCmd(`set_json:${jsonString}`);
}

function applyBackgroundColors(topColor, bottomColor) {
    // Applica il gradiente al body della pagina
    document.body.style.background = `linear-gradient(to bottom, ${topColor}, ${bottomColor})`;
    
    // Aggiorna anche i selettori colore nell'interfaccia
    colorTopPicker.value = topColor;
    colorBottomPicker.value = bottomColor;
}

// Applica i colori salvati al caricamento della pagina (o usa quelli di default)
const savedBgTop = localStorage.getItem('bgTop') || "#FFA0C8";
const savedBgBottom = localStorage.getItem('bgBottom') || "#82F0FF";
applyBackgroundColors(savedBgTop, savedBgBottom);

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
		localStorage.setItem('selectedTimezone', mochiSettings.timezone);
    }
	
	if(mochiSettings.bgTop && mochiSettings.bgBottom) {
        applyBackgroundColors(mochiSettings.bgTop, mochiSettings.bgBottom);
        localStorage.setItem('bgTop', mochiSettings.bgTop);       
        localStorage.setItem('bgBottom', mochiSettings.bgBottom);
    }
	
	if(mochiSettings.brightness) {
        sliderBrightness.value = mochiSettings.brightness;
        brightnessVal.innerText = Math.round((mochiSettings.brightness / 255) * 100);
        localStorage.setItem('brightness', mochiSettings.brightness);
    }
}

function onDisconnected() {
    statusText.innerHTML = `<span class="status-dot"></span> Disconnesso`;
    mochiCharacteristic = null;
    btnConnect.style.display = "block";
    btnDisconnect.style.display = "none";
    cmdButtons.forEach(b => b.setAttribute('disabled', 'true'));

    // Ferma il polling social e svuota le liste
    if (socialPollTimer) { clearInterval(socialPollTimer); socialPollTimer = null; }
    nearbyListEl.innerHTML = '<p class="social-empty">Nessun Mochi vicino</p>';
    friendsListEl.innerHTML = '<p class="social-empty">Nessun amico</p>';
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
	mochiSettings.bgTop = colorTopPicker.value;       
    mochiSettings.bgBottom = colorBottomPicker.value; 
    mochiSettings.brightness = parseInt(sliderBrightness.value); // <-- AGGIUNTO
	
	applyBackgroundColors(mochiSettings.bgTop, mochiSettings.bgBottom);

    // 2. Salva localmente (per sicurezza)
    localStorage.setItem('selectedTimezone', mochiSettings.timezone);
	localStorage.setItem('bgTop', mochiSettings.bgTop);       
    localStorage.setItem('bgBottom', mochiSettings.bgBottom); 
	localStorage.setItem('brightness', mochiSettings.brightness); // <-- AGGIUNTO

    // 3. Invia al Mochi via BLE
    // Usiamo un prefisso "set:" per far capire all'ESP32 che deve salvare
    const payload = `set_json:${JSON.stringify(mochiSettings)}`;
    await sendCmd(payload);
    
    console.log("Impostazioni inviate al Mochi!");
    closeSettings(); // Chiudi la sidebarV
	
	if (mochiCharacteristic) {
        await syncMochiTime(); // <-- RIGA AGGIUNTA
    }
}

// 5. Event Listeners
btnConnect.onclick = connectToMochi;
btnDisconnect.onclick = disconnectMochi;

const actionButtons = document.querySelectorAll('.action-btn');

function setActiveAction(btn) {
    actionButtons.forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
}

cmdButtons.forEach(btn => {
    btn.onclick = () => {
        const cmd = btn.getAttribute('data-cmd');
        if (!cmd) return;
        if (cmd.startsWith('queue:')) {
            setActiveAction(btn);
        }
        sendCmd(cmd);
    };
});

// Il tasto "Cerca vicini" ricarica entrambe le liste (sovrascrive l'handler generico).
if (btnRefreshSocial) btnRefreshSocial.onclick = pollSocial;

// --- AMICI ---

// Chiede al Mochi sia la lista amici che i vicini (separati nel tempo per
// non far interleavare le notifiche).
async function pollSocial() {
    if (!mochiCharacteristic) return;
    lastArrayRequest = 'friends';
    await sendCmd('get_friends');
    await new Promise(r => setTimeout(r, 250));
    lastArrayRequest = 'nearby';
    await sendCmd('get_nearby');
}

// Invia un comando amico e poi ricarica le liste.
async function socialAction(cmd) {
    await sendCmd(cmd);
    await new Promise(r => setTimeout(r, 250));
    pollSocial();
}

function rssiDot(rssi) {
    if (rssi >= -60) return '🟢';
    if (rssi >= -75) return '🟡';
    return '🔴';
}

function renderNearby(arr) {
    nearbyListEl.innerHTML = '';
    if (!arr.length) {
        nearbyListEl.innerHTML = '<p class="social-empty">Nessun Mochi vicino</p>';
        return;
    }
    arr.forEach(m => {
        const row = document.createElement('div');
        row.className = 'social-row';
        row.innerHTML = `<span class="social-rssi">${rssiDot(m.rssi)}</span><span class="social-name">${m.id}</span>`;
        const btn = document.createElement('button');
        if (m.isFriend) {
            btn.className = 'social-act del';
            btn.textContent = 'Rimuovi';
            btn.onclick = () => socialAction('del_friend:' + m.id);
        } else {
            btn.className = 'social-act add';
            btn.textContent = 'Aggiungi';
            btn.onclick = () => socialAction('add_friend:' + m.id);
        }
        row.appendChild(btn);
        nearbyListEl.appendChild(row);
    });
}

function renderFriends(arr) {
    friendsListEl.innerHTML = '';
    if (!arr.length) {
        friendsListEl.innerHTML = '<p class="social-empty">Nessun amico</p>';
        return;
    }
    arr.forEach(f => {
        const row = document.createElement('div');
        row.className = 'social-row';
        row.innerHTML = `<span class="social-name">${f.id}</span>`;
        const btn = document.createElement('button');
        btn.className = 'social-act del';
        btn.textContent = 'Rimuovi';
        btn.onclick = () => socialAction('del_friend:' + f.id);
        row.appendChild(btn);
        friendsListEl.appendChild(row);
    });
}

function handleNotifications(event) {
    let receivedString = new TextDecoder().decode(event.target.value);

    if (receivedString.startsWith("[")) {
        try {
            const arr = JSON.parse(receivedString);
            // I vicini hanno il campo "rssi"; gli amici no. Per gli array vuoti
            // ci affidiamo a quale richiesta è stata appena inviata.
            const looksNearby = arr.length > 0 ? (arr[0].rssi !== undefined) : (lastArrayRequest === 'nearby');
            if (looksNearby) renderNearby(arr);
            else renderFriends(arr);
        } catch (error) {
            console.error("Errore parsing array social:", error);
        }
        return;
    }

    if (receivedString.startsWith("{")) {
        try {
            let syncData = JSON.parse(receivedString);
            console.log("%c[BLE SYNC]", "color: #00d2ff; font-weight: bold;");
            console.dir(syncData);

            // Stats update (has "str" key)
            if (syncData.str !== undefined) {
                updateStatsUI(syncData);
                return;
            }

            if(syncData.timezone) tzSelector.value = syncData.timezone;
            if(syncData.bgTop && syncData.bgBottom) {
                applyBackgroundColors(syncData.bgTop, syncData.bgBottom);
            }
            if(syncData.brightness) {
                sliderBrightness.value = syncData.brightness;
                brightnessVal.innerText = Math.round((syncData.brightness / 255) * 100);
                localStorage.setItem('brightness', syncData.brightness);
            }
            
            // Qui poi aggiornerai l'interfaccia (Fuso orario, Colori, ecc.)

        } catch (error) {
            console.error("Errore nel parsing del JSON:", error);
        }
    } else {
        console.log("[BLE RAW] Ricevuto:", receivedString);
    }
}