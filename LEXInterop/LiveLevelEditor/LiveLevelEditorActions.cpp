#include "LiveLevelEditorActions.hpp"
#include "LEXInterop/Utilities.hpp"
#include "LEXInterop/AdditionalFunctions.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "Common/json.hpp"

#include <sstream>

using json = nlohmann::json;

namespace LEXInterop
{
    std::map<FString, FString> ObjFullNameToFileNameMap;

    void MoveAction::Execute()
    {
        if (Actor && !Common::IsDestroyed(Actor))
        {
            FarMove(Actor, Position, 0, 1, 0);
        }
    }

    void Scale3DAction::Execute()
    {
        if (Actor && !Common::IsDestroyed(Actor))
        {
            ScopedMoveActorEnable moveable;
            Actor->SetDrawScale3D(ScaleVector);
        }
    }

    void ScaleAction::Execute()
    {
        if (Actor && !Common::IsDestroyed(Actor))
        {
            ScopedMoveActorEnable moveable;
            Actor->SetDrawScale(Scale);
        }
    }

    void RotateAction::Execute()
    {
        if (Actor && !Common::IsDestroyed(Actor))
        {
            ScopedMoveActorEnable moveable;
            Actor->SetRotation(Rotation);
        }
    }

    void HideAction::Execute()
    {
        if (Actor && !Common::IsDestroyed(Actor))
        {
            Actor->SetHidden(Hidden);
        }
    }

    void ComponentHideAction::Execute()
    {
        if (Component && !Common::IsDestroyed(Component))
        {
            Component->SetHidden(Hidden);
        }
    }

    void ComponentMoveAction::Execute()
    {
        if (Component && !Common::IsDestroyed(Component))
        {
            Component->SetTranslation(Position);
        }
    }

    void ComponentScaleAction::Execute()
    {
        if (Component && !Common::IsDestroyed(Component))
        {
            Component->SetScale(Scale);
        }
    }

    void ComponentScale3DAction::Execute()
    {
        if (Component && !Common::IsDestroyed(Component))
        {
            Component->SetScale3D(ScaleVector);
        }
    }

    void ComponentRotateAction::Execute()
    {
        if (Component && !Common::IsDestroyed(Component))
        {
            Component->SetRotation(Rotation);
        }
    }

    void ComponentSetMaterialAction::Execute()
    {
        if (Component && !Common::IsDestroyed(Component))
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

    UMaterialInstance* ComponentSetParameterAction::GetMaterialInstance()
    {
        if (!Component || Common::IsDestroyed(Component)) return nullptr;

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
                mat = static_cast<UMaterialInstanceConstant*>(staticMesh->LODInfo(0).Elements(MaterialIndex).Material);
            }
#else
            if (staticMesh && staticMesh->NativeLODInfo.Any() && staticMesh->NativeLODInfo(0)->Elements.Count() > static_cast<unsigned int>(MaterialIndex)
                && staticMesh->NativeLODInfo(0)->Elements(MaterialIndex).Material->IsA<UMaterialInstance>())
            {
                mat = static_cast<UMaterialInstance*>(staticMesh->NativeLODInfo(0)->Elements(MaterialIndex).Material);
            }
#endif
        }

        return mat;
    }

    void ComponentSetScalarParameterAction::Execute()
    {
        UMaterialInstance* mat = GetMaterialInstance();

        if (mat && mat->IsA(UMaterialInstance::StaticClass()))
        {
            SFXName paramName(ParameterName.c_str(), 0, true);
            mat->SetScalarParameterValue(paramName, Value);
        }
    }

    void ComponentSetVectorParameterAction::Execute()
    {
        UMaterialInstance* mat = GetMaterialInstance();

        if (mat)
        {
            SFXName paramName(ParameterName.c_str(), 0, true);
            mat->SetVectorParameterValue(paramName, &Value);
        }
    }

    void LEXMessageSendComponentMaterialsAction::Execute()
    {
        if (!Component || Common::IsDestroyed(Component)) return;

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
                            it = ObjFullNameToFileNameMap.find(fullName);
                            if (it != ObjFullNameToFileNameMap.end()) {
                                // It was found. This will probably get the right file but we won't have a correct path...
                                source = it->second;
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
                    info[SOURCE_TAG] = source.Chars();
                }
                topLevelJson.push_back(info);
            }
        }
        else if (Component->IsA<UStaticMeshComponent>())
        {
            auto smc = static_cast<UStaticMeshComponent*>(Component);
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
                            auto it = ObjFullNameToFileNameMap.find(fullName);
                            if (it == ObjFullNameToFileNameMap.end())
                            {
                                // Not found
                                info[SOURCE_TAG] = nullptr;
                            }
                            else
                            {
                                info[SOURCE_TAG] = it->second.Chars();
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
            auto smc = static_cast<USkeletalMeshComponent*>(Component);
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
                        auto it = ObjFullNameToFileNameMap.find(fullName);
                        if (it == ObjFullNameToFileNameMap.end())
                        {
                            // Not found
                            info[SOURCE_TAG] = nullptr;
                        }
                        else
                        {
                            info[SOURCE_TAG] = it->second.Chars();
                        }
                    }
                    topLevelJson.push_back(info);
                }
            }
        }

        std::wostringstream ss;
        ss << L"MATERIALEDITOR COMPONENTMATERIALS ";
        ss << s2ws(topLevelJson.dump());
        SendStringToLEX(ss.str(), 1000);
    }
}
