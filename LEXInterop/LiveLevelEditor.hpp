#pragma once

#include <set>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <LESDK/Headers.hpp>
#include "ActionQueue.hpp"
#include "Utilities.hpp"
#include "SharedData.hpp"
#include "LiveLevelEditorActions.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "Common/json.hpp"
#include "Common/Utils.hpp"
#include "AdditionalFunctions.hpp"
using json = nlohmann::json;

namespace LEXInterop
{
    // Live Level Editor - allows real-time manipulation of actors from LEX
    class LiveLevelEditor
    {
    public:
        static inline bool IsActive = false;
        static inline bool IsPendingDeactivation = false;
        static inline AActor* SelectedActor = nullptr;
        static inline SFXName SelectedActorMapName{};
        static inline int SelectedCollectionComponentIndex = -1;
        static inline int SelectedActorComponentIndex = -1;
        static inline bool DrawLineToSelected = true;
        static inline FLinearColor TraceLineColor{ 1.0f, 1.0f, 0.0f, 1.0f };
        static inline float TraceWidth = 3.0f;
        static inline float CoordinateScale = 100.0f;
        static inline std::set<SFXName> LevelsLoadedByLLE{};

        // Handle pipe commands
        static bool HandleCommand(char* command)
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

        // Process game events
        static bool ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result)
        {
			static bool reActivateAfterMapChange = false;

            LEASI_UNUSED_2(Parms, Result);

            if (!IsActive) return true;

            static constexpr QWORD RF_FinishDestroyed = 0x0000000000010000;
            if (SelectedActor && (SelectedActor->ObjectFlags & RF_FinishDestroyed) != 0)
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
            else if(reActivateAfterMapChange && funcName.Equals(L"PostCommitMapChange") && Context->IsA<AGameInfo>())
            {
                IsActive = true;
                reActivateAfterMapChange = true;
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

                // Check for level changes
                if (hud->WorldInfo)
                {
                    CheckLevelChanges(hud->WorldInfo);
                }

                // Draw trace line to selected actor
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

    private:
        static void ClearSelectedActor()
        {
            SelectedActor = nullptr;
            SelectedActorMapName = {};
            SelectedCollectionComponentIndex = -1;
            SelectedActorComponentIndex = -1;
        }

        static void CheckLevelChanges(AWorldInfo* worldInfo)
        {
            // Add main level
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
            std::set_difference(LevelsLoadedByLLE.begin(), LevelsLoadedByLLE.end(), visibleLevels.begin(), visibleLevels.end(), std::back_inserter(diff));
            if (!diff.empty())
            {
                RefreshLevels(worldInfo, &diff);
            }
            else
            {
                //check for newly loaded levels
                std::set_difference(visibleLevels.begin(), visibleLevels.end(), LevelsLoadedByLLE.begin(), LevelsLoadedByLLE.end(), std::back_inserter(diff));
                if (!diff.empty())
                {
                    RefreshLevels(worldInfo);
                }
            }
        }

        //consolidates all transformation data into the properties, to simplify editing them
        static void NormalizeSMC(UStaticMeshComponent* smc)
        {
            FVector translation;
            FVector scale3D;
            float pitch_rad;
            float yaw_rad;
            float roll_rad;

            if (smc->Scale3D.X < 0.0f)
            {
                __noop;
            }

   //         MatrixDecompose(smc->LocalToWorld, translation, scale3D, pitch_rad, yaw_rad, roll_rad);
   //         smc->CachedParentToWorld = IdentityMatrix;
			//smc->AbsoluteTranslation = false;
   //         smc->AbsoluteScale = false;
   //         smc->AbsoluteRotation = false;
   //         smc->Translation = translation;
   //         smc->Rotation = FRotator{ RadiansToUnrealRotationUnits(pitch_rad), RadiansToUnrealRotationUnits(yaw_rad), RadiansToUnrealRotationUnits(roll_rad) };
   //         smc->Scale = 1;
   //         smc->Scale3D = scale3D;

   //         return;
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
            FMatrix checkMatrix = MatrixCompose(translation, scale3D, pitch_rad, yaw_rad, roll_rad);
            smc->Translation = translation;
            smc->Rotation = FRotator{ RadiansToUnrealRotationUnits(pitch_rad), RadiansToUnrealRotationUnits(yaw_rad), RadiansToUnrealRotationUnits(roll_rad) };
            smc->Scale = 1;
            smc->Scale3D = scale3D;
            smc->AbsoluteTranslation = false;
            smc->AbsoluteScale = false;
            smc->AbsoluteRotation = false;
        }

        static json AppendActorsInLevel(ULevelBase* const level)
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
                    const auto smca = reinterpret_cast<AStaticMeshCollectionActor*>(actor);
                    json components = json::array();
                    for (const auto component : smca->Components)
                    {
                        json jcomponent = json::object();
                        if (component && component->IsA<UStaticMeshComponent>())
                        {
                            auto smc = reinterpret_cast<UStaticMeshComponent*>(component);
                            NormalizeSMC(smc);
                            jcomponent["SMCAName"] = smc->GetName().Chars();
                            if (smc->StaticMesh)
                            {
                                jcomponent["SMCAMesh"] = smc->StaticMesh->GetName().Chars();
                            }
                            else
                            {
                                jcomponent["SMCAMesh"] = nullptr;;
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

        static void RefreshLevels(AWorldInfo* worldInfo, const std::vector<SFXName>* removedLevels = nullptr)
        {
            if (removedLevels && SelectedActor)
            {
                for (SFXName levelName : *removedLevels)
                {
                    if (levelName == SelectedActorMapName)
                    {
                        ClearSelectedActor();
                        break;
                    }
                }
            }
            LevelsLoadedByLLE.clear();
            const auto mainLevel = reinterpret_cast<ULevelBase*>(worldInfo->Outer);
            const auto mainPackage = mainLevel->Outer->Outer;
            LevelsLoadedByLLE.insert(mainPackage->Name);

            /* JSON format example
            [
               {
                  "Name":"BioD_Nor",
                  "Actors":[
                     {
                        "Name":"BioPawn_0",
                        "Tag":"Liara"
                     },
                     {
                        "Name":"StaticMeshCollectionActor_23",
                        "Components":[
                           "StaticMeshActor_34",
                           "StaticMeshActor_36"
                        ]
                     }
                  ]
               }
            ]
             */
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
                LevelsLoadedByLLE.insert(streaming_level->PackageName);

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

        static void UpdateSelectedActor(char* selectionStr)
        {
            char* nextToken;
            SFXName mapName(strtok_s(selectionStr, " ", &nextToken), 0, true);
            SFXName actorName(strtok_s(nullptr, " ", &nextToken), 0, true);

            // Search for the actor
            for (Common::TypedObjectIterator<AActor, true, false> It; It; ++It)
            {
                auto actor = *It;
                if (!actor) continue;

                auto objMapName = GetContainingMapName(actor);
                if (mapName != objMapName) continue;

                if (actorName == actor->Name)
                {
                    SelectedActor = actor;
                    SelectedActorMapName = objMapName;
                    SelectedCollectionComponentIndex = -1;
                    SelectedActorComponentIndex = -1;

                    // Check for component index
                    if (nextToken && strlen(nextToken) > 0)
                    {
                        int compIdx = static_cast<int>(strtol(nextToken, nullptr, 10));
                        if (SelectedActor->IsA< AStaticMeshCollectionActor>())
                        {
                            if (compIdx < 0 || static_cast<unsigned int>(compIdx) >= static_cast<AStaticMeshCollectionActor*>(SelectedActor)->Components.Count())
                            {
                                goto NotFound;
							}
                            SelectedCollectionComponentIndex = compIdx;
                        }
                        else if (SelectedActor->IsA<AStaticLightCollectionActor>())
                        {
                            if (compIdx < 0 || static_cast<unsigned int>(compIdx) >= static_cast<AStaticLightCollectionActor*>(SelectedActor)->Components.Count())
                            {
                                goto NotFound;
                            }
                            SelectedCollectionComponentIndex = compIdx;
                        }
                        else
                        {
                            //if it's -1, we're not selecting a component
                            if (compIdx > 0 && static_cast<unsigned int>(compIdx) >= SelectedActor->Components.Count())
                            {
                                goto NotFound;
							}
                            SelectedActorComponentIndex = compIdx;
                        }
                    }

                    SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED");
                    return;
                }
            }
        NotFound:
            ClearSelectedActor();
            SendStringToLEX(L"LIVELEVELEDITOR ACTORSELECTED NOTFOUND");
        }

        static void SendActorData()
        {
            if (!SelectedActor) return;

            FVector translation{};
            float scale = 1.0f;
            FVector scale3D{ 1.0f, 1.0f, 1.0f };
            FRotator rotation{};
            bool hidden = false;

            if (SelectedActor->IsA<AStaticMeshCollectionActor>() &&
                SelectedCollectionComponentIndex >= 0)
            {
                auto smca = static_cast<AStaticMeshCollectionActor*>(SelectedActor);
                if (static_cast<unsigned int>(SelectedCollectionComponentIndex) < smca->Components.Count())
                {
                    auto comp = smca->Components(SelectedCollectionComponentIndex);
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
                translation = SelectedActor->LOCATION;
                scale = SelectedActor->DrawScale;
                scale3D = SelectedActor->DrawScale3D;
                rotation = SelectedActor->Rotation;
                hidden = SelectedActor->bHidden;
            }

            std::wstringstream ss;
            ss << L"LIVELEVELEDITOR ACTORLOC " << translation.X << L" " << translation.Y << L" " << translation.Z;

            ss << L" ACTORROT " << rotation.Pitch << L" " << rotation.Yaw << L" " << rotation.Roll;

            ss << L" ACTORSCALE " << scale << L" " << scale3D.X << L" " << scale3D.Y << L" " << scale3D.Z;

            ss << L" HIDDEN " << (hidden ? 1 : 0);
            SendStringToLEX(ss.str());
        }

        template<class dataType, std::derived_from<ActionBase> componentAction, std::derived_from<ActionBase> actorAction>
        static void SetActorData(dataType data)
        {
            if (!SelectedActor) return;

            if (SelectedActor->IsA<AStaticMeshCollectionActor>() &&
                SelectedCollectionComponentIndex >= 0)
            {
                auto smca = static_cast<AStaticMeshCollectionActor*>(SelectedActor);
                if (static_cast<unsigned int>(SelectedCollectionComponentIndex) < smca->Components.Count())
                {
                    auto comp = smca->Components(SelectedCollectionComponentIndex);
                    if (comp && comp->IsA<UStaticMeshComponent>())
                    {
                        QueueAction(new componentAction(static_cast<UStaticMeshComponent*>(comp), data));
                    }
                }
            }
            else
            {
                QueueAction(new actorAction(SelectedActor, data));
            }
        }

        static void SetActorPosition(float x, float y, float z)
        {
			SetActorData<FVector, ComponentMoveAction, MoveAction>(FVector{ x, y, z });
        }

        static void SetActorRotation(int pitch, int yaw, int roll)
        {
			SetActorData<FRotator, ComponentRotateAction, RotateAction>(FRotator{ pitch, yaw, roll });
        }

        static void SetActorDrawScale3D(float x, float y, float z)
        {
			SetActorData<FVector, ComponentScale3DAction, Scale3DAction>(FVector{ x, y, z });
        }

        static void SetActorDrawScale(float scale)
        {
			SetActorData<float, ComponentScaleAction, ScaleAction>(scale);
        }

        static void SetActorHidden(bool hidden)
        {
			SetActorData<bool, ComponentHideAction, HideAction>(hidden);
        }

        static void SetActorMaterial(int idx, std::string matPath)
        {
            if (!SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new ComponentSetMaterialAction(comp, idx, matPath));
            }
        }

        static void SetActorScalarParameter(int idx, std::string paramName, float value)
        {
            if (!SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new ComponentSetScalarParameterAction(comp, idx, s2ws(paramName), value));
            }
        }

        static void SetActorVectorParameter(int idx, std::string paramName, FLinearColor value)
        {
            if (!SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new ComponentSetVectorParameterAction(comp, idx, s2ws(paramName), value));
            }
        }

        static UMeshComponent* GetSelectedMeshComponent()
        {
            if (!SelectedActor) return nullptr;

            if (SelectedActor->IsA<AStaticMeshCollectionActor>() &&
                SelectedCollectionComponentIndex >= 0)
            {
                auto smca = static_cast<AStaticMeshCollectionActor*>(SelectedActor);
                if (static_cast<unsigned int>(SelectedCollectionComponentIndex) < smca->Components.Count())
                {
                    auto comp = smca->Components(SelectedCollectionComponentIndex);
                    if (comp && comp->IsA<UMeshComponent>())
                    {
                        return static_cast<UMeshComponent*>(comp);
                    }
                }
            }
            else if (SelectedActorComponentIndex >= 0 &&
                     static_cast<unsigned int>(SelectedActorComponentIndex) < SelectedActor->Components.Count())
            {
                auto comp = SelectedActor->Components(SelectedActorComponentIndex);
                if (comp && comp->IsA<UMeshComponent>())
                {
                    return static_cast<UMeshComponent*>(comp);
                }
            }

            return nullptr;
        }

        static void SendComponentMaterials()
        {
            if (!SelectedActor) return;

            UMeshComponent* comp = GetSelectedMeshComponent();
            if (comp)
            {
                QueueAction(new LEXMessageSendComponentMaterialsAction(comp));
            }
        }

        static void SendAllLoadedMaterials()
        {
            // Find all loaded materials and send to LEX
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

        static std::string ReadStringFromCommand(char** command, bool hasAnotherParam)
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
    };
}
