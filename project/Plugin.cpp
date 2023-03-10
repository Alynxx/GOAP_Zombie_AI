#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include "GoapActions.h"

using namespace std;

//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);
	m_pSteering = new SteeringPlugin_Output();
	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "GOAP_Alynxx";
	info.Student_FirstName = "Amber";
	info.Student_LastName = "Perard";
	info.Student_Class = "2DAE08E";

	m_pMemoryHouse = new std::vector<HouseInfoExtended>;
	m_pMemoryEntities = new std::vector<EntityInfoExtended>;
	m_pMemoryPistol = new std::vector<EntityInfoExtended>;
	m_pMemoryShotGuns = new std::vector<EntityInfoExtended>;
	m_pMemoryMedKits = new std::vector<EntityInfoExtended>;
	m_pMemoryFood = new std::vector<EntityInfoExtended>;
	m_pMemoryGarbage = new std::vector<EntityInfoExtended>;

	CreateBlackboard();
	InitializeWorldState();
	AddActions();
	AddGoals();
}

//Called only once
void Plugin::DllInit()
{
	//Called when the plugin is loaded
}

//Called only once
void Plugin::DllShutdown()
{
	//Called wheb the plugin gets unloaded
}

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = false; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be useful to inspect certain behaviors (Default = false)
	params.LevelFile = "GameLevel.gppl";
	params.AutoGrabClosestItem = false; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
	params.StartingDifficultyStage = 1;
	params.InfiniteStamina = false;
	params.SpawnDebugPistol = false;
	params.SpawnDebugShotgun = false;
	params.SpawnPurgeZonesOnMiddleClick = true;
	params.PrintDebugMessages = true;
	params.ShowDebugItemNames = true;
	params.Seed = 1;
	params.SpawnZombieOnRightClick = true;
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::Update(float dt)
{
}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	m_TotalElapsedTime += dt;
	GetEntitiesInFOV();
	updateHousesInMemory();
	GetNewHousesInFOV(dt);
	CheckIfInisdePurgeZone();
	GetEnemiesInFOV();
	m_DeltaTime = dt;

	auto agentInfo = m_pInterface->Agent_GetInfo();

	m_pBlackboard->ChangeData("Target", m_Target);
	m_pBlackboard->ChangeData("AgentInfo", agentInfo);
	m_pBlackboard->ChangeData("Enemies", m_EnemiesInFOV);
	m_pBlackboard->ChangeData("deltaTime", m_DeltaTime);

	m_WorldState.SetCondition("LowHealth", agentInfo.Health <= 4.f);
	m_WorldState.SetCondition("LowFood", agentInfo.Energy <= 4.f);
	m_WorldState.SetCondition("inDanger", agentInfo.WasBitten || !m_EnemiesInFOV.empty());
	m_WorldState.SetCondition("enemiesInRange", !m_EnemiesInFOV.empty());

	WorldState* newGoal = GetHighestPriorityGoal();
	if (m_CurrentGoal == nullptr || newGoal != m_CurrentGoal || empty(m_pPlan))
	{
		m_CurrentGoal = newGoal;
		FindingPath(m_WorldState, *m_CurrentGoal, m_pActions);
	}
	else ExecutingPlan();

	return *m_pSteering;
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	//This Render function should only contain calls to Interface->Draw_... functions
	m_pInterface->Draw_SolidCircle(m_Target, .7f, { 0,0 }, { 1, 0, 0 });

	auto sizeWorld = m_pInterface->World_GetInfo().Dimensions;
	sizeWorld.y *= 0.35f;
	sizeWorld.x *= 0.35f;

	m_pInterface->Draw_Circle(m_pInterface->World_GetInfo().Center, sizeWorld.y, { 0,0,0 }, 0.5f);
}

