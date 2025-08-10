## Patch 1.4.4
```
-New: Option to set skid effect speed by surface type
-Fix: Physics refusing to come out of sleep causing vehicles to ignore inputs
-Fix: Trailers not hitching at the correct location when a vehicle is scaled
-Fix: Suspension aggressively flinging vehicle when it lands at an angle
```


## Patch 1.4.3
```
-New: 'SteeringRecenterSpeed' option (The speed at which steering returns to zero)
	-Note: This will change steering behavior, unless you are using the default value for SteeringSpeed. Configure it after updating the plugin.
-Change: Debug menu now appears on clients as well, when "Display Debug Menu" is toggled
-Fix: Copy how epic sets up their root components to hopefully avoid future BP corruption bugs
-Fix: WheelConfig.WheelPrim not having a default value, causing compile issues in some cases

```


## Patch 1.4.2
```
- Unreal 5.4 Support, this patch is for UE5.4 only.
```


## Patch 1.4.1
```
-New: 'GetIsSteerableWheel' for vehicle wheels
-New: Implemented passive mode in wheel components
-Change: Add manual control over wheel brake torque amount (deprecated "BrakePressure" in Wheel in favor of this)
-Change: vehicle movement networking to use Unreal's replicated world time, instead of calculating times locally. This allows the state queue to more accurately drop late rpcs, decreasing jitter.
-Change: Improved suspension for physics wheel mode
	-Springs now work on wheels with no contact
	-Contact trace is now works the same as raycast wheels
-Fix: Wheel spinning in place when a vehicle is at rest
-Fix: Vehicles 'floating' around after going into network rest on clients
-Fix: Physics jitter when stationary and pressing an input
-Fix: NaN error and poor performance when not using a wheel mesh with raycast wheels
-Fix: Corrected return name on GetWheelRadius
-Fix: Suspension ignores contact normal, causing vehicle to move incorrectly. Thanks @Elitic
```


## Release 1.4.0
```
-New: Raycast wheel mode, supports runtime wheel mode switching
-New: Passive Mode  Vehicles now enter a passive mode while idling, which disables ticking
-New: Suspension limits preview in VehicleSetupHUD
-Change: Resting now waits 3 seconds after entering the velocity threshold (to avoid RPC thrashing)
-Change: Network resting is now decoupled from local resting, but copies it's state
-Change: Reworked all suspension related code, large performance increase over 1.3
-Change: Physics wheels no longer use collision hits to detect contact (instead uses raycast)
-Change: TeleportVehicle now includes an option to maintain velocities relative to the vehicle chassis
-Fix: Network relevancy not being updated when not using a custom camera manager
```


## Patch 1.3.3
```
-Fix compiler issue on linux
-Support for Unreal 5.1
```


## Patch 1.3.2
```
-Fix: Update suspension to only use chaos transforms in calculations (Fix physics jittering and other weirdness)
-Fix: ChangeStaticMesh on Vehicle_Wheel causing physics errors after toggling physics off/on (Fixed contact modification not updating)
-Fix: Suspension downforce not working when vehicle is upside down
-Change: Updated VehicleSetupHUD to show Vehicle Mass and Physics tick rate. 
-Change: Updated VehicleSetupHUD coloring to help with readability
-Change: Moved some wheel specific code out of AVS_Vehicle and into Vehicle_Wheel
```


## Patch 1.3.1
```
-Fix: Occasional physics thread error on vehicle initialization (Fixed race condition between vehicle init and physics thread)
-Fix: Wheels go to sleep and don't follow vehicle when in network rest mode
-Change: Suspension damper updated to be more realistic
-Change: Suspension damper (Shock Absorption) now uses kNs/m (Kilonewton second per meter) as units
```

## Release 1.3.0
```
-UE5/Chaos Port
```

