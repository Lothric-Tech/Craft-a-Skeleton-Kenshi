# Craft a Skeleton!

Source code for the native plugin used by **Craft a Skeleton!**, a Kenshi mod that adds a production chain for building Standard Skeletons.

Steam Workshop item: **3779513209**

## Requirements

- Kenshi
- RE_Kenshi v0.3.4
- KenshiLib development files for compiling the plugin
- Visual Studio with the Visual C++ 2010 (`v100`) x64 toolset

RE_Kenshi should show its version on the Kenshi main menu when installed correctly.

## How it works

The mod adds a multi-stage Skeleton production chain built around the Skeleton Assembly Cradle.

The final crafted item is an **Undeployed Skeleton**. It can be carried, stored, or transported. Dropping it on the ground deploys one living Standard Skeleton at that location and adds it to the player's squad.

The FCS display name is **Undeployed Skeleton**. Its internal String ID remains:

```text
CAS_ActivatedSkeletonChassis
```

The plugin depends on that internal ID.

## Repository layout

- `src/CraftASkeleton.cpp` - native plugin source
- `CraftASkeletonPlugin.vcxproj` - Visual Studio project
- `CraftASkeleton.sln` - Visual Studio solution, when present
- `fcs/CraftASkeleton!.mod` - FCS mod data
- `runtime/CraftASkeleton.ini` - plugin configuration
- `runtime/RE_Kenshi.json` - RE_Kenshi plugin declaration
- `build/Build_From_Source.ps1` - build helper

## Building

Install the RE_Kenshi/KenshiLib plugin development dependencies first.

Then run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\build\Build_From_Source.ps1
```

The build helper rebuilds the x64 Release configuration. It does not install the resulting DLL into Kenshi.

## Workshop troubleshooting

If an Undeployed Skeleton does nothing after being dropped:

1. Confirm RE_Kenshi v0.3.4 is installed.
2. Confirm the RE_Kenshi version appears on the Kenshi main menu.
3. Confirm Craft a Skeleton! is enabled in the mod list.
4. Reproduce the problem.
5. Check `Kenshi\RE_Kenshi_log.txt`.

A successful deployment should produce log entries containing:

```text
GROUND ACTIVATION READY
GROUND ACTIVATION R14 COMPLETE
```

## CPU item note

**CPU Unit** and **Old CPU Unit** are different Kenshi items.

The Repaired CPU Unit recipe requires an **Old CPU Unit** and a Skeleton Repair Kit. A normal CPU Unit cannot be substituted.

## Version

Craft a Skeleton! v0.1.0

Verified Workshop DLL SHA256:

```text
0DD37884CFAF08738BA2DD5874DE13DE8735F78AF187CC5B590062637178E664
```

## License

The native CraftASkeleton plugin source is released under the **GNU General Public License v3.0**.

See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
