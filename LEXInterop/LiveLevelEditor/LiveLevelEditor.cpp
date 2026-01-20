#include "LiveLevelEditor.hpp"
#include "LEXInterop/Utilities.hpp"
#include "LEXInterop/SharedData.hpp"
#include "LEXInterop/AdditionalFunctions.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "Common/json.hpp"
#include "Common/Utils.hpp"

#include <sstream>
#include <cstring>
#include <cstdlib>

using json = nlohmann::json;

namespace LEXInterop
{   
    
    namespace
    {
        void ClearSelectedActor()
        {
            LiveLevelEditor::SelectedActor = nullptr;
            LiveLevelEditor::SelectedActorMapName = {};
            LiveLevelEditor::SelectedCollectionComponentIndex = -1;
            LiveLevelEditor::SelectedActorComponentIndex = -1;
        }

        //consolidates all transformation data into the properties, to simplify editing them
        void NormalizeSMC(UStaticMeshComponent* smc)
        {
            FVector translation;
            FVector scale3D;
            float pitch_rad;
            float yaw_rad;
            float roll_rad;

            FMatrix tmp = smc->CachedParentToWorld;
            smc->CachedParentToWorld = IdentityMatrix;
            if (smc->AbsoluteTranslation)
            {
                tmp.WPlane = FPlane{ {0, 0, 0}, 1 };
            }
            if (smc->AbsoluteRotation || smc->AbsoluteScale)
            {
                MatrixDecompose(tmp, translation, scale3D, pitch_rad, yaw_rad, roll_rad);
                if (smc->AbsoluteRotation)
                {
                    pitch_rad = yaw_rad = roll_rad = 0;
                }
                if (smc->AbsoluteScale)
                {
                    scale3D = FVector{ 1,1,1 };
                }
                tmp = MatrixCompose(translation, scale3D, pitch_rad, yaw_rad, roll_rad);
            }
            const auto pitch = UnrealRotationUnitsToRadians(smc->Rotation.Pitch);
            const auto yaw = UnrealRotationUnitsToRadians(smc->Rotation.Yaw);
            const auto roll = UnrealRotationUnitsToRadians(smc->Rotation.Roll);
            tmp = MatrixCompose(smc->Translation, smc->Scale3D * smc->Scale, pitch, yaw, roll) * tmp;

            MatrixDecompose(tmp, translation, scale3D, pitch_rad, yaw_rad, roll_rad);
            smc->Translation = translation;
            smc->Rotation = FRotator{ RadiansToUnrealRotationUnits(pitch_rad), RadiansToUnrealRotationUnits(yaw_rad), RadiansToUnrealRotationUnits(roll_rad) };
            smc->Scale = 1;
            smc->Scale3D = scale3D;
            smc->AbsoluteTranslation = false;
            smc->AbsoluteScale = false;
            smc->AbsoluteRotation = false;
        }

        json AppendActorsInLevel(ULevelBase* level)
        {
            json actorsArray = json::array();
            for (const auto actor : level->Actors)
            {
                if (!actor || Common::IsDefaultObject(actor) || actor->IsA<ABioWorldInfo>())
                {
                    continue;
                }
                json actorJson;
                actorJson["Name"] = actor->GetName().Chars();
                if (actor->IsA(AStaticMeshCollectionActor::StaticClass()))
                {
                    const auto smca = static_cast<AStaticMeshCollectionActor*>(actor);
                    json components = json::array();
                    for (const auto component : smca->Components)
                    {
                        json jcomponent = json::object();
                        if (component && component->IsA<UStaticMeshComponent>())
                        {
                            auto smc = static_cast<UStaticMeshComponent*>(component);
                            NormalizeSMC(smc);
                            jcomponent["SMCAName"] = smc->GetName().Chars();
                            if (smc->StaticMesh)
                            {
                                jcomponent["SMCAMesh"] = smc->StaticMesh->GetName().Chars();
                            }
                            else
                            {
                                jcomponent["SMCAMesh"] = nullptr;
                            }
                            components.push_back(jcomponent);
                        }
                        else
                        {
                            components.push_back(nullptr);
                        }
                    }
                    actorJson["StaticMeshCollectionComponents"] = components;
                }
                else
                {
                    const auto tag = actor->Tag.ToString();
                    if (tag.Length() > 0 && tag.Equals(actor->Class->GetName(), true))
                    {
                        // Tag != ClassName
                        actorJson["Tag"] = tag.Chars();
                    }
                    json components = json::array();
                    for (const auto component : actor->Components)
                    {
                        if (component)
                        {
                            components.push_back(component->GetName().Chars());
                        }
                        else
                        {
                            components.push_back(nullptr);
                        }
                    }
                    actorJson["ActorComponents"] = components;
                }
                actorsArray.push_back(actorJson);
            }
            return actorsArray;
        }

