# Craft a Skeleton!

Source for **Craft a Skeleton!**, a Kenshi mod that adds a production chain for manufacturing Standard Skeletons.

Steam Workshop item: **3779513209**

## Requirements

- Kenshi
- RE_Kenshi v0.3.4
- KenshiLib development files when building from source
- Visual Studio with the Visual C++ 2010 (`v100`) x64 toolset

RE_Kenshi must be installed for Undeployed Skeletons to deploy.

## What the mod adds

Craft a Skeleton! adds a multi-stage production chain centered on the **Skeleton Assembly Cradle**.

The final product is an **Undeployed Skeleton**. It can be carried and transported like an item. Drop it on the ground and the plugin creates one Standard Skeleton at that location, adds it to the player's squad, and consumes the chassis.

The internal String ID for the deployable item is:

```text
CAS_ActivatedSkeletonChassis
```

The plugin depends on that ID.

### Skeleton Component Storage

v0.2 adds dedicated component storage for the custom intermediate parts used by the production chain. Workers can fetch components and haul finished intermediates between the storage and production benches.

The final Undeployed Skeleton is intentionally excluded from this storage.

### Reboot speech

Newly deployed Skeletons say one randomized reboot line when they come online. The lines reflect the use of repaired Old CPU Units and incomplete memories from the original Skeleton.

### Old CPU Unit availability

**CPU Unit** and **Old CPU Unit** are different Kenshi items.

The Repaired CPU Unit recipe requires an **Old CPU Unit** and a Skeleton Repair Kit. A normal CPU Unit cannot be substituted.

v0.2 also adds Old CPU Units to the vanilla **ancient lab** and **ancient lab ruins** loot lists. Existing containers that were already generated in a save may keep their old contents.

## Repository layout

- `src/CraftASkeleton.cpp` - native plugin source
- `CraftASkeletonPlugin.vcxproj` - Visual Studio project
- `fcs/CraftASkeleton!.mod` - FCS mod data
- `runtime/CraftASkeleton.ini` - plugin configuration
- `runtime/RE_Kenshi.json` - RE_Kenshi plugin declaration
- `build/Build_From_Source.ps1` - Release x64 build helper

## Building

The project expects these environment variables:

```text
KENSHILIB_DIR
KENSHILIB_DEPS_DIR
BOOST_INCLUDE_PATH
```

After the KenshiLib build dependencies are configured, run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\build\Build_From_Source.ps1
```

The helper rebuilds the Release x64 configuration and prints the DLL SHA256. It does not install the DLL into Kenshi.

## Troubleshooting

If an Undeployed Skeleton does nothing after being dropped:

1. Confirm RE_Kenshi v0.3.4 is installed.
2. Confirm Craft a Skeleton! is enabled.
3. Drop one Undeployed Skeleton in a loaded area.
4. Check `Kenshi\RE_Kenshi_log.txt`.

Useful deployment log entries include:

```text
deployment ready
deployment complete
```

`DebugLogging` is disabled by default in `runtime/CraftASkeleton.ini`.

## Version

Craft a Skeleton! **v0.2.0**

Release DLL SHA256:

```text
D09394A1082814CE3E314DDD16AAFE0FC5C0FB75BE837D055B40EE9538F64A6D
```

FCS mod SHA256:

```text
58A35B40DF5627029B3AE07E29279C287819C6C051EC9C5B00672893F2FBB5CE
```

## License

The native plugin source is released under the **GNU General Public License v3.0**.

See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
