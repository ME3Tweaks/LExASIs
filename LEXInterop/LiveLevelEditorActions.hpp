#pragma once

#include <string>
#include <map>
#include <LESDK/Headers.hpp>
#include "ActionQueue.hpp"
#include "Utilities.hpp"
#include "Common/Base.hpp"
#include "Common/json.hpp"
#include "AdditionalFunctions.hpp"
using json = nlohmann::json;

namespace LEXInterop
{
    // Map tracking which file each asset loaded from
    inline std::map<FString, FString> ObjFullNameToFileNameMap;

    // Actor move action
    struct MoveAction final : ActionBase
    {
        AActor* Actor;
        FVector Position;

        MoveAction(AActor* actor, FVector pos) : Actor(actor), Position(pos) {}

        void Execute() override
        {
            if (Actor)
            {
                FarMove(Actor, Position, 0, 1, 0);
            }
        }
    };

    // Actor scale3D action
    struct Scale3DAction final : ActionBase
    {
        AActor* Actor;
        FVector ScaleVector;

        Scale3DAction(AActor* actor, FVector scale) : Actor(actor), ScaleVector(scale) {}

        void Execute() override
        {
            if (Actor)
            {
                Actor->SetDrawScale3D(ScaleVector);
            }
        }
    };

    // Actor scale action
    struct ScaleAction final : ActionBase
    {
        AActor* Actor;
        float Scale;

        ScaleAction(AActor* actor, float scale) : Actor(actor), Scale(scale) {}

        void Execute() override
        {
            if (Actor)
            {
                Actor->SetDrawScale(Scale);
            }
        }
    };

    // Actor rotation action
    struct RotateAction final : ActionBase
    {
        AActor* Actor;
        FRotator Rotation;

        RotateAction(AActor* actor, FRotator rot) : Actor(actor), Rotation(rot) {}

        void Execute() override
        {
            if (Actor)
            {
                Actor->SetRotation(Rotation);
            }
        }
    };

    // Actor hide action
    struct HideAction final : ActionBase
    {
        AActor* Actor;
        bool Hidden;

        HideAction(AActor* actor, bool hidden) : Actor(actor), Hidden(hidden) {}

        void Execute() override
        {
            if (Actor)
            {
                Actor->SetHidden(Hidden);
            }
        }
    };

    struct ComponentHideAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        bool Hidden;

        ComponentHideAction(UStaticMeshComponent* comp, bool hidden) : Component(comp), Hidden(hidden) {}