## Patch 1.2.8
```
-Change: Resting now waits 3 seconds after entering the velocity threshold (to avoid RPC thrashing)
-Change: Network resting is now decoupled from local resting, but copies it's state
-New: Passive Mode  Vehicles now enter a passive mode while idling, which disables ticking and sets network dormancy
-the following backported from 1.3.x-
-Fix network relevancy not being updated when not using a custom camera manager
-Fix gear struct variables not being initialized properly, causing warnings while packaging
-Fix linux compiler issue
-Update plugin structure to be compliant with BuildSettings.V2
-Add GetUnrealEngineVersion function
-Update VehicleSetup_HUD similar to the UE5 version (Add VehicleMass, PhysicsState, UnrealVersion)
-Add "PreviewInEditor" setting in wheels, used to preview wheel travel
-Add "RollingResistance" setting in wheels
```


## Patch 1.2.7
```
-New: Experimental Feature: WheelReprojectionCamber, adds on to the wheel reprojection feature allowing cosmetic wheel camber
-Fix: Revert change to PostPhysics tick group as default (PostPhysics causes issues with network replication)
-Change: VehicleSetupHUD now shows Handbrake input
```


## Patch 1.2.6
```
-New: Stability Control System, attempts to keep vehicle aligned with direction of travel (disabled by default)
-New: By request, helper function to destroy components owned by the AVS_Vehicle
-New: Network options for syncing location/rotation individually, can be changed during runtime
-New: Support for cinematics in Unreal 4.26+ (Requires wheel meshes to be added manually when setting up vehicles)
-Change: ChangedGear function now receives Previous and Current gear values (useful for checking shift direction)
-Change: Removed "LinearDrag" option, now just uses the Linear Damping from VehicleMesh directly
-Fix: Wheels not working correctly while detached when using the "Prevent Wheel Clipping" setting
-Fix: Errors on dedicated servers related to wheel controllers
-Fix: Vehicle suspension raising/dropping when pressing the throttle while the "Dynamic Air Drag" setting is enabled
-Fix: VehicleMesh physics never go to sleep (performance improvements)
-Fix: Host client sending double inputs (network improvements)
-Fix: UpdateWheelLocks being called more than necessary (performance improvements)
-Fix: Wheel properties not being properly applied to manually added wheel meshes
```


## Patch 1.2.5
```
-New: Vehicle_Wheel can now change location/rotation at runtime easily using the SetWheelPosition function
-New: Automatic Shifter Position, automatically changes PRND based on current vehicle inputs
-New: Combined function for Throttle and Brake called SetThrottleAndBrakeInput, Negative throttle used as brake, brake as reverse when Automatic Shifter Position is enabled
-New: ChangedGear event
-New: New option "Prevent Wheels Clipping" that prevents wheels from getting stuck in the ground or under objects
-Fix: UpdateLightDecorations function changed from private to protected, as it's intended to be overridden
-Fix: Dedicated server error spam related to wheel particles
-Fix: Various improvements to access specifiers
```


## Patch 1.2.4
```
-Fix: MP: Client vehicles being flung when physics gets enabled in certain circumstances
-Fix: Vehicle_Wheel ChangeStaticMesh function failing when called after construction but before onBeginPlay (such as in onRep functions)
```


## Patch 1.2.3
```
-New: EXPERIMENTAL FEATURE - Wheel Reprojection, fixes wheels 'tilting' in high friction/weight scenarios
	Note: Does not currently work with Adding/Removing wheels, or wheel detachment. Possibly broken with other features enabled
-New: Zero throttle while shifting config setting
-Change: VehicleSetupHUD now shows the internally used speed unit instead of MPH
-Change: Added acceleration to the VehicleSetupHUD, and made many back end improvements
-Change: Added Vehicle Hitch to the correct category in the component menu
-Change: Added/Updated function descriptions related to the 1.2 update
-Fix: Removed "Host Trailer Rotation" from showing in the config
-Fix: Access error after killing a trailer that is hitched to a vehicle
-Fix: Error spam for engine and skid sounds on dedicated servers
-Fix: "No mesh detected!" error showing incorrectly when duplicating wheels in the editor
-Removed various sections of old code
```


