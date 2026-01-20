#pragma once

#include <string>
#include <map>
#include <LESDK/Headers.hpp>
#include "LEXInterop/ActionQueue.hpp"

namespace LEXInterop
{
    // Map tracking which file each asset loaded from
    extern std::map<FString, FString> ObjFullNameToFileNameMap;

    // Actor move action
    struct MoveAction final : ActionBase
    {
        AActor* Actor;
        FVector Position;

        MoveAction(AActor* actor, FVector pos) : Actor(actor), Position(pos) {}
        void Execute() override;
    };

    // Actor scale3D action
    struct Scale3DAction final : ActionBase
    {
        AActor* Actor;
        FVector ScaleVector;

        Scale3DAction(AActor* actor, FVector scale) : Actor(actor), ScaleVector(scale) {}
        void Execute() override;
    };

    // Actor scale action
    struct ScaleAction final : ActionBase
    {
        AActor* Actor;
        float Scale;

        ScaleAction(AActor* actor, float scale) : Actor(actor), Scale(scale) {}
        void Execute() override;
    };

    // Actor rotation action
    struct RotateAction final : ActionBase
    {
        AActor* Actor;
        FRotator Rotation;

        RotateAction(AActor* actor, FRotator rot) : Actor(actor), Rotation(rot) {}
        void Execute() override;
    };

    // Actor hide action
    struct HideAction final : ActionBase
    {
        AActor* Actor;
        bool Hidden;

        HideAction(AActor* actor, bool hidden) : Actor(actor), Hidden(hidden) {}
        void Execute() override;
    };

    // Component hide action
    struct ComponentHideAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        bool Hidden;

        ComponentHideAction(UStaticMeshComponent* comp, bool hidden) : Component(comp), Hidden(hidden) {}
        void Execute() override;
    };

    // Component move action
    struct ComponentMoveAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        FVector Position;

        ComponentMoveAction(UStaticMeshComponent* comp, FVector pos) : Component(comp), Position(pos) {}
        void Execute() override;
    };

    // Component scale action
    struct ComponentScaleAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        float Scale;

        ComponentScaleAction(UStaticMeshComponent* comp, float scale) : Component(comp), Scale(scale) {}
        void Execute() override;
    };

    // Component scale3D action
    struct ComponentScale3DAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        FVector ScaleVector;

        ComponentScale3DAction(UStaticMeshComponent* comp, FVector scale) : Component(comp), ScaleVector(scale) {}
        void Execute() override;
    };

    // Component rotation action
    struct ComponentRotateAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        FRotator Rotation;

        ComponentRotateAction(UStaticMeshComponent* comp, FRotator rot) : Component(comp), Rotation(rot) {}
        void Execute() override;
    };

    // Component set material action
    struct ComponentSetMaterialAction final : ActionBase
    {
        UMeshComponent* Component;
        int MaterialIndex;
        std::string MaterialPath;

        ComponentSetMaterialAction(UMeshComponent* comp, int idx, std::string path)
            : Component(comp), MaterialIndex(idx), MaterialPath(std::move(path)) {}
        void Execute() override;
    };

    // Base class for material parameter actions
    struct ComponentSetParameterAction : ActionBase
    {
        UMeshComponent* Component;
        int MaterialIndex;
        std::wstring ParameterName;

        ComponentSetParameterAction(UMeshComponent* comp, int idx, std::wstring name)
            : Component(comp), MaterialIndex(idx), ParameterName(std::move(name)) {}

    protected:
        UMaterialInstance* GetMaterialInstance();
    };

    // Component set scalar parameter action
    struct ComponentSetScalarParameterAction final : ComponentSetParameterAction
    {
        float Value;

        ComponentSetScalarParameterAction(UMeshComponent* comp, int idx, std::wstring name, float val)
            : ComponentSetParameterAction(comp, idx, std::move(name)), Value(val) {}
        void Execute() override;
    };

    // Component set vector parameter action
    struct ComponentSetVectorParameterAction final : ComponentSetParameterAction
    {
        FLinearColor Value;

        ComponentSetVectorParameterAction(UMeshComponent* comp, int idx, std::wstring name, FLinearColor val)
            : ComponentSetParameterAction(comp, idx, std::move(name)), Value(val) {}
        void Execute() override;
    };

    // Action to send component materials info to LEX
    struct LEXMessageSendComponentMaterialsAction final : ActionBase
    {
        UMeshComponent* Component;

        explicit LEXMessageSendComponentMaterialsAction(UMeshComponent* component) : Component(component) {}
        void Execute() override;
    };
}