        void Execute() override
        {
            if (Component)
            {
                Component->SetHidden(Hidden);
            }
        }
    };

    // Component move action
    struct ComponentMoveAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        FVector Position;

        ComponentMoveAction(UStaticMeshComponent* comp, FVector pos) : Component(comp), Position(pos) {}

        void Execute() override
        {
            if (Component)
            {
                Component->SetTranslation(Position);
            }
        }
    };

    // Component scale action
    struct ComponentScaleAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        float Scale;

        ComponentScaleAction(UStaticMeshComponent* comp, float scale) : Component(comp), Scale(scale) {}

        void Execute() override
        {
            if (Component)
            {
                Component->SetScale(Scale);
            }
        }
    };

    // Component scale3D action
    struct ComponentScale3DAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        FVector ScaleVector;

        ComponentScale3DAction(UStaticMeshComponent* comp, FVector scale) : Component(comp), ScaleVector(scale) {}

        void Execute() override
        {
            if (Component)
            {
                Component->SetScale3D(ScaleVector);
            }
        }
    };

    // Component rotation action
    struct ComponentRotateAction final : ActionBase
    {
        UStaticMeshComponent* Component;
        FRotator Rotation;

        ComponentRotateAction(UStaticMeshComponent* comp, FRotator rot) : Component(comp), Rotation(rot) {}

        void Execute() override
        {
            if (Component)
            {
                Component->SetRotation(Rotation);
            }
        }
    };

    // Component set material action
    struct ComponentSetMaterialAction final : ActionBase
    {
        UMeshComponent* Component;
        int MaterialIndex;
        std::string MaterialPath;

        ComponentSetMaterialAction(UMeshComponent* comp, int idx, std::string path)
            : Component(comp), MaterialIndex(idx), MaterialPath(std::move(path)) {}

        void Execute() override
        {
            if (Component)
            {
                auto mat = UObject::FindObject<UMaterialInterface>(s2ws(MaterialPath).data());
                if (mat)
                {
                    Component->SetMaterial(MaterialIndex, mat);
                }
                else
                {
                    LEASI_WARN("Material not found in memory: {}", MaterialPath);
                }
            }
        }
    };

    struct ComponentSetParameterAction : ActionBase
    {
        UMeshComponent* Component;
        int MaterialIndex;
        std::wstring ParameterName;

        ComponentSetParameterAction(UMeshComponent* Component, int MaterialIndex, const std::wstring ParameterName)
            : Component(Component), MaterialIndex(MaterialIndex), ParameterName(std::move(ParameterName)) {}

    protected:

        UMaterialInstance* GetMaterialInstance()
        {
            if (!Component) return nullptr;

            UMaterialInstance* mat = nullptr;
            if (static_cast<unsigned int>(MaterialIndex) < Component->Materials.Count())
            {
                mat = static_cast<UMaterialInstance*>(Component->Materials(MaterialIndex));
            }
            else if (Component->IsA<USkeletalMeshComponent>())
            {
                auto smc = static_cast<USkeletalMeshComponent*>(Component);
                auto skelMesh = smc->SkeletalMesh;
                if (skelMesh && static_cast<unsigned int>(MaterialIndex) < skelMesh->Materials.Count()
                    && skelMesh->Materials(MaterialIndex)->IsA<UMaterialInstance>())
                {
                    mat = static_cast<UMaterialInstance*>(skelMesh->Materials(MaterialIndex));
                }
            }
            else if (Component->IsA<UStaticMeshComponent>())
            {
                auto smc = static_cast<UStaticMeshComponent*>(Component);
                auto staticMesh = smc->StaticMesh;
#if defined(SDK_TARGET_LE1)
                if (staticMesh && staticMesh->LODInfo.Any() && staticMesh->LODInfo(0).Elements.Count() > static_cast<unsigned int>(MaterialIndex)
                    && staticMesh->LODInfo(0).Elements(MaterialIndex).Material->IsA<UMaterialInstance>())
                {
                    mat = (UMaterialInstanceConstant*)staticMesh->LODInfo(0).Elements(MaterialIndex).Material;
                }
#else
                if (staticMesh && staticMesh->NativeLODInfo.Any() && staticMesh->NativeLODInfo(0)->Elements.Count() > static_cast<unsigned int>(MaterialIndex)
                    && staticMesh->NativeLODInfo(0)->Elements(MaterialIndex).Material->IsA<UMaterialInstance>())
                {
                    mat = (UMaterialInstance*)staticMesh->NativeLODInfo(0)->Elements(MaterialIndex).Material;
                }
#endif
            }

			return mat;
        }
    };

    // Component set scalar parameter action
	struct ComponentSetScalarParameterAction final : ComponentSetParameterAction
    {
        float Value;

        ComponentSetScalarParameterAction(UMeshComponent* comp, int idx, std::wstring name, float val)
            : ComponentSetParameterAction(comp, idx, std::move(name)), Value(val) {}

        void Execute() override
        {
            UMaterialInstance* mat = GetMaterialInstance();

            if (mat && mat->IsA(UMaterialInstance::StaticClass()))
            {
                SFXName paramName(ParameterName.c_str(), 0, true);
                mat->SetScalarParameterValue(paramName, Value);
            }
        }
    };

    // Component set vector parameter action
    struct ComponentSetVectorParameterAction final : ComponentSetParameterAction
    {
        UMeshComponent* Component;
        int MaterialIndex;
        std::wstring ParameterName;
        FLinearColor Value;

        ComponentSetVectorParameterAction(UMeshComponent* comp, int idx, std::wstring name, FLinearColor val)
            : ComponentSetParameterAction(comp, idx, std::move(name)), Value(val) {}

        void Execute() override
        {
            UMaterialInstance* mat = GetMaterialInstance();

            if (mat)
            {
                SFXName paramName(ParameterName.c_str(), 0, true);
                mat->SetVectorParameterValue(paramName, &Value);
            }
        }
    };



    struct LEXMessageSendComponentMaterialsAction final : ActionBase
    {
        UMeshComponent* Component;

        explicit LEXMessageSendComponentMaterialsAction(UMeshComponent* component) : Component(component) {}

        void Execute() override
        {
            constexpr auto MAT_PATH_TAG = "material";
            constexpr auto MAT_CLASS_TAG = "materialclass";
            constexpr auto MAT_INDEX_TAG = "materialindex";
            constexpr auto SOURCE_TAG = "source";

            json topLevelJson = json::array();

            // Check overrides first
            if (Component->Materials.Count() > 0) 
            {
                int i = -1;
                for (auto mat : Component->Materials)
                {
                    i++;
                    FString source{};
                    auto info = json::object();
                    info[MAT_INDEX_TAG] = i;
                    if (mat == nullptr)
                    {
                        info[MAT_PATH_TAG] = nullptr;
                        info[MAT_CLASS_TAG] = nullptr;
                    }
                    else
                    {
                        auto asset = mat->GetFullPath();
                        info[MAT_PATH_TAG] = asset.Chars();
                        info[MAT_CLASS_TAG] = mat->Class->Name.ToString().Chars();

                        // Try to find the asset directly
                        auto fullName = mat->GetFullName();
                        auto it = ObjFullNameToFileNameMap.find(fullName);
                        if (it == ObjFullNameToFileNameMap.end())
                        {
                            // Not found. It may have been dynamically spawned! Look at the archetype to try to see which file this probably loaded out of
                            auto archetype = mat->ObjectArchetype;
                            if (archetype) {
                                fullName = archetype->GetFullName();
                                if (ObjFullNameToFileNameMap.find(fullName) != ObjFullNameToFileNameMap.end()) {
                                    // It was found. This will probably get the right file but we won't have a correct path...
                                    source = ObjFullNameToFileNameMap.find(fullName)->second;
                                }
                            }
                        }
                        else
                        {
                            // It was found
                            source = it->second;
                        }
                    }
                    if (!source.Empty())
                    {
                        info[SOURCE_TAG] = source.Empty() ? nullptr : source.Chars();
                    }
                    topLevelJson.push_back(info);
                }
            }
            else if (Component->IsA<UStaticMeshComponent>())
            {
                auto smc = reinterpret_cast<UStaticMeshComponent*>(Component);
                if (smc->StaticMesh)
                {

#if defined(SDK_TARGET_LE1)
                    if (smc->StaticMesh->LODInfo.Count() > 0)
                    {
                        auto lodElements = smc->StaticMesh->LODInfo(0).Elements;
#else
                    if (smc->StaticMesh->NativeLODInfo.Count() > 0)
                    {
                        auto lodElements = smc->StaticMesh->NativeLODInfo(0)->Elements;
#endif
                        auto meshName = smc->StaticMesh->GetFullName();
                        for (auto&& lodElement : lodElements)
                        {
                            auto info = json::object();

                            if (lodElement.Material == nullptr)
                            {
                                info[MAT_PATH_TAG] = nullptr;
                                info[MAT_CLASS_TAG] = nullptr;
                                info[SOURCE_TAG] = nullptr;
                            }
                            else
                            {
                                auto ifp = lodElement.Material->GetFullPath();
                                info[MAT_PATH_TAG] = ifp.Chars();
                                info[MAT_CLASS_TAG] = lodElement.Material->Class->Name.ToString().Chars();
                                auto fullName = lodElement.Material->GetFullName();
                                if (ObjFullNameToFileNameMap.find(fullName) == ObjFullNameToFileNameMap.end())
                                {
                                    // Not found
                                    info[SOURCE_TAG] = nullptr;
                                }
                                else
                                {
                                    info[SOURCE_TAG] = ObjFullNameToFileNameMap.find(fullName)->second.Chars();
                                }
                            }
                            topLevelJson.push_back(info);
                        }
                    }
                }
            }
            else if (Component->IsA<USkeletalMeshComponent>())
            {
                // SKM has parameter 'Materials' which is different from the property of UMeshComponent.
                auto smc = reinterpret_cast<USkeletalMeshComponent*>(Component);
                auto skelMesh = smc->SkeletalMesh;
                if (skelMesh) {
                    for (auto mat : skelMesh->Materials)
                    {
                        auto info = json::object();
                        if (mat == nullptr)
                        {
                            info[MAT_PATH_TAG] = nullptr;
                            info[MAT_CLASS_TAG] = nullptr;
                            info[SOURCE_TAG] = nullptr;
                        }
                        else
                        {
                            auto asset = mat->GetFullPath();
                            info[MAT_PATH_TAG] = asset.Chars();
                            info[MAT_CLASS_TAG] = mat->Class->Name.ToString().Chars();
                            auto fullName = mat->GetFullName();
                            if (ObjFullNameToFileNameMap.find(fullName) == ObjFullNameToFileNameMap.end())
                            {
                                // Not found
                                info[SOURCE_TAG] = nullptr;
                            }
                            else
                            {
                                info[SOURCE_TAG] = ObjFullNameToFileNameMap.find(fullName)->second.Chars();
                            }
                        }
                        topLevelJson.push_back(info);
                    }
                }
            }
            else {
                // Handle others
            }

            std::wostringstream ss;
            ss << L"MATERIALEDITOR COMPONENTMATERIALS ";
            ss << s2ws(topLevelJson.dump());
            SendStringToLEX(ss.str(), 1000);
        }
    };
}
