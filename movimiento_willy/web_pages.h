#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

// =============================================================================
// PÁGINA DE INICIO (INDEX.HTML)
// =============================================================================
const char PAGE_INDEX[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="diseno.css">
    <title>Robot Willy - Inicio</title>
</head>
<body>
    <div class="container">
        <h1>Proyecto Willy</h1>
        <p class="subtitle">Robot Explorador Autónomo y Teleoperado</p>
        <p>Plataforma robótica móvil con ESP32-CAM, tracción diferencial trasera, transmisión de video en tiempo real, control por WebSockets y mapeo de trayectorias.</p>
        <p style="font-size: 0.88rem; color: var(--color-primary);">Desarrollo de Aplicaciones | Sistemas Informáticos II | Proyectos de Producción</p>
        
        <div class="botones">
            <a href="control.html"><button>Panel de Control</button></a>
            <a href="explicacion.html"><button class="btn-secondary">Explicación del Proyecto</button></a>
        </div>
        
        <footer>Integrantes: Foglia, Pallas, T. Martinez, Milano, Moreno Perez, Zajic</footer>
    </div>
</body>
</html>)rawliteral";

// =============================================================================
// PANEL DE CONTROL (CONTROL.HTML)
// =============================================================================
const char PAGE_CONTROL[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control de Robot Willy</title>
  <link rel="stylesheet" href="control.css">
</head>
<body>

  <div class="dashboard">
    <div class="top-nav">
      <a href="index.html" class="back-link">&#8592; Volver al Inicio</a>
    </div>

    <header>
      <h1>PANEL DE CONTROL</h1>
      <p>Cámara en Vivo, Mapeo y Control Digital</p>
    </header>

    <div class="main-content">

      <!-- Panel Izquierdo: Video y Canvas -->
      <div class="panel-card viewers-container">
        <div class="view-box camera-view-box">
          <span class="view-label">CÁMARA EN VIVO</span>
          <div id="stream-wrapper">
            <img id="video-stream" src="" alt="Cámara Raspberry Pi">
            <div id="stream-placeholder">
              <div class="placeholder-icon">📡</div>
              <div class="placeholder-text">Sin señal</div>
              <div class="placeholder-sub" id="stream-retry-msg">Buscando ESP32...</div>
            </div>
          </div>
        </div>
        <div class="camera-stream-config" style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;width:100%;">
          <label for="stream-url">Stream MJPEG de Raspberry Pi:</label>
          <input id="stream-url" type="url" inputmode="url" placeholder="http://IP-DE-LA-PI:PUERTO/stream" style="flex:1;min-width:210px;padding:10px;border:1px solid #9aa8bd;border-radius:8px;">
          <button id="stream-connect" type="button" style="padding:10px 14px;border:0;border-radius:8px;background:#1a237e;color:white;">Conectar cámara</button>
        </div>

        <div class="view-box map-view-box">
          <span class="view-label">MAPA DE RECORRIDO</span>
          <canvas id="mapaCanvas" width="240" height="360" aria-label="Mapa vertical: el robot comienza abajo y orientado hacia adelante"></canvas>
        </div>
      </div>

      <!-- Panel de botones -->
      <div class="panel-card">
        <div class="controls-panel">
          <h2>Movimiento</h2>
          <p class="controls-hint">Mantené presionado un botón para mover. Al soltar, Willy se detiene.</p>
          <div class="control-grid" role="group" aria-label="Controles de movimiento">
            <button class="btn btn-up" id="btn-F" type="button" aria-label="Avanzar" title="Avanzar">&#9650;</button>
            <button class="btn btn-left" id="btn-L" type="button" aria-label="Girar a la izquierda" title="Izquierda">&#9664;</button>
            <button class="btn btn-stop" id="btn-S" type="button">STOP</button>
            <button class="btn btn-right" id="btn-R" type="button" aria-label="Girar a la derecha" title="Derecha">&#9654;</button>
            <button class="btn btn-down" id="btn-B" type="button" aria-label="Retroceder" title="Retroceder">&#9660;</button>
          </div>
        </div>

        <div class="status-bar">
          <div class="status-dot" id="status-dot"></div>
          <span id="status-text">Conectando...</span>
        </div>

        <div class="keyboard-hint">
          Teclado: flechas o W A S D | Espacio = detener
        </div>
      </div>

    </div>
  </div>

  <script>
    /* ================================================================
       CONFIGURACIÓN DE RED DINÁMICA
    ================================================================ */
    const ESP32_IP = window.location.hostname || "192.168.4.1";
    const WS_URL   = `ws://${ESP32_IP}/ws`;

    const statusDot  = document.getElementById('status-dot');
    const statusText = document.getElementById('status-text');

    /* ================================================================
       WEBSOCKET CON AUTO-RECONEXIÓN
    ================================================================ */
    let ws = null;

    function setStatus(estado, texto) {
      statusDot.className = `status-dot ${estado}`;
      statusText.innerText = texto;
    }

    function conectar() {
      setStatus('', 'Conectando...');
      ws = new WebSocket(WS_URL);

      ws.onopen = () => {
        setStatus('active', 'Conectado al ESP32');
      };

      ws.onclose = () => {
        setStatus('error', 'Desconectado — reintentando...');
        setTimeout(conectar, 2500);
      };

      ws.onerror = () => {
        setStatus('error', 'Error de conexión');
      };
    }

    function enviarComando(dir) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(dir);
      }
    }

    conectar();

    /* ================================================================
       STREAM DE CÁMARA
    ================================================================ */
    const videoImg       = document.getElementById('video-stream');
    const placeholder    = document.getElementById('stream-placeholder');
    const retryMsg       = document.getElementById('stream-retry-msg');
    const streamUrlInput = document.getElementById('stream-url');
    const streamConnect  = document.getElementById('stream-connect');
    let   STREAM_URL     = localStorage.getItem('willyPiStreamUrl') || '';
    let   streamTimer    = null;
    let   streamIntento  = 0;

    streamUrlInput.value = STREAM_URL;

    function mostrarPlaceholder(msg) {
      videoImg.style.display    = 'none';
      placeholder.style.display = 'flex';
      retryMsg.textContent      = msg;
    }

    function ocultarPlaceholder() {
      placeholder.style.display = 'none';
      videoImg.style.display    = 'block';
    }

    function streamConectar() {
      STREAM_URL = streamUrlInput.value.trim();
      if (!STREAM_URL) {
        mostrarPlaceholder('Ingresá la URL MJPEG de la Raspberry Pi');
        return;
      }
      streamIntento++;
      retryMsg.textContent = `Intento ${streamIntento}… conectando con Raspberry Pi`;
      const separador = STREAM_URL.includes('?') ? '&' : '?';
      videoImg.src = `${STREAM_URL}${separador}t=${Date.now()}`;
    }

    streamConnect.addEventListener('click', () => {
      STREAM_URL = streamUrlInput.value.trim();
      localStorage.setItem('willyPiStreamUrl', STREAM_URL);
      streamIntento = 0;
      clearTimeout(streamTimer);
      streamConectar();
    });

    videoImg.onload = () => {
      streamIntento = 0;
      ocultarPlaceholder();
      clearTimeout(streamTimer);
    };

    videoImg.onerror = () => {
      if (!streamUrlInput.value.trim()) {
        mostrarPlaceholder('Ingresá la URL MJPEG de la Raspberry Pi');
        return;
      }
      mostrarPlaceholder(`Reintentando stream… (intento ${streamIntento})`);
      clearTimeout(streamTimer);
      streamTimer = setTimeout(streamConectar, 4000);
    };

    mostrarPlaceholder('Ingresá la URL MJPEG de la Raspberry Pi');
    if (STREAM_URL) streamConectar();

    /* ================================================================
       CANVAS — mapa vertical, con el frente del robot hacia arriba
    ================================================================ */
    const canvas = document.getElementById('mapaCanvas');
    const ctx    = canvas.getContext('2d');

    let posX   = canvas.width  / 2;
    let posY   = canvas.height * 0.78;
    let angulo = 0;

    const historialPuntos = [{ x: posX, y: posY }];

    function renderizarMapa() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      ctx.fillStyle = '#e8edf5';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.strokeStyle = 'rgba(26, 35, 126, 0.12)';
      ctx.lineWidth   = 1;
      for (let x = 0; x < canvas.width; x += 20) {
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke();
      }
      for (let y = 0; y < canvas.height; y += 20) {
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
      }

      if (historialPuntos.length > 1) {
        ctx.beginPath();
        ctx.moveTo(historialPuntos[0].x, historialPuntos[0].y);
        for (let i = 1; i < historialPuntos.length; i++) {
          ctx.lineTo(historialPuntos[i].x, historialPuntos[i].y);
        }
        ctx.strokeStyle = '#89b4fa';
        ctx.lineWidth   = 2;
        ctx.stroke();
      }

      ctx.save();
      ctx.translate(posX, posY);
      ctx.rotate(angulo);
      ctx.beginPath();
      ctx.moveTo(0, -11);
      ctx.lineTo(8, 9);
      ctx.lineTo(0, 5);
      ctx.lineTo(-8, 9);
      ctx.closePath();
      ctx.fillStyle = '#e53967';
      ctx.fill();
      ctx.restore();
    }

    renderizarMapa();

    function actualizarMapa(dir) {
      const paso = 8;
      const giro = 0.12;
      if (dir === 'F') { posX += Math.sin(angulo) * paso; posY -= Math.cos(angulo) * paso; }
      if (dir === 'B') { posX -= Math.sin(angulo) * paso; posY += Math.cos(angulo) * paso; }
      if (dir === 'L') angulo -= giro;
      if (dir === 'R') angulo += giro;
      historialPuntos.push({ x: posX, y: posY });
      renderizarMapa();
    }

    /* Botones táctiles y mouse: movimiento continuo mientras se mantienen */
    const botones = [
      { id: 'btn-F', cmd: 'F' },
      { id: 'btn-B', cmd: 'B' },
      { id: 'btn-L', cmd: 'L' },
      { id: 'btn-R', cmd: 'R' },
      { id: 'btn-S', cmd: 'S' }
    ];

    botones.forEach(({ id, cmd }) => {
      const boton = document.getElementById(id);
      let intervalo = null;
      const iniciar = (e) => {
        e.preventDefault();
        if (intervalo) return;
        try { boton.setPointerCapture(e.pointerId); } catch (error) {}
        boton.classList.add('pressed');
        enviarComando(cmd);
        if (cmd !== 'S') {
          actualizarMapa(cmd);
          intervalo = setInterval(() => { enviarComando(cmd); actualizarMapa(cmd); }, 150);
        }
      };
      const soltar = () => {
        clearInterval(intervalo);
        intervalo = null;
        boton.classList.remove('pressed');
        if (cmd !== 'S') enviarComando('S');
      };
      boton.addEventListener('pointerdown', iniciar);
      boton.addEventListener('pointerup', soltar);
      boton.addEventListener('pointercancel', soltar);
      boton.addEventListener('lostpointercapture', soltar);
    });

    /* ================================================================
       TECLADO — Flechas + WASD
    ================================================================ */
    const keyMap = {
      ArrowUp: 'F', w: 'F', W: 'F', ArrowDown: 'B', s: 'B', S: 'B',
      ArrowLeft: 'L', a: 'L', A: 'L', ArrowRight: 'R', d: 'R', D: 'R'
    };
    let teclaActiva = null;
    let intervaloTecla = null;
    document.addEventListener('keydown', (e) => {
      const cmd = keyMap[e.key];
      if (e.key === ' ') { e.preventDefault(); enviarComando('S'); return; }
      if (!cmd || e.repeat) return;
      e.preventDefault();
      teclaActiva = cmd;
      enviarComando(cmd);
      actualizarMapa(cmd);
      clearInterval(intervaloTecla);
      intervaloTecla = setInterval(() => { enviarComando(cmd); actualizarMapa(cmd); }, 150);
    });
    document.addEventListener('keyup', (e) => {
      const cmd = keyMap[e.key];
      if (cmd && cmd === teclaActiva) {
        clearInterval(intervaloTecla);
        intervaloTecla = null;
        teclaActiva = null;
        enviarComando('S');
      }
    });
    window.addEventListener('blur', () => { clearInterval(intervaloTecla); teclaActiva = null; enviarComando('S'); });

    /* ================================================================
       CANVAS RESPONSIVE
    ================================================================ */
    function resizeCanvas() {
      const newWidth = Math.round(canvas.clientWidth);
      const newHeight = Math.round(canvas.clientHeight);
      if (newWidth > 0 && newHeight > 0 && (newWidth !== canvas.width || newHeight !== canvas.height)) {
        canvas.width  = newWidth;
        canvas.height = newHeight;
        posX = canvas.width  / 2;
        posY = canvas.height * 0.78;
        historialPuntos.length = 0;
        historialPuntos.push({ x: posX, y: posY });
        renderizarMapa();
      }
    }

    window.addEventListener('resize', resizeCanvas);
    resizeCanvas();
  </script>

