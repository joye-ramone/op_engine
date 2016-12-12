#include "pch_script.h"
#include "script_game_object.h"
#include "alife_space.h"
#include "script_entity_space.h"
#include "movement_manager_space.h"
#include "pda_space.h"
#include "memory_space.h"
#include "cover_point.h"
#include "script_hit.h"
#include "script_binder_object.h"
#include "InventoryOwner.h"
#include "Artifact.h"
#include "HUDManager.h"
#include "ui/UIMainIngameWnd.h"
#include "action_planner.h"
#include "script_storage_space.h"
#include "script_engine.h"
#include "CustomMonster.h"
#include "Actor.h"
#include "Actor_Flags.h"
#include "Inventory.h"
#include "inventory_item.h"
#include "car.h"

using namespace luabind;

void CInventoryItem::script_register(lua_State *L)
{
	module(L)
		[
			class_<CInventoryItem>("CInventoryItem")
			.def("Name",				&CInventoryItem::Name)
			.def("GetGameObject",		&CInventoryItem::GetGameObject)
		];
}

#pragma region functions for export

u32 CScriptGameObject::GetSlot() const
{
	CInventoryItem		*inventory_item = smart_cast<CInventoryItem*>(&object());
	if (!inventory_item) {
		ai().script_engine().script_log			(ScriptStorage::eLuaMessageTypeError,"CSciptEntity : cannot access class member GetSlot!");
		return			(false);
	}
	return				(inventory_item->GetSlot());
}

LPCSTR CScriptGameObject::GetVisualName() const
{
	if (!g_pGameLevel)
	{
		Msg("Error! CScriptGameObject::GetVisualName : game level doesn't exist. wtf?????");
		return "";
	}
	return	*(object().cNameVisual());

}

#pragma region invulnerabilities from scripts
bool CScriptGameObject::invulnerable		() const
{
	CCustomMonster	*monster = smart_cast<CCustomMonster*>(&object());
	if (!monster) {
		CActor* actor=smart_cast<CActor*>(&object());
		if (actor)
		{
			return actor_invulnerable();
		}
		else
		{
			ai().script_engine().script_log			(ScriptStorage::eLuaMessageTypeError,"CCustomMonster : cannot access class member invulnerable!");
			return		(false);
		}
	}

	return			(monster->invulnerable());
}

void CScriptGameObject::invulnerable		(bool invulnerable)
{
	CCustomMonster	*monster = smart_cast<CCustomMonster*>(&object());
	if (!monster) {
		CActor* actor=smart_cast<CActor*>(&object());
		if (actor)
		{
			actor_invulnerable(invulnerable);
			return;
		}
		else
		{
			ai().script_engine().script_log			(ScriptStorage::eLuaMessageTypeError,"CCustomMonster : cannot access class member invulnerable!");
			return;
		}
	}

	monster->invulnerable	(invulnerable);
}

bool				CScriptGameObject::actor_invulnerable						() const
{
	return !!psActorFlags.test(AF_GODMODE_PARTIAL);
}

void				CScriptGameObject::actor_invulnerable						(bool invulnerable)
{
	psActorFlags.set(AF_GODMODE_PARTIAL,invulnerable);
}
#pragma endregion

#pragma region weight functions

float CScriptGameObject::GetActorMaxWeight() const
{
	CActor* pActor = smart_cast<CActor*>(&object());
	if(!pActor) {
		ai().script_engine().script_log			(ScriptStorage::eLuaMessageTypeError,"CActor : cannot access class member GetActorMaxWeight!");
		return			(false);
	}
	return				(pActor->inventory().GetMaxWeight());
}

float CScriptGameObject::GetTotalWeight() const
{
	CInventoryOwner	*inventory_owner = smart_cast<CInventoryOwner*>(&object());
	if (!inventory_owner) {
		ai().script_engine().script_log			(ScriptStorage::eLuaMessageTypeError,"CInventoryOwner : cannot access class member GetTotalWeight!");
		return			(false);
	}
	return				(inventory_owner->inventory().TotalWeight());
}

float CScriptGameObject::Weight() const
{
	CInventoryItem		*inventory_item = smart_cast<CInventoryItem*>(&object());
	if (!inventory_item) {
		ai().script_engine().script_log			(ScriptStorage::eLuaMessageTypeError,"CSciptEntity : cannot access class member Weight!");
		return			(false);
	}
	return				(inventory_item->Weight());
}
#pragma endregion