void Plugin::GetEntitiesInFOV()
{
	EntityInfoExtended entityInfo = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, entityInfo))
		{

			if (entityInfo.Type != eEntityType::ITEM) continue;

			ItemInfo item{};
			m_pInterface->Item_GetInfo(entityInfo, item);
			//// Check if we're not already aware of the entity
			if (std::find(m_pMemoryEntities->begin(), m_pMemoryEntities->end(), entityInfo) == m_pMemoryEntities->end())
			{
				m_pMemoryEntities->push_back(entityInfo);


				switch (item.Type)
				{
				case eItemType::PISTOL:
					m_pMemoryPistol->emplace_back(entityInfo);
					m_WorldState.SetCondition("savedPistol", true);
					SortByDistance(m_pMemoryPistol);
					break;
				case eItemType::SHOTGUN:
					m_pMemoryShotGuns->emplace_back(entityInfo);
					m_WorldState.SetCondition("savedShotgun", true);
					SortByDistance(m_pMemoryShotGuns);
					break;
				case eItemType::MEDKIT:
					m_pMemoryMedKits->emplace_back(entityInfo);
					m_WorldState.SetCondition("savedMedkit", true);
					SortByDistance(m_pMemoryMedKits);
					break;
				case eItemType::FOOD:
					m_pMemoryFood->emplace_back(entityInfo);
					m_WorldState.SetCondition("savedFood", true);
					SortByDistance(m_pMemoryFood);
					break;
				case eItemType::GARBAGE:
					m_pMemoryGarbage->emplace_back(entityInfo);
					m_WorldState.SetCondition("savedGarbage", true);
					SortByDistance(m_pMemoryGarbage);
					break;
				default:
					continue;
				}
			}
			else
			{
				// We are already aware of the entity, update it's hash
				switch (item.Type)
				{
				case eItemType::PISTOL:
				{
					auto& it = std::find(m_pMemoryPistol->begin(), m_pMemoryPistol->end(), entityInfo);
					if (it != m_pMemoryPistol->end())
						it->EntityHash = entityInfo.EntityHash;
					break;
				}
				case eItemType::SHOTGUN:
				{
					auto& it = std::find(m_pMemoryShotGuns->begin(), m_pMemoryShotGuns->end(), entityInfo);
					if (it != m_pMemoryShotGuns->end())
						it->EntityHash = entityInfo.EntityHash;
					break;
				}
				case eItemType::MEDKIT:
				{
					auto& it = std::find(m_pMemoryMedKits->begin(), m_pMemoryMedKits->end(), entityInfo);
					if (it != m_pMemoryMedKits->end())
						it->EntityHash = entityInfo.EntityHash;
					break;
				}
				case eItemType::FOOD:
				{
					auto& it = std::find(m_pMemoryFood->begin(), m_pMemoryFood->end(), entityInfo);
					if (it != m_pMemoryFood->end())
						it->EntityHash = entityInfo.EntityHash;
					break;
				}
				case eItemType::GARBAGE:
				{
					auto& it = std::find(m_pMemoryGarbage->begin(), m_pMemoryGarbage->end(), entityInfo);
					if (it != m_pMemoryGarbage->end())
						it->EntityHash = entityInfo.EntityHash;
					break;
				}


				}
			}
		}
		break;
	}
}

void Plugin::GetEnemiesInFOV()
{
	std::vector<EnemyInfo> enemiesInFOV;

	EntityInfo ei;
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			if (ei.Type == eEntityType::ENEMY)
			{
				EnemyInfo enemy;
				m_pInterface->Enemy_GetInfo(ei, enemy);
				enemiesInFOV.push_back(enemy);
			}
			continue;
		}
		break;
	}

	SortByDistance(&enemiesInFOV);
	m_EnemiesInFOV = enemiesInFOV;
}

bool Plugin::CheckIfInisdePurgeZone()
{
	EntityInfo ei{};
	m_WorldState.SetCondition("insidePurgezone", false);
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			if (ei.Type == eEntityType::PURGEZONE)
			{
				m_pInterface->PurgeZone_GetInfo(ei, m_PurgeZoneInFov);
				const Elite::Vector2 dir_vector = m_pInterface->Agent_GetInfo().Position - m_PurgeZoneInFov.Center;
				if (dir_vector.MagnitudeSquared() <= ((m_PurgeZoneInFov.Radius+5.f) * (m_PurgeZoneInFov.Radius+5.f)) )
				{
					m_Target = m_PurgeZoneInFov.Center + dir_vector.GetNormalized() * (m_PurgeZoneInFov.Radius * 1.35f);
					std::cout << "PurgezoneTarget: " << m_Target << std::endl;
					m_pBlackboard->ChangeData("PurgeZoneLocation", m_Target);
					m_WorldState.SetCondition("insidePurgezone", true);
					return true;
				}
			}
			continue;
		}
		break;
	}
	return false;
}