</body>
</html>

)rawliteral";

// =============================================================================
// PÁGINA DE EXPLICACIÓN (EXPLICACION.HTML)
// =============================================================================
const char PAGE_EXPLICACION[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="description" content="Cómo funciona Willy, robot móvil controlado por Wi-Fi con ESP32-CAM, transmisión de video y tracción diferencial.">
    <link rel="stylesheet" href="diseno.css">
    <title>Proyecto Willy | Cómo funciona</title>
</head>
<body>
    <main class="container explanation">
        <header class="page-header">
            <p class="eyebrow">Proyecto de robótica móvil</p>
            <h1>Conocé a Willy</h1>
            <p class="subtitle">Un robot móvil que combina control remoto, video en vivo y una interfaz web.</p>
            <div class="tech-tags" aria-label="Tecnologías principales">
                <span>ESP32-CAM</span><span>Wi-Fi</span><span>WebSocket</span><span>MJPEG</span>
            </div>
        </header>

        <section class="info-card" aria-labelledby="intro-title">
            <h2 id="intro-title">¿Qué hace el proyecto?</h2>
            <p>Willy es una plataforma móvil teleoperada. Una persona se conecta a la red Wi-Fi del robot, abre el panel desde el celular o la computadora y envía órdenes de movimiento. Al mismo tiempo puede ver la cámara y una representación gráfica del recorrido estimado.</p>
            <p>El sistema documentado se controla a distancia: no incluye sensores de distancia ni un modo autónomo para detectar y esquivar obstáculos.</p>
        </section>

        <section class="info-card" aria-labelledby="flujo-title">
            <h2 id="flujo-title">Cómo viaja una orden</h2>
            <p>El control, el video y la trayectoria se presentan juntos en el navegador, pero cada función sigue un camino distinto.</p>
            <div class="flow-diagram" role="img" aria-label="El navegador envía órdenes por Wi-Fi al ESP32-CAM, que controla el L298N y los motores. La cámara envía video al navegador y el panel dibuja el movimiento estimado.">
                <div class="flow-node"><span class="flow-icon" aria-hidden="true">⌖</span><strong>Panel web</strong><small>Celular o PC</small></div>
                <span class="flow-arrow" aria-hidden="true">⇄</span>
                <div class="flow-node flow-core"><span class="flow-icon" aria-hidden="true">◉</span><strong>ESP32-CAM</strong><small>Wi-Fi y control</small></div>
                <span class="flow-arrow" aria-hidden="true">→</span>
                <div class="flow-node"><span class="flow-icon" aria-hidden="true">⚙</span><strong>L298N</strong><small>Potencia y dirección</small></div>
                <span class="flow-arrow" aria-hidden="true">→</span>
                <div class="flow-node"><span class="flow-icon" aria-hidden="true">◖</span><strong>Motores</strong><small>Desplazan el robot</small></div>
            </div>
            <div class="data-lanes">
                <p><strong>Órdenes:</strong> navegador → WebSocket → ESP32 → controlador L298N.</p>
                <p><strong>Video:</strong> cámara OV2640 → flujo MJPEG → navegador.</p>
                <p><strong>Mapa:</strong> el panel dibuja el movimiento a partir de los comandos enviados; no es una medición de posición hecha por sensores.</p>
            </div>
        </section>

        <section class="info-card" aria-labelledby="hardware-title">
            <h2 id="hardware-title">Componentes y conexiones de control</h2>
            <p>La tracción es diferencial: dos motores DC independientes impulsan los lados izquierdo y derecho. Las ruedas delanteras libres sirven de apoyo y permiten que el chasis gire.</p>
            <div class="table-wrap">
                <table>
                    <thead><tr><th>ESP32-CAM</th><th>L298N</th><th>Función</th></tr></thead>
                    <tbody>
                        <tr><td>GPIO 14</td><td>ENA</td><td>Velocidad del motor A (PWM)</td></tr>
                        <tr><td>GPIO 12</td><td>IN1</td><td>Dirección del motor A</td></tr>
                        <tr><td>GPIO 13</td><td>IN2</td><td>Dirección del motor A</td></tr>
                        <tr><td>GPIO 15</td><td>IN3</td><td>Dirección del motor B</td></tr>
                        <tr><td>GPIO 2</td><td>IN4</td><td>Dirección del motor B</td></tr>
                        <tr><td>GPIO 4</td><td>ENB</td><td>Velocidad del motor B (PWM)</td></tr>
                    </tbody>
                </table>
            </div>
            <p class="small-note">El programa actual controla dos motores DC. Los servos mencionados en otros materiales no están conectados ni configurados en este firmware.</p>
        </section>

        <section class="info-card" aria-labelledby="software-title">
            <h2 id="software-title">Red, control y respuesta</h2>
            <ol class="steps-list">
                <li><strong>Conexión directa:</strong> el robot crea la red <code>Robot_Willy_AP</code>, con dirección local <code>192.168.4.1</code>. La contraseña está vacía en la configuración actual.</li>
                <li><strong>Órdenes:</strong> el navegador mantiene una conexión WebSocket en <code>/ws</code>. Los botones se pueden mantener presionados; al soltarlos se envía la orden de detenerse. También se admite teclado.</li>
                <li><strong>Movimiento:</strong> el ESP32 cambia el sentido de giro y la señal PWM de cada motor mediante el L298N. Así puede avanzar, retroceder, detenerse o curvar hacia un lado.</li>
                <li><strong>Cámara:</strong> la OV2640 envía video MJPEG por el puerto 81. La imagen requiere que la cámara haya iniciado correctamente.</li>
                <li><strong>Interfaz:</strong> el indicador muestra la conexión del control. Si se pierde WebSocket, el firmware detiene los motores al desconectar el cliente.</li>
            </ol>
        </section>

        <section class="info-card" aria-labelledby="uso-title">
            <h2 id="uso-title">Puesta en marcha</h2>
            <ol class="steps-list">
                <li>Encendé el robot y conectá el teléfono o la computadora a <code>Robot_Willy_AP</code>.</li>
                <li>Abrí <code>http://192.168.4.1</code> en el navegador y entrá al panel de control.</li>
                <li>Comprobá el estado de conexión y la señal de cámara. Usá movimientos breves en un espacio despejado.</li>
                <li>Soltá el control o presioná <strong>STOP</strong> para frenar. Si se corta la conexión, volvé a conectarte a la red del robot.</li>
            </ol>
            <p class="small-note">Alimentación: los motores requieren una fuente adecuada conectada al L298N. El ESP32-CAM se alimenta por separado y todas las tierras deben compartir GND. No conectes la batería de motores directamente al ESP32.</p>
        </section>

        <nav class="botones" aria-label="Navegación del proyecto">
            <a href="control.html"><button type="button">Abrir panel de control</button></a>
            <a href="index.html"><button type="button" class="btn-secondary">Volver al inicio</button></a>
        </nav>
        <footer>Integrantes: Foglia, Pallas, T. Martinez, Milano, Moreno Perez y Zajic</footer>
    </main>
</body>
</html>
)rawliteral";