        void RefreshLevels(AWorldInfo* worldInfo, const std::vector<SFXName>* removedLevels = nullptr)
        {
            if (removedLevels && LiveLevelEditor::SelectedActor)
            {
                for (SFXName levelName : *removedLevels)
                {
                    if (levelName == LiveLevelEditor::SelectedActorMapName)
                    {
                        ClearSelectedActor();
                        break;
                    }
                }
            }
            LiveLevelEditor::LevelsLoadedByLLE.clear();
            const auto mainLevel = static_cast<ULevelBase*>(worldInfo->Outer);
            const auto mainPackage = mainLevel->Outer->Outer;
            LiveLevelEditor::LevelsLoadedByLLE.insert(mainPackage->Name);

            json topLevelJson = json::array();

            json mainPackageJson;
            mainPackageJson["Name"] = mainPackage->GetName().Chars();
            mainPackageJson["Actors"] = AppendActorsInLevel(mainLevel);
            topLevelJson.push_back(mainPackageJson);

            for (const auto streaming_level : worldInfo->StreamingLevels)
            {
                if (!streaming_level->bIsVisible || !streaming_level->LoadedLevel)
                {
                    continue;
                }
                LiveLevelEditor::LevelsLoadedByLLE.insert(streaming_level->PackageName);

                json streamingLevelJson;
                streamingLevelJson["Name"] = streaming_level->PackageName.ToString().Chars();
                streamingLevelJson["Actors"] = AppendActorsInLevel(streaming_level->LoadedLevel);
                topLevelJson.push_back(streamingLevelJson);
            }

            std::wostringstream ss;
            ss << L"LIVELEVELEDITOR LEVELSUPDATE ";
            ss << s2ws(topLevelJson.dump());
            SendStringToLEX(ss.str(), 1000);
        }

        void CheckLevelChanges(AWorldInfo* worldInfo)
        {
            if (!worldInfo->Outer || !worldInfo->Outer->Outer || !worldInfo->Outer->Outer->Outer || !worldInfo->Outer->IsA(ULevelBase::StaticClass()))
            {
                return;
            }
            std::set<SFXName> visibleLevels;

            // Add main level
            visibleLevels.insert(worldInfo->Outer->Outer->Outer->Name);

            // Add streaming levels
            for (auto streamingLevel : worldInfo->StreamingLevels)
            {
                if (streamingLevel && streamingLevel->bIsVisible && streamingLevel->LoadedLevel)
                {
                    auto nameStr = streamingLevel->PackageName;
                    visibleLevels.insert(nameStr);
                }
            }

            //check if any levels have been unloaded
            std::vector<SFXName> diff;
            std::set_difference(LiveLevelEditor::LevelsLoadedByLLE.begin(), LiveLevelEditor::LevelsLoadedByLLE.end(), visibleLevels.begin(), visibleLevels.end(), std::back_inserter(diff));
            if (!diff.empty())
            {
                RefreshLevels(worldInfo, &diff);
            }
            else
            {
                //check for newly loaded levels
                std::set_difference(visibleLevels.begin(), visibleLevels.end(), LiveLevelEditor::LevelsLoadedByLLE.begin(), LiveLevelEditor::LevelsLoadedByLLE.end(), std::back_inserter(diff));
                if (!diff.empty())
                {
                    RefreshLevels(worldInfo);
                }
            }
        }


