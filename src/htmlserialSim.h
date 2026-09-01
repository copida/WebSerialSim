#pragma once

const char serial_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Web Serial Monitor (SSE)</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: 'Courier New', Courier, monospace;
            background-color: #1e1e1e;
            color: #33ff33;
            padding: 20px;
            margin: 0;
            display: flex;
            flex-direction: column;
            height: 100vh;
        }

        .header-container {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #33ff33;
            padding-bottom: 10px;
            margin-bottom: 10px;
            flex-shrink: 0;
        }
        h1 {
            color: #ffffff;
            font-size: 1.4rem;
            margin: 0;
        }
        .controls-group {
            display: flex;
            align-items: center;
            gap: 20px;
            font-size: 0.9rem;
            color: #ffffff;
            flex-wrap: wrap;
        }
        .checkbox-container {
            display: flex;
            align-items: center;
            gap: 8px;
            cursor: pointer;
            user-select: none;
        }
        .checkbox-container input {
            cursor: pointer;
            width: 16px;
            height: 16px;
        }

        /* Terminale */
        #serial-terminal {
            flex-grow: 1;
            background-color: #000000;
            border: 1px solid #333333;
            border-radius: 4px;
            overflow-y: auto;
            padding: 12px;
            white-space: pre-wrap;
            word-break: break-all;
            margin-bottom: 10px;
        }

        .log-line {
            min-height: 1.2em;
            line-height: 1.3;
            transition: background-color 0.1s ease;
        }
        .show-linenum .log-line::before {
            content: "[" attr(data-line) "] ";
            color: #888888;
        }

        /* Stato filtro */
        .log-line.hidden {
            display: none;
        }
        .log-line.match {
            background-color: #1a3a1a;
            border-left: 2px solid #33ff33;
            padding-left: 6px;
        }

        /* Scrollbar Custom */
        #serial-terminal::-webkit-scrollbar { width: 8px; }
        #serial-terminal::-webkit-scrollbar-track { background: #0a0a0a; }
        #serial-terminal::-webkit-scrollbar-thumb { background: #333; border-radius: 4px; }
        #serial-terminal::-webkit-scrollbar-thumb:hover { background: #555; }

        /* Quick Action Toolbar per HISTORY */
        .history-bar {
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
            margin-bottom: 10px;
            flex-shrink: 0;
            background-color: #121212;
            padding: 8px;
            border-radius: 4px;
            border: 1px solid #2a2a2a;
            align-items: center;
        }
        .history-label {
            color: #aaa;
            font-size: 0.85rem;
            font-weight: bold;
            margin-right: 5px;
        }

        /* Filter Bar per REGEX */
        .filter-bar {
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
            margin-bottom: 10px;
            flex-shrink: 0;
            background-color: #121212;
            padding: 8px;
            border-radius: 4px;
            border: 1px solid #2a2a2a;
            align-items: center;
        }
        .filter-label {
            color: #aaa;
            font-size: 0.85rem;
            font-weight: bold;
            min-width: 50px;
        }
        #filterInput {
            flex-grow: 1;
            background-color: #1a1a1a;
            color: #33ff33;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 8px 10px;
            font-family: inherit;
            font-size: 0.9rem;
            outline: none;
            min-width: 200px;
        }
        #filterInput:focus {
            border-color: #33ff33;
        }
        #filterInput::placeholder {
            color: #666;
        }
        .filter-count {
            color: #aaa;
            font-size: 0.85rem;
            min-width: 60px;
        }

        /* Input e Pulsanti Bar */
        .input-bar {
            display: flex;
            gap: 10px;
            flex-shrink: 0;
            flex-wrap: wrap;
        }
        input[type=text] {
            flex-grow: 1;
            background-color: #121212;
            color: #33ff33;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 10px;
            font-family: inherit;
            font-size: 0.95rem;
            outline: none;
            min-width: 200px;
        }
        input[type=text]:focus {
            border-color: #33ff33;
        }
        .btn {
            padding: 8px 14px;
            background-color: #2a2a2a;
            color: #fff;
            border: 1px solid #444;
            border-radius: 4px;
            cursor: pointer;
            font-family: inherit;
            font-size: 0.85rem;
            transition: background-color 0.15s ease, border-color 0.15s ease;
            white-space: nowrap;
        }
        .btn:hover { background-color: #3d3d3d; }
        
        /* Bottoni Azione Specifica */
        .btn-cmd { border-color: #0088cc; color: #66ccff; }
        .btn-cmd:hover { background-color: #004466; }
        
        .btn-danger { border-color: #882222; color: #ff8888; }
        .btn-danger:hover { background-color: #551111; }

        .btn-clear { border-color: #555; }
        .btn-clear:hover { background-color: #444; }
    </style>
</head>
<body>

    <div class="header-container">
        <h1>ESP32 Web Serial Monitor</h1>

        <div class="controls-group">
            <label class="checkbox-container">
                <input type="checkbox" id="autoscroll-check" checked>
                Auto-scroll
            </label>

            <label class="checkbox-container">
                <input type="checkbox" id="shownum-check" onchange="toggleLineNumbers(this.checked)">
                Numerazione
            </label>

            <label class="checkbox-container">
                <input type="checkbox" id="timestamp-check" onchange="toggleTimestamp(this.checked)">
                Timestamp
            </label>

            <div>Utenti: <span id="counter-val" style="color: #66ff66; font-weight: bold;">1</span></div>
        </div>
    </div>

    <div id="serial-terminal">--- Inizializzazione Monitor ---</div>

    <!-- Toolbar per la gestione rapida della HISTORY -->
    <div class="history-bar">
        <span class="history-label">HISTORY:</span>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY ON')">ON</button>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY OFF')">OFF</button>
        <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY INFO')">INFO</button>
        <button class="btn btn-cmd" onclick="window.open('/view-buffer', '_blank');">VIEW</button>
       <!-- <button class="btn btn-cmd" onclick="sendDirectCommand('HISTORY FLUSH')">FLUSH</button> -->
       <!--  <button class="btn btn-danger" onclick="sendDirectCommand('HISTORY CLEAR')">CLEAR</button> -->

	<span class="filter-label">FILTER:</span>
        <input type="text" id="filterInput" placeholder="Regex (es: ERROR|WARN)" autocomplete="off">
        <span class="filter-count" id="filter-count"></span>
        <button class="btn btn-clear" onclick="clearFilter()">Reset</button>


    </div>

    <!-- Filter Bar per REGEX 
    <div class="filter-bar">
        <span class="filter-label">FILTER:</span>
        <input type="text" id="filterInput" placeholder="Regex (es: ERROR|WARN)" autocomplete="off">
        <span class="filter-count" id="filter-count"></span>
        <button class="btn btn-clear" onclick="clearFilter()">Reset</button>
    </div> -->

    <!-- Barra Input Comandi -->
    <div class="input-bar">
        <input type="text" id="commandInput" placeholder="Inserisci comando..." autocomplete="off" autofocus>
        <button class="btn btn-cmd" onclick="sendCommand()">Invia</button>
        <button class="btn btn-clear" onclick="clearMonitor()">Pulisci</button>
    </div>

<script>
if (!!window.EventSource) {
    const source = new EventSource('/events/serial');
    const terminal = document.getElementById('serial-terminal');
    const autoscrollCheck = document.getElementById('autoscroll-check');
    const counterVal = document.getElementById('counter-val');
    const filterCountSpan = document.getElementById('filter-count');
    
    let myClientId = null;
    let rigaNumero = 0;
    let scrollInAttesa = false;
    let showTimestamp = false;
    let lastTimestamp = "";
    let currentFilterRegex = null;

    function toggleLineNumbers(show) {
        if (!terminal) return;
        if (show) {
            terminal.classList.add('show-linenum');
        } else {
            terminal.classList.remove('show-linenum');
        }
    }

    function toggleTimestamp(enable) {
        showTimestamp = enable;
        if (enable) {
            executePost("TIMESTAMP ON");
        } else {
            executePost("TIMESTAMP OFF");
        }
    }
    
    async function requestNumClients() {
        try {
            const response = await fetch('/get-clientcount');
            if (response.ok) {
                const count = await response.text();
                if (counterVal) counterVal.textContent = count;
            }
        } catch (err) {
            console.warn("Impossibile recuperare il conteggio client via GET:", err);
        }
    }
    
    function appendToTerminal(testo) {
        if (!terminal) return;
        
        const righeInArrivo = testo.split("\n");
        const fragment = document.createDocumentFragment();
        
        righeInArrivo.forEach(function(riga) {
            if (riga === "") return; // Skip righe vuote
            
            rigaNumero++;
            
            const nuovaRigaDiv = document.createElement("div");
            nuovaRigaDiv.className = "log-line";
            nuovaRigaDiv.setAttribute("data-line", rigaNumero);
            
            // Prependi timestamp se abilitato e disponibile
            if (showTimestamp && lastTimestamp) {
                nuovaRigaDiv.textContent = lastTimestamp + " " + riga;
                lastTimestamp = "";  // Usa una sola volta
            } else {
                nuovaRigaDiv.textContent = riga;
            }
            
            // Applica filtro se attivo
            if (currentFilterRegex) {
                if (currentFilterRegex.test(nuovaRigaDiv.textContent)) {
                    nuovaRigaDiv.classList.add('match');
                } else {
                    nuovaRigaDiv.classList.add('hidden');
                }
            }
            
            fragment.appendChild(nuovaRigaDiv);
        });

        terminal.appendChild(fragment);
                
        // Manutenzione DOM (limite 2000 righe per fluidità del browser)
        while (terminal.children.length > 2000) {
            terminal.removeChild(terminal.firstChild);
        }
        
        updateFilterCount();
        requestScrollToBottom();
    }
    
    // --- EVENT LISTENER SSE ---
    
    source.addEventListener('message', function(e) {
        if (e.data.startsWith("ID_ASSEGNATO:")) {
            myClientId = e.data.split(":")[1];
            console.log("ID Client assegnato: " + myClientId);
        }
    });
    
    source.addEventListener('open', function(e) {
        appendToTerminal("[INFO] Connessione seriale stabilita.");
        requestNumClients();
    }, false);
    
    source.addEventListener('error', function(e) {
        if (e.readyState === EventSource.CLOSED) {
            appendToTerminal("[ERRORE] Connessione interrotta.");
        }
    }, false);
    
    source.addEventListener('serial_print', function(e) {
        appendToTerminal(e.data);
    }, false);

    source.addEventListener('timestamp', function(e) {
        lastTimestamp = e.data;
    }, false);

source.addEventListener('timestamp_enabled', function(e) {
    showTimestamp = (e.data === "1");
    document.getElementById('timestamp-check').checked = showTimestamp;
    console.log("Timestamp " + (showTimestamp ? "abilitato" : "disabilitato"));
}, false);
    
    source.addEventListener('client_count', function(e) {
        if (counterVal) counterVal.textContent = e.data;
    }, false);
    
    // --- REGEX FILTER ---
    
    function applyFilter() {
        const filterInput = document.getElementById('filterInput');
        const pattern = filterInput.value.trim();
        
        if (!pattern) {
            clearFilter();
            return;
        }
        
        try {
            currentFilterRegex = new RegExp(pattern, 'i'); // 'i' = case-insensitive
            filterInput.style.borderColor = '#33ff33'; // Verde se valida
        } catch (e) {
            console.error("Regex non valida:", e);
            filterInput.style.borderColor = '#ff6666'; // Rosso se invalida
            return;
        }
        
        updateFilterDisplay();
    }

    function updateFilterDisplay() {
        const lines = document.querySelectorAll('.log-line');
        
        lines.forEach(line => {
            if (currentFilterRegex && currentFilterRegex.test(line.textContent)) {
                line.classList.remove('hidden');
                line.classList.add('match');
            } else if (currentFilterRegex) {
                line.classList.add('hidden');
                line.classList.remove('match');
            } else {
                line.classList.remove('hidden', 'match');
            }
        });
        
        updateFilterCount();
    }

    function updateFilterCount() {
        if (!currentFilterRegex) {
            filterCountSpan.textContent = "";
            return;
        }
        
        const matchCount = document.querySelectorAll('.log-line.match').length;
        const totalCount = document.querySelectorAll('.log-line').length;
        filterCountSpan.textContent = `${matchCount}/${totalCount}`;
    }

    function clearFilter() {
        const filterInput = document.getElementById('filterInput');
        filterInput.value = '';
        currentFilterRegex = null;
        filterInput.style.borderColor = '#444';
        filterCountSpan.textContent = '';
        
        const lines = document.querySelectorAll('.log-line');
        lines.forEach(line => {
            line.classList.remove('hidden', 'match');
        });
    }
    
    // --- UTILITIES E INVIO COMANDI ---
    
    function requestScrollToBottom() {
        if (!autoscrollCheck || !autoscrollCheck.checked) return;
        if (scrollInAttesa) return;
        
        scrollInAttesa = true;
        requestAnimationFrame(function() {
            terminal.scrollTop = terminal.scrollHeight;
            scrollInAttesa = false;
        });
    }
    
    function clearMonitor() {
        if (terminal) terminal.innerHTML = "";
        rigaNumero = 0;
        filterCountSpan.textContent = '';
    }
    
    // Invio da input di testo
    function sendCommand() {
        const inputElement = document.getElementById('commandInput');
        if (!inputElement) return;
        
        const val = inputElement.value.trim();
        if (val === "") return;

        executePost(val);
        inputElement.value = '';
    }

    // Invio diretto tramite pulsanti della toolbar
    function sendDirectCommand(cmdString) {
        executePost(cmdString);
    }

    // Helper per inviare la richiesta POST all'ESP32
    function executePost(commandText) {
        const payload = (myClientId ? myClientId : "0") + ':' + commandText;
        
        fetch('/parsingCmd', {
            method: 'POST',
            headers: { 'Content-Type': 'text/plain' },
            body: payload
        }).catch(error => {
            console.error('Errore invio comando:', error);
        });
    }
    
    // Event listeners
    document.getElementById('commandInput')?.addEventListener('keydown', function(event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            sendCommand();
        }
    });

    document.getElementById('filterInput')?.addEventListener('input', applyFilter);
    document.getElementById('filterInput')?.addEventListener('keydown', function(event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            applyFilter();
        }
    });
    
} else {
    alert("Il tuo browser non supporta Server-Sent Events (SSE).");
}
</script>
</body>
</html>
)rawliteral";
