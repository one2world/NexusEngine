-- bouncer.lua — vertical sinusoidal bounce around the entity's
-- on_create position.  Demonstrates Math + Entity transform bindings
-- working together end-to-end through real Lua.
bouncer = bouncer or {
    height    = 1.5,    -- peak amplitude in world units
    frequency = 2.0,    -- bounces per second
}

bouncer.state = bouncer.state or {}

function bouncer.on_create(entity)
    local p = Entity.get_position(entity)
    bouncer.state[entity.id] = {
        rest_x = p.x,
        rest_y = p.y,
        rest_z = p.z,
        clock  = 0.0,
    }
    print("[bouncer] rest pos = (" ..
          tostring(p.x) .. ", " .. tostring(p.y) .. ", " .. tostring(p.z) .. ")")
end

function bouncer.on_update(entity, dt)
    local s = bouncer.state[entity.id]
    if s == nil then return end
    s.clock = s.clock + dt
    -- |sin| keeps the bounce above the rest position, mirroring the
    -- old hard-coded sandbox bouncing-sphere demo.
    local lift = math.abs(math.sin(s.clock * bouncer.frequency)) * bouncer.height
    Entity.set_position(entity, s.rest_x, s.rest_y + lift, s.rest_z)
end