void Plugin::CreateBlackboard()
{
	m_pBlackboard = new Elite::Blackboard();
	m_pBlackboard->AddData("WorldState", &m_WorldState);
	m_pBlackboard->AddData("AgentInfo", AgentInfo{});
	m_pBlackboard->AddData("TargetItem", EntityInfoExtended{});
	m_pBlackboard->AddData("TargetHouse", new HouseInfoExtended{});
	m_pBlackboard->AddData("InventorySlot", 0U);
	m_pBlackboard->AddData("Target", Elite::Vector2{});
	m_pBlackboard->AddData("pSteering", m_pSteering);
	m_pBlackboard->AddData("pInterface", m_pInterface);
	m_pBlackboard->AddData("deltaTime", m_DeltaTime);
	m_pBlackboard->AddData("PurgeZoneLocation", Elite::Vector2{});

	// Entities
	m_pBlackboard->AddData("Houses", m_pMemoryHouse);
	m_pBlackboard->AddData("Pistols", m_pMemoryPistol);
	m_pBlackboard->AddData("Shotguns", m_pMemoryShotGuns);
	m_pBlackboard->AddData("Medkits", m_pMemoryMedKits);
	m_pBlackboard->AddData("Food", m_pMemoryFood);
	m_pBlackboard->AddData("Garbage", m_pMemoryGarbage);
	m_pBlackboard->AddData("Enemies", std::vector<EnemyInfo>{});

}

void Plugin::InitializeWorldState()
{
	// Initial world state
	m_WorldState.SetCondition("insidePurgezone", false);
	m_WorldState.SetCondition("inDanger", false);
	m_WorldState.SetCondition("LowHealth", false);
	m_WorldState.SetCondition("LowFood", false);

	m_WorldState.SetCondition("enemiesInRange", false);
	m_WorldState.SetCondition("houseInRange", false);
	m_WorldState.SetCondition("targetInRange", false);
	m_WorldState.SetCondition("itemInRange", false);
	m_WorldState.SetCondition("garbageInRange", false);

	m_WorldState.SetCondition("savedPistol", false);
	m_WorldState.SetCondition("savedShotgun", false);
	m_WorldState.SetCondition("savedMedkit", false);
	m_WorldState.SetCondition("savedFood", false);
	m_WorldState.SetCondition("savedGarbage", false);
	m_WorldState.SetCondition("destroyedGarbage", false);

	m_WorldState.SetCondition("pistolInInv", false);
	m_WorldState.SetCondition("foodInInv", false);
	m_WorldState.SetCondition("shotgunInInv", false);
	m_WorldState.SetCondition("medkitInInv", false);

	m_WorldState.SetCondition("exploring", false);
}

void Plugin::AddActions()
{
	m_pActions.push_back(new GOAP::Action_Explore);
	m_pActions.push_back(new GOAP::Action_MoveTo);
	m_pActions.push_back(new GOAP::Action_GrabFood);
	m_pActions.push_back(new GOAP::Action_GrabMedkit);
	m_pActions.push_back(new GOAP::Action_GrabPistol);
	m_pActions.push_back(new GOAP::Action_GrabShotGun);
	m_pActions.push_back(new GOAP::Action_DestroyGarbage);
	m_pActions.push_back(new GOAP::Action_ConsumeFood);
	m_pActions.push_back(new GOAP::Action_ConsumeMedKit);
	m_pActions.push_back(new GOAP::Action_KillPistol);
	m_pActions.push_back(new GOAP::Action_KillShotGun);
	m_pActions.push_back(new GOAP::Action_FleePurgezone);
}

