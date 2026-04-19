-- ============================================================================
-- BBFx v3.5 plugin template — Output Template (multi-projector / dome setup)
-- ============================================================================
-- The exported plugin captures an OutputManager JSON blob and restores
-- it on enable. `bbfx.authoring.exportOutputTemplate` can generate this
-- automatically from the current Studio OutputManager configuration.

local outputs = {
    { id = 0, width = 1920, height = 1080, position = { 0,       0 } },
    { id = 1, width = 1920, height = 1080, position = { 1920,    0 } },
}

return {
    onEnable = function()
        if bbfx.output and bbfx.output.applyTemplate then
            bbfx.output.applyTemplate(outputs)
        else
            print("[my-output-template] bbfx.output.applyTemplate not "
                   .. "yet available in this BBFx build.")
        end
    end,
    capture = outputs,
}
