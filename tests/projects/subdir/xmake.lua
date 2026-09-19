-- The target lives in a subdirectory with its own xmake.lua. Its include
-- directories are then project-relative, and the generated code has to spell
-- an #include the way the project does.
includes("../../../xmake/xrefl.lua")
includes("lib/xmake.lua")
