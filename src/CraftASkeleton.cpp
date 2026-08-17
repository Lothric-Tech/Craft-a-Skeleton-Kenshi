#include <windows.h>
#include <cmath>

#include <ogre/OgreQuaternion.h>
#include <ogre/OgreVector3.h>

#include <kenshi/GameWorld.h>
#include <kenshi/GameDataManager.h>
#include <kenshi/RootObjectBase.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/Character.h>
#include <kenshi/Animation/AnimationClass.h>
#include <kenshi/Faction.h>
#include <kenshi/RootObject.h>
#include <kenshi/GameData.h>
#include <kenshi/Enums.h>
#include <kenshi/Inventory.h>
#include <kenshi/util/hand.h>

// Must remain before UseableStuff.h.
#include <kenshi/gui/InventoryGUI.h>

#include <kenshi/Building/Building.h>
#include <kenshi/Building/UseableStuff.h>

#include <core/Functions.h>
#include <Debug.h>

#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cctype>

static const char* PLUGIN_VERSION =
    "0.1.0-dev-deployable-chassis-r14";

static const char* TARGET_CRADLE_STRING_ID =
    "10-CraftASkeleton!.mod";

static const char* TARGET_CHASSIS_STRING_ID =
    "CAS_ActivatedSkeletonChassis";

static const bool SHOW_PLAYER_MESSAGES =
    true;

static const char* ACTIVATION_PLAYER_MESSAGE =
    "Skeleton Activated: A newly constructed Skeleton has completed its boot sequence and joined your squad.";

static const unsigned int READINESS_RESET_ABSENT_SCANS =
    3;

/*
 * Purpose-built FCS CHARACTER owned by Craft a Skeleton.
 *
 * The record uses the normal Skeleton race and was cleaned of
 * recruit dialogue, guard AI, starting equipment, and weapons.
 */
static const char* STANDARD_SKELETON_TEMPLATE_STRING_ID =
    "19-CraftASkeleton!.mod";

static const char* STANDARD_SKELETON_TEMPLATE_EXPECTED_NAME =
    "Crafted Standard Skeleton";

/*
 * This file is diagnostic only. Unlike the earlier development
 * locks, its existence does not block later Skeletons.
 */
/*
 * Diagnostic snapshot only. Its existence never blocks a later
 * assembly-ready event.
 */
/*
 * Diagnostic snapshot only. Its existence never blocks a later
 * assembly-ready event.
 */
static const char* REAL_SPAWN_LOCK_FILE_NAME =
    "CraftASkeleton.deployable-chassis-r14.snapshot";

static const char* SPAWNED_STANDARD_SKELETON_NAME =
    "Skeleton";

static const char* FINISHED_SKELETON_SLEEP_ANIMATION =
    "sleepinbed";

static const unsigned int WAKE_SIGNAL_GRACE_SCANS =
    2;

static const unsigned int POSE_SETUP_DELAY_SCANS =
    2;

static const unsigned int POSE_RETRY_LIMIT =
    3;

static const unsigned int SESSION_SPAWN_LIMIT =
    20;

static const float WAKE_MOVEMENT_SPEED =
    0.05f;

static const float FORCED_ANIMATION_SPEED =
    1.0f;

static const float FORCED_ANIMATION_SYNC =
    0.0f;

/*
 * These offsets are intentionally zero for R5B. The repair-bed
 * mesh origin appears to be closer to the bed surface than the
 * crafting operator node used in R4. They are isolated here for
 * easy tuning after the visual test.
 */
static const float SLEEP_LOCAL_OFFSET_X =
    0.0f;

static const float SLEEP_LOCAL_OFFSET_Y =
    0.0f;

static const float SLEEP_LOCAL_OFFSET_Z =
    0.0f;

static const float PLANNED_LOCAL_SPAWN_X =
    0.0f;

/*
 * Animation R3 starts at the building origin, then uses the
 * crafting station's operator node for alignment. The selected
 * slave animation provides the horizontal body pose.
 */
static const float PLANNED_LOCAL_SPAWN_Y =
    0.0f;

static const float PLANNED_LOCAL_SPAWN_Z =
    0.0f;

/*
 * Relative lying rotation:
 * -90 degrees around the cradle-local X axis.
 *
 * This is a dry-run candidate and may be tuned after the first
 * harmless position/orientation log review.
 */
static const float LYING_QUATERNION_W =
    0.70710678f;

static const float LYING_QUATERNION_X =
    -0.70710678f;

static const float DISCOVERY_RANGE =
    250.0f;

static const float UPDATE_INTERVAL =
    2.0f;

static const unsigned int UNRESOLVED_FORGET_SCANS =
    30;

static HMODULE g_moduleHandle =
    NULL;

static bool g_debugLogging =
    false;

static const float BESIDE_CRADLE_NUDGE =
    2.5f;

struct CradleState
{
    unsigned int unresolvedScans;
    bool wasUnresolved;
    bool hasChassis;
    bool assemblyReady;
    bool realSpawnAttempted;
    bool safeSpawnCached;
    unsigned int chassisAbsentScans;
    unsigned int readinessEvent;
    unsigned int realSpawnAttempt;
    int chassisQuantity;
    int safeSpawnFloor;
    float safeSpawnX;
    float safeSpawnY;
    float safeSpawnZ;
    std::string stringID;
    std::string chassisStringID;
    std::string inventorySignature;

    CradleState()
        : unresolvedScans(0),
          wasUnresolved(false),
          hasChassis(false),
          assemblyReady(false),
          realSpawnAttempted(false),
          safeSpawnCached(false),
          chassisAbsentScans(0),
          readinessEvent(0),
          realSpawnAttempt(0),
          chassisQuantity(0),
          safeSpawnFloor(0),
          safeSpawnX(0.0f),
          safeSpawnY(0.0f),
          safeSpawnZ(0.0f),
          stringID(),
          chassisStringID(),
          inventorySignature()
    {
    }
};

static std::map<hand, CradleState>
g_cradleStates;

static GameWorld*
g_activeWorld =
    NULL;

static GameData*
g_standardSkeletonTemplate =
    NULL;

static bool
g_templateResolutionFailureLogged =
    false;

static bool
g_spawnLockNoticeLogged =
    false;

static unsigned int
g_sessionSpawnCount =
    0;

static bool
g_sessionSpawnLimitLogged =
    false;

static std::map<
    hand,
    bool
> g_groundDeploymentGuards;

static unsigned int
g_groundDeploymentEvent =
    0;

static const float
GROUND_DEPLOY_SCAN_RANGE =
    250.0f;

static const int
GROUND_DEPLOY_SCAN_MAX_ITEMS =
    1000;

void (*GameWorld_mainLoop_orig)(
    GameWorld* thisptr,
    float time
) = NULL;

std::string BuildLogMessage(
    const std::string& message
)
{
    std::stringstream output;

    output
        << "Craft a Skeleton v"
        << PLUGIN_VERSION
        << ": "
        << message;

    return output.str();
}

void LogInfo(
    const std::string& message
)
{
    const std::string formatted =
        BuildLogMessage(
            message
        );

    DebugLog(
        formatted.c_str()
    );
}

void LogDebugMessage(
    const std::string& message
)
{
    if (!g_debugLogging)
    {
        return;
    }

    const std::string formatted =
        BuildLogMessage(
            std::string("[debug] ") +
            message
        );

    DebugLog(
        formatted.c_str()
    );
}

