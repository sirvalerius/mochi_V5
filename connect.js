// UUID definiti nel firmware C++ (MochiBLE.h)
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let mochiCharacteristic = null;

// Riferimenti UI
const btnConnect = document.getElementById('btn-connect');
const statusText = document.getElementById('status-text');
const cmdButtons = document.querySelectorAll('.cmd-btn');
const versionLabel = document.getElementById('version-label');

// 1. Caricamento versione dal file manifest (generato dalla CI)
fetch('manifest.json')
    .then(response => response.json())
    .then(data => {
        if(versionLabel) versionLabel.innerText = `(v${data.version})`;
    })
    .catch(err => console.log("Impossibile caricare la versione", err));


// 2. Funzione Connessione BLE
async function connectToMochi() {
    try {
        statusText.innerHTML = `<span class="status-dot"></span> Ricerca in corso...`;

        // Cerca dispositivi che iniziano con "Mochi-" (gestione colonia)
        const device = await navigator.bluetooth.requestDevice({
			// Accettiamo tutti i dispositivi per vedere se appare nella lista
			acceptAllDevices: true, 
			optionalServices: [SERVICE_UUID] 
		});

        // Gestione disconnessione (se Mochi si spegne o esce dal raggio)
        device.addEventListener('gattserverdisconnected', onDisconnected);

        statusText.innerHTML = `<span class="status-dot"></span> Connessione a ${device.name}...`;

        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        mochiCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);

        // Successo!
        onConnected(device.name);

    } catch (error) {
        console.error("Errore connessione:", error);
        // Se l'utente annulla, resetta lo stato
        statusText.innerHTML = `<span class="status-dot"></span> Disconnesso`;
    }
}

// Helper: Aggiorna UI quando connesso
function onConnected(name) {
    statusText.innerHTML = `<span class="status-dot online"></span> Connesso a <b>${name}</b>`;
    btnConnect.innerText = "Cambia Mochi";
    btnConnect.style.backgroundColor = "#a55eea"; // Cambio colore leggero per feedback
    
    // Abilita tutti i pulsanti di comando
    cmdButtons.forEach(btn => btn.removeAttribute('disabled'));
}

// Helper: Aggiorna UI quando disconnesso
function onDisconnected() {
    statusText.innerHTML = `<span class="status-dot"></span> Mochi perso! Riconnetti.`;
    mochiCharacteristic = null;
    btnConnect.innerText = "🔍 Cerca Mochi";
    
    // Disabilita i pulsanti per evitare errori
    cmdButtons.forEach(btn => btn.setAttribute('disabled', 'true'));
}

// 3. Funzione Invio Comandi
async function sendCmd(action) {
    if (!mochiCharacteristic) {
        alert("Non sei connesso a nessun Mochi!");
        return;
    }

    try {
        const encoder = new TextEncoder();
        await mochiCharacteristic.writeValue(encoder.encode(action));
        console.log(`Comando inviato BLE: ${action}`);
    } catch (error) {
        console.error("Errore invio comando:", error);
        onDisconnected(); // Se l'invio fallisce, consideriamo disconnesso
    }
}

// 4. Setup Event Listeners
btnConnect.addEventListener('click', connectToMochi);

// Assegna l'azione a tutti i pulsanti con classe .cmd-btn
cmdButtons.forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.getAttribute('data-cmd');
        if (action) sendCmd(action);
    });
});