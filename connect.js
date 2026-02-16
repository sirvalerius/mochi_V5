// UUID definiti nel firmware C++ (MochiBLE.h)
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let mochiCharacteristic = null;
let connectedDevice = null;

// Riferimenti UI
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
const statusText = document.getElementById('status-text');
const cmdButtons = document.querySelectorAll('.cmd-btn');
const versionLabel = document.getElementById('version-label');

// Riferimenti Pannello Settings
const btnSettings = document.getElementById('btn-settings');
const settingsPanel = document.getElementById('settings-panel');
const settingsBackdrop = document.getElementById('settings-backdrop'); // NUOVO
const btnSaveSettings = document.getElementById('btn-save-settings');
const btnCloseSettings = document.getElementById('btn-close-settings'); // NUOVO Tasto annulla
const tzSelector = document.getElementById('tz-selector');


// 1. Caricamento versione dal file manifest (generato dalla CI)
fetch('manifest.json')
    .then(response => response.json())
    .then(data => {
        if(versionLabel) versionLabel.innerText = `(v${data.version})`;
    })
    .catch(err => console.log("Impossibile caricare la versione", err));


// 2. Funzioni Ausiliarie (Timezone & Sync)
async function loadTimezones() {
    console.log("Caricamento timezones...");
    try {
        // CORRETTO: Usa HTTPS e TimeAPI.io per evitare blocchi Mixed Content
        const response = await fetch('https://timeapi.io/api/TimeZone/AvailableTimeZones');
        const zones = await response.json();
        
        const savedTz = localStorage.getItem('selectedTimezone') || 'Europe/Rome';
        
        tzSelector.innerHTML = ''; // Svuota il messaggio "Caricamento..."
        
        zones.forEach(zone => {
            const opt = document.createElement('option');
            opt.value = zone;
            opt.innerText = zone; // TimeAPI non usa underscore solitamente
            if (zone === savedTz) opt.selected = true;
            tzSelector.appendChild(opt);
        });
    } catch (e) {
        console.error("Errore caricamento zone:", e);
        // Fallback in caso di errore API
        tzSelector.innerHTML = '<option value="Europe/Rome">Europe/Rome (Default)</option>';
    }
}

async function syncMochiTime() {
    try {
        const selectedTz = localStorage.getItem('selectedTimezone') || 'Europe/Rome';
        
        // Chiamata API per ottenere l'ora della zona scelta
        const response = await fetch(`https://timeapi.io/api/Time/current/zone?timeZone=${selectedTz}`);
        const data = await response.json();

        const now = new Date(data.dateTime);
        
        const dateStr = now.toLocaleDateString('it-IT', { day: '2-digit', month: '2-digit', year: 'numeric' });
        const timeStr = now.toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit' });
        
        const fullStatus = `${dateStr} ${timeStr}`;
        console.log(`Sincronizzazione (${selectedTz}):`, fullStatus);
        
        if (mochiCharacteristic) {
            await sendCmd(`time:${fullStatus}`);
        }
        
    } catch (error) {
        console.error("Errore API Time, attivo fallback locale:", error);
        
        // Fallback: usa l'orologio del computer/telefono
        const local = new Date();
        const fallbackDate = local.toLocaleDateString('it-IT', { day: '2-digit', month: '2-digit', year: 'numeric' });
        const fallbackTime = local.toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit' });
        
        if (mochiCharacteristic) {
             await sendCmd(`time:${fallbackDate} ${fallbackTime}`);
        }
    }
}


// 3. Gestione Pannello Settings
// Funzioni per Aprire/Chiudere
function openSettings() {
    settingsPanel.classList.add('open');
    settingsBackdrop.classList.add('visible');
    loadTimezones(); 
}

function closeSettings() {
    settingsPanel.classList.remove('open');
    settingsBackdrop.classList.remove('visible');
}

// Event Listeners
btnSettings.onclick = openSettings;
btnCloseSettings.onclick = closeSettings; // Chiude con il tasto Annulla
settingsBackdrop.onclick = closeSettings; // Chiude cliccando sullo sfondo scuro

btnSaveSettings.onclick = () => {
    localStorage.setItem('selectedTimezone', tzSelector.value);
    closeSettings();
    
    // Feedback visivo o logica sync
    if (mochiCharacteristic) syncMochiTime();
};


// 4. Logica Bluetooth (BLE)
async function connectToMochi() {
    try {
        statusText.innerHTML = `<span class="status-dot"></span> Ricerca Mochi in corso...`;

        const device = await navigator.bluetooth.requestDevice({
            filters: [{ namePrefix: 'MOCHI-' }],
            optionalServices: [SERVICE_UUID]
        });
		
		connectedDevice = device;

        device.addEventListener('gattserverdisconnected', onDisconnected);
        statusText.innerHTML = `<span class="status-dot"></span> Connessione a ${device.name}...`;

        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        mochiCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);

        await onConnected(device.name); // Aggiunto await per gestire la sync iniziale

    } catch (error) {
        console.log("Ricerca annullata o errore:", error);
        statusText.innerHTML = `<span class="status-dot"></span> Disconnesso`;
    }
}

async function disconnectMochi() {
    if (!connectedDevice) return;

    if (connectedDevice.gatt.connected) {
        console.log("Disconnessione manuale...");
        connectedDevice.gatt.disconnect();
    }
}

async function sendCmd(action) {
    if (!mochiCharacteristic) {
        alert("Non sei connesso a nessun Mochi!");
        return;
    }

    try {
        const encoder = new TextEncoder();
        await mochiCharacteristic.writeValue(encoder.encode(action));
        console.log(`Comando inviato: ${action}`);
    } catch (error) {
        console.error("Errore invio comando:", error);
        onDisconnected(); 
    }
}

// 5. Gestione Stato Connessione (UI)
async function onConnected(name) {
    statusText.innerHTML = `<span class="status-dot online"></span> Connesso a <b>${name}</b>`;
    
    // Gestione bottoni
    btnConnect.style.display = "none";
    btnDisconnect.style.display = "block";
    cmdButtons.forEach(btn => btn.removeAttribute('disabled'));
	
	// Sincronizzazione Iniziale
	try {
        console.log("Attesa stabilizzazione BLE...");
        // CORRETTO: setTimeout dentro una Promise per funzionare con await
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        console.log("Avvio sync ora...");
		await syncMochiTime();
        console.log("Sincronizzazione completata.");
    } catch (error) {
        console.error("Errore durante la sync iniziale:", error);
    }
}

function onDisconnected() {
    statusText.innerHTML = `<span class="status-dot"></span> Mochi perso! Riconnetti.`;
    mochiCharacteristic = null;
    connectedDevice = null;
	
    // Reset bottoni
	btnConnect.innerText = "🔍 Cerca Mochi";
    btnConnect.style.backgroundColor = ""; // Reset colore originale (gestito da CSS o default)
	btnConnect.style.display = "block";
    btnDisconnect.style.display = "none";
    
    cmdButtons.forEach(btn => btn.setAttribute('disabled', 'true'));
}


// 6. Setup Event Listeners
btnConnect.addEventListener('click', connectToMochi);
btnDisconnect.addEventListener('click', disconnectMochi);

// Assegna l'azione a tutti i pulsanti comando
cmdButtons.forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.getAttribute('data-cmd');
        if (action) sendCmd(action);
    });
});