void LogErrorMessage(
    const std::string& message
)
{
    const std::string formatted =
        BuildLogMessage(
            message
        );

    ErrorLog(
        formatted.c_str()
    );
}

void ShowPlayerMessage(
    GameWorld* world,
    const std::string& message
)
{
    if (
        !SHOW_PLAYER_MESSAGES ||
        world == NULL ||
        message.empty()
    )
    {
        return;
    }

    world->showPlayerAMessage(
        message,
        true
    );
}

int CountActivatedChassisInInventory(
    Inventory* inventory
)
{
    if (inventory == NULL)
    {
        return 0;
    }

    int totalQuantity =
        0;

    const lektor<Item*>& items =
        inventory->getAllItems();

    for (
        unsigned int index = 0;
        index < items.size();
        ++index
    )
    {
        Item* item =
            items[index];

        if (
            item == NULL ||
            !item->isValid() ||
            item->quantity <= 0
        )
        {
            continue;
        }

        GameData* itemData =
            item->getGameData();

        if (
            itemData == NULL ||
            itemData->stringID !=
                TARGET_CHASSIS_STRING_ID
        )
        {
            continue;
        }

        totalQuantity +=
            item->quantity;
    }

    return totalQuantity;
}

bool CleanupActivatedChassisFromSpawnedInventory(
    Character* spawnedCharacter,
    int& quantityBefore,
    int& quantityRemoved,
    int& quantityAfter
)
{
    quantityBefore =
        0;

    quantityRemoved =
        0;

    quantityAfter =
        0;

    if (
        spawnedCharacter == NULL ||
        !spawnedCharacter->isValid()
    )
    {
        return false;
    }

    Inventory* inventory =
        spawnedCharacter->getInventory();

    if (inventory == NULL)
    {
        return false;
    }

    quantityBefore =
        CountActivatedChassisInInventory(
            inventory
        );

    for (;;)
    {
        Item* chassisItem =
            NULL;

        const lektor<Item*>& items =
            inventory->getAllItems();

        for (
            unsigned int index = 0;
            index < items.size();
            ++index
        )
        {
            Item* item =
                items[index];

            if (
                item == NULL ||
                !item->isValid() ||
                item->quantity <= 0
            )
            {
                continue;
            }

            GameData* itemData =
                item->getGameData();

            if (
                itemData == NULL ||
                itemData->stringID !=
                    TARGET_CHASSIS_STRING_ID
            )
            {
                continue;
            }

            chassisItem =
                item;

            break;
        }

        if (chassisItem == NULL)
        {
            break;
        }

        int removeQuantity =
            chassisItem->quantity;

        if (removeQuantity < 1)
        {
            removeQuantity =
                1;
        }

        if (!inventory->
            removeItemAutoDestroy(
                chassisItem,
                removeQuantity
            ))
        {
            quantityAfter =
                CountActivatedChassisInInventory(
                    inventory
                );

            return false;
        }

        quantityRemoved +=
            removeQuantity;
    }

    if (quantityRemoved > 0)
    {
        inventory->
            notifyModified();
    }

    quantityAfter =
        CountActivatedChassisInInventory(
            inventory
        );

    return quantityAfter == 0;
}

std::string Trim(
    const std::string& value
)
{
    std::string::size_type begin =
        0;

    while (
        begin < value.size() &&
        std::isspace(
            static_cast<unsigned char>(
                value[begin]
            )
        )
    )
    {
        ++begin;
    }

    std::string::size_type end =
        value.size();

    while (
        end > begin &&
        std::isspace(
            static_cast<unsigned char>(
                value[end - 1]
            )
        )
    )
    {
        --end;
    }

    return value.substr(
        begin,
        end - begin
    );
}

std::string ToLower(
    const std::string& value
)
{
    std::string lowered =
        value;

    for (
        std::string::size_type index = 0;
        index < lowered.size();
        ++index
    )
    {
        lowered[index] =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        lowered[index]
                    )
                )
            );
    }

    return lowered;
}

bool EqualsInsensitive(
    const std::string& left,
    const std::string& right
)
{
    return
        ToLower(left) ==
        ToLower(right);
}

std::string GetPluginDirectory()
{
    if (g_moduleHandle == NULL)
    {
        return std::string();
    }

    char modulePath[MAX_PATH] =
        { 0 };

    const DWORD length =
        GetModuleFileNameA(
            g_moduleHandle,
            modulePath,
            MAX_PATH
        );

    if (
        length == 0 ||
        length >= MAX_PATH
    )
    {
        return std::string();
    }

    const std::string fullPath(
        modulePath,
        length
    );

    const std::string::size_type separator =
        fullPath.find_last_of(
            "\\/"
        );

    if (separator == std::string::npos)
    {
        return std::string();
    }

    return fullPath.substr(
        0,
        separator
    );
}

bool ParseBoolean(
    const std::string& value
)
{
    const std::string lowered =
        ToLower(
            Trim(
                value
            )
        );

    return
        lowered == "true" ||
        lowered == "1" ||
        lowered == "yes" ||
        lowered == "on";
}

bool ReadDebugToggle(
    std::string& configPath,
    bool& configFound
)
{
    configFound =
        false;

    const std::string pluginDirectory =
        GetPluginDirectory();

    if (pluginDirectory.empty())
    {
        configPath =
            "CraftASkeleton.ini";

        return false;
    }

    configPath =
        pluginDirectory +
        "\\CraftASkeleton.ini";

    std::ifstream input(
        configPath.c_str()
    );

    if (!input.is_open())
    {
        return false;
    }

    configFound =
        true;

    std::string line;

    while (
        std::getline(
            input,
            line
        )
    )
    {
        line =
            Trim(
                line
            );

        if (line.empty())
        {
            continue;
        }

        if (
            line[0] == '#' ||
            line[0] == ';'
        )
        {
            continue;
        }

        const std::string::size_type equals =
            line.find(
                '='
            );

        if (equals == std::string::npos)
        {
            continue;
        }

        const std::string key =
            ToLower(
                Trim(
                    line.substr(
                        0,
                        equals
                    )
                )
            );

        const std::string value =
            Trim(
                line.substr(
                    equals + 1
                )
            );

        if (key == "debuglogging")
        {
            return ParseBoolean(
                value
            );
        }
    }

    return false;
}

bool IsTargetCradle(
    GameData* data
)
{
    return
        data != NULL &&
        data->stringID ==
        TARGET_CRADLE_STRING_ID;
}

bool GetStableCradleHandle(
    Building* cradleBuilding,
    hand& cradleHandle
)
{
    if (cradleBuilding == NULL)
    {
        return false;
    }

    UseableStuff* usable =
        cradleBuilding->getUseableStuff();

    if (usable == NULL)
    {
        return false;
    }

    Inventory* inventory =
        usable->getInventory();

    if (inventory == NULL)
    {
        return false;
    }

    const hand& inventoryHandle =
        inventory->getHandle();

    if (inventoryHandle.isNull())
    {
        return false;
    }

    cradleHandle =
        inventoryHandle;

    return
        cradleHandle.getBuilding() != NULL;
}

/*
 * Perform only scalar arithmetic here.
 *
 * KenshiLib exposes Ogre vector/quaternion fields through its
 * headers, but this plugin does not link the full Ogre runtime.
 * Constructing Ogre::Vector3/Ogre::Quaternion values or using
 * their non-inline operators would introduce unresolved imports.
 */
