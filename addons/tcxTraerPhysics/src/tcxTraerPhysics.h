#pragma once
// =============================================================================
// tcxTraerPhysics.h — Umbrella include for Traer Physics addon
// =============================================================================
// Particle system physics: springs, N-body attraction, multiple integrators.
// Modern C++ port of Jeffrey Traer Bernstein's Traer Physics.
//
// Quick start:
//   auto ps = ParticleSystem::create();
//   auto p1 = ps->makeParticle(1.0f, 100, 200, 0);
//   auto p2 = ps->makeParticle(1.0f, 300, 200, 0);
//   ps->makeSpring(p1, p2, 0.2f, 0.01f, 100);
//   ps->tick(1.0f / 60.0f);  // Step physics
//   // Then draw: p1->position.x, p1->position.y
// =============================================================================

#include "tcxParticle.h"
#include "tcxForce.h"
#include "tcxSpring.h"
#include "tcxAttraction.h"
#include "tcxIntegrator.h"
#include "tcxEulerIntegrator.h"
#include "tcxModifiedEulerIntegrator.h"
#include "tcxRungeKuttaIntegrator.h"
#include "tcxParticleSystem.h"