// =============================================================================
// ESTILOS DE INICIO Y EXPLICACIÓN (DISENO.CSS)
// =============================================================================
const char CSS_DISENO[] PROGMEM = R"rawliteral(/*
  =====================================================================
  PROYECTO WILLY — diseno.css
  Estilos del Index (página de inicio) y Explicación
  Colores consolidados en variables CSS en :root
  =====================================================================
*/

/* VARIABLES GLOBALES DE COLOR */
:root {
  --color-primary:              #1a237e;
  --color-primary-hover:        #283593;
  --color-primary-shadow:       rgba(26, 35, 126, 0.30);
  --color-primary-shadow-hover: rgba(26, 35, 126, 0.40);

  --color-bg:                   #f0f4f8;
  --color-card:                 #ffffff;
  --color-text-main:            #333333;
  --color-text-muted:           #555555;
  --color-footer-text:          #c5cae9;
  --color-border:               #c5cae9;
}

/* RESET Y BASE */
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background-color: var(--color-bg);
  color: var(--color-text-main);
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 40px 20px 70px 20px;
}

/* CONTENEDOR CENTRAL */
.container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  width: 100%;
  max-width: 720px;
  gap: 22px;
}

/* TÍTULO PRINCIPAL */
h1 {
  font-size: 2.6rem;
  color: var(--color-primary);
  text-align: center;
  text-transform: uppercase;
  letter-spacing: 3px;
  text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.15);
}