void RotateLocalOffsetToWorld(
    const Ogre::Quaternion& rotation,
    float localX,
    float localY,
    float localZ,
    float& worldX,
    float& worldY,
    float& worldZ
)
{
    const float xx =
        rotation.x * rotation.x;

    const float yy =
        rotation.y * rotation.y;

    const float zz =
        rotation.z * rotation.z;

    const float xy =
        rotation.x * rotation.y;

    const float xz =
        rotation.x * rotation.z;

    const float yz =
        rotation.y * rotation.z;

    const float wx =
        rotation.w * rotation.x;

    const float wy =
        rotation.w * rotation.y;

    const float wz =
        rotation.w * rotation.z;

    worldX =
        (1.0f - 2.0f * (yy + zz)) * localX +
        2.0f * (xy - wz) * localY +
        2.0f * (xz + wy) * localZ;

    worldY =
        2.0f * (xy + wz) * localX +
        (1.0f - 2.0f * (xx + zz)) * localY +
        2.0f * (yz - wx) * localZ;

    worldZ =
        2.0f * (xz - wy) * localX +
        2.0f * (yz + wx) * localY +
        (1.0f - 2.0f * (xx + yy)) * localZ;
}

void CalculatePlannedLyingQuaternion(
    const Ogre::Quaternion& cradleOrientation,
    float& plannedW,
    float& plannedX,
    float& plannedY,
    float& plannedZ
)
{
    const float localW =
        LYING_QUATERNION_W;

    const float localX =
        LYING_QUATERNION_X;

    plannedW =
        cradleOrientation.w * localW -
        cradleOrientation.x * localX;

    plannedX =
        cradleOrientation.w * localX +
        cradleOrientation.x * localW;

    plannedY =
        cradleOrientation.y * localW +
        cradleOrientation.z * localX;

    plannedZ =
        -cradleOrientation.y * localX +
        cradleOrientation.z * localW;
}

std::string GetRealSpawnLockPath()
{
    const std::string pluginDirectory =
        GetPluginDirectory();

    if (pluginDirectory.empty())
    {
        return std::string(
            REAL_SPAWN_LOCK_FILE_NAME
        );
    }

    return
        pluginDirectory +
        "\\" +
        REAL_SPAWN_LOCK_FILE_NAME;
}

bool RealSpawnLockExists()
{
    const std::string lockPath =
        GetRealSpawnLockPath();

    std::ifstream input(
        lockPath.c_str()
    );

    return input.is_open();
}

bool WriteRealSpawnLock(
    const std::string& status,
    const std::string& details
)
{
    const std::string lockPath =
        GetRealSpawnLockPath();

    std::ofstream output(
        lockPath.c_str(),
        std::ios::out |
        std::ios::trunc
    );

    if (!output.is_open())
    {
        return false;
    }

    output
        << "Craft a Skeleton real-spawn safety lock"
        << std::endl
        << "version="
        << PLUGIN_VERSION
        << std::endl
        << "status="
        << status
        << std::endl
        << details
        << std::endl;

    output.flush();

    return output.good();
}

Faction* FindPlayerFactionForDryRun(
    GameWorld* world
)
{
    if (
        world == NULL ||
        world->factionMgr == NULL
    )
    {
        return NULL;
    }

    const lektor<Faction*>* factions =
        world->factionMgr->
        getAllFactions();

    if (factions == NULL)
    {
        return NULL;
    }

    for (
        unsigned int index = 0;
        index < factions->size();
        ++index
    )
    {
        Faction* faction =
            (*factions)[index];

        if (
            faction != NULL &&
            faction->isThePlayer()
        )
        {
            return faction;
        }
    }

    return NULL;
}

bool ResolveStandardSkeletonTemplate(
    GameWorld* world
)
{
    if (
        g_standardSkeletonTemplate != NULL &&
        g_standardSkeletonTemplate->isValid()
    )
    {
        return true;
    }

    if (world == NULL)
    {
        return false;
    }

    GameData* resolved =
        world->gamedata.getData(
            STANDARD_SKELETON_TEMPLATE_STRING_ID
        );

    if (
        resolved == NULL ||
        !resolved->isValid()
    )
    {
        if (!g_templateResolutionFailureLogged)
        {
            g_templateResolutionFailureLogged =
                true;

            std::stringstream message;

            message
                << "could not resolve purpose-built Standard Skeleton CHARACTER; requestedStringID=\""
                << STANDARD_SKELETON_TEMPLATE_STRING_ID
                << "\"; expectedName=\""
                << STANDARD_SKELETON_TEMPLATE_EXPECTED_NAME
                << "\"; real spawning is waiting.";

            LogErrorMessage(
                message.str()
            );
        }

        return false;
    }

    g_standardSkeletonTemplate =
        resolved;

    g_templateResolutionFailureLogged =
        false;

    std::stringstream message;

    message
        << "resolved purpose-built Standard Skeleton template; requestedStringID=\""
        << STANDARD_SKELETON_TEMPLATE_STRING_ID
        << "\"; expectedName=\""
        << STANDARD_SKELETON_TEMPLATE_EXPECTED_NAME
        << "\"; resolvedName=\""
        << resolved->name
        << "\"; stringID=\""
        << resolved->stringID
        << "\"; category=CHARACTER; purposeBuiltTemplate=true.";

    LogInfo(
        message.str()
    );

    return true;
}

void UpdateSafeSpawnCache(
    Building* cradleBuilding,
    CradleState& state
)
{
    if (
        cradleBuilding == NULL ||
        !cradleBuilding->isValid()
    )
    {
        return;
    }

    UseableStuff* cradleUsable =
        cradleBuilding->getUseableStuff();

    if (cradleUsable == NULL)
    {
        return;
    }

    std::set<
        hand,
        std::less<hand>,
        Ogre::STLAllocator<
            hand,
            Ogre::GeneralAllocPolicy
        >
    >::const_iterator iterator =
        cradleUsable->currentOperators.begin();

    for (
        ;
        iterator !=
            cradleUsable->currentOperators.end();
        ++iterator
    )
    {
        Character* worker =
            iterator->getCharacter();

        if (
            worker == NULL ||
            !worker->isValid() ||
            worker->isDead() ||
            worker->isOnARoof()
        )
        {
            continue;
        }

        const Ogre::Vector3 workerPosition =
            worker->getPosition();

        state.safeSpawnCached =
            true;

        state.safeSpawnFloor =
            worker->getFloor();

        state.safeSpawnX =
            workerPosition.x;

        state.safeSpawnY =
            workerPosition.y;

        state.safeSpawnZ =
            workerPosition.z;

        return;
    }
}

bool ResolveBesideCradleSpawnPosition(
    Building* cradleBuilding,
    CradleState& state,
    Ogre::Vector3& spawnPosition,
    int& spawnFloor,
    std::string& spawnSource
)
{
    UpdateSafeSpawnCache(
        cradleBuilding,
        state
    );

    if (
        !state.safeSpawnCached ||
        cradleBuilding == NULL ||
        !cradleBuilding->isValid()
    )
    {
        spawnSource =
            "none";

        return false;
    }

    const Ogre::Vector3& cradlePosition =
        cradleBuilding->getPosition();

    const float deltaX =
        state.safeSpawnX -
        cradlePosition.x;

    const float deltaZ =
        state.safeSpawnZ -
        cradlePosition.z;

    const float absoluteDeltaX =
        deltaX < 0.0f ?
        -deltaX :
        deltaX;

    const float absoluteDeltaZ =
        deltaZ < 0.0f ?
        -deltaZ :
        deltaZ;

    spawnPosition.x =
        state.safeSpawnX;

    spawnPosition.y =
        state.safeSpawnY;

    spawnPosition.z =
        state.safeSpawnZ;

    /*
     * The worker already stands on a valid interaction side of
     * the cradle. Nudge the new Skeleton a little farther away on
     * that same side so it does not overlap the worker.
     */
    if (absoluteDeltaX >= absoluteDeltaZ)
    {
        spawnPosition.x +=
            deltaX >= 0.0f ?
            BESIDE_CRADLE_NUDGE :
            -BESIDE_CRADLE_NUDGE;
    }
    else
    {
        spawnPosition.z +=
            deltaZ >= 0.0f ?
            BESIDE_CRADLE_NUDGE :
            -BESIDE_CRADLE_NUDGE;
    }

    spawnFloor =
        state.safeSpawnFloor;

    spawnSource =
        "cached-crafting-operator-side";

    return true;
}