        void UpdateSelectedActor(char* selectionStr)
        {
            char* nextToken;
            SFXName mapName(strtok_s(selectionStr, " ", &nextToken), 0, true);
            SFXName actorName(strtok_s(nullptr, " ", &nextToken), 0, true);

            for (Common::TypedObjectIterator<AActor, true, false> It; It; ++It)
            {
                auto actor = *It;
                if (!actor) continue;

                auto objMapName = GetContainingMapName(actor);
                if (mapName != objMapName) continue;

                if (actorName == actor->Name)
                {
                    LiveLevelEditor::SelectedActor = actor;
                    LiveLevelEditor::SelectedActorMapName = objMapName;
                    LiveLevelEditor::SelectedCollectionComponentIndex = -1;
                    LiveLevelEditor::SelectedActorComponentIndex = -1;

                    if (nextToken && strlen(nextToken) > 0)
                    {
                        int compIdx = static_cast<int>(strtol(nextToken, nullptr, 10));
                        if (LiveLevelEditor::SelectedActor->IsA<AStaticMeshCollectionActor>())
                        {
                            if (compIdx < 0 || static_cast<unsigned int>(compIdx) >= static_cast<AStaticMeshCollectionActor*>(LiveLevelEditor::SelectedActor)->Components.Count())
                            {
                                ClearSelectedActor();
                                SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED NOTFOUND");
                                return;
                            }
                            LiveLevelEditor::SelectedCollectionComponentIndex = compIdx;
                        }
                        else if (LiveLevelEditor::SelectedActor->IsA<AStaticLightCollectionActor>())
                        {
                            if (compIdx < 0 || static_cast<unsigned int>(compIdx) >= static_cast<AStaticLightCollectionActor*>(LiveLevelEditor::SelectedActor)->Components.Count())
                            {
                                ClearSelectedActor();
                                SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED NOTFOUND");
                                return;
                            }
                            LiveLevelEditor::SelectedCollectionComponentIndex = compIdx;
                        }
                        else
                        {
                            //if it's -1, we're not selecting a component
                            if (compIdx > 0 && static_cast<unsigned int>(compIdx) >= LiveLevelEditor::SelectedActor->Components.Count())
                            {
                                ClearSelectedActor();
                                SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED NOTFOUND");
                                return;
                            }
                            LiveLevelEditor::SelectedActorComponentIndex = compIdx;
                        }
                    }

                    SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED");
                    return;
                }
            }

            ClearSelectedActor();
            SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED NOTFOUND");
        }

        void SendActorData()
        {
            if (!LiveLevelEditor::SelectedActor) return;

            FVector translation{};
            float scale = 1.0f;
            FVector scale3D{ 1.0f, 1.0f, 1.0f };
            FRotator rotation{};
            bool hidden = false;

            if (LiveLevelEditor::SelectedActor->IsA<AStaticMeshCollectionActor>() &&
                LiveLevelEditor::SelectedCollectionComponentIndex >= 0)
            {
                auto smca = static_cast<AStaticMeshCollectionActor*>(LiveLevelEditor::SelectedActor);
                if (static_cast<unsigned int>(LiveLevelEditor::SelectedCollectionComponentIndex) < smca->Components.Count())
                {
                    auto comp = smca->Components(LiveLevelEditor::SelectedCollectionComponentIndex);
                    if (comp && comp->IsA<UStaticMeshComponent>())
                    {
                        auto smc = static_cast<UStaticMeshComponent*>(comp);
                        translation = smc->Translation;
                        scale = smc->Scale;
                        scale3D = smc->Scale3D;
                        rotation = smc->Rotation;
                    }
                }
            }
            else
            {
                translation = LiveLevelEditor::SelectedActor->LOCATION;
                scale = LiveLevelEditor::SelectedActor->DrawScale;
                scale3D = LiveLevelEditor::SelectedActor->DrawScale3D;
                rotation = LiveLevelEditor::SelectedActor->Rotation;
                hidden = LiveLevelEditor::SelectedActor->bHidden;
            }

            std::wstringstream ss;
            ss << L"LIVELEVELEDITOR ACTORLOC " << translation.X << L" " << translation.Y << L" " << translation.Z;
            ss << L" ACTORROT " << rotation.Pitch << L" " << rotation.Yaw << L" " << rotation.Roll;
            ss << L" ACTORSCALE " << scale << L" " << scale3D.X << L" " << scale3D.Y << L" " << scale3D.Z;
            ss << L" HIDDEN " << (hidden ? 1 : 0);
            SendStringToLEX(ss.str());
        }

