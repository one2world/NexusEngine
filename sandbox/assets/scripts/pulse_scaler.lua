-- pulse_scaler.lua — pulses the entity's scale between min and max,
-- modulated by a cosine.  Different from bouncer.lua: it touches scale
-- not position, exercising the set_scale binding.
pulse_scaler = pulse_scaler or {
    min   = 0.6,
    max   = 1.6,
    speed = 1.2,
}

pulse_scaler.state = pulse_scaler.state or {}

function pulse_scaler.on_create(entity)
    pulse_scaler.state[entity.id] = { clock = 0.0 }
end

function pulse_scaler.on_update(entity, dt)
    local s = pulse_scaler.state[entity.id]
    if s == nil then return end
    s.clock = s.clock + dt
    -- map cos in [-1,1] -> [min,max]
    local t  = (math.cos(s.clock * pulse_scaler.speed) + 1.0) * 0.5
    local sf = pulse_scaler.min + (pulse_scaler.max - pulse_scaler.min) * t
    Entity.set_scale(entity, sf, sf, sf)
end