bool FindFreshChassisForConsumption(
    Building* cradleBuilding,
    Inventory*& inventory,
    Item*& chassisItem,
    int& totalQuantity
)
{
    inventory =
        NULL;

    chassisItem =
        NULL;

    totalQuantity =
        0;

    if (
        cradleBuilding == NULL ||
        !cradleBuilding->isValid()
    )
    {
        return false;
    }

    UseableStuff* usable =
        cradleBuilding->getUseableStuff();

    if (usable == NULL)
    {
        return false;
    }

    inventory =
        usable->getInventory();

    if (inventory == NULL)
    {
        return false;
    }

    const lektor<Item*>& items =
        inventory->getAllItems();

    for (
        unsigned int index = 0;
        index < items.size();
        ++index
    )
    {
        Item* item =
            items[index];

        if (
            item == NULL ||
            !item->isValid() ||
            item->quantity <= 0
        )
        {
            continue;
        }

        GameData* itemData =
            item->getGameData();

        if (
            itemData == NULL ||
            itemData->stringID !=
                TARGET_CHASSIS_STRING_ID
        )
        {
            continue;
        }

        totalQuantity +=
            item->quantity;

        if (chassisItem == NULL)
        {
            chassisItem =
                item;
        }
    }

    return
        inventory != NULL &&
        chassisItem != NULL &&
        totalQuantity > 0;
}

int CountFreshChassisQuantity(
    Building* cradleBuilding
)
{
    Inventory* inventory =
        NULL;

    Item* chassisItem =
        NULL;

    int totalQuantity =
        0;

    FindFreshChassisForConsumption(
        cradleBuilding,
        inventory,
        chassisItem,
        totalQuantity
    );

    return totalQuantity;
}

bool ConsumeExactlyOneChassisAfterSpawn(
    Building* cradleBuilding,
    int& quantityBefore,
    int& quantityAfter
)
{
    quantityBefore =
        0;

    quantityAfter =
        0;

    Inventory* inventory =
        NULL;

    Item* chassisItem =
        NULL;

    if (!FindFreshChassisForConsumption(
        cradleBuilding,
        inventory,
        chassisItem,
        quantityBefore
    ))
    {
        return false;
    }

    if (!inventory->
        removeItemAutoDestroy(
            chassisItem,
            1
        ))
    {
        return false;
    }

    inventory->
        notifyModified();

    quantityAfter =
        CountFreshChassisQuantity(
            cradleBuilding
        );

    return
        quantityAfter ==
        quantityBefore - 1;
}

void ApplyPostConsumptionReadinessState(
    CradleState& state,
    int remainingQuantity
)
{
    state.inventorySignature.clear();

    state.assemblyReady =
        false;

    state.realSpawnAttempted =
        false;

    state.chassisAbsentScans =
        0;

    state.chassisQuantity =
        remainingQuantity;

    if (remainingQuantity > 0)
    {
        state.hasChassis =
            true;

        state.chassisStringID =
            TARGET_CHASSIS_STRING_ID;

        return;
    }

    state.hasChassis =
        false;

    state.chassisStringID.clear();
}

