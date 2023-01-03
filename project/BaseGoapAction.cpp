#include "stdafx.h"
#include "BaseGoapAction.h"

BaseGoapAction::BaseGoapAction(const std::string& name, const int cost)
	:m_Cost(cost),
	m_Name(name)
{
}

void BaseGoapAction::DoReset()
{
	m_InRange = false;
	m_Target = { 0,0 };
	Reset();
}

bool BaseGoapAction::checkProceduralPreconditions(Elite::Blackboard* pBlackboard)
{
    for (const auto& preconditions : m_Preconditions)
    {
        try
        {
            //check world for precondition
        }
        catch (const std::out_of_range&)
        {
            return false;
        }
    }
    return true;
}