.subtitle {
  font-size: 1.15rem;
  font-weight: 600;
  color: var(--color-primary-hover);
}

/* PÁRRAFOS */
p {
  font-size: 1.05rem;
  color: var(--color-text-muted);
  text-align: center;
  max-width: 600px;
  line-height: 1.7;
}

/* TARJETA DE CONTENIDO (para explicacion.html) */
.info-card {
  background: var(--color-card);
  border: 1px solid var(--color-border);
  border-radius: 16px;
  padding: 24px;
  text-align: left;
  box-shadow: 0 8px 24px var(--color-primary-shadow);
  display: flex;
  flex-direction: column;
  gap: 16px;
  width: 100%;
}

.info-card h2 {
  font-size: 1.3rem;
  color: var(--color-primary);
  border-bottom: 2px solid var(--color-border);
  padding-bottom: 6px;
}

.info-card p, .info-card ul {
  text-align: left;
  font-size: 0.98rem;
  line-height: 1.6;
  color: var(--color-text-main);
}

.info-card ul {
  padding-left: 20px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

/* Recursos visuales de la explicación */
.explanation {
  max-width: 900px;
  align-items: stretch;
  text-align: left;
}

.page-header {
  text-align: center;
  padding: 12px 0 4px;
}

.page-header .eyebrow {
  color: var(--color-primary-hover);
  text-transform: uppercase;
  letter-spacing: 2px;
  font-size: 0.78rem;
  font-weight: 700;
  margin: 0 auto 8px;
}

.page-header h1 { text-transform: none; }
.page-header .subtitle { margin: 8px auto 16px; }

.tech-tags {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 8px;
}

.tech-tags span {
  padding: 6px 12px;
  border-radius: 999px;
  background: #e8edf5;
  color: var(--color-primary);
  font-size: 0.82rem;
  font-weight: 700;
}

.flow-diagram {
  display: flex;
  align-items: center;
  justify-content: center;
  flex-wrap: wrap;
  gap: 10px;
  padding: 20px 12px;
  background: #f4f7fc;
  border: 1px solid #d9e1ef;
  border-radius: 12px;
}

.flow-node {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 5px;
  min-width: 112px;
  padding: 12px 10px;
  border: 1px solid #d9e1ef;
  border-radius: 10px;
  background: #fff;
  text-align: center;
}

.flow-node strong { color: var(--color-primary); font-size: 0.9rem; }
.flow-node small { color: var(--color-text-muted); font-size: 0.77rem; }
.flow-core { border-color: var(--color-primary); box-shadow: 0 3px 12px var(--color-primary-shadow); }
.flow-icon { color: var(--color-primary-hover); font-size: 1.45rem; line-height: 1; }
.flow-arrow { color: var(--color-primary-hover); font-size: 1.4rem; font-weight: 700; }

.data-lanes { display: grid; gap: 8px; }
.data-lanes p, .small-note { margin: 0; font-size: 0.9rem; }
.small-note { color: var(--color-text-muted) !important; }

.table-wrap { width: 100%; overflow-x: auto; }
.table-wrap table { width: 100%; border-collapse: collapse; font-size: 0.92rem; }
.table-wrap th, .table-wrap td { padding: 10px 12px; border: 1px solid #d9d9d9; text-align: left; }
.table-wrap th { background: var(--color-primary); color: #fff; }
.table-wrap tbody tr:nth-child(even) { background: #f4f7fc; }
.table-wrap td:first-child, .table-wrap td:nth-child(2) { font-weight: 700; white-space: nowrap; }

.steps-list { padding-left: 22px; display: flex; flex-direction: column; gap: 10px; line-height: 1.65; }
.steps-list li { padding-left: 3px; }
.steps-list code, .info-card code { color: var(--color-primary); font-weight: 600; }
.info-card h2 { border-bottom: 0; padding-bottom: 0; }
.info-card { border-top: 3px solid #c5cae9; }
.explanation .botones { align-self: center; }
.explanation footer { position: static; width: 100%; border-radius: 10px 10px 0 0; }

/* FILA DE BOTONES */
.botones {
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: center;
  flex-wrap: wrap;
  gap: 16px;
  margin-top: 10px;
}

/* BOTONES */
a {
  text-decoration: none;
}

button, .btn-action {
  background-color: var(--color-primary);
  color: white;
  border: none;
  padding: 14px 28px;
  font-size: 1rem;
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  font-weight: 600;
  border-radius: 8px;
  cursor: pointer;
  letter-spacing: 1px;
  transition: background-color 0.3s ease, transform 0.2s ease, box-shadow 0.3s ease;
  box-shadow: 0 4px 10px var(--color-primary-shadow);
  display: inline-block;
  text-align: center;
}

button:hover, .btn-action:hover {
  background-color: var(--color-primary-hover);
  transform: translateY(-3px);
  box-shadow: 0 8px 16px var(--color-primary-shadow-hover);
}

button:active, .btn-action:active {
  transform: translateY(0);
  box-shadow: 0 2px 6px var(--color-primary-shadow);
}

.btn-secondary {
  background-color: transparent;
  color: var(--color-primary);
  border: 2px solid var(--color-primary);
  box-shadow: none;
}

.btn-secondary:hover {
  background-color: var(--color-primary);
  color: #ffffff;
}

/* FOOTER */
footer {
  position: fixed;
  bottom: 0;
  left: 0;
  width: 100%;
  background-color: var(--color-primary);
  color: var(--color-footer-text);
  text-align: center;
  padding: 12px;
  font-size: 0.85rem;
  letter-spacing: 0.5px;
  z-index: 10;
}

/* RESPONSIVE TABLET */
@media (max-width: 768px) {
  body {
    padding: 30px 16px 70px 16px;
    justify-content: flex-start;
  }

  h1 {
    font-size: 2rem;
    letter-spacing: 2px;
  }

  .container {
    gap: 18px;
  }

  button, .btn-action {
    padding: 14px 22px;
    font-size: 0.95rem;
  }
}

/* RESPONSIVE MÓVIL */
@media (max-width: 480px) {
  body {
    padding: 24px 12px 70px 12px;
  }

  h1 {
    font-size: 1.5rem;
    letter-spacing: 1px;
  }

  p {
    font-size: 0.95rem;
  }

  button, .btn-action {
    min-height: 48px;
    padding: 12px 20px;
    font-size: 0.9rem;
    width: 100%;
    max-width: 280px;
  }

  .botones {
    flex-direction: column;
    align-items: center;
    gap: 12px;
    width: 100%;
  }

  .info-card { padding: 19px 16px; }
  .flow-diagram { gap: 6px; padding: 14px 8px; }
  .flow-node { min-width: 105px; }
  .flow-arrow { font-size: 1.1rem; }
  .table-wrap table { font-size: 0.82rem; }
  .table-wrap th, .table-wrap td { padding: 8px; }

  footer {
    font-size: 0.75rem;
    padding: 10px 8px;
  }
}

)rawliteral";

// =============================================================================
// ESTILOS DEL PANEL DE CONTROL (CONTROL.CSS)
// =============================================================================
const char CSS_CONTROL[] PROGMEM = R"rawliteral(/* 
  =====================================================================
  PROYECTO WILLY — control.css
  Estilos del Panel de Control
  =====================================================================
*/

/* VARIABLES GLOBALES DE COLOR Y FORMA */
:root {
  /* Paleta principal (azul) */
  --color-primary:        #1a237e;               /* Azul oscuro base */
  --color-primary-hover:  #283593;               /* Hover de botones */
  --color-primary-active: #3949ab;               /* Botón presionado */
  --color-primary-bg:     rgba(26, 35, 126, 0.12); /* Fondo translúcido */
  --color-primary-border: rgba(26, 35, 126, 0.20); /* Borde translúcido */
  --color-primary-shadow: rgba(26, 35, 126, 0.15); /* Sombra de tarjetas */

  /* Botón STOP (rojo) */
  --color-danger:         #b00020;               /* Texto del STOP */
  --color-danger-bg:      #fce4ec;               /* Fondo rosado del STOP */
  --color-danger-border:  #ef9a9a;               /* Borde del STOP */
  --color-danger-hover:   #ffc1cc;               /* Hover del STOP */
  --color-danger-press:   #7f0000;               /* STOP al presionar */

  /* Estado de conexión */
  --color-success:        #4caf50;               /* Punto verde (conectado) */
  --color-success-glow:   rgba(76, 175, 80, 0.6);/* Brillo del punto verde */
  --color-error:          #f44336;               /* Punto rojo (error) */
  --color-dot-idle:       #b0bec5;               /* Punto gris (inactivo) */

  /* Fondo y texto */
  --color-bg:             #f0f4f8;               /* Fondo de la página */
  --color-card:           #ffffff;               /* Fondo de los paneles */
  --color-canvas-bg:      #e8edf5;               /* Fondo de los view-box */
  --color-text-main:      #333333;               /* Texto principal */
  --color-text-muted:     #555555;               /* Texto secundario */
  --color-border:         #c5cae9;               /* Bordes */
}

/* RESET BASE */
* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background-color: var(--color-bg);
  color: var(--color-text-main);
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 20px;
}

/* CONTENEDOR PRINCIPAL */
.dashboard {
  display: flex;
  flex-direction: column;
  gap: 18px;
  width: 100%;
  max-width: 980px;
}

/* NAVEGACIÓN SUPERIOR */
.top-nav {
  display: flex;
  justify-content: flex-start;
  align-items: center;
}

.back-link {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  color: var(--color-primary);
  text-decoration: none;
  font-weight: 600;
  font-size: 0.9rem;
  padding: 6px 14px;
  border-radius: 8px;
  background-color: var(--color-card);
  border: 1px solid var(--color-border);
  box-shadow: 0 2px 6px rgba(0,0,0,0.06);
  transition: all 0.2s ease;
}

.back-link:hover {
  background-color: var(--color-primary);
  color: #ffffff;
  transform: translateX(-3px);
}

/* ENCABEZADO */
header {
  text-align: center;
}

header h1 {
  font-size: 1.6rem;
  font-weight: 600;
  color: var(--color-primary);
  text-transform: uppercase;
  letter-spacing: 2px;
}

header p {
  font-size: 0.85rem;
  color: var(--color-text-muted);
  margin-top: 5px;
}

/* LAYOUT RESPONSIVE GRID */
.main-content {
  display: grid;
  grid-template-columns: 1fr;
  gap: 20px;
}

@media (min-width: 768px) {
  .main-content {
    grid-template-columns: minmax(0, 1.15fr) minmax(300px, 0.85fr);
  }
}

/* TARJETAS DE PANELES */
.panel-card {
  background-color: var(--color-card);
  border: 1px solid var(--color-border);
  border-radius: 16px;
  padding: 22px;
  box-shadow: 0 6px 20px var(--color-primary-shadow);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}

/* CONTENEDOR DE CÁMARA Y MAPA */
.viewers-container {
  display: flex;
  flex-direction: column;
  gap: 14px;
  width: 100%;
  background-color: transparent;
  border: 0;
  padding: 0;
  box-shadow: none;
}

/* Caja individual: cámara o mapa */
.view-box {
  width: 100%;
  background-color: var(--color-canvas-bg);
  border: 1px solid var(--color-border);
  border-radius: 12px;
  overflow: hidden;
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 180px;
}

.view-box img,
.view-box canvas {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: block;
}

.camera-view-box {
  min-height: 0;
  aspect-ratio: 4 / 3;
  background: #dde4ef;
}

/* Etiqueta superpuesta */
.view-label {
  position: absolute;
  top: 8px;
  left: 12px;
  background: var(--color-primary-bg);
  padding: 4px 8px;
  border-radius: 6px;
  font-size: 0.75rem;
  color: var(--color-primary);
  border: 1px solid var(--color-primary-border);
  z-index: 2;
  font-weight: 600;
}

/* STREAM WRAPPER Y PLACEHOLDER */
#stream-wrapper {
  width: 100%;
  height: 100%;
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}

#stream-wrapper img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: none;
}

#stream-placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  width: 100%;
  height: 100%;
  min-height: 180px;
  background-color: var(--color-canvas-bg);
  color: var(--color-text-muted);
  user-select: none;
}

