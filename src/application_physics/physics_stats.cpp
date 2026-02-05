// physics_stats.cpp
//
// Physics engine performance statistics reporting.

#include "plc_emulator/core/application.h"

#include <chrono>
#include <cmath>
#include <iostream>

namespace plc {

void Application::UpdatePhysicsPerformanceStats() {
  if (!physics_engine_ || !physics_engine_->isInitialized)
    return;

  PhysicsPerformanceStats& stats = physics_engine_->performanceStats;
  stats.totalComputeTime = static_cast<float>(physics_stage_total_ms_);
  stats.electricalSolveTime =
      static_cast<float>(physics_stage_advanced_electrical_ms_);
  stats.pneumaticSolveTime =
      static_cast<float>(physics_stage_advanced_pneumatic_ms_);
  stats.mechanicalSolveTime =
      static_cast<float>(physics_stage_advanced_mechanical_ms_);
  stats.networkBuildTime =
      static_cast<float>(physics_stage_advanced_build_ms_);

  // Network size stats
  stats.electricalNodes = physics_engine_->electricalNetwork->nodeCount;
  stats.electricalEdges = physics_engine_->electricalNetwork->edgeCount;
  stats.pneumaticNodes = physics_engine_->pneumaticNetwork->nodeCount;
  stats.pneumaticEdges = physics_engine_->pneumaticNetwork->edgeCount;
  stats.mechanicalNodes = physics_engine_->mechanicalSystem->nodeCount;
  stats.mechanicalEdges = physics_engine_->mechanicalSystem->edgeCount;

  // Convergence stats
  stats.electricalConverged = physics_engine_->electricalNetwork->isConverged;
  stats.pneumaticConverged = physics_engine_->pneumaticNetwork->isConverged;
  stats.mechanicalStable = physics_engine_->mechanicalSystem->isStable;
  stats.electricalIterations =
      physics_engine_->electricalNetwork->currentIteration;
  stats.pneumaticIterations =
      physics_engine_->pneumaticNetwork->currentIteration;

  // Quality metrics
  ElectricalNetwork* elec = physics_engine_->electricalNetwork;
  if (elec && elec->nodeCount > 0) {
    // Electrical residual
    float residual = 0.0f;
    for (int i = 0; i < elec->nodeCount; i++) {
      residual += elec->nodes[i].netCurrent * elec->nodes[i].netCurrent;
    }
    stats.electricalResidual = std::sqrt(residual);

    // Total power consumption
    float totalPower = 0.0f;
    for (int e = 0; e < elec->edgeCount; e++) {
      totalPower += elec->edges[e].powerDissipation;
    }
    stats.powerConsumption = totalPower;
  }

  PneumaticNetwork* pneu = physics_engine_->pneumaticNetwork;
  if (pneu && pneu->nodeCount > 0) {
    // Pneumatic residual (mass conservation check)
    float residual = 0.0f;
    for (int i = 0; i < pneu->nodeCount; i++) {
      residual += pneu->nodes[i].netMassFlow * pneu->nodes[i].netMassFlow;
    }
    stats.pneumaticResidual = std::sqrt(residual);

    // Total air consumption (L/min)
    float totalAirConsumption = 0.0f;
    for (int e = 0; e < pneu->edgeCount; e++) {
      totalAirConsumption += std::abs(pneu->edges[e].volumeFlowRate);
    }
    stats.airConsumption = totalAirConsumption;
  }

  MechanicalSystem* mech = physics_engine_->mechanicalSystem;
  if (mech && mech->nodeCount > 0) {
    // Mechanical total energy
    float totalEnergy = 0.0f;
    for (int i = 0; i < mech->nodeCount; i++) {
      const MechanicalNode& node = mech->nodes[i];
      // Kinetic energy: 1/2 m v^2
      float kineticEnergy = 0.5f * node.mass *
                            (node.velocity[0] * node.velocity[0] +
                             node.velocity[1] * node.velocity[1] +
                             node.velocity[2] * node.velocity[2]);
      totalEnergy += kineticEnergy;
    }

    // Elastic potential energy (springs)
    for (int e = 0; e < mech->edgeCount; e++) {
      totalEnergy += mech->edges[e].energyStored;
    }

    stats.mechanicalEnergy = totalEnergy;
  }

  // Real-time performance
  static auto lastUpdateTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      currentTime - lastUpdateTime);

  if (elapsed.count() > 0) {
    float deltaSeconds = elapsed.count() / 1000000.0f;
    stats.simulationFPS = 1.0f / deltaSeconds;
    stats.realTimeRatio = (physics_engine_->deltaTime) / deltaSeconds;

    lastUpdateTime = currentTime;
  }

  // Approximate memory usage
  int totalMemory = 0;
  totalMemory += elec ? (elec->nodeCount * sizeof(ElectricalNode) +
                         elec->edgeCount * sizeof(ElectricalEdge))
                      : 0;
  totalMemory += pneu ? (pneu->nodeCount * sizeof(PneumaticNode) +
                         pneu->edgeCount * sizeof(PneumaticEdge))
                      : 0;
  totalMemory += mech ? (mech->nodeCount * sizeof(MechanicalNode) +
                         mech->edgeCount * sizeof(MechanicalEdge))
                      : 0;
  totalMemory += physics_engine_->maxComponents * sizeof(TypedPhysicsState);

  stats.memoryUsage = totalMemory / (1024.0f * 1024.0f);  // bytes -> MB

  // Debug output (optional)
  if (physics_engine_->enableLogging && physics_engine_->logLevel >= 3) {
    static int debugCounter = 0;
    if (++debugCounter % 180 == 0) {  // Every 3s at 60 FPS
      std::cout << "[Physics Stats] FPS:" << stats.simulationFPS
                << " Update:" << physics_stage_total_ms_ << "ms"
                << " Power:" << stats.powerConsumption << "W"
                << " Air:" << stats.airConsumption << "L/min"
                << " Energy:" << stats.mechanicalEnergy << "J"
                << " Memory:" << stats.memoryUsage << "MB"
                << " Basic[E:" << physics_stage_electrical_ms_
                << " L:" << physics_stage_component_ms_
                << " P:" << physics_stage_pneumatic_ms_
                << " A:" << physics_stage_actuator_ms_
                << " PLC:" << physics_stage_plc_ms_ << "]"
                << " Adv[B:" << physics_stage_advanced_build_ms_
                << " S:" << physics_stage_advanced_solve_ms_
                << " X:" << physics_stage_advanced_sync_ms_
                << " Steps:" << physics_stage_advanced_steps_ << "/"
                << physics_stage_steps_ << "]" << std::endl;
    }
  }
}

}  // namespace plc
