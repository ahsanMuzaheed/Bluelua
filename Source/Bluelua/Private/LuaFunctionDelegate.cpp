#include "LuaFunctionDelegate.h"

#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaObjectBase.h"
#include "LuaState.h"
#include "LuaStackGuard.h"
#include "LuaUObject.h"

DECLARE_CYCLE_STAT(TEXT("HandleLuaDelegate"), STAT_HandleLuaDelegate, STATGROUP_Bluelua);

const TCHAR* ULuaFunctionDelegate::DelegateFunctionName = TEXT("NeverUsed");

void ULuaFunctionDelegate::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (Function && Function->GetName().Equals(DelegateFunctionName))
	{
		if (!IsBound())
		{
			UE_LOG(LogBluelua, Warning, TEXT("Call lua delegate failed! Lua function is not bound! SignatureFunction[%s], FunctionDelegate[%s], Owner[%s]."), *LastSignatureFunctionName, *GetName(), *GetOuter()->GetName());
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_HandleLuaDelegate);

		lua_State* L = LuaState.Pin()->GetState();
		FLuaStackGuard Gurad(L);

		lua_rawgeti(L, LUA_REGISTRYINDEX, LuaFunctionIndex);
		if (lua_type(L, -1) != LUA_TFUNCTION)
		{
			return;
		}

		bool bWithSelf = false;
		if (LuaFunctionOwnerIndex != LUA_NOREF)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, LuaFunctionOwnerIndex);
			bWithSelf = true;
		}

		// stack = [BindingFunction]
		LuaState.Pin()->CallLuaFunction(SignatureFunction, Parameters, bWithSelf);
	}
	else
	{
		Super::ProcessEvent(Function, Parameters);
	}
}

void ULuaFunctionDelegate::BeginDestroy()
{
	Clear();

	Super::BeginDestroy();
}

ULuaFunctionDelegate* ULuaFunctionDelegate::Create(UObject* InDelegateOwner, TSharedPtr<FLuaState> InLuaState, UFunction* InSignatureFunction, int InLuaFunctionIndex)
{
	if (!InLuaState.IsValid())
	{
		return nullptr;
	}

	ULuaFunctionDelegate* FunctionDelegate = NewObject<ULuaFunctionDelegate>(InDelegateOwner, ULuaFunctionDelegate::StaticClass(), NAME_None);
	if (!FunctionDelegate)
	{
		return nullptr;
	}

	FunctionDelegate->BindLuaState(InLuaState);

	lua_pushvalue(InLuaState->GetState(), InLuaFunctionIndex);
	FunctionDelegate->BindLuaFunction(InSignatureFunction, luaL_ref(InLuaState->GetState(), LUA_REGISTRYINDEX));

	if (InLuaFunctionIndex != 2)
	{
		lua_pushvalue(InLuaState->GetState(), InLuaFunctionIndex - 1);
		FunctionDelegate->BindLuaFunctionOwner(luaL_ref(InLuaState->GetState(), LUA_REGISTRYINDEX));
	}

	return FunctionDelegate;
}

ULuaFunctionDelegate* ULuaFunctionDelegate::Fetch(lua_State* L, int32 Index)
{
	UObject* DelegateObject = FLuaUObject::Fetch(L, Index);
	ULuaFunctionDelegate* FunctionDelegate = Cast<ULuaFunctionDelegate>(DelegateObject);
	if (!FunctionDelegate)
	{
		luaL_error(L, "Param %d is not an ULuaFunctionDelegate! Use CreateFunctionDelegate to create one.", Index);
	}

	return FunctionDelegate;
}

int ULuaFunctionDelegate::CreateFunctionDelegate(lua_State* L)
{
	UObject* DelegateOwner = FLuaUObject::Fetch(L, 1);
	if (!DelegateOwner)
	{
		luaL_error(L, "Create delegate failed! Param 1 must be a UObject as owner!");
		return 0;
	}

	const int Param2Type = lua_type(L, 2);
	if (Param2Type != LUA_TFUNCTION && lua_type(L, 3) != LUA_TFUNCTION)
	{
		luaL_error(L, "Create delegate failed! Param 2 or Param 3 must be a function!");
	}

	const int FunctionIndex = (Param2Type == LUA_TFUNCTION) ? 2 : 3;

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (!LuaStateWrapper)
	{
		return 0;
	}

	ULuaFunctionDelegate* FunctionDelegate = ULuaFunctionDelegate::Create(DelegateOwner, LuaStateWrapper->AsShared(), nullptr, FunctionIndex);
	if (!FunctionDelegate)
	{
		return 0;
	}

	return FLuaUObject::Push(L, FunctionDelegate, DelegateOwner);
}

void ULuaFunctionDelegate::BindLuaState(TSharedPtr<FLuaState> InLuaState)
{
	LuaState = InLuaState;
}

void ULuaFunctionDelegate::BindLuaFunction(UFunction* InSignatureFunction, int InLuaFunctionIndex)
{
	Clear();

	LastSignatureFunctionName = InSignatureFunction ? InSignatureFunction->GetName() : TEXT("");
	SignatureFunction = InSignatureFunction;
	LuaFunctionIndex = InLuaFunctionIndex;
}

void ULuaFunctionDelegate::BindLuaFunctionOwner(int InLuaFunctionOwerIndex)
{
	LuaFunctionOwnerIndex = InLuaFunctionOwerIndex;
}

void ULuaFunctionDelegate::BindSignatureFunction(UFunction* InSignatureFunction)
{
	LastSignatureFunctionName = InSignatureFunction ? InSignatureFunction->GetName() : TEXT("");
	SignatureFunction = InSignatureFunction;
}

bool ULuaFunctionDelegate::IsBound() const
{
	return (LuaState.IsValid() && LuaFunctionIndex != LUA_NOREF);
}

void ULuaFunctionDelegate::Clear()
{
	SignatureFunction = nullptr;

	if (LuaState.IsValid())
	{
		if (LuaFunctionIndex != LUA_NOREF)
		{
			luaL_unref(LuaState.Pin()->GetState(), LUA_REGISTRYINDEX, LuaFunctionIndex);
		}

		if (LuaFunctionOwnerIndex != LUA_NOREF)
		{
			luaL_unref(LuaState.Pin()->GetState(), LUA_REGISTRYINDEX, LuaFunctionOwnerIndex);
		}
	}

	LuaFunctionIndex = LUA_NOREF;
	LuaFunctionOwnerIndex = LUA_NOREF;
}