void TryActivateStandardSkeletonR13B(
    Building* cradleBuilding,
    CradleState& state
)
{
    if (
        !state.assemblyReady ||
        state.realSpawnAttempted ||
        cradleBuilding == NULL ||
        !cradleBuilding->isValid()
    )
    {
        return;
    }

    if (
        g_sessionSpawnCount >=
        SESSION_SPAWN_LIMIT
    )
    {
        state.realSpawnAttempted =
            true;

        if (!g_sessionSpawnLimitLogged)
        {
            g_sessionSpawnLimitLogged =
                true;

            LogErrorMessage(
                "R13 session activation limit reached; later chassis events are blocked until Kenshi restarts."
            );
        }

        return;
    }

    if (
        g_activeWorld == NULL ||
        g_activeWorld->theFactory == NULL ||
        !ResolveStandardSkeletonTemplate(
            g_activeWorld
        )
    )
    {
        return;
    }

    Faction* playerFaction =
        FindPlayerFactionForDryRun(
            g_activeWorld
        );

    if (playerFaction == NULL)
    {
        LogErrorMessage(
            "ACTIVATION R13B ABORTED; the player faction could not be resolved. Chassis was not consumed."
        );

        return;
    }

    ActivePlatoon* playerSquad =
        playerFaction->choosePlatoon();

    if (playerSquad == NULL)
    {
        LogErrorMessage(
            "ACTIVATION R13B ABORTED; an active player squad could not be resolved. Chassis was not consumed."
        );

        return;
    }

    Ogre::Vector3 spawnPosition =
        cradleBuilding->getPosition();

    int spawnFloor =
        cradleBuilding->getFloor();

    std::string spawnSource;

    const bool safeSpawnResolved =
        ResolveBesideCradleSpawnPosition(
            cradleBuilding,
            state,
            spawnPosition,
            spawnFloor,
            spawnSource
        );

    if (!safeSpawnResolved)
    {
        LogDebugMessage(
            "ACTIVATION R13B WAITING; no non-roof crafting-worker position has been cached. Chassis remains untouched."
        );

        return;
    }

    ++state.realSpawnAttempt;

    state.realSpawnAttempted =
        true;

    const unsigned int nextSessionSpawn =
        g_sessionSpawnCount +
        1;

    std::stringstream attemptDetails;

    attemptDetails
        << "templateStringID="
        << g_standardSkeletonTemplate->stringID
        << std::endl
        << "cradleStringID="
        << state.stringID
        << std::endl
        << "chassisStringID="
        << state.chassisStringID
        << std::endl
        << "readinessEvent="
        << state.readinessEvent
        << std::endl
        << "spawnAttempt="
        << state.realSpawnAttempt
        << std::endl
        << "sessionSpawn="
        << nextSessionSpawn
        << std::endl
        << "spawnSource="
        << spawnSource
        << std::endl
        << "safeSpawnResolved=true"
        << std::endl
        << "transactionOrder=spawn-then-fresh-resolve-then-consume"
        << std::endl
        << "requestedConsumeQuantity=1"
        << std::endl
        << "snapshotIsBlocking=false";

    WriteRealSpawnLock(
        "attempting-spawn-before-consumption",
        attemptDetails.str()
    );

    RootObjectContainer* playerContainer =
        reinterpret_cast<
            RootObjectContainer*
        >(
            playerSquad
        );

    RootObject* spawnedRoot =
        g_activeWorld->theFactory->
        createRandomCharacter(
            playerFaction,
            spawnPosition,
            playerContainer,
            g_standardSkeletonTemplate,
            NULL,
            1.0f
        );

    if (spawnedRoot == NULL)
    {
        WriteRealSpawnLock(
            "spawn-failed-chassis-not-consumed",
            attemptDetails.str()
        );

        LogErrorMessage(
            "ACTIVATION R13B FAILED; createRandomCharacter returned NULL; chassis was not consumed."
        );

        return;
    }

    ++g_sessionSpawnCount;

    Character* spawnedCharacter =
        static_cast<Character*>(
            spawnedRoot
        );

    spawnedCharacter->setFaction(
        playerFaction,
        playerSquad
    );

    spawnedCharacter->setName(
        SPAWNED_STANDARD_SKELETON_NAME
    );

    spawnedCharacter->setFloor(
        spawnFloor
    );

    spawnedCharacter->inSomething =
        IN_NOTHING;

    spawnedCharacter->inWhat.setNull();

    spawnedCharacter->setVisible(
        true
    );

    spawnedCharacter->
        resetRagdollNavmeshSafePos();

    spawnedCharacter->
        reThinkCurrentAIAction();

    const Ogre::Vector3 actualPosition =
        spawnedCharacter->getPosition();

    int chassisQuantityBefore =
        0;

    int chassisQuantityAfter =
        0;

    const bool chassisConsumed =
        ConsumeExactlyOneChassisAfterSpawn(
            cradleBuilding,
            chassisQuantityBefore,
            chassisQuantityAfter
        );

    if (!chassisConsumed)
    {
        std::stringstream failureDetails;

        failureDetails
            << attemptDetails.str()
            << std::endl
            << "status=spawned-but-chassis-consumption-failed"
            << std::endl
            << "spawnedName="
            << spawnedCharacter->getName()
            << std::endl
            << "chassisQuantityBeforeAttempt="
            << chassisQuantityBefore
            << std::endl
            << "chassisQuantityAfterAttempt="
            << chassisQuantityAfter
            << std::endl
            << "sameSessionDuplicateGuard=true"
            << std::endl
            << "reloadSafe=false"
            << std::endl
            << "manualInterventionRequired=true";

        WriteRealSpawnLock(
            "spawned-but-chassis-consumption-failed",
            failureDetails.str()
        );

        std::stringstream failureMessage;

        failureMessage
            << "ACTIVATION R13B CONSUMPTION FAILED AFTER SPAWN; spawnedName=\""
            << spawnedCharacter->getName()
            << "\"; chassisQuantityBeforeAttempt="
            << chassisQuantityBefore
            << "; chassisQuantityAfterAttempt="
            << chassisQuantityAfter
            << "; duplicateGuardRemainsArmed=true"
            << "; DO NOT SAVE/RELOAD THIS FAILED TEST; chassisConsumed=false.";

        LogErrorMessage(
            failureMessage.str()
        );

        return;
    }

    ApplyPostConsumptionReadinessState(
        state,
        chassisQuantityAfter
    );

    int spawnedCharacterChassisBefore =
        0;

    int spawnedCharacterChassisRemoved =
        0;

    int spawnedCharacterChassisAfter =
        0;

    const bool spawnedInventoryCleanupSucceeded =
        CleanupActivatedChassisFromSpawnedInventory(
            spawnedCharacter,
            spawnedCharacterChassisBefore,
            spawnedCharacterChassisRemoved,
            spawnedCharacterChassisAfter
        );

    std::stringstream cleanupMessage;

    cleanupMessage
        << "ACTIVATION R13B INVENTORY CLEANUP; spawnedName=\""
        << spawnedCharacter->getName()
        << "\"; spawnedCharacterChassisBefore="
        << spawnedCharacterChassisBefore
        << "; spawnedCharacterChassisRemoved="
        << spawnedCharacterChassisRemoved
        << "; spawnedCharacterChassisAfter="
        << spawnedCharacterChassisAfter
        << "; spawnedInventoryCleanupSucceeded="
        << (
            spawnedInventoryCleanupSucceeded ?
            "true" :
            "false"
        )
        << "; targetStringID=\""
        << TARGET_CHASSIS_STRING_ID
        << "\".";

    if (spawnedInventoryCleanupSucceeded)
    {
        LogInfo(
            cleanupMessage.str()
        );
    }
    else
    {
        LogErrorMessage(
            cleanupMessage.str()
        );
    }

    std::stringstream completedDetails;

    completedDetails
        << attemptDetails.str()
        << std::endl
        << "status=spawned-and-chassis-consumed"
        << std::endl
        << "spawnedName="
        << spawnedCharacter->getName()
        << std::endl
        << "requestedPosition="
        << spawnPosition.x << ","
        << spawnPosition.y << ","
        << spawnPosition.z
        << std::endl
        << "actualPosition="
        << actualPosition.x << ","
        << actualPosition.y << ","
        << actualPosition.z
        << std::endl
        << "spawnFloor="
        << spawnFloor
        << std::endl
        << "isOnRoof="
        << (
            spawnedCharacter->isOnARoof() ?
            "true" :
            "false"
        )
        << std::endl
        << "characterIsDead="
        << (
            spawnedCharacter->isDead() ?
            "true" :
            "false"
        )
        << std::endl
        << "unconscious="
        << (
            spawnedCharacter->isUnconcious() ?
            "true" :
            "false"
        )
        << std::endl
        << "chassisQuantityBefore="
        << chassisQuantityBefore
        << std::endl
        << "chassisQuantityAfter="
        << chassisQuantityAfter
        << std::endl
        << "chassisConsumed=true"
        << std::endl
        << "inventoryNotified=true"
        << std::endl
        << "readinessReset=true"
        << std::endl
        << "reloadSafeNormalPath=true";

    WriteRealSpawnLock(
        "spawned-and-chassis-consumed",
        completedDetails.str()
    );

    std::stringstream message;

    message
        << "ACTIVATION R13B COMPLETE; spawnedName=\""
        << spawnedCharacter->getName()
        << "\"; readinessEvent="
        << state.readinessEvent
        << "; sessionSpawn="
        << g_sessionSpawnCount
        << "; spawnSource="
        << spawnSource
        << "; safeSpawnResolved=true"
        << "; actualPosition=("
        << actualPosition.x << ", "
        << actualPosition.y << ", "
        << actualPosition.z
        << "); isOnRoof="
        << (
            spawnedCharacter->isOnARoof() ?
            "true" :
            "false"
        )
        << "; characterIsDead="
        << (
            spawnedCharacter->isDead() ?
            "true" :
            "false"
        )
        << "; unconscious="
        << (
            spawnedCharacter->isUnconcious() ?
            "true" :
            "false"
        )
        << "; chassisQuantityBefore="
        << chassisQuantityBefore
        << "; chassisQuantityAfter="
        << chassisQuantityAfter
        << "; chassisConsumed=true"
        << "; inventoryNotified=true"
        << "; readinessReset=true"
        << "; reloadSafeNormalPath=true"
        << "; playerFactionAssigned=true"
        << "; playerSquadAssigned=true"
        << "; homeBuildingAssigned=false"
        << "; damageApplied=false"
        << "; knockoutApplied=false"
        << "; ragdollApplied=false"
        << "; animationApplied=false"
        << "; spawnedCharacterChassisBefore="
        << spawnedCharacterChassisBefore
        << "; spawnedCharacterChassisRemoved="
        << spawnedCharacterChassisRemoved
        << "; spawnedCharacterChassisAfter="
        << spawnedCharacterChassisAfter
        << "; spawnedInventoryCleanupSucceeded="
        << (
            spawnedInventoryCleanupSucceeded ?
            "true" :
            "false"
        )
        << "; notificationSent="
        << (
            SHOW_PLAYER_MESSAGES ?
            "true" :
            "false"
        )
        << ".";

    LogInfo(
        message.str()
    );

    ShowPlayerMessage(
        g_activeWorld,
        ACTIVATION_PLAYER_MESSAGE
    );

    if (SHOW_PLAYER_MESSAGES)
    {
        LogDebugMessage(
            "ACTIVATION R13B player notification sent."
        );
    }
}