## Patch 1.2.2
```
-Fix: Wheels being in the wrong location on skeletal meshes
```


## Patch 1.2.1
```
-Fix: Compile error due to private function in Vehicle_Hitch
```


## Release 1.2.0
```
-Fix: GetCurrentSteeringInput's output name being "AirSpeed" instead of "SteeringInput"
-Fix: All spotlights connected to vehicle incorrectly getting their visibility set to false during onBeginPlay
-Fix: SpeedUnit incorrectly showing in config (Is used internally only)
-Fix: Divide by Zero error on vehicles with no driving wheels (like trailers)
-Fix: Engine not starting on clients on vehicles with no engine audio
-Fix: LightControllers not being marked active when used with no light sources
-Fix: Possible Editor crash when reordering components in a vehicle blueprint
-Change: GetPluginVersion now reads directly from the Plugin info
-Change: Ignition sound now plays on all clients (EngineRunning is now networked differently)
-Change: Lights System now supports all light types (as opposed to just spotlights)
-Change: Neutral gear now properly calculates independent of wheel speed
-Change: EngineRunning now defaults to false
-Change: Movement synchronization is now smoothed, new option for smoothing amount
-Change: Vehicles now limit their max angular velocity, added corresponding setting in "Vehicle - Physics"
-New: Trailer Support and Hitching system
-New: Added network rest states for significantly better network performance
-New: Functions for Adding/Removing Wheels at runtime
-New: Function for offsetting custom center of mass
-New: Options to Start with Physics (default true), Start with Engine Running (default false)
```


## Patch 1.1.1
```
-New: Wheel effects now support different surfaces
-New: Config for unit of speed used internally
-New: GetAcceleration node (returns change of speed per second)
-Fix: Rare full desync on clients caused by the network owner changing
-Fix: Fixed throttle sticking when unpossessing a vehicle (Previous fix only worked in singleplayer)
-Change: Updated vehicle setup HUD with better layout and now shows default values instead of 0
-Experimental: Added experimental option to limit acceleration to (SelectedSpeedUnit per Second)
```


## Release 1.1.0
```
-New: Unreal 4.25 Support
-New: Wheel Effects! Skid Marks, Smoke, Sound Effects!
-New: Support for overriding/controlling torque and speed on a per wheel basis
-Fix: VehicleSetup_HUD breaking vehicles onBeginPlay (due to a change in UE4.23)
-Fix: GetRotationSpeed output name
-Fix: Wheel meshes that had their scale changed would report the wrong radius causing vehicles to be over/underpowered
```


## Patch 1.0.2
```
-New: Proper input functions, deprecated input nodes now link to this internally
-New: Added GetPluginVersion function
-New: Option for DynamicAirDrag, instead of always being on
-New: Added new component for editing and visualizing center of mass
-Change: Wheel variables now initialize during construction instead of onBeginPlay
-Change: Default steering curve now has a higher minimum steering percent (0.3 vs 0.1)
-Change: Netcode now takes latency into account when syncing physics, making it much smoother
-Change: Renamed some wheel detach/attachment nodes to make their use more clear. Old nodes still exist and are marked as deprecated.
-Change: DynamicAirDrag now allows editing the default Drag
-Change: DynamicAirDrag now aggressively applies drag when not moving to prevent unwanted movements
-Fix: Clamped throttle and brake inputs to prevent controllers doubling inputs
-Fix: Stutter every 5 seconds or so on clients when replicating
-Fix: Some config variables unable to be changed on UE4.24
-Fix: Engine Audio component throwing errors on dedicated servers
-Fix: Brakes/Throttle inputs stuck when unpossessing vehicle
```


## Patch 1.0.1
```
-Initial marketplace launch
-Fixed compile error
```


## Release 1.0.0
[Release Video](https://www.youtube.com/watch?v=MNq4VPvIvj0)