        template<class dataType, std::derived_from<ActionBase> componentAction, std::derived_from<ActionBase> actorAction>
        void SetActorData(dataType data)
        {
            if (!LiveLevelEditor::SelectedActor) return;

            if (LiveLevelEditor::SelectedActor->IsA<AStaticMeshCollectionActor>() &&
                LiveLevelEditor::SelectedCollectionComponentIndex >= 0)
            {
                auto smca = static_cast<AStaticMeshCollectionActor*>(LiveLevelEditor::SelectedActor);
                if (static_cast<unsigned int>(LiveLevelEditor::SelectedCollectionComponentIndex) < smca->Components.Count())
                {
                    auto comp = smca->Components(LiveLevelEditor::SelectedCollectionComponentIndex);
                    if (comp && comp->IsA<UStaticMeshComponent>())
                    {
                        QueueAction(new componentAction(static_cast<UStaticMeshComponent*>(comp), data));
                    }
                }
            }
            else
            {
                QueueAction(new actorAction(LiveLevelEditor::SelectedActor, data));
            }
        }

        UMeshComponent* GetSelectedMeshComponent()
        {
            if (!LiveLevelEditor::SelectedActor) return nullptr;

            if (LiveLevelEditor::SelectedActor->IsA<AStaticMeshCollectionActor>() &&
                LiveLevelEditor::SelectedCollectionComponentIndex >= 0)
            {
                auto smca = static_cast<AStaticMeshCollectionActor*>(LiveLevelEditor::SelectedActor);
                if (static_cast<unsigned int>(LiveLevelEditor::SelectedCollectionComponentIndex) < smca->Components.Count())
                {
                    auto comp = smca->Components(LiveLevelEditor::SelectedCollectionComponentIndex);
                    if (comp && comp->IsA<UMeshComponent>())
                    {
                        return static_cast<UMeshComponent*>(comp);
                    }
                }
            }
            else if (LiveLevelEditor::SelectedActorComponentIndex >= 0 &&
                     static_cast<unsigned int>(LiveLevelEditor::SelectedActorComponentIndex) < LiveLevelEditor::SelectedActor->Components.Count())
            {
                auto comp = LiveLevelEditor::SelectedActor->Components(LiveLevelEditor::SelectedActorComponentIndex);
                if (comp && comp->IsA<UMeshComponent>())
                {
                    return static_cast<UMeshComponent*>(comp);
                }
            }

            return nullptr;
        }

        void SetActorPosition(float x, float y, float z)
        {
            SetActorData<FVector, ComponentMoveAction, MoveAction>({ x, y, z });
        }

        void SetActorRotation(int pitch, int yaw, int roll)
        {
            SetActorData<FRotator, ComponentRotateAction, RotateAction>({ pitch, yaw, roll });
        }

        void SetActorDrawScale3D(float x, float y, float z)
        {
            SetActorData<FVector, ComponentScale3DAction, Scale3DAction>({ x, y, z });
        }

        void SetActorDrawScale(float scale)
        {
            SetActorData<float, ComponentScaleAction, ScaleAction>(scale);
        }

        void SetActorHidden(bool hidden)
        {
            SetActorData<bool, ComponentHideAction, HideAction>(hidden);
        }

        void SetActorMaterial(int idx, std::string matPath)
        {
            if (!LiveLevelEditor::SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new ComponentSetMaterialAction(comp, idx, matPath));
            }
        }

