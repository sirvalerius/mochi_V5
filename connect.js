// UUID definiti nel firmware C++ (MochiBLE.h)
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let mochiCharacteristic = null;

// Riferimenti UI
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
const statusText = document.getElementById('status-text');
const cmdButtons = document.querySelectorAll('.cmd-btn');
const versionLabel = document.getElementById('version-label');

let connectedDevice = null; // Ci serve per salvare il riferimento al device

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
        statusText.innerHTML = `<span class="status-dot"></span> Ricerca Mochi in corso...`;

        const device = await navigator.bluetooth.requestDevice({
            // FILTRO: Mostra solo dispositivi che iniziano con "MOCHI-"
            filters: [
                { namePrefix: 'MOCHI-' }
            ],
            // IMPORTANTE: Dichiariamo comunque il servizio che useremo
            optionalServices: [SERVICE_UUID]
        });
		
		connectedDevice = device;

        device.addEventListener('gattserverdisconnected', onDisconnected);
        statusText.innerHTML = `<span class="status-dot"></span> Connessione a ${device.name}...`;

        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        mochiCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);

        onConnected(device.name);
    } catch (error) {
        console.log("Ricerca annullata o nessun Mochi trovato.");
        statusText.innerHTML = `<span class="status-dot"></span> Disconnesso`;
    }
}

async function disconnectMochi() {
    if (!connectedDevice) return;

    if (connectedDevice.gatt.connected) {
        console.log("Disconnessione manuale...");
        connectedDevice.gatt.disconnect();
        // Nota: onDisconnected() verrà chiamata automaticamente dall'event listener
    }
}

// Helper: Aggiorna UI quando connesso
async function onConnected(name) {
    statusText.innerHTML = `<span class="status-dot online"></span> Connesso a <b>${name}</b>`;
    btnConnect.innerText = "Cambia Mochi";
    btnConnect.style.backgroundColor = "#a55eea"; // Cambio colore leggero per feedback
	
	btnConnect.style.display = "none";      // Nasconde "Cerca Mochi"
    btnDisconnect.style.display = "block";  // MOSTRA "Scollega"
    
    // Abilita tutti i pulsanti di comando
    cmdButtons.forEach(btn => btn.removeAttribute('disabled'));
	
	// Eseguiamo la sincronizzazione in modo asincrono.
    // Usiamo un piccolo delay iniziale per permettere al GATT di stabilizzarsi
	
	try {
        console.log("Avvio sincronizzazione ora...");
        await setTimeout(syncMochiTime, 1000); 
        console.log("Sincronizzazione completata con successo.");
    } catch (error) {
        console.error("Errore durante la sincronizzazione iniziale:", error);
    }
}

// Helper: Aggiorna UI quando disconnesso
function onDisconnected() {
    statusText.innerHTML = `<span class="status-dot"></span> Mochi perso! Riconnetti.`;
    mochiCharacteristic = null;
    btnConnect.innerText = "🔍 Cerca Mochi";
	
	btnConnect.style.display = "block";     // Torna "Cerca Mochi"
    btnDisconnect.style.display = "none";    // Sparisce "Scollega"
    
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

async function syncMochiTime() {
    try {
        const response = await fetch('https://worldtimeapi.org/api/ip');
        const data = await response.json();
        const now = new Date(data.datetime);
        
        // Formattiamo Giorno/Mese (es: 14/02)
        const dateStr = now.toLocaleDateString('it-IT', { day: '2-digit', month: '2-digit', year: '4-digit'});
        // Formattiamo Ora (es: 15:30)
        const timeStr = now.toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit' });
        
        const fullStatus = `${dateStr} ${timeStr}`;
        console.log("Sincronizzazione completa:", fullStatus);
        
        // Inviamo il comando "sync:14/02 15:30"
        await sendCmd(`time:${fullStatus}`);
        
    } catch (error) {
        console.error("Errore API esterna, uso ora locale:", error);
        const local = new Date();
        const fallback = local.toLocaleDateString('it-IT', { day: '2-digit', month: '2-digit' }) + 
                         " " + local.toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit' });
        sendCmd(`time:${fallback}`);
    }
}

// 4. Setup Event Listeners
btnConnect.addEventListener('click', connectToMochi);
// 5. Listener per il tasto disconnetti
btnDisconnect.addEventListener('click', disconnectMochi);

// Assegna l'azione a tutti i pulsanti con classe .cmd-btn
cmdButtons.forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.getAttribute('data-cmd');
        if (action) sendCmd(action);
    });
});