void InspectCradleInventory(
    Building* cradleBuilding,
    CradleState& state
)
{
    if (cradleBuilding == NULL)
    {
        return;
    }

    UseableStuff* usable =
        cradleBuilding->getUseableStuff();

    if (usable == NULL)
    {
        return;
    }

    Inventory* inventory =
        usable->getInventory();

    if (inventory == NULL)
    {
        return;
    }

    const lektor<Item*>& items =
        inventory->getAllItems();

    bool chassisFound =
        false;

    int totalChassisQuantity =
        0;

    std::string detectedStringID;
    std::stringstream signatureBuilder;
    std::stringstream inventorySummary;

    unsigned int validItemCount =
        0;

    for (
        unsigned int index = 0;
        index < items.size();
        ++index
    )
    {
        Item* item =
            items[index];

        if (
            item == NULL ||
            !item->isValid() ||
            item->quantity <= 0
        )
        {
            continue;
        }

        GameData* itemData =
            item->getGameData();

        if (itemData == NULL)
        {
            continue;
        }

        const std::string itemName =
            item->getName();

        ++validItemCount;

        signatureBuilder
            << itemData->stringID
            << ":"
            << item->quantity
            << ";";

        if (validItemCount > 1)
        {
            inventorySummary
                << ", ";
        }

        inventorySummary
            << "\""
            << itemName
            << "\"["
            << itemData->stringID
            << "]x"
            << item->quantity;

        if (
            itemData->stringID !=
            TARGET_CHASSIS_STRING_ID
        )
        {
            continue;
        }

        chassisFound =
            true;

        detectedStringID =
            itemData->stringID;

        totalChassisQuantity +=
            item->quantity;
    }

    const std::string newSignature =
        signatureBuilder.str();

    if (
        state.inventorySignature !=
        newSignature
    )
    {
        state.inventorySignature =
            newSignature;

        std::stringstream message;

        message
            << "cradle inventory changed; item count="
            << validItemCount;

        if (validItemCount > 0)
        {
            message
                << "; items="
                << inventorySummary.str();
        }

        message
            << ".";

        LogDebugMessage(
            message.str()
        );
    }

    if (chassisFound)
    {
        const bool chassisChanged =
            !state.hasChassis ||
            state.chassisStringID !=
                detectedStringID ||
            state.chassisQuantity !=
                totalChassisQuantity;

        state.hasChassis =
            true;

        state.chassisAbsentScans =
            0;

        state.chassisStringID =
            detectedStringID;

        state.chassisQuantity =
            totalChassisQuantity;

        if (chassisChanged)
        {
            std::stringstream message;

            message
                << "detected Activated Skeleton Chassis; stringID=\""
                << detectedStringID
                << "\"; quantity="
                << totalChassisQuantity
                << "; transactional=true.";

            LogInfo(
                message.str()
            );
        }

        if (!state.assemblyReady)
        {
            state.assemblyReady =
                true;

            ++state.readinessEvent;

            std::stringstream message;

            message
                << "PORTABLE CHASSIS READY; cradleStringID=\""
                << state.stringID
                << "\"; chassisStringID=\""
                << state.chassisStringID
                << "\"; quantity="
                << state.chassisQuantity
                << "; readinessEvent="
                << state.readinessEvent
                << "; portableOutput=true; cradleActivation=false; groundActivation=true.";

            LogInfo(
                message.str()
            );
        }

        /*
         * R14: the cradle manufactures a portable Activated Skeleton
         * Chassis. Activation is intentionally deferred until that
         * item is dropped on the ground.
         */

        return;
    }

    state.hasChassis =
        false;

    state.chassisQuantity =
        0;

    state.chassisStringID.clear();

    if (!state.assemblyReady)
    {
        state.chassisAbsentScans =
            0;

        return;
    }

    ++state.chassisAbsentScans;

    if (
        state.chassisAbsentScans <
        READINESS_RESET_ABSENT_SCANS
    )
    {
        LogDebugMessage(
            "assembly-ready cradle briefly lacks its chassis; duplicate guard remains armed."
        );

        return;
    }

    state.assemblyReady =
        false;

    state.realSpawnAttempted =
        false;

    state.chassisAbsentScans =
        0;

    LogDebugMessage(
        "assembly readiness reset after three confirmed chassis-absent scans; a later chassis may create one new readiness event."
    );
}

bool IsGroundedActivatedSkeletonChassis(
    Item* item
)
{
    if (
        item == NULL ||
        !item->isValid() ||
        !item->onGround() ||
        item->quantity != 1
    )
    {
        return false;
    }

    GameData* itemData =
        item->getGameData();

    return
        itemData != NULL &&
        itemData->stringID ==
            TARGET_CHASSIS_STRING_ID;
}

bool HasGroundDeploymentGuard(
    const hand& chassisHandle
)
{
    return
        g_groundDeploymentGuards.find(
            chassisHandle
        ) !=
        g_groundDeploymentGuards.end();
}

