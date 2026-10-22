-- ROC_brakes_rc5_global_auto.lua
--
-- Freno automatico usando RC1 + RC2 + RC5 y RELAY3/RELAY4.
-- Version actualizada para AUTO:
--   - En MANUAL/ACRO/etc. mantiene la logica de zonas muertas RC1/RC2.
--   - En AUTO ignora las zonas muertas de RC1/RC2 para no bloquear la mision.
--   - RC5 se mantiene como condicion global de seguridad.
--   - Si el Rover esta desarmado, los frenos quedan aplicados.
--
-- Entradas:
--   RC1 = giro izquierda/derecha
--   RC2 = adelante/atras
--   RC5 = habilita/libera ambos frenos cuando esta ON
--
-- Salidas:
--   Relay3 = freno izquierdo
--   Relay4 = freno derecho
--
-- Logica:
--   DESARMADO:
--      -> freno izquierdo ON
--      -> freno derecho ON
--
--   ARMADO + RC5 OFF o sin senal:
--      -> freno izquierdo ON
--      -> freno derecho ON
--      RC5 funciona como bloqueo global en cualquier modo.
--
--   ARMADO + RC5 ON + modo AUTO:
--      -> freno izquierdo OFF
--      -> freno derecho OFF
--      En AUTO no se usan RC1/RC2 para decidir freno, porque normalmente
--      los sticks permanecen centrados mientras la mision genera el movimiento.
--
--   ARMADO + RC5 ON + modo distinto de AUTO:
--      Si RC1 y RC2 estan dentro de banda muerta:
--         -> frenos ON
--      Si RC1 o RC2 salen de banda muerta:
--         -> frenos OFF
--
-- IMPORTANTE:
--   Deja RC5_OPTION = 0.
--   El script lee RC5, pero ArduPilot no debe accionarlo directamente.
--
--   RC6 ya no se usa para controlar los frenos.
--   Se recomienda dejar RC6_OPTION = 0 si no se va a usar para otra funcion.
--
-- Relay indexing en Lua:
--   Relay1 = 0
--   Relay2 = 1
--   Relay3 = 2
--   Relay4 = 3

local RC_STEERING = 1          -- RC1: giro izquierda/derecha
local RC_THROTTLE = 2          -- RC2: adelante/atras

local CENTER_RC1 = 1500        -- Centro nominal RC1
local CENTER_RC2 = 1500        -- Centro nominal RC2

local DEADBAND_RC1 = 60        -- Banda muerta RC1: CENTER_RC1 +/- 60 us
local DEADBAND_RC2 = 60        -- Banda muerta RC2: CENTER_RC2 +/- 60 us

local RELAY_LEFT = 2           -- Relay3 = freno izquierdo
local RELAY_RIGHT = 3          -- Relay4 = freno derecho

-- Hardware verificado con logica invertida:
--   enable_brake = true  -> relay:off()
--   enable_brake = false -> relay:on()
local BRAKE_ACTIVE_HIGH = false

-- Switch global de seguridad:
-- En esta version se mantiene la convencion del script original:
--   RC5 >= ENABLE_PWM -> permite liberar frenos
--   RC5 <  ENABLE_PWM -> frenos ON siempre
local RC_BRAKE_ENABLE = 5      -- RC5 habilita/libera ambos frenos
local ENABLE_PWM = 1700        -- Switch ON si RC5 >= 1700 us

-- Modo AUTO en ArduPilot Rover
local MODE_AUTO = 10

local UPDATE_MS = 20           -- 50 Hz
local warned = false
local last_armed_state = nil
local last_auto_state = nil

local function brake_relay(relay_num, enable_brake)
    if BRAKE_ACTIVE_HIGH then
        if enable_brake then
            relay:on(relay_num)     -- salida 1 = freno
        else
            relay:off(relay_num)    -- salida 0 = libre
        end
    else
        if enable_brake then
            relay:off(relay_num)
        else
            relay:on(relay_num)
        end
    end