.placeholder-icon {
  font-size: 2.5rem;
  animation: pulso 2s ease-in-out infinite;
}

@keyframes pulso {
  0%, 100% { opacity: 1;   transform: scale(1);    }
  50%       { opacity: 0.4; transform: scale(0.92); }
}

.placeholder-text {
  font-size: 0.95rem;
  font-weight: 600;
  color: var(--color-primary);
  letter-spacing: 1px;
}

.placeholder-sub {
  font-size: 0.72rem;
  color: var(--color-text-muted);
  text-align: center;
  padding: 0 12px;
}

/* BOTONERA COMPACTA */
.controls-panel {
  width: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 9px;
  text-align: center;
}

.controls-panel h2 {
  color: var(--color-primary);
  font-size: 1rem;
}

.controls-hint {
  max-width: 250px;
  color: var(--color-text-muted);
  font-size: 0.78rem;
  line-height: 1.45;
}

.control-grid {
  display: grid;
  grid-template-columns: repeat(3, 56px);
  grid-template-rows: repeat(3, 56px);
  gap: 8px;
  justify-content: center;
  margin: 5px auto 0;
}

.btn {
  min-width: 48px;
  min-height: 48px;
  display: grid;
  place-items: center;
  border: 1px solid var(--color-border);
  border-radius: 12px;
  background: var(--color-primary);
  color: #fff;
  font: inherit;
  font-size: 1.15rem;
  font-weight: 700;
  cursor: pointer;
  user-select: none;
  -webkit-tap-highlight-color: transparent;
  touch-action: none;
  transition: background-color .15s ease, transform .1s ease;
}