        void SetActorScalarParameter(int idx, std::string paramName, float value)
        {
            if (!LiveLevelEditor::SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new ComponentSetScalarParameterAction(comp, idx, s2ws(paramName), value));
            }
        }

        void SetActorVectorParameter(int idx, std::string paramName, FLinearColor value)
        {
            if (!LiveLevelEditor::SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new ComponentSetVectorParameterAction(comp, idx, s2ws(paramName), value));
            }
        }

        void SendComponentMaterials()
        {
            if (!LiveLevelEditor::SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new LEXMessageSendComponentMaterialsAction(comp));
            }
        }

        void SendAllLoadedMaterials()
        {
            std::wstringstream ss;
            for (Common::TypedObjectIterator<UMaterialInterface, true, false> It; It; ++It)
            {
                auto obj = *It;
                if (obj)
                {
                    ss.str(L"");
                    ss << L"MATERIALEDITOR LOADEDMATERIAL " << obj->GetFullName().Chars();
                    SendStringToLEX(ss.str());
                }
            }
        }

        std::string ReadStringFromCommand(char** command, bool hasAnotherParam)
        {
            int bufSize = 0;
            bool isQuoted = (*command)[0] == '"';

            if (isQuoted)
            {
                (*command)++;
                while ((*command)[bufSize] != '\0' && (*command)[bufSize] != '"')
                    bufSize++;
                if ((*command)[bufSize] == '\0')
                    isQuoted = false;
            }
            else
            {
                while ((*command)[bufSize] != '\0' && (*command)[bufSize] != ' ')
                    bufSize++;
            }
            std::string str(*command, bufSize);

            *command += bufSize;

            if (isQuoted) (*command)++;
            if (hasAnotherParam) (*command)++;

            return str;
        }
    } // anonymous namespace
    
    bool LiveLevelEditor::HandleCommand(char* command)
    {
        if (IsCmd(&command, "LLE_TEST_ACTIVE"))
        {
            ClearSelectedActor();
            LevelsLoadedByLLE.clear();
            SendStringToLEX(L"LIVELEVELEDITOR READY");
            IsActive = true;
            return true;
        }

        if (IsCmd(&command, "LLE_DEACTIVATE"))
        {
            IsPendingDeactivation = true;
            return true;
        }

        if (IsCmd(&command, "LLE_SHOW_TRACE"))
        {
            DrawLineToSelected = true;
            return true;
        }

        if (IsCmd(&command, "LLE_HIDE_TRACE"))
        {
            DrawLineToSelected = false;
            return true;
        }

        if (IsCmd(&command, "LLE_TRACE_COLOR "))
        {
            TraceLineColor.R = strtof(command, &command);
            TraceLineColor.G = strtof(command, &command);
            TraceLineColor.B = strtof(command, &command);
            return true;
        }

        if (IsCmd(&command, "LLE_TRACE_WIDTH "))
        {
            TraceWidth = strtof(command, &command);
            return true;
        }

        if (IsCmd(&command, "LLE_AXES_Scale "))
        {
            CoordinateScale = strtof(command, &command);
            return true;
        }

        if (IsCmd(&command, "LLE_SELECT_ACTOR "))
        {
            UpdateSelectedActor(command);
            return true;
        }

        if (IsCmd(&command, "LLE_GET_ACTOR_POSDATA"))
        {
            SendActorData();
            return true;
        }

        if (IsCmd(&command, "LLE_UPDATE_ACTOR_POS "))
        {
            const auto posX = strtof(command, &command);
            const auto posY = strtof(command, &command);
            const auto posZ = strtof(command, &command);
            SetActorPosition(posX, posY, posZ);
            return true;
        }

        if (IsCmd(&command, "LLE_UPDATE_ACTOR_ROT "))
        {
            const auto pitch = static_cast<int>(strtol(command, &command, 10));
            const auto yaw = static_cast<int>(strtol(command, &command, 10));
            const auto roll = static_cast<int>(strtol(command, &command, 10));
            SetActorRotation(pitch, yaw, roll);
            return true;
        }

        if (IsCmd(&command, "LLE_SET_ACTOR_DRAWSCALE3D "))
        {
            const auto scaleX = strtof(command, &command);
            const auto scaleY = strtof(command, &command);
            const auto scaleZ = strtof(command, &command);
            SetActorDrawScale3D(scaleX, scaleY, scaleZ);
            return true;
        }

        if (IsCmd(&command, "LLE_SET_ACTOR_DRAWSCALE "))
        {
            const auto scale = strtof(command, &command);
            SetActorDrawScale(scale);
            return true;
        }

        if (IsCmd(&command, "LLE_SET_ACTOR_HIDDEN "))
        {
            bool hidden = (_stricmp(command, "true") == 0 || strcmp(command, "1") == 0);
            SetActorHidden(hidden);
            return true;
        }

        if (IsCmd(&command, "LLE_SET_MATERIAL "))
        {
            const auto idx = static_cast<int>(strtol(command, &command, 10));
            command++; // Skip space
            auto matPath = ReadStringFromCommand(&command, false);
            SetActorMaterial(idx, matPath);
            return true;
        }

        if (IsCmd(&command, "LLE_SET_MATEXPR_SCALAR "))
        {
            const auto idx = static_cast<int>(strtol(command, &command, 10));
            command++;
            auto paramName = ReadStringFromCommand(&command, true);
            const auto value = strtof(command, &command);
            SetActorScalarParameter(idx, paramName, value);
            return true;
        }

        if (IsCmd(&command, "LLE_SET_MATEXPR_VECTOR "))
        {
            const auto idx = static_cast<int>(strtol(command, &command, 10));
            command++;
            auto paramName = ReadStringFromCommand(&command, true);
            FLinearColor lc{};
            lc.R = strtof(command, &command);
            lc.G = strtof(command, &command);
            lc.B = strtof(command, &command);
            lc.A = strtof(command, &command);
            SetActorVectorParameter(idx, paramName, lc);
            return true;
        }

        if (IsCmd(&command, "LLE_GET_COMPONENT_MATERIALS"))
        {
            SendComponentMaterials();
            return true;
        }

        if (IsCmd(&command, "LLE_GET_LOADED_MATERIALS"))
        {
            SendAllLoadedMaterials();
            return true;
        }

        return false;
    }

    bool LiveLevelEditor::ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result)
    {
        static bool reActivateAfterMapChange = false;

        LEASI_UNUSED_2(Parms, Result);

        if (!IsActive) return true;

        if (SelectedActor && Common::IsDestroyed(SelectedActor))
        {
            ClearSelectedActor();
        }

        auto funcName = Function->GetName();

        if (funcName.Equals(L"PreCommitMapChange") && Context->IsA<AGameInfo>())
        {
            ClearSelectedActor();
            LevelsLoadedByLLE.clear();
            IsActive = false;
            reActivateAfterMapChange = true;
        }
        else if (reActivateAfterMapChange && funcName.Equals(L"PostCommitMapChange") && Context->IsA<AGameInfo>())
        {
            IsActive = true;
            reActivateAfterMapChange = false;
        }
        else if (funcName.Equals(L"PostRender") && Context->IsA<ABioHUD>())
        {
            auto hud = static_cast<ABioHUD*>(Context);
            hud->FlushPersistentDebugLines();

            if (IsPendingDeactivation)
            {
                IsActive = false;
                ClearSelectedActor();
                LevelsLoadedByLLE.clear();
                IsPendingDeactivation = false;
                return true;
            }

            if (hud->WorldInfo)
            {
                CheckLevelChanges(hud->WorldInfo);
            }

            if (SelectedActor && DrawLineToSelected)
            {
                FVector lineEnd = SelectedActor->LOCATION;
                if (SelectedCollectionComponentIndex >= 0 && SelectedActor->IsA<AStaticMeshCollectionActor>())
                {
                    auto smca = static_cast<AStaticMeshCollectionActor*>(SelectedActor);
                    if (static_cast<unsigned int>(SelectedCollectionComponentIndex) < smca->Components.Count())
                    {
                        auto comp = smca->Components(SelectedCollectionComponentIndex);
                        if (comp && comp->IsA<UStaticMeshComponent>())
                        {
                            lineEnd = static_cast<UStaticMeshComponent*>(comp)->Translation;
                        }
                    }
                }
                DrawDebugLine(SharedData::CachedPlayerPosition, lineEnd, TraceLineColor, TraceWidth);
                DrawCoordinateSystem(lineEnd, CoordinateScale, TraceWidth);
                //DO NOT DELETE! Lines of non-zero width will NOT be rendered unless a line of 0 width is also drawn.
                DrawDebugLine(SharedData::CachedPlayerPosition, lineEnd, TraceLineColor, 0);
            }
        }

        return true;
    }


}
