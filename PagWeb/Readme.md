Aca va a estar todas las modificaciones del proyecto.
Comenzamos con la creacion del index.
luego creamos el archivo css, para comenzar con el diseño del index. Luego con gemini adaptamos el codigo css del index a al Control (Uso.html).

archivo uso: le pedimos a gemini que nos creara un archivo que nos permitiera controlar al robot al mismo tiempo que nos mostraba la imagen de la camara y mapeaba que movimientos hizo, le marcamos que componentes vamos a utilizar y a los pines que conectariamos cada cosa para que funcione desde el momento 0 de la conexion, junto con el codigo nos dio que colocar en c++ (dentro del esp32) para activar cada pin y garantizar un correcto funcionamiento

---

## Mejoras de código — 31/08/2026

Se aplicaron 5 mejoras técnicas al proyecto:

### 1. WebSocket en lugar de HTTP (fetch)
- **Archivo nuevo:** `control.css` *(ver punto 3)*
- **Archivo modificado:** `Uso.html`
- Reemplazamos los `fetch()` individuales por una **conexión WebSocket persistente** al ESP32 (`ws://192.168.4.1/ws`).
- Latencia reducida de ~100–300 ms a ~5–15 ms por comando.
- Se agregó **auto-reconexión automática** cada 3 segundos si la conexión se pierde.
- En el ESP32 se debe usar la librería `ESPAsyncWebServer` + `AsyncWebSocket` para recibir los mensajes.

### 2. Hold-to-move con Pointer Events API
- **Archivo modificado:** `Uso.html`
- Se reemplazaron los 3 listeners por botón (`click` + `touchstart` + `touchend`) por **2 listeners** (`pointerdown` / `pointerup`) que unifican mouse, touch y stylus.
- Al mantener un botón presionado, se envían comandos repetidos cada **150 ms** (movimiento continuo).
- Al soltar cualquier botón de movimiento, se envía **STOP automático**.
- Se usa `setPointerCapture()` para mantener el evento aunque el dedo se deslice fuera del botón.

### 3. Separación del CSS de Uso.html
- **Archivo nuevo:** `control.css`
- Se extrajeron las ~337 líneas de CSS embebido en `<style>` dentro de `Uso.html` a un archivo externo `control.css`.
- `Uso.html` ahora linkea el CSS con `<link rel="stylesheet" href="control.css">`.
- Beneficio: el CSS queda en caché del navegador y el HTML es más legible.

### 4. Variables CSS consolidadas
- **Archivos modificados:** `control.css`, `diseño.css`
- Se agregó un bloque `:root { }` con **variables de color nombradas** en ambos archivos CSS.
- Todos los colores hardcodeados (`#1a237e`, `#283593`, `rgba(26,35,126,...)`, etc.) fueron reemplazados por sus variables (`--color-primary`, `--color-primary-hover`, `--color-primary-shadow`, etc.).
- Facilita futuros cambios de tema: basta modificar los valores en `:root`.

### 5. Indicador de conexión en tiempo real
- **Archivo modificado:** `Uso.html`
- El `status-dot` ahora refleja el **estado real del WebSocket**:
  - Gris → conectando
  - Verde (con brillo) → conectado al ESP32
  - Rojo → desconectado o error de red
- Antes solo cambiaba al enviar un comando; ahora responde instantáneamente a la conexión/desconexión.

---

## Adaptación Responsive — 31/08/2026

Se adaptó el proyecto para que funcione correctamente en dispositivos **Android e iOS** sin modificar la lógica del ESP32 ni el diseño visual base.

### Archivos modificados

#### `diseño.css`
- Se agregaron dos bloques `@media` al final del archivo:
  - **`@media (max-width: 768px)` (tablet):** reduce el `h1` a `2rem`, reduce `letter-spacing` y ajusta el `padding` del `body` para que el footer fijo no tape el contenido.
  - **`@media (max-width: 480px)` (móvil):** reduce el `h1` a `1.6rem`, establece área táctil mínima de `48px` de alto en los botones (según guías de Apple y Android), y cambia `.botones` a `flex-direction: column` para apilar los botones verticalmente en pantallas pequeñas.