#pragma region crouch functions
void CScriptGameObject::actor_set_crouch() const
{
	CActor* actor= smart_cast<CActor*>(&this->object());
	if (!actor)
	{
		ai().script_engine().script_log		(ScriptStorage::eLuaMessageTypeError,"CActor::set_crouch non-Actor object !!!");
		return;
	}
	extern bool g_bAutoClearCrouch;
	g_bAutoClearCrouch=false;
	actor->character_physics_support()->movement()->EnableCharacter();
	actor->character_physics_support()->movement()->ActivateBoxDynamic(1);
	actor->set_state(actor->get_state() | mcCrouch);
	actor->set_state_wishful(actor->get_state_wishful() | mcCrouch);
	HUD().GetUI()->UIMainIngameWnd->MotionIcon().ShowState(CUIMotionIcon::stCrouch);
}

bool CScriptGameObject::actor_is_crouch() const
{
	CActor* actor= smart_cast<CActor*>(&this->object());
	if (!actor)
	{
		ai().script_engine().script_log		(ScriptStorage::eLuaMessageTypeError,"CActor::is_crouch non-Actor object !!!");
		return false;
	}
	return !!(actor->get_state()&mcCrouch);
}
#pragma endregion

#pragma region artefact immunities

luabind::object CScriptGameObject::GetImmunitiesTable() const
{
	luabind::object immunities = luabind::newtable(ai().script_engine().lua());
	CArtefact* artefact=smart_cast<CArtefact*>(&this->object());
	if (!artefact)
	{
		Msg("! ERROR CScriptGameObject::GetImmunitiesTable cannot cast object [%s] to CArtefact",this->cName().c_str());
		return immunities;
	}
	HitImmunity::HitTypeSVec hitVector=artefact->m_ArtefactHitImmunities.GetHitTypeVec();
	for (HitImmunity::HitTypeSVec::iterator it=hitVector.begin();it!=hitVector.end();++it)
		if (*it!=1.0f)
			immunities[std::distance(hitVector.begin(),it)]=1.0f-*it;
	return immunities;
}

luabind::object	CScriptGameObject::GetImmunitiesFromBeltTable() const
{
	CInventoryOwner	*inventory_owner = smart_cast<CInventoryOwner*>(&object());
	if (!inventory_owner) {
		return		luabind::object();
	}
	luabind::object result=luabind::newtable(ai().script_engine().lua());
	xr_vector<CInventoryItem* >::const_iterator bi=inventory_owner->inventory().m_belt.begin();
	xr_vector<CInventoryItem* >::const_iterator ei=inventory_owner->inventory().m_belt.end();
	xr_map<int,float> data;
	std::for_each(bi,ei,[&](CInventoryItem* item)
	{
		CArtefact* artefact=smart_cast<CArtefact*>(item);
		if (item)
		{
			HitImmunity::HitTypeSVec hitVector=artefact->m_ArtefactHitImmunities.GetHitTypeVec();
			for (HitImmunity::HitTypeSVec::iterator it=hitVector.begin();it!=hitVector.end();++it)
				if (*it!=1.0f)
				{
					int immIndex=std::distance(hitVector.begin(),it);
					xr_map<int,float>::iterator current=data.find(immIndex);
					float immValue=1.0f-*it; 
					if (current!=data.end())
						current->second+=immValue;
					else
						data.insert(mk_pair<int,float>(immIndex,immValue));
				}
		}
	});
	if (data.size()>0)
		std::for_each(data.begin(),data.end(),[&](std::pair<int,float> it)
		{
			result[it.first]=it.second;
		});
	return result;
}

#pragma endregion

#pragma endregion

class_<CScriptGameObject> &script_register_game_object3(class_<CScriptGameObject> &instance)
{
	instance
		.def("get_slot",					&CScriptGameObject::GetSlot)
		.def("invulnerable",				static_cast<bool (CScriptGameObject::*)		() const>	(&CScriptGameObject::invulnerable))
		.def("invulnerable",				static_cast<void (CScriptGameObject::*)		(bool)>		(&CScriptGameObject::invulnerable))
		.def("max_weight",					&CScriptGameObject::GetActorMaxWeight)
		.def("total_weight",				&CScriptGameObject::GetTotalWeight)
		.def("item_weight",					&CScriptGameObject::Weight)
		.def("is_crouch",					&CScriptGameObject::actor_is_crouch)
		.def("set_crouch",					&CScriptGameObject::actor_set_crouch)
		.def("get_visual_name",				&CScriptGameObject::GetVisualName)
		.def("get_immunities_from_belt",	&CScriptGameObject::GetImmunitiesFromBeltTable)
		.def("get_immunities",				&CScriptGameObject::GetImmunitiesTable);

	return	(instance);
}
