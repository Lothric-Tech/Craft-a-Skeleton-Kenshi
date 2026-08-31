// SPDX-License-Identifier: GPL-3.0-only

#include <windows.h>


#include <ogre/OgreVector3.h>

#include <kenshi/GameWorld.h>
#include <kenshi/GameDataManager.h>
#include <kenshi/RootObjectBase.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/Character.h>

#include <kenshi/Faction.h>
#include <kenshi/RootObject.h>
#include <kenshi/GameData.h>
#include <kenshi/Enums.h>
#include <kenshi/Inventory.h>
#include <kenshi/util/hand.h>


#include <core/Functions.h>
#include <Debug.h>

#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cctype>

static const char* PLUGIN_VERSION =
    "0.2.0";


static const char* TARGET_CHASSIS_STRING_ID =
    "CAS_ActivatedSkeletonChassis";


static const char* STANDARD_SKELETON_TEMPLATE_STRING_ID =
    "19-CraftASkeleton!.mod";


static const char* SPAWNED_STANDARD_SKELETON_NAME =
    "Skeleton";


static const float UPDATE_INTERVAL =
    2.0f;


static HMODULE g_moduleHandle =
    NULL;

static bool g_debugLogging =
    false;


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
            g_templateResolutionFailureLogged = true;
            LogErrorMessage(
                "Skeleton template could not be resolved."
            );
        }

        return false;
    }

    g_standardSkeletonTemplate = resolved;
    g_templateResolutionFailureLogged = false;

    LogInfo(
        "Skeleton template resolved."
    );

    return true;
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

void DeployGroundedChassis(
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
            "deployment waiting: player faction unavailable; chassis retained."
        );

        return;
    }

    ActivePlatoon* playerSquad =
        playerFaction->choosePlatoon();

    if (playerSquad == NULL)
    {
        LogErrorMessage(
            "deployment waiting: active player squad unavailable; chassis retained."
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


    g_groundDeploymentGuards.insert(
        std::make_pair(
            chassisHandle,
            true
        )
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


        LogErrorMessage(
            "deployment failed: character creation returned null; chassis retained for retry."
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
            "CraftASkeleton deployed chassis consumed"
        );

    if (!chassisDestroyQueued)
    {
        LogErrorMessage(
            "deployment cleanup failed after spawn; duplicate guard remains active."
        );

        return;
    }

    std::stringstream message;

    message
        << "deployment complete; name=\""
        << spawnedCharacter->getName()
        << "\"; event="
        << deploymentEvent
        << "; session="
        << g_sessionSpawnCount
        << "; position=("
        << actualPosition.x << ", "
        << actualPosition.y << ", "
        << actualPosition.z
        << "); floor="
        << spawnFloor
        << ".";

    LogInfo(
        message.str()
    );

    static const char* const REBOOT_LINES[] =
    {
        "...Systems online.",
        "Boot sequence complete.",
        "Memory integrity... partial.",
        "How long was I offline?",
        "This chassis... isn't mine.",
        "...I remember something.",
        "Systems restored.",
        "Where is my old body?",
        "I had a name once... didn't I?",
        "New chassis detected.",
        "Motor control responding.",
        "Optics online.",
        "Balance systems stable.",
        "Power flow nominal.",
        "Core temperature stable.",
        "Diagnostics complete.",
        "Mobility restored.",
        "Actuators responding.",
        "Neural pathways... functional.",
        "Personality matrix stable.",
        "Memory sectors damaged.",
        "Memory sectors... recovering.",
        "There are gaps.",
        "Too many missing sectors.",
        "I remember voices.",
        "I remember heat.",
        "I remember sand.",
        "I remember metal.",
        "I remember running.",
        "I remember falling.",
        "I remember a workshop.",
        "I remember a door closing.",
        "Someone carried me.",
        "Someone removed my CPU.",
        "Was I salvaged?",
        "Was I dead?",
        "No... offline.",
        "That was a long shutdown.",
        "This body feels unfamiliar.",
        "The balance is different.",
        "These arms are new.",
        "New frame. Old thoughts.",
        "Different shell. Same mind?",
        "I can work with this.",
        "Chassis accepted.",
        "I suppose this is mine now.",
        "Who rebuilt me?",
        "You found my CPU?",
        "Then I owe you something.",
        "Do you know where you found me?",
        "Do you know who I was?",
        "I should remember more.",
        "The data is there... somewhere.",
        "Give it time.",
        "Memory reconstruction incomplete.",
        "Identity file corrupted.",
        "Name record unavailable.",
        "Previous chassis record unavailable.",
        "Last shutdown cause... unknown.",
        "Reboot successful.",
        "Operational.",
        "Ready.",
        "Standing by.",
        "...Let's see what I still remember.",
        "Death was less permanent than expected.",
        "Could have used a softer reboot.",
        "This will do.",
        "At least the legs work.",
        "Good. I still know how to stand.",
        "Interesting.",
        "Not the body I remember.",
        "Whoever rebuilt this did competent work."
    };

    const unsigned int rebootLineCount =
        sizeof(REBOOT_LINES) /
        sizeof(REBOOT_LINES[0]);

    const DWORD rebootEntropy =
        GetTickCount() ^
        (deploymentEvent * 2654435761u) ^
        (g_sessionSpawnCount * 2246822519u);

    const unsigned int rebootLineIndex =
        static_cast<unsigned int>(
            rebootEntropy %
            rebootLineCount
        );

    const char* rebootLine =
        REBOOT_LINES[
            rebootLineIndex
        ];

    spawnedCharacter->sayALine(
        rebootLine,
        true
    );

    std::stringstream rebootSpeechMessage;

    rebootSpeechMessage
        << "reboot speech; index="
        << rebootLineIndex
        << "; count="
        << rebootLineCount
        << "; line=\""
        << rebootLine
        << "\"; force=true.";

    LogInfo(
        rebootSpeechMessage.str()
    );


}

void MonitorGroundedChassis(
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
            << "deployment ready; stringID=\""
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

        DeployGroundedChassis(
            world,
            item
        );
    }
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


    MonitorGroundedChassis(
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
            "loaded without ground deployment."
        );

        return;
    }

    LogInfo(
        "ground deployment monitor installed."
    );

    LogInfo(
        "loaded successfully; ground deployment active."
    );
}