#### `Index.html`
- Corregido atributo `lang="en"` → `lang="es"`.
- Se movió el `<link rel="stylesheet">` debajo de los `<meta>` (orden correcto para rendimiento).
- Los dos `<a><button>` fueron envueltos en `<div class="botones">` para que el CSS responsive pueda apilarlos en columna en móvil.

#### `Uso.html`
- **CSS interno — `@media (max-width: 768px)`:** el `body` pasa a `justify-content: flex-start` para que el contenido arranque desde arriba (no quede cortado en pantallas cortas). El header `h1` reduce su `letter-spacing`.
- **CSS interno — `@media (max-width: 480px)`:** la `control-grid` pasa de `80px × 80px` a `65px × 65px` por celda para que la cruz de botones quepa en pantallas de 360px. Se reduce la `min-height` de `.view-box`.
- **JS — Soporte táctil (`touchstart`/`touchend`):** Se reemplazó el listener único de `click` por tres listeners por botón (`click`, `touchstart`, `touchend`). Los eventos táctiles usan `e.preventDefault()` para eliminar el delay de 300ms que los navegadores móviles añaden al `click`, y `{ passive: false }` para permitir el `preventDefault`. Esto mejora la respuesta del control en Android/iOS.
- **JS — Canvas responsive (`resizeCanvas()`):** Se agregó una función que ajusta el `canvas.width` al ancho real de su contenedor (`.view-box`) y escala la altura proporcionalmente (ratio ≈ 47%). Se ejecuta al cargar la página y en cada evento `resize` (útil al rotar el dispositivo). Al redimensionar, el historial de trayectoria se reinicia para evitar coordenadas fuera de rango.

---

## Integración con movimiento_willy — 20/10/2026

Se integraron de forma completa los archivos HTML y CSS dentro del sketch del robot `movimiento_willy`:

### 1. Sistema Dual: Memoria Flash (PROGMEM) + SPIFFS
- **Archivo de cabecera:** `movimiento_willy/web_pages.h`
  - Contiene todo el HTML (`PAGE_INDEX`, `PAGE_CONTROL`, `PAGE_EXPLICACION`) y CSS (`CSS_DISENO`, `CSS_CONTROL`) almacenados en memoria Flash del microcontrolador mediante `PROGMEM`.
  - **Ventaja crítica:** No depende obligatoriamente de la herramienta de carga de SPIFFS en el Arduino IDE. Con solo presionar **Subir (Upload)** en el Arduino IDE, la página web ya funciona al 100%.
- **Directorio de datos:** `movimiento_willy/data/`
  - Contiene los archivos físicos `index.html`, `control.html`, `explicacion.html`, `diseno.css` y `control.css` listos para quien desee utilizar la herramienta *ESP32 Sketch Data Upload* (SPIFFS).
  - El servidor web verifica si el archivo existe en SPIFFS; si está presente lo sirve desde ahí, y si no o si SPIFFS no fue cargado, recurre automáticamente a PROGMEM.

### 2. Rutas del Servidor Web (Puerto 80)
- `http://192.168.4.1/` o `/index.html`: Portal de inicio del Proyecto Willy.
- `http://192.168.4.1/control.html` (también `/control`, `/uso.html` y `/DataIno/Index.html`): Panel de teleoperación con video, joystick y mapeo.
- `http://192.168.4.1/explicacion.html`: Documentación y explicación técnica del robot.
- `/diseno.css` y `/control.css`: Hojas de estilo unificadas y responsivas.
- `/ws`: Endpoint WebSocket para control y telemetría de baja latencia.

### 3. Servidor de Video en Vivo MJPEG (Puerto 81)
- Se integró el servidor nativo `esp_http_server` en el puerto 81 (`/stream`), permitiendo transmitir video multipart MJPEG de la cámara OV2640 de forma fluida a la etiqueta `<img>` de la interfaz web.
