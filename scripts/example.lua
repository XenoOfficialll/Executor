-- scripts/example.lua
-- Test script

print("Hello from executor!")

local Players = game:GetService("Players")
local LocalPlayer = Players.LocalPlayer

if LocalPlayer then
    print("Player: " .. LocalPlayer.Name)
    print("Character: " .. tostring(LocalPlayer.Character))
end

-- Example: infinite jump
local function enableInfiniteJump()
    local humanoid = LocalPlayer.Character and LocalPlayer.Character:FindFirstChild("Humanoid")
    if humanoid then
        humanoid.JumpPower = 100000
        print("Infinite jump enabled")
    end
end

enableInfiniteJump()