void TryDeployGroundedActivatedChassisR14(
    GameWorld* world,
    Item* chassisItem
)
{
    if (
        world == NULL ||
        world->theFactory == NULL ||
        !IsGroundedActivatedSkeletonChassis(
            chassisItem
        )
    )
    {
        return;
    }

    const hand& chassisHandle =
        chassisItem->getHandle();

    if (
        chassisHandle.isNull() ||
        HasGroundDeploymentGuard(
            chassisHandle
        )
    )
    {
        return;
    }

    if (
        g_sessionSpawnCount >=
        SESSION_SPAWN_LIMIT
    )
    {
        if (!g_sessionSpawnLimitLogged)
        {
            g_sessionSpawnLimitLogged =
                true;

            LogErrorMessage(
                "R14 session deployment limit reached; grounded Activated Skeleton Chassis items remain intact until Kenshi restarts."
            );
        }

        return;
    }

    if (!ResolveStandardSkeletonTemplate(
        world
    ))
    {
        return;
    }

    Faction* playerFaction =
        FindPlayerFactionForDryRun(
            world
        );

    if (playerFaction == NULL)
    {
        LogErrorMessage(
            "GROUND ACTIVATION R14 WAITING; the player faction could not be resolved; grounded chassis remains intact."
        );

        return;
    }

    ActivePlatoon* playerSquad =
        playerFaction->choosePlatoon();

    if (playerSquad == NULL)
    {
        LogErrorMessage(
            "GROUND ACTIVATION R14 WAITING; an active player squad could not be resolved; grounded chassis remains intact."
        );

        return;
    }

    Ogre::Vector3 spawnPosition =
        chassisItem->getPosition();

    const int spawnFloor =
        chassisItem->getFloor();

    ++g_groundDeploymentEvent;

    const unsigned int deploymentEvent =
        g_groundDeploymentEvent;

    const unsigned int nextSessionSpawn =
        g_sessionSpawnCount +
        1;

    g_groundDeploymentGuards.insert(
        std::make_pair(
            chassisHandle,
            true
        )
    );

    std::stringstream attemptDetails;

    attemptDetails
        << "templateStringID="
        << g_standardSkeletonTemplate->stringID
        << std::endl
        << "chassisStringID="
        << TARGET_CHASSIS_STRING_ID
        << std::endl
        << "deploymentEvent="
        << deploymentEvent
        << std::endl
        << "sessionSpawn="
        << nextSessionSpawn
        << std::endl
        << "spawnSource=grounded-activated-chassis"
        << std::endl
        << "requestedPosition="
        << spawnPosition.x << ","
        << spawnPosition.y << ","
        << spawnPosition.z
        << std::endl
        << "spawnFloor="
        << spawnFloor
        << std::endl
        << "transactionOrder=ground-detect-then-spawn-then-destroy-chassis"
        << std::endl
        << "sameSessionDuplicateGuard=true";

    WriteRealSpawnLock(
        "ground-deployment-attempt",
        attemptDetails.str()
    );

    RootObjectContainer* playerContainer =
        reinterpret_cast<
            RootObjectContainer*
        >(
            playerSquad
        );

    RootObject* spawnedRoot =
        world->theFactory->
        createRandomCharacter(
            playerFaction,
            spawnPosition,
            playerContainer,
            g_standardSkeletonTemplate,
            NULL,
            1.0f
        );

    if (spawnedRoot == NULL)
    {
        g_groundDeploymentGuards.erase(
            chassisHandle
        );

        WriteRealSpawnLock(
            "ground-spawn-failed-chassis-retained",
            attemptDetails.str()
        );

        LogErrorMessage(
            "GROUND ACTIVATION R14 FAILED; createRandomCharacter returned NULL; grounded chassis was retained and its deployment guard was cleared for retry."
        );

        return;
    }

    ++g_sessionSpawnCount;

    Character* spawnedCharacter =
        static_cast<Character*>(
            spawnedRoot
        );

    spawnedCharacter->setFaction(
        playerFaction,
        playerSquad
    );

    spawnedCharacter->setName(
        SPAWNED_STANDARD_SKELETON_NAME
    );

    spawnedCharacter->setFloor(
        spawnFloor
    );

    spawnedCharacter->inSomething =
        IN_NOTHING;

    spawnedCharacter->inWhat.setNull();

    spawnedCharacter->setVisible(
        true
    );

    spawnedCharacter->
        resetRagdollNavmeshSafePos();

    spawnedCharacter->
        reThinkCurrentAIAction();

    const Ogre::Vector3 actualPosition =
        spawnedCharacter->getPosition();

    const bool chassisDestroyQueued =
        world->destroy(
            static_cast<RootObject*>(
                chassisItem
            ),
            false,
            "CraftASkeleton R14 grounded Activated Skeleton Chassis consumed"
        );

    if (!chassisDestroyQueued)
    {
        std::stringstream failureDetails;

        failureDetails
            << attemptDetails.str()
            << std::endl
            << "status=spawned-but-ground-chassis-destroy-failed"
            << std::endl
            << "spawnedName="
            << spawnedCharacter->getName()
            << std::endl
            << "sameSessionDuplicateGuard=true"
            << std::endl
            << "reloadSafe=false"
            << std::endl
            << "manualInterventionRequired=true";

        WriteRealSpawnLock(
            "spawned-but-ground-chassis-destroy-failed",
            failureDetails.str()
        );

        LogErrorMessage(
            "GROUND ACTIVATION R14 CLEANUP FAILED AFTER SPAWN; same-session duplicate guard remains armed; DO NOT SAVE/RELOAD THIS FAILED TEST."
        );

        return;
    }

    std::stringstream completedDetails;

    completedDetails
        << attemptDetails.str()
        << std::endl
        << "status=grounded-chassis-deployed"
        << std::endl
        << "spawnedName="
        << spawnedCharacter->getName()
        << std::endl
        << "actualPosition="
        << actualPosition.x << ","
        << actualPosition.y << ","
        << actualPosition.z
        << std::endl
        << "isOnRoof="
        << (
            spawnedCharacter->isOnARoof() ?
            "true" :
            "false"
        )
        << std::endl
        << "characterIsDead="
        << (
            spawnedCharacter->isDead() ?
            "true" :
            "false"
        )
        << std::endl
        << "unconscious="
        << (
            spawnedCharacter->isUnconcious() ?
            "true" :
            "false"
        )
        << std::endl
        << "groundChassisDestroyQueued=true"
        << std::endl
        << "sameSessionDuplicateGuard=true"
        << std::endl
        << "playerFactionAssigned=true"
        << std::endl
        << "playerSquadAssigned=true";

    WriteRealSpawnLock(
        "grounded-chassis-deployed",
        completedDetails.str()
    );

    std::stringstream message;

    message
        << "GROUND ACTIVATION R14 COMPLETE; spawnedName=\""
        << spawnedCharacter->getName()
        << "\"; deploymentEvent="
        << deploymentEvent
        << "; sessionSpawn="
        << g_sessionSpawnCount
        << "; spawnSource=grounded-activated-chassis"
        << "; actualPosition=("
        << actualPosition.x << ", "
        << actualPosition.y << ", "
        << actualPosition.z
        << "); spawnFloor="
        << spawnFloor
        << "; isOnRoof="
        << (
            spawnedCharacter->isOnARoof() ?
            "true" :
            "false"
        )
        << "; characterIsDead="
        << (
            spawnedCharacter->isDead() ?
            "true" :
            "false"
        )
        << "; unconscious="
        << (
            spawnedCharacter->isUnconcious() ?
            "true" :
            "false"
        )
        << "; groundChassisDestroyQueued=true"
        << "; sameSessionDuplicateGuard=true"
        << "; playerFactionAssigned=true"
        << "; playerSquadAssigned=true"
        << "; notificationSent="
        << (
            SHOW_PLAYER_MESSAGES ?
            "true" :
            "false"
        )
        << ".";

    LogInfo(
        message.str()
    );

    ShowPlayerMessage(
        world,
        ACTIVATION_PLAYER_MESSAGE
    );

    if (SHOW_PLAYER_MESSAGES)
    {
        LogDebugMessage(
            "GROUND ACTIVATION R14 player notification sent."
        );
    }
}

void MonitorGroundedActivatedChassis(
    GameWorld* world
)
{
    if (
        world == NULL ||
        world->isLoadingFromASaveGame()
    )
    {
        return;
    }

    lektor<RootObject*>
        nearbyItems;

    world->getObjectsWithinSphere(
        nearbyItems,
        world->getCameraCenter(),
        GROUND_DEPLOY_SCAN_RANGE,
        ITEM,
        GROUND_DEPLOY_SCAN_MAX_ITEMS,
        NULL
    );

    for (
        unsigned int index = 0;
        index < nearbyItems.size();
        ++index
    )
    {
        RootObject* object =
            nearbyItems[index];

        if (
            object == NULL ||
            !object->isValid() ||
            object->getDataType() != ITEM
        )
        {
            continue;
        }

        Item* item =
            static_cast<Item*>(
                object
            );

        if (!IsGroundedActivatedSkeletonChassis(
            item
        ))
        {
            continue;
        }

        const hand& itemHandle =
            item->getHandle();

        if (
            itemHandle.isNull() ||
            HasGroundDeploymentGuard(
                itemHandle
            )
        )
        {
            continue;
        }

        std::stringstream detected;

        const Ogre::Vector3 itemPosition =
            item->getPosition();

        detected
            << "GROUND ACTIVATION READY; stringID=\""
            << TARGET_CHASSIS_STRING_ID
            << "\"; quantity="
            << item->quantity
            << "; position=("
            << itemPosition.x << ", "
            << itemPosition.y << ", "
            << itemPosition.z
            << "); floor="
            << item->getFloor()
            << "; onGround=true.";

        LogInfo(
            detected.str()
        );

        TryDeployGroundedActivatedChassisR14(
            world,
            item
        );
    }
}

