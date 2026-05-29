-- brake_armed_rc1_rc2_relays_3_4_safety_enable.lua
--
-- Freno automatico usando RC1 + RC2 y RELAY3/RELAY4.
-- Incluye bloqueo de seguridad por estado ARM/DISARM.
--
-- Entradas:
--   RC1 = giro izquierda/derecha
--   RC2 = adelante/atras
--   RC5 = habilita/desfrena lado izquierdo cuando esta ON
--   RC6 = habilita/desfrena lado derecho cuando esta ON
--
-- Salidas:
--   Relay3 = freno izquierdo
--   Relay4 = freno derecho
--
-- Logica de seguridad:
--   Si el Rover NO esta armado:
--      -> freno izquierdo ON
--      -> freno derecho ON
--
--   Si el Rover esta armado:
--      Si RC1 y RC2 estan dentro de banda muerta:
--         -> frenos ON
--
--      Si RC1 o RC2 salen de banda muerta:
--         -> se permite liberar frenos, pero solo si RC5/RC6 estan ON
--
--   RC5 OFF:
--      -> freno izquierdo ON siempre
--
--   RC5 ON:
--      -> permite liberar freno izquierdo si el Rover esta armado
--         y RC1 o RC2 estan fuera de banda muerta
--
--   RC6 OFF:
--      -> freno derecho ON siempre
--
--   RC6 ON:
--      -> permite liberar freno derecho si el Rover esta armado
--         y RC1 o RC2 estan fuera de banda muerta
--
-- IMPORTANTE:
--   Deja RC5_OPTION = 0
--   Deja RC6_OPTION = 0
--   El script lee RC5/RC6, pero ArduPilot no debe accionarlos directamente.
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

-- Tu controlador:
--   0 = libre
--   1 = freno
-- Por eso debe ser true.
local BRAKE_ACTIVE_HIGH = false

-- Switches de seguridad:
local RC_LEFT_ENABLE = 5       -- RC5 habilita/libera lado izquierdo
local RC_RIGHT_ENABLE = 6      -- RC6 habilita/libera lado derecho
local ENABLE_PWM = 1700        -- Switch ON si RCx >= 1700 us

local UPDATE_MS = 20           -- 50 Hz
local warned = false
local last_armed_state = nil

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
    -- Esto evita liberar freno por perdida de senal.
    if pwm == nil or pwm == false then
        return true
    end

    return math.abs(pwm - center) <= deadband
end

function update()
    if not warned then
        if not relay:enabled(RELAY_LEFT) then
            gcs:send_text(3, "Lua brake: Relay3 no parece habilitado")
        end
        if not relay:enabled(RELAY_RIGHT) then
            gcs:send_text(3, "Lua brake: Relay4 no parece habilitado")
        end

        gcs:send_text(6, "Lua brake: armado + RC1/RC2 + RC5/RC6")
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

    -- Regla principal de seguridad:
    -- Si el vehiculo esta desarmado, nunca liberar frenos.
    if not armed then
        apply_both_brakes()
        return update, UPDATE_MS
    end

    local rc1_in_deadband = channel_in_deadband(RC_STEERING, CENTER_RC1, DEADBAND_RC1)
    local rc2_in_deadband = channel_in_deadband(RC_THROTTLE, CENTER_RC2, DEADBAND_RC2)

    -- Hay comando de movimiento si RC1 o RC2 salen de banda muerta.
    local movement_requested = (not rc1_in_deadband) or (not rc2_in_deadband)

    local left_enabled = switch_is_on(RC_LEFT_ENABLE)
    local right_enabled = switch_is_on(RC_RIGHT_ENABLE)

    -- Freno izquierdo:
    -- Se frena si NO hay movimiento pedido O si RC5 no esta habilitado.
    local left_brake = (not movement_requested) or (not left_enabled)

    -- Freno derecho:
    -- Se frena si NO hay movimiento pedido O si RC6 no esta habilitado.
    local right_brake = (not movement_requested) or (not right_enabled)

    brake_relay(RELAY_LEFT, left_brake)
    brake_relay(RELAY_RIGHT, right_brake)

    return update, UPDATE_MS
end

return update()