void Plugin::AddGoals()
{
	m_pGoals.push_back(new Goal_ExploreWorld);
	m_pGoals.push_back(new Goal_LootHouse);
	m_pGoals.push_back(new Goal_GrabFood);
	m_pGoals.push_back(new Goal_GrabMedkit);
	m_pGoals.push_back(new Goal_GrabPistol);
	m_pGoals.push_back(new Goal_GrabShotgun);
	m_pGoals.push_back(new Goal_DestroyGarbage);
	m_pGoals.push_back(new Goal_EatFood);
	m_pGoals.push_back(new Goal_Heal);
	m_pGoals.push_back(new Goal_FleePurgeZone);
	m_pGoals.push_back(new Goal_ShootEnemies);
}

void Plugin::GetNewHousesInFOV(float deltaTime)
{
	HouseInfoExtended houseInfo = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, houseInfo))
		{
			//// Check if we're not already aware of that house
			if (std::find(m_pMemoryHouse->begin(), m_pMemoryHouse->end(), houseInfo) == m_pMemoryHouse->end())
			{
				m_pInterface->Draw_Point(houseInfo.Center, 2.0f, { 0,0,1 });
				houseInfo.topPoint = Elite::Vector2{ houseInfo.Center.x , houseInfo.Center.y + ((houseInfo.Size.y-10) / 3) };
				houseInfo.bottomPoint = Elite::Vector2{ houseInfo.Center.x , houseInfo.Center.y - ((houseInfo.Size.y-10) / 3) };
				houseInfo.hasRecentlyBeenLooted = houseInfo.lastSinceTimeVisited < houseInfo.ReactivationTime;
				houseInfo.Location = houseInfo.Center;

				m_pMemoryHouse->push_back(houseInfo);
			}
		}
		break;
	}
	if (!m_pMemoryHouse->empty())
		SortByDistance(m_pMemoryHouse);

	m_WorldState.SetCondition("houseInRange", !m_pMemoryHouse);
}

bool Plugin::FindingPath(const WorldState& worldState, const WorldState& desiredState, std::vector<BaseGoapAction*>& actions)
{
	std::cout << "Finding plan for goal [" << desiredState.m_Name << "]\n";
	try
	{
		m_pPlan = m_ASPlanner.FindCurrentPlan(worldState, desiredState, actions);
		if (!empty(m_pPlan))
		{
			std::cout << "Found a plan: ";
			for (auto action : m_pPlan)
			{
				std::cout << " << " << action->GetName();
			}
			std::cout << std::endl;
			return true;
		}
		return false;
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
		return false;
	}
}

bool Plugin::ExecutingPlan()
{
	if (empty(m_pPlan)) return true;

	// There are still actions in the plan, execute the first action in line
	BaseGoapAction* currentAction = m_pPlan.back();
	if (!currentAction->checkProceduralPreconditions(m_pBlackboard))
	{
		m_CurrentGoal = GetHighestPriorityGoal();
		FindingPath(m_WorldState, *m_CurrentGoal, m_pActions);
		return true;
	}
	else if (currentAction->Execute(m_pBlackboard))
	{
		std::cout << "Finished excecuting " << currentAction->GetName() << std::endl;
		m_pPlan.pop_back();
		return empty(m_pPlan);
	}
	return false;
}

WorldState* Plugin::GetHighestPriorityGoal()
{
	WorldState* newGoal{};
	for (const auto goal : m_pGoals)
	{
		if ((newGoal == nullptr || goal->m_Priority > newGoal->m_Priority) && goal->IsValid(m_pBlackboard))
		{
			newGoal = goal;
		}
	}
	return newGoal;
}

void Plugin::updateHousesInMemory()
{
	auto agentInfo = m_pInterface->Agent_GetInfo();
	if (!m_pMemoryHouse->empty())
	{
		for (auto& house : *m_pMemoryHouse)
		{
			house.lastSinceTimeVisited += m_DeltaTime;
			house.hasRecentlyBeenLooted = house.lastSinceTimeVisited < house.ReactivationTime;
		}
	}
}
