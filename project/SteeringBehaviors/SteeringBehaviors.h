/*=============================================================================*/
// Copyright 2021-2022 Elite Engine
// Authors: Matthieu Delaere, Thomas Goussaert
/*=============================================================================*/
// SteeringBehaviors.h: SteeringBehaviors interface and different implementations
/*=============================================================================*/
#ifndef ELITE_STEERINGBEHAVIORS
#define ELITE_STEERINGBEHAVIORS

//-----------------------------------------------------------------
// Includes & Forward Declarations
//-----------------------------------------------------------------
#include <Exam_HelperStructs.h>
#include <IExamInterface.h>

#include "SteeringHelpers.h"

namespace Elite
{
	class Blackboard;
}

class Obstacle;

#pragma region **ISTEERINGBEHAVIOR** (BASE)
class ISteeringBehavior
{
public:
	ISteeringBehavior() = default;
	virtual ~ISteeringBehavior() = default;

	virtual SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) = 0;

	//Seek Functions
	void SetTarget(const TargetData& target) { m_Target = target; }
	void setBlackBoard( Elite::Blackboard* pBlackboard) { m_pBlackboard = pBlackboard; }
	void setInterface(IExamInterface* pInterface) { m_pInterface = pInterface; }

	template<class T, typename std::enable_if<std::is_base_of<ISteeringBehavior, T>::value>::type* = nullptr>
	T* As()
	{ return static_cast<T*>(this); }

protected:
	TargetData m_Target;
	 Elite::Blackboard* m_pBlackboard;
	 IExamInterface* m_pInterface = nullptr;
};
#pragma endregion

///////////////////////////////////////
//SEEK
//****
class Seek : public ISteeringBehavior
{
public:
	Seek() = default;
	virtual ~Seek() = default;

	//Seek Behaviour
	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;
};

///////////////////////////////////////
//FLEE
//****
class Flee : public ISteeringBehavior
{
public:
	Flee() = default;
	virtual ~Flee() = default;

	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;
	void SetFleeRadius(float fleeRadius) { m_FleeRadius = fleeRadius; }
	float GetFleeRadius() const { return m_FleeRadius; }
private:
	float m_FleeRadius = 40.f;
};


///////////////////////////////////////
//ARRIVE
//****
class Arrive : public ISteeringBehavior
{
public:
	Arrive() = default;
	virtual ~Arrive() = default;

	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;
	void SetSlowRadius(float slowRadius) { m_SlowRadius = slowRadius; }
	void SetTargetRadius(float targetRadius) { m_TargetRadius = targetRadius; }
private:
	float m_SlowRadius = 15.f;
	float m_TargetRadius = 1.0f;
};

///////////////////////////////////////
//FACE
//****
class Face : public ISteeringBehavior
{
public:
	Face() = default;
	virtual ~Face() = default;

	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;
};

///////////////////////////////////////
//WANDER
//****
class Wander : public Seek
{
public:
	Wander()= default;
	virtual ~Wander() = default;

	//wanderer behaviour
	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;

	void SetWanderOffset(float offset);
	void SetWanderRadius(float radius);
	void SetMaxAngleChange(float rad);

protected:
	float m_OffsetDistance = 6.f;
	float m_Radius = 2.f;
	int m_MaxAngleChange = 45;
	float m_WanderAngle = 0.f;
};

///////////////////////////////////////
//Pursuit
//****
class Pursuit : public Seek
{
public:
	Pursuit() = default;
	virtual ~Pursuit() = default;

	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;
};

///////////////////////////////////////
//Evade
//****
class Evade : public Flee
{
public:
	Evade() = default;
	virtual ~Evade() = default;

	SteeringPlugin_Output* CalculateSteering(  AgentInfo pAgent) override;
};
///////////////////////////////////////
//Hide
//****
class Hide : public Seek
{
public:
	Hide() = default;
	virtual ~Hide() = default;
protected:
	SteeringPlugin_Output* CalculateSteering(AgentInfo pAgent) override;
};
///////////////////////////////////////
//Interpose
//****
class Interpose : public Seek
{
public:
	Interpose() = default;
	virtual ~Interpose() = default;

	SteeringPlugin_Output* CalculateSteering(AgentInfo pAgent) override;
};


///////////////////////////////////////
//AvoidObstacle
//****
class AvoidObstacle : public Seek
{
public:
	AvoidObstacle() = default;
	virtual ~AvoidObstacle() = default;

	SteeringPlugin_Output* CalculateSteering(AgentInfo pAgent) override;
private:
	std::vector<HouseInfo>* m_pMemoryHouse{};
};

///////////////////////////////////////
//Offest Pursuit
//****
class OffsetPursuit : public Pursuit
{
public:
	OffsetPursuit() = default;
	virtual ~OffsetPursuit() = default;

protected:
	Elite::Vector2 m_Offset{ 2,2 };
	SteeringPlugin_Output* CalculateSteering( AgentInfo pAgent) override;
};

///////////////////////////////////////
//Align
//****
class Align : public Face
{
public:
	Align() = default;
	virtual ~Align() = default;

	SteeringPlugin_Output* CalculateSteering(AgentInfo pAgent) override;
};

#endif


