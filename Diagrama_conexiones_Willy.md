# Conexiones de Willy: ESP32, L298N y servos

El plano visual está en [Diagrama_conexiones_Willy.svg](./Diagrama_conexiones_Willy.svg).

## Mapa de señales

| Desde ESP32 | Hacia | Uso |
| --- | --- | --- |
| GPIO14 | ENA | PWM de velocidad del motor A |
| GPIO27 | IN1 | Dirección del motor A |
| GPIO26 | IN2 | Dirección del motor A |
| GPIO32 | ENB | PWM de velocidad del motor B |
| GPIO25 | IN3 | Dirección del motor B |
| GPIO33 | IN4 | Dirección del motor B |
| GPIO18* | Señal del servo 1 | Señal de posición |
| GPIO19* | Señal del servo 2 | Señal de posición |

`*` GPIO18 y GPIO19 son una propuesta para completar el cableado: no están asignados en el firmware actual `movimiento_willy/movimiento_willy.ino`, que maneja únicamente los dos motores DC. Para mover los servos habrá que configurar esas señales en el firmware y definir sus ángulos/función.

## Potencia y motores

1. Batería de motores (+) → terminal `Vs`, `12V` o `+12V` del módulo L298N (el nombre depende de la placa).
2. Batería de motores (−) → `GND` del L298N.
3. `OUT1` y `OUT2` → los dos cables del motor DC A.
4. `OUT3` y `OUT4` → los dos cables del motor DC B.
5. Retirar los jumpers `ENA` y `ENB` del L298N si el módulo los trae; así GPIO14 y GPIO32 pueden aplicar PWM. Mantenerlos puestos anularía el control de velocidad por esos GPIO.
6. Alimentar el ESP32 por USB o por una entrada regulada apropiada para la placa. No conectar la batería de motores directamente a un pin de alimentación del ESP32.

## Servos y tierra común

1. Usar un convertidor DC-DC regulado a **5 V** para los servos, con corriente suficiente para los dos servos bajo carga. Confirmar el voltaje permitido por el modelo concreto de servo.
2. Cable rojo de cada servo → `+5 V` del convertidor; cable marrón/negro → `GND`.
3. Cable de señal del servo 1 → GPIO18; señal del servo 2 → GPIO19.
4. Unir en común: negativo de la batería, GND de salida del convertidor, GND del ESP32, GND del L298N y GND de ambos servos. Las señales solo funcionan correctamente con esta referencia compartida.
5. No alimentar los servos desde `3V3` del ESP32. No conectar la salida `5V` del L298N al ESP32 sin verificar el modelo exacto de módulo y la alimentación de la placa.

## Notas para el montaje

- El L298N es el controlador de los motores DC, no de los servos.
- En muchos módulos L298N, el borne de alimentación de motor está rotulado `12V` aunque el voltaje real sea el de la batería dentro del rango permitido por ese módulo. Seguir la serigrafía del módulo concreto.
- Si un motor gira en sentido contrario al esperado, intercambiar sus dos cables en la salida correspondiente (`OUT1/OUT2` o `OUT3/OUT4`).
- Una pila rectangular alcalina de 9 V normalmente no entrega bien los picos de corriente de motores y servos; usar un paquete de celdas/regulador adecuado a la corriente y al voltaje requerido por los motores.
- El documento de fases registra positivo/negativo por colores para el montaje. Confirmar la polaridad con multímetro antes de conectar la batería.

## Base usada para el diagrama

- GPIO del L298N: `movimiento_willy/movimiento_willy.ino`.
- Alimentación de motores de 9 V y tierra compartida: `Fases de willy.docx`.
- Dos servomotores como componentes: `Fases de willy.docx`; no se encontró asignación de pines ni alimentación de servos en los archivos del proyecto. Por eso se muestran GPIO18/19 y el regulador de 5 V como propuesta explícita.
