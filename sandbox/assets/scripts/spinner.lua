-- spinner.lua — drives an entity's local rotation around the Y axis
-- (yaw spin) with a constant angular velocity.  Rotation rate is read
-- from the global table `spinner.rate` so different entities sharing
-- the script can override it via host-set globals.
spinner = spinner or { rate = 1.5 }

-- Per-entity yaw accumulator keyed by entity.id.  Keeping it in a Lua
-- table avoids leaking into other scripts and lets the host inspect
-- the state from the Watch panel (`spinner.angle[3]` etc).
spinner.angle = spinner.angle or {}

function spinner.on_create(entity)
    spinner.angle[entity.id] = 0.0
    print("[spinner] attached to entity " .. tostring(entity.id))
end

function spinner.on_update(entity, dt)
    local id    = entity.id
    local angle = (spinner.angle[id] or 0.0) + dt * spinner.rate
    spinner.angle[id] = angle
    -- Spin around Y; pitch a little so the cube reads as 3D.
    Entity.set_rotation_euler(entity, angle * 0.4, angle, 0.0)
end
