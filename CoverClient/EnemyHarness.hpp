#pragma once

#include <LESDK/Headers.hpp>

namespace CoverClient
{
	// ! Enemy harness -- putting a real AI on the generated network
	// ========================================
	//
	// Everything about this network has been verified two ways: by reading counters, and by
	// Mgamerz walking into cover himself. Neither exercises the thing it is FOR. The player's
	// cover entry uses BioPlayerController::FindPlayerCover and an octree radius query; an
	// enemy uses the ReachSpecs -- the whole point of the routing passes, and the part nothing
	// has ever tested.
	//
	// `cc.enemyspawnpoint` only ever placed SFXEnemySpawnPoint actors and said so: "Something
	// still has to spawn enemies at them." This is that something.
	//
	// The recipe turned out to be short, because the shipped data does the work:
	//
	//   * `SFXPawn_Trooper.pcc` holds the class, the AI (`SFXGameContent.SFXAI_Trooper`,
	//     which derives SFXAI_Cover), and the archetype
	//     `Char_Enemies.Archetypes.Cerberus.AssaultTrooper`.
	//   * `Default__SFXPawn_Trooper` already carries **ControllerClass = SFXAI_Trooper** and a
	//     **Loadout**, so the pawn knows what to spawn to drive it and what to hold.
	//   * `AActor::Spawn` takes an ActorTemplate we have always passed null for. Passing the
	//     archetype is what makes the thing a Cerberus trooper rather than a bare pawn.
	//   * `APawn::SpawnDefaultController` then reads ControllerClass and possesses.
	//
	// So: load the package, spawn from the archetype, let the pawn make its own controller.

	/// cc.spawnenemy [count] -- puts `count` of EACH enemy type on the generated network, far
	/// enough away that they have to path to reach the player rather than starting on top of
	/// them. Default 1, i.e. one of each.
	///
	/// The roster is Trooper, Banshee, Brute and Atlas. Only the Trooper takes cover; the
	/// other three are too large for a cover slot and their AI classes do not derive
	/// SFXAI_Cover. That makes them the better test of the ROUTING half of the network --
	/// a Brute failing to cross a room a Trooper crosses is a path-size problem, not a cover
	/// one.
	///
	/// Spawns on path nodes, because those are known-standable ground the routing passes have
	/// already connected, and spaces them by the widest pawn's collision radius so an Atlas
	/// and a Brute do not start the test inside each other.
	void CmdSpawnEnemy(int count);

	/// cc.despawnenemies -- destroys the troopers and their controllers.
	void CmdDespawnEnemies();
}