void DiscoverNearbyCradles(
    GameWorld* world
)
{
    if (world == NULL)
    {
        return;
    }

    lektor<RootObject*>
        nearbyBuildings;

    world->getObjectsWithinSphere(
        nearbyBuildings,
        world->getCameraCenter(),
        DISCOVERY_RANGE,
        BUILDING,
        1000,
        NULL
    );

    for (
        unsigned int index = 0;
        index < nearbyBuildings.size();
        ++index
    )
    {
        RootObject* object =
            nearbyBuildings[index];

        if (
            object == NULL ||
            !object->isValid()
        )
        {
            continue;
        }

        GameData* data =
            object->getGameData();

        if (!IsTargetCradle(data))
        {
            continue;
        }

        Building* cradleBuilding =
            static_cast<Building*>(
                object
            );

        hand cradleHandle;

        if (!GetStableCradleHandle(
            cradleBuilding,
            cradleHandle
        ))
        {
            LogDebugMessage(
                "found a matching cradle but could not obtain a stable handle."
            );

            continue;
        }

        std::map<
            hand,
            CradleState
        >::iterator existing =
            g_cradleStates.find(
                cradleHandle
            );

        if (existing ==
            g_cradleStates.end())
        {
            CradleState newState;

            newState.stringID =
                data->stringID;

            std::pair<
                std::map<
                    hand,
                    CradleState
                >::iterator,
                bool
            > insertion =
                g_cradleStates.insert(
                    std::make_pair(
                        cradleHandle,
                        newState
                    )
                );

            existing =
                insertion.first;

            std::stringstream message;

            message
                << "detected Skeleton Assembly Cradle; stringID=\""
                << data->stringID
                << "\"; tracked cradles="
                << g_cradleStates.size()
                << ".";

            LogInfo(
                message.str()
            );
        }

        existing->second.unresolvedScans =
            0;

        existing->second.stringID =
            data->stringID;

        InspectCradleInventory(
            cradleBuilding,
            existing->second
        );
    }
}

void MonitorKnownCradles()
{
    std::map<
        hand,
        CradleState
    >::iterator iterator =
        g_cradleStates.begin();

    while (
        iterator !=
        g_cradleStates.end()
    )
    {
        const hand& cradleHandle =
            iterator->first;

        CradleState& state =
            iterator->second;

        Building* cradleBuilding =
            cradleHandle.getBuilding();

        if (
            cradleBuilding == NULL ||
            !cradleBuilding->isValid()
        )
        {
            /* R11B: no unresolved-cradle corpse cleanup remains. */

            ++state.unresolvedScans;

            if (!state.wasUnresolved)
            {
                state.wasUnresolved =
                    true;

                LogDebugMessage(
                    "a tracked cradle became temporarily unavailable; its state was retained."
                );
            }

            if (
                state.unresolvedScans >=
                UNRESOLVED_FORGET_SCANS
            )
            {
                std::map<
                    hand,
                    CradleState
                >::iterator eraseIterator =
                    iterator;

                ++iterator;

                g_cradleStates.erase(
                    eraseIterator
                );

                LogDebugMessage(
                    "forgot a cradle that remained unresolved."
                );
            }
            else
            {
                ++iterator;
            }

            continue;
        }

        if (state.wasUnresolved)
        {
            state.wasUnresolved =
                false;

            LogDebugMessage(
                "a tracked cradle resumed after unloading."
            );
        }

        GameData* data =
            cradleBuilding->getGameData();

        if (!IsTargetCradle(data))
        {
            std::map<
                hand,
                CradleState
            >::iterator eraseIterator =
                iterator;

            ++iterator;

            g_cradleStates.erase(
                eraseIterator
            );

            LogDebugMessage(
                "removed a handle that no longer belongs to a Skeleton Assembly Cradle."
            );

            continue;
        }

        state.unresolvedScans =
            0;

        state.stringID =
            data->stringID;

        UpdateSafeSpawnCache(
            cradleBuilding,
            state
        );

        InspectCradleInventory(
            cradleBuilding,
            state
        );

        /* R11 has no persistent corpse or pose state to maintain. */

        ++iterator;
    }
}

void MonitorAllCradles(
    GameWorld* world
)
{
    g_activeWorld =
        world;

    ResolveStandardSkeletonTemplate(
        world
    );

    DiscoverNearbyCradles(
        world
    );

    MonitorKnownCradles();
}

void GameWorld_mainLoop_hook(
    GameWorld* thisptr,
    float time
)
{
    GameWorld_mainLoop_orig(
        thisptr,
        time
    );

    static float updateTimer =
        0.0f;

    updateTimer +=
        time;

    if (updateTimer <
        UPDATE_INTERVAL)
    {
        return;
    }

    updateTimer =
        0.0f;

    MonitorAllCradles(
        thisptr
    );

    MonitorGroundedActivatedChassis(
        thisptr
    );
}

BOOL APIENTRY DllMain(
    HMODULE module,
    DWORD reason,
    LPVOID reserved
)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        g_moduleHandle =
            module;

        DisableThreadLibraryCalls(
            module
        );
    }

    return TRUE;
}

__declspec(dllexport)
void startPlugin()
{
    LogInfo(
        "loading."
    );

    std::string configPath;

    bool configFound =
        false;

    g_debugLogging =
        ReadDebugToggle(
            configPath,
            configFound
        );

    std::stringstream configurationMessage;

    if (configFound)
    {
        configurationMessage
            << "configuration loaded; debug="
            << (g_debugLogging ? "on" : "off")
            << ".";
    }
    else
    {
        configurationMessage
            << "configuration file not found; debug defaults to off; path="
            << configPath
            << ".";
    }

    LogInfo(
        configurationMessage.str()
    );

    const bool mainLoopInstalled =
        KenshiLib::SUCCESS ==
        KenshiLib::AddHook(
            KenshiLib::GetRealAddress(
                &GameWorld::
                _NV_mainLoop_GPUSensitiveStuff
            ),
            &GameWorld_mainLoop_hook,
            &GameWorld_mainLoop_orig
        );

    if (!mainLoopInstalled)
    {
        LogErrorMessage(
            "failed to install the game-world monitoring hook."
        );

        LogInfo(
            "loaded with cradle discovery disabled."
        );

        return;
    }

    LogInfo(
        "game-world monitoring hook installed."
    );

    LogInfo(
        "loaded successfully; deployable-chassis R14 is active; the Skeleton Assembly Cradle now produces CAS_ActivatedSkeletonChassis as a portable item and never auto-activates it from cradle or character inventory; when exactly one Activated Skeleton Chassis is dropped on the ground within the loaded camera area, the plugin spawns one player Skeleton at that item's position and then queues the grounded chassis for destruction; spawn failure retains the chassis; same-session duplicate guards, the session spawn cap, and the Skeleton Activated player notification are enabled."
    );
}