end

local function apply_both_brakes()
    brake_relay(RELAY_LEFT, true)
    brake_relay(RELAY_RIGHT, true)
end

local function release_both_brakes()
    brake_relay(RELAY_LEFT, false)
    brake_relay(RELAY_RIGHT, false)
end

local function switch_is_on(ch)
    local pwm = rc:get_pwm(ch)

    -- Seguridad: si no hay senal del switch, se considera OFF.
    -- OFF = freno aplicado.
    if pwm == nil or pwm == false then
        return false
    end

    return pwm >= ENABLE_PWM
end

local function channel_in_deadband(ch, center, deadband)
    local pwm = rc:get_pwm(ch)

    -- Seguridad: si no hay senal RC valida, se considera en deadband.
    -- Esto evita liberar freno por perdida de senal en modos manuales.
    if pwm == nil or pwm == false then
        return true
    end

    return math.abs(pwm - center) <= deadband
end

local function is_auto_mode()
    return vehicle:get_mode() == MODE_AUTO
end

function update()
    if not warned then
        if not relay:enabled(RELAY_LEFT) then
            gcs:send_text(3, "Lua brake: Relay3 no parece habilitado")
        end
        if not relay:enabled(RELAY_RIGHT) then
            gcs:send_text(3, "Lua brake: Relay4 no parece habilitado")
        end

        gcs:send_text(6, "Lua brake: RC5 global + AUTO sin deadband")
        warned = true
    end

    local armed = arming:is_armed()

    if armed ~= last_armed_state then
        if armed then
            gcs:send_text(6, "Lua brake: vehiculo ARMADO")
        else
            gcs:send_text(6, "Lua brake: vehiculo DESARMADO, frenos ON")
        end
        last_armed_state = armed
    end

    -- Regla 1 de seguridad:
    -- Si el vehiculo esta desarmado, nunca liberar frenos.
    if not armed then
        apply_both_brakes()
        return update, UPDATE_MS
    end

    -- Regla 2 de seguridad:
    -- RC5 se mantiene como bloqueo global en cualquier modo.
    -- Si RC5 esta OFF o no hay senal RC5, los frenos quedan ON.
    local rc5_release_enabled = switch_is_on(RC_BRAKE_ENABLE)
    if not rc5_release_enabled then
        apply_both_brakes()
        return update, UPDATE_MS
    end

    local auto_mode = is_auto_mode()

    if auto_mode ~= last_auto_state then
        if auto_mode then
            gcs:send_text(6, "Lua brake: AUTO, ignorando deadband RC1/RC2")
        else
            gcs:send_text(6, "Lua brake: modo manual, usando deadband RC1/RC2")
        end
        last_auto_state = auto_mode
    end

    -- En AUTO, RC1/RC2 no deben aplicar freno por estar centrados.
    -- Si llegamos aqui, el vehiculo esta armado y RC5 permite liberar frenos.
    if auto_mode then
        release_both_brakes()
        return update, UPDATE_MS
    end

    -- En modos no AUTO se conserva la logica original de zonas muertas.
    local rc1_in_deadband = channel_in_deadband(RC_STEERING, CENTER_RC1, DEADBAND_RC1)
    local rc2_in_deadband = channel_in_deadband(RC_THROTTLE, CENTER_RC2, DEADBAND_RC2)

    -- Hay comando de movimiento si RC1 o RC2 salen de banda muerta.
    local movement_requested = (not rc1_in_deadband) or (not rc2_in_deadband)

    -- Se frena si no hay movimiento pedido.
    -- RC5 ya fue evaluado como condicion global antes de esta parte.
    local brake_due_to_deadband = not movement_requested

    brake_relay(RELAY_LEFT, brake_due_to_deadband)
    brake_relay(RELAY_RIGHT, brake_due_to_deadband)

    return update, UPDATE_MS
end

return update()
