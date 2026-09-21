#include "CoverClient/EnemyHarness.hpp"
#include "CoverClient/PathNodes.hpp"
#include "CoverClient/NavRuntime.hpp"
#include "CoverClient/Helpers.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace CoverClient
{
	namespace
	{
		// ! The enemy roster
		// ========================================
		//
		// Every enemy needs the same four things, and all four live in its own pawn package,
		// so loading that one package gets the lot:
		//
		//   * the ARCHETYPE -- the configured instance the game itself spawns from. Passing it
		//     as Spawn's ActorTemplate is what makes the result a Cerberus Atlas rather than a
		//     bare pawn with class defaults.
		//   * the pawn CLASS, taken off the archetype rather than looked up (see the note on
		//     StaticClass below).
		//   * the AI class, read from the pawn CDO's `ControllerClass`.
		//   * the base combat command, without which a fully possessed AI just stands there.
		//
		// Resolved from the shipped packages rather than guessed:
		//
		//   package            archetype                                    ControllerClass
		//   SFXPawn_Trooper    Char_Enemies.Archetypes.Cerberus.AssaultTrooper  SFXAI_Trooper
		//   SFXPawn_Banshee    Char_Enemies.Archetypes.Reapers.Banshee          SFXAI_Banshee
		//   SFXPawn_Brute      Char_Enemies.Archetypes.Reapers.Brute            SFXAI_Brute
		//   SFXPawn_Atlas      Char_Enemies.Archetypes.Cerberus.Atlas           SFXAI_Atlas
		//
		// and each package also carries its own `SFXAICmd_Base_<Name>`.
		//
		// Worth knowing when reading the results: **only the trooper uses cover.** Banshee,
		// Brute and Atlas are all far bigger than the default pawn, and nothing that size can
		// occupy a cover slot -- their AI classes do not derive SFXAI_Cover at all. They
		// exercise the ROUTING half of the network (path nodes, ReachSpecs, sizes) and not the
		// cover half, which makes them a useful second opinion: if a Brute cannot cross a room
		// a trooper crosses fine, that is a path-size problem rather than a cover problem.
		struct EnemyType
		{
			const wchar_t* label;
			const wchar_t* package;
			const wchar_t* archetypeName;   // "<Class> <Package.Path>" for UObject::FindObject
			const wchar_t* aiClassName;
			const wchar_t* combatCommandName;
		};

		constexpr EnemyType k_enemyTypes[] = {
			{ L"Trooper", L"SFXPawn_Trooper",
			  L"SFXPawn_Trooper Char_Enemies.Archetypes.Cerberus.AssaultTrooper",
			  L"Class SFXGameContent.SFXAI_Trooper",
			  L"Class SFXGameContent.SFXAICmd_Base_Trooper" },
			{ L"Banshee", L"SFXPawn_Banshee",
			  L"SFXPawn_Banshee Char_Enemies.Archetypes.Reapers.Banshee",
			  L"Class SFXGameContent.SFXAI_Banshee",
			  L"Class SFXGameContent.SFXAICmd_Base_Banshee" },
			{ L"Brute", L"SFXPawn_Brute",
			  L"SFXPawn_Brute Char_Enemies.Archetypes.Reapers.Brute",
			  L"Class SFXGameContent.SFXAI_Brute",
			  L"Class SFXGameContent.SFXAICmd_Base_Brute" },
			{ L"Atlas", L"SFXPawn_Atlas",
			  L"SFXPawn_Atlas Char_Enemies.Archetypes.Cerberus.Atlas",
			  L"Class SFXGameContent.SFXAI_Atlas",
			  L"Class SFXGameContent.SFXAICmd_Base_Atlas" },
		};

		/// How far from the player a spawn has to be. The point is to watch one PATH to cover,
		/// so dropping it on top of the player would test nothing.
		constexpr float k_minSpawnDistance = 768.0f;
		constexpr float k_maxSpawnDistance = 4096.0f;

		/// Floor on the gap between two spawn points, used when a collision radius cannot be
		/// read. Path nodes sit 256u apart, so without this the roster lands on four adjacent
		/// nodes and an Atlas and a Brute start the test interpenetrating.
		constexpr float k_minSpawnSeparation = 200.0f;

		struct SpawnedEnemy
		{
			APawn*       pawn{ nullptr };
			AController* controller{ nullptr };
		};
		std::vector<SpawnedEnemy> g_SpawnedEnemies;

		ABioWorldInfo* GetWorldInfo()
		{
			auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
			return (engine != nullptr)
				? static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo()) : nullptr;
		}

		std::string Narrow(const FString& value)
		{
			const wchar_t* const data = *value;
			return (data != nullptr) ? ws2s(data) : std::string{};
		}

		std::string PathOf(UObject* object)
		{
			return (object != nullptr) ? Narrow(object->GetFullPath()) : std::string{ "<null>" };
		}

		float Distance2D(const FVector& a, const FVector& b)
		{
			const float dx = b.X - a.X;
			const float dy = b.Y - a.Y;
			return std::sqrt((dx * dx) + (dy * dy));
		}

		/// What one resolved enemy type needs to actually spawn.
		struct ResolvedEnemy
		{
			const EnemyType* type{ nullptr };
			UObject*  archetype{ nullptr };
			UClass*   pawnClass{ nullptr };
			UClass*   aiClass{ nullptr };
			UClass*   combatCommand{ nullptr };
			float     collisionRadius{ 0.0f };
		};

		/// Reads the archetype's own cylinder so the spacing below reflects the actual pawn
		/// rather than an assumption. An Atlas is several times a trooper's footprint.
		float ArchetypeRadius(UObject* archetype)
		{
			auto* const pawn = (archetype != nullptr)
				? archetype->Cast<APawn>() : nullptr;
			if (pawn == nullptr || pawn->CylinderComponent == nullptr) { return 0.0f; }
			return pawn->CylinderComponent->CollisionRadius;
		}

		/// Finds (loading the package if needed) everything one enemy type needs. Returns
		/// false and says why if the type cannot be spawned, so one missing enemy does not
		/// take the rest of the roster down with it.
		bool ResolveEnemy(ABioWorldInfo* wi, const EnemyType& type, ResolvedEnemy& out)
		{
			out = ResolvedEnemy{};
			out.type = &type;

			// ! Look the archetype up by NAME, never with a typed iterator
			//
			// The first version of this used Common::FindFirstObject<ASFXPawn_Trooper>, and it
			// hung the game solid. ASFXPawn_Trooper::StaticClass() caches its lookup in a
			// static -- but only on success. Before the package is loaded the class does not
			// exist, so nothing is cached, and every step of the iteration re-ran FindClass: a
			// full scan of the object array doing FString compares, per object, over the whole
			// object array.
			//
			// Looking the archetype up by full name is one scan, and it works whether or not
			// the class is loaded because the compare is against UObject.
			out.archetype = UObject::FindObject<UObject>(type.archetypeName);
			if (out.archetype == nullptr)
			{
				auto* const game = static_cast<ASFXGame*>(wi->Game);
				if (game == nullptr)
				{
					LEASI_ERROR("{}: no SFXGame to load through, and {} is not loaded.",
						ws2s(type.label), ws2s(type.package));
					return false;
				}
				LEASI_INFO("{}: loading {} ...", ws2s(type.label), ws2s(type.package));
				game->LoadPackage(FString{ type.package });
				out.archetype = UObject::FindObject<UObject>(type.archetypeName);
			}
			if (out.archetype == nullptr)
			{
				LEASI_ERROR("{}: could not find {} even after loading {}.",
					ws2s(type.label), ws2s(type.archetypeName), ws2s(type.package));
				return false;
			}

			// The class comes off the archetype, exactly as the shipped script does it
			// (`Class<SFXPawn_PlayerMP>(PlayerArchetype.@Class)` in SFXGameMP). No
			// StaticClass() call, so no chance of the scan above.
			out.pawnClass = out.archetype->Class;
			out.aiClass = UObject::FindClass(type.aiClassName);
			if (out.pawnClass == nullptr || out.aiClass == nullptr)
			{
				LEASI_ERROR("{}: archetype {} resolved but its class ({}) or AI class {} ({}) "
					"did not.", ws2s(type.label), PathOf(out.archetype),
					(out.pawnClass != nullptr) ? "ok" : "null",
					ws2s(type.aiClassName), (out.aiClass != nullptr) ? "ok" : "null");
				return false;
			}

			out.combatCommand = UObject::FindClass(type.combatCommandName);
			if (out.combatCommand == nullptr)
			{
				LEASI_WARN("{}: combat command {} not found. It will spawn, possess and "
					"acquire, and then stand still -- an AI with no combat command is idle.",
					ws2s(type.label), ws2s(type.combatCommandName));
			}

			out.collisionRadius = ArchetypeRadius(out.archetype);
			LEASI_INFO("{}: archetype {}, class {}, AI {}, command {}, radius {:.0f}u.",
				ws2s(type.label), PathOf(out.archetype), PathOf(out.pawnClass),
				PathOf(out.aiClass),
				(out.combatCommand != nullptr) ? PathOf(out.combatCommand)
					: std::string{ "<none>" },
				out.collisionRadius);
			return true;
		}

		/// Spawns one pawn of `enemy` at `at`, facing `facing`, possesses it and starts it
		/// fighting. Returns the pawn, or null with the reason logged.
		APawn* SpawnOne(ABioWorldInfo* wi, const ResolvedEnemy& enemy, const FVector& at,
			const FRotator& facing, int enemyTeam, int& possessed, int& commanded)
		{
			// The ActorTemplate parameter is what makes this a Cerberus trooper (or a Banshee,
			// or an Atlas) -- mesh, loadout, speeds -- rather than a bare pawn with class
			// defaults.
			auto* const actor = wi->Spawn(enemy.pawnClass, nullptr, SFXName{}, at, facing,
				static_cast<AActor*>(enemy.archetype), TRUE /*bNoCollisionFail*/, FALSE);
			if (actor == nullptr)
			{
				LEASI_ERROR("  {}: pawn Spawn returned null.", ws2s(enemy.type->label));
				return nullptr;
			}
			auto* const pawn = static_cast<APawn*>(actor);

			// ! The controller is spawned and possesses explicitly
			//
			// SpawnDefaultController() would read ControllerClass and do this, but the shipped
			// sequence (SFXPowerCustomAction_SentryTurret::SpawnTurret in BIOP_MP_COMMON) does
			// it by hand and then does two more things that matter:
			//
			//     TurretAI.Possess(Turret, FALSE);
			//     TurretAI = SFXAI_Core(Turret.Controller);
			//     TurretAI.SetTeam(int(m_oPawn.Controller.GetTeamNum()));
			//     TurretAI.AutoAcquireEnemy();
			//
			// **SetTeam is on the CONTROLLER, not the pawn**, which is why looking for a team
			// field on SFXPawn found nothing. And AutoAcquireEnemy is what makes it pick a
			// target rather than stand there -- a spawned AI is idle until something tells it
			// there is an enemy. Re-reading Controller after Possess is the shipped idiom too:
			// the controller that ends up driving the pawn is not necessarily the one spawned.
			auto* const aiActor = wi->Spawn(enemy.aiClass, nullptr, SFXName{}, at, facing,
				nullptr, TRUE /*bNoCollisionFail*/, FALSE);
			if (aiActor == nullptr)
			{
				g_SpawnedEnemies.push_back(SpawnedEnemy{ pawn, nullptr });
				LEASI_WARN("  {} spawned but its AI did not. It will stand still.",
					PathOf(pawn));
				return pawn;
			}

			// ! Possess through ABioAiController, NOT through AController
			//
			// These generated event wrappers are not virtual. Each one looks up a UFunction by
			// a HARDCODED name and calls ProcessEvent with it, so the name is decided by the
			// static type of the pointer, not by what the object actually is:
			//
			//     AController::eventPossess      -> "Function Engine.Controller.Possess"
			//     ABioAiController::eventPossess -> "Function SFXGame.BioAiController.Possess"
			//
			// Calling it through an AController* ran the BASE implementation and skipped the
			// override -- which is the one that sets `MyBP`, the AI's own reference to its
			// BioPawn. The pawn was possessed and the AI had no idea what it was driving, so
			// every command that touched the pawn threw:
			//
			//     Accessed None 'MyBP'
			//     Error in 'SFXGameContent.SFXAICmd_Base_Cover.ShouldAttack' ... HealthPct: 0
			//     Error in 'SFXGame.SFXAICmd_FireWeapon.Popped' ...
			//
			// HealthPct reading 0 is the same cause: health comes off MyBP.
			//
			// **Rule for this SDK: call a script function through the most DERIVED type that
			// declares it.** A static_cast to a base class silently changes which function
			// runs.
			auto* const ai = aiActor->Cast<ABioAiController>();
			if (ai == nullptr)
			{
				g_SpawnedEnemies.push_back(SpawnedEnemy{ pawn, nullptr });
				LEASI_WARN("  {} spawned but its AI is not a BioAiController, so Possess would "
					"run the base implementation and leave MyBP null.", PathOf(pawn));
				return pawn;
			}
			ai->eventPossess(pawn, FALSE);

			auto* const driving = pawn->Controller;
			if (driving != nullptr)
			{
				if (auto* const core = driving->Cast<ASFXAI_Core>())
				{
					// Order copied from SFXWave_Horde::SpawnEnemy: team, then acquire, then
					// the combat command. The command last, because it is what starts acting
					// on the other two.
					core->eventSetTeam(enemyTeam);
					core->eventAutoAcquireEnemy();
					if (enemy.combatCommand != nullptr)
					{
						core->BeginCombatCommand(enemy.combatCommand,
							FString{ L"cc.spawnenemy" }, TRUE /*bForced*/);
						commanded++;
					}
				}
				possessed++;
			}

			g_SpawnedEnemies.push_back(SpawnedEnemy{ pawn,
				(driving != nullptr) ? driving : static_cast<AController*>(ai) });
			return pawn;
		}
	}

	void CmdSpawnEnemy(int count)
	{
		LEASI_INFO("=== cc.spawnenemy ===");

		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_ERROR("No WorldInfo.");
			return;
		}
		auto* const pc = wi->GetALocalPlayerController();
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn.");
			return;
		}
		// `count` is per TYPE, not a total: the default is one of each, which is the whole
		// point of having a roster.
		if (count < 1) { count = 1; }

		// Resolve the whole roster first. A missing enemy is reported and skipped rather than
		// aborting, so one bad entry does not cost the other three.
		std::vector<ResolvedEnemy> roster;
		for (const EnemyType& type : k_enemyTypes)
		{
			ResolvedEnemy resolved{};
			if (ResolveEnemy(wi, type, resolved)) { roster.push_back(resolved); }
		}
		if (roster.empty())
		{
			LEASI_ERROR("No enemy type resolved. Nothing to spawn.");
			return;
		}

		// Where to put them. Path nodes are known-standable and already joined to the routing
		// network, so an enemy spawned on one starts on the thing being tested.
		const FVector playerLoc = pc->Pawn->location;
		std::vector<std::pair<float, FVector>> spots;
		for (auto* const node : GetSpawnedPathNodes())
		{
			if (node == nullptr || ::Common::IsDestroyed(node)) { continue; }
			const float d = Distance2D(node->LOCATION, playerLoc);
			if (d < k_minSpawnDistance || d > k_maxSpawnDistance) { continue; }
			spots.emplace_back(d, node->LOCATION);
		}
		std::sort(spots.begin(), spots.end(),
			[](const std::pair<float, FVector>& a, const std::pair<float, FVector>& b)
			{ return a.first < b.first; });

		if (spots.empty())
		{
			LEASI_WARN("No path node between {:.0f}u and {:.0f}u away, so there is nowhere "
				"known-standable to spawn on. Run cc.pathnodes first.",
				k_minSpawnDistance, k_maxSpawnDistance);
			return;
		}

		// ! Keep them apart
		//
		// Path nodes are 256u apart, so taking the N nearest would stack a Brute and an Atlas
		// on adjacent nodes and start the test with them interpenetrating. Separation is sized
		// from the widest pawn actually being spawned rather than a guess: two pawns need
		// their radii plus clearance, and the widest-versus-widest case is the one that has to
		// fit.
		float widestRadius = 0.0f;
		for (const ResolvedEnemy& enemy : roster)
		{
			widestRadius = (enemy.collisionRadius > widestRadius)
				? enemy.collisionRadius : widestRadius;
		}
		const float separation =
			std::max(k_minSpawnSeparation, (widestRadius * 2.0f) + 64.0f);

		// Players are team 0 and enemies team 1 in this game; read the player's rather than
		// assume, and take the other one.
		const int playerTeam = static_cast<int>(pc->GetTeamNum());
		const int enemyTeam = (playerTeam == 0) ? 1 : 0;

		const int wantedTotal = count * static_cast<int>(roster.size());
		LEASI_INFO("Spawning {} of each of {} type(s) = {}, at least {:.0f}u apart "
			"(widest pawn radius {:.0f}u), {}-{}u from the player.",
			count, roster.size(), wantedTotal, separation, widestRadius,
			static_cast<int>(k_minSpawnDistance), static_cast<int>(k_maxSpawnDistance));

		std::vector<FVector> taken;
		size_t nextSpot = 0;
		int spawned = 0, possessed = 0, commanded = 0, noRoom = 0;

		// Round-robin by type so that a shortage of room costs every type equally rather than
		// spawning four troopers and no Atlas.
		for (int round = 0; round < count; round++)
		{
			for (const ResolvedEnemy& enemy : roster)
			{
				// Nearest unused spot that clears everything already placed.
				bool placed = false;
				for (size_t i = nextSpot; i < spots.size(); i++)
				{
					const FVector& candidate = spots[i].second;
					bool clear = true;
					for (const FVector& used : taken)
					{
						if (Distance2D(candidate, used) < separation) { clear = false; break; }
					}
					if (!clear) { continue; }

					const FVector toPlayer{
						playerLoc.X - candidate.X, playerLoc.Y - candidate.Y, 0.0f };
					FRotator facing{};
					facing.Yaw = static_cast<int>(
						std::atan2(toPlayer.Y, toPlayer.X) * 10430.378f);

					auto* const pawn = SpawnOne(wi, enemy, candidate, facing, enemyTeam,
						possessed, commanded);
					if (pawn != nullptr)
					{
						taken.push_back(candidate);
						spawned++;
						LEASI_INFO("  {} at ({:.0f}, {:.0f}, {:.0f}), {:.0f}u away, team {}.",
							ws2s(enemy.type->label), candidate.X, candidate.Y, candidate.Z,
							spots[i].first, enemyTeam);
					}
					// Consumed whether or not the spawn worked -- retrying the same spot with
					// the same geometry would fail the same way.
					if (i == nextSpot) { nextSpot++; }
					placed = true;
					break;
				}
				if (!placed) { noRoom++; }
			}
		}

		LEASI_INFO("Spawned {} of {}; {} possessed, {} given a combat command. "
			"Player is team {}, they are team {}.",
			spawned, wantedTotal, possessed, commanded, playerTeam, enemyTeam);
		if (noRoom > 0)
		{
			LEASI_WARN("{} enemy/enemies had no path node left that was {:.0f}u clear of the "
				"others. Widen the search or run cc.pathnodes with a bigger radius.",
				noRoom, separation);
		}
		if (possessed < spawned)
		{
			LEASI_WARN("{} enemy/enemies have no Controller after Possess, so they will stand "
				"still however good the network is. That is a spawn problem, not a pathing "
				"one.", spawned - possessed);
		}
		LEASI_INFO("Only the Trooper takes cover -- the other three are too large for a cover "
			"slot and their AI does not derive SFXAI_Cover. They test the ROUTING half: if a "
			"Brute cannot cross a room a Trooper crosses, that is a path-size problem.");
		LEASI_INFO("Watch it move: 'show paths' draws the network it is using.");
	}

	void CmdDespawnEnemies()
	{
		LEASI_INFO("=== cc.despawnenemies ===");
		if (g_SpawnedEnemies.empty())
		{
			LEASI_INFO("Nothing to remove.");
			return;
		}

		int destroyed = 0, controllersDestroyed = 0;
		for (const auto& entry : g_SpawnedEnemies)
		{
			// The controller is a separate actor the pawn merely refers to, so destroying the
			// pawn leaves it behind -- the same trap the cover markers had. It goes first, or
			// it spends a frame driving a destroyed pawn.
			if (entry.controller != nullptr && !::Common::IsDestroyed(entry.controller))
			{
				entry.controller->Destroy();
				controllersDestroyed++;
			}
			if (entry.pawn != nullptr && !::Common::IsDestroyed(entry.pawn))
			{
				entry.pawn->Destroy();
				destroyed++;
			}
		}
		LEASI_INFO("Destroyed {} of {} enemy/enemies and {} controller(s).",
			destroyed, static_cast<int>(g_SpawnedEnemies.size()), controllersDestroyed);
		g_SpawnedEnemies.clear();
	}
}