.btn:hover { background: var(--color-primary-hover); }
.btn:active, .btn.pressed { background: var(--color-primary-active); transform: scale(.96); }
.btn:focus-visible { outline: 3px solid rgba(26, 35, 126, .25); outline-offset: 2px; }
.btn-up { grid-column: 2; grid-row: 1; }
.btn-left { grid-column: 1; grid-row: 2; }
.btn-stop {
  grid-column: 2;
  grid-row: 2;
  background: var(--color-danger-bg);
  border-color: var(--color-danger-border);
  color: var(--color-danger);
  font-size: .72rem;
  letter-spacing: .4px;
}
.btn-stop:hover { background: var(--color-danger-hover); }
.btn-stop:active, .btn-stop.pressed { background: var(--color-danger-press); color: #fff; }
.btn-right { grid-column: 3; grid-row: 2; }
.btn-down { grid-column: 2; grid-row: 3; }

.map-view-box { min-height: 330px; padding: 18px 10px 10px; }
#mapaCanvas { width: min(100%, 190px); height: auto; aspect-ratio: 2 / 3; object-fit: contain; }

/* BARRA DE ESTADO DE CONEXIÓN */
.status-bar {
  margin-top: 18px;
  padding-top: 14px;
  border-top: 1px solid var(--color-border);
  font-size: 0.8rem;
  color: var(--color-text-muted);
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  width: 100%;
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: var(--color-dot-idle);
  transition: background-color 0.3s ease, box-shadow 0.3s ease;
}

.status-dot.active {
  background-color: var(--color-success);
  box-shadow: 0 0 8px var(--color-success-glow);
}

.status-dot.error {
  background-color: var(--color-error);
  box-shadow: none;
}

/* INSTRUCCIONES RÁPIDAS DE TECLADO */
.keyboard-hint {
  margin-top: 12px;
  font-size: 0.72rem;
  color: var(--color-text-muted);
  text-align: center;
}

/* RESPONSIVE TABLET */
@media (max-width: 768px) {
  body {
    padding: 16px;
    justify-content: flex-start;
  }

  .dashboard {
    gap: 16px;
  }

  header h1 {
    font-size: 1.3rem;
    letter-spacing: 2px;
  }
}

/* RESPONSIVE MÓVIL */
@media (max-width: 480px) {
  header h1 {
    font-size: 1.1rem;
    letter-spacing: 1px;
  }

  .view-box {
    min-height: 150px;
  }

  .camera-view-box { min-height: 0; }
  .map-view-box { min-height: 330px; }
  .control-grid { grid-template-columns: repeat(3, 54px); grid-template-rows: repeat(3, 54px); gap: 7px; }
  .panel-card { padding: 18px; }
}

)rawliteral";

#endif // WEB_PAGES_H

