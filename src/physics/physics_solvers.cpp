// physics_solvers.cpp
//
// Numerical solvers for physics equations.

// src/PhysicsSolvers.cpp

#include "plc_emulator/physics/physics_engine.h"
#include "plc_emulator/physics/physics_network.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <iostream>

namespace plc {

namespace {
constexpr int kMaxComponents = PhysicsEngineDefaults::DEFAULT_MAX_COMPONENTS;
constexpr int kMaxElectricalNodes = kMaxComponents * 32;
constexpr int kMaxElectricalEdges = kMaxComponents * 50;
constexpr int kMaxPneumaticNodes = kMaxComponents * 16;
constexpr int kMaxPneumaticEdges = kMaxComponents * 30;
constexpr int kMaxMechanicalNodes = kMaxComponents * 8;
constexpr int kMaxMechanicalEdges = kMaxComponents * 20;

void LogFixedBufferLimitOnce(const char* label, int nodeCount, int maxNodes, int edgeCount, int maxEdges) {
  static bool loggedElectrical = false;
  static bool loggedPneumatic = false;
  static bool loggedMechanical = false;
  bool* logged = nullptr;

  if (std::strcmp(label, "Electrical network") == 0) {
    logged = &loggedElectrical;
  } else if (std::strcmp(label, "Pneumatic network") == 0) {
    logged = &loggedPneumatic;
  } else if (std::strcmp(label, "Mechanical system") == 0) {
    logged = &loggedMechanical;
  }
  if (logged && *logged) {
    return;
  }

  std::cout << "[PhysicsSolver] " << label
            << " exceeds fixed buffer (nodes=" << nodeCount << "/"
            << maxNodes << ", edges=" << edgeCount << "/" << maxEdges
            << ")." << std::endl;

  char message[256];
  std::snprintf(message, sizeof(message),
                "[PhysicsSolver] %s exceeds fixed buffer (nodes=%d/%d, edges=%d/%d).",
                label, nodeCount, maxNodes, edgeCount, maxEdges);
  NotifyPhysicsWarning(message);

  if (logged)
    *logged = true;
}

}  // namespace

namespace MathUtils {

float DotProduct(const float* a, const float* b, int n) {
  float result = 0.0f;
  for (int i = 0; i < n; i++) {
    result += a[i] * b[i];
  }
  return result;
}
float VectorNorm(const float* x, int n) {
  return std::sqrt(DotProduct(x, x, n));
}

void MatrixVectorMultiply(float** A, const float* x, float* y, int rows,
                          int cols) {
  for (int i = 0; i < rows; i++) {
    y[i] = 0.0f;
    for (int j = 0; j < cols; j++) {
      y[i] += A[i][j] * x[j];
    }
  }
}

void VectorCopy(const float* src, float* dest, int n) {
  for (int i = 0; i < n; i++) {
    dest[i] = src[i];
  }
}

void VectorScale(float* x, float alpha, int n) {
  for (int i = 0; i < n; i++) {
    x[i] *= alpha;
  }
}

void VectorAdd(const float* x, const float* y, float* z, int n) {
  for (int i = 0; i < n; i++) {
    z[i] = x[i] + y[i];
  }
}

void VectorSubtract(const float* x, const float* y, float* z, int n) {
  for (int i = 0; i < n; i++) {
    z[i] = x[i] - y[i];
  }
}

}
    // namespace MathUtils



class ElectricalSolver {
 public:
  static int SolveElectricalNetwork(ElectricalNetwork* network,
                                    float deltaTime) {
    (void)deltaTime;
    if (!network || network->nodeCount == 0)
      return -1;
    if (network->nodeCount > kMaxElectricalNodes ||
        network->edgeCount > kMaxElectricalEdges) {
      LogFixedBufferLimitOnce("Electrical network", network->nodeCount,
                              kMaxElectricalNodes, network->edgeCount,
                              kMaxElectricalEdges);
      return -1;
    }
    UpdateConductanceMatrix(network);
    UpdateCurrentVector(network);

    if (!network->solverVoltageOld ||
        network->solverVoltageCapacity < network->nodeCount) {
      return -1;
    }
    float* voltageOld = network->solverVoltageOld;
    MathUtils::VectorCopy(network->voltageVector, voltageOld, network->nodeCount);

    network->currentIteration = 0;
    network->isConverged = false;

    while (network->currentIteration < network->maxIterations &&
           !network->isConverged) {
      for (int i = 0; i < network->nodeCount; i++) {
        if (i == network->groundNodeId) {
          network->voltageVector[i] = 0.0f;
          continue;
        }
        bool isVoltageSource = false;
        for (int j = 0; j < network->voltageSourceCount; j++) {
          if (network->voltageSourceNodeIds[j] == i) {
            network->voltageVector[i] = network->nodes[i].sourceVoltage;
            isVoltageSource = true;
            break;
          }
        }
        if (isVoltageSource)
          continue;

        float sum = 0.0f;
        for (int j = 0; j < network->nodeCount; j++) {
          if (i != j) {
            sum += network->conductanceMatrix[i][j] * network->voltageVector[j];
          }
        }

        if (std::abs(network->conductanceMatrix[i][i]) > 1e-12f) {
          network->voltageVector[i] = (network->currentVector[i] - sum) /
                                      network->conductanceMatrix[i][i];
        } else {
          network->voltageVector[i] = 0.0f;
        }
      }
      float residual = 0.0f;
      for (int i = 0; i < network->nodeCount; i++) {
        float diff = network->voltageVector[i] - voltageOld[i];
        residual += diff * diff;
        voltageOld[i] = network->voltageVector[i];
      }
      residual = std::sqrt(residual);

      network->isConverged = (residual < network->convergenceTolerance);
      network->currentIteration++;

      if (network->currentIteration % 10 == 0) {
        std::cout << "Electrical iteration " << network->currentIteration
                  << ", residual = " << residual << std::endl;
      }
    }
    if (network->isConverged) {
      CalculateEdgeCurrents(network);
      CalculateNodeCurrents(network);
    }

    return network->isConverged ? 0 : -1;
  }

 private:
  static void UpdateConductanceMatrix(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      for (int j = 0; j < network->nodeCount; j++) {
        network->conductanceMatrix[i][j] = 0.0f;
      }
    }
    for (int e = 0; e < network->edgeCount; e++) {
      ElectricalEdge* edge = &network->edges[e];
      if (!edge->base.isActive)
        continue;

      int i = edge->base.fromNodeId;
      int j = edge->base.toNodeId;
      float g = edge->conductance;

      if (i >= 0 && i < network->nodeCount && j >= 0 &&
          j < network->nodeCount) {
        network->conductanceMatrix[i][j] -= g;
        network->conductanceMatrix[j][i] -= g;

        network->conductanceMatrix[i][i] += g;
        network->conductanceMatrix[j][j] += g;
      }
    }
    for (int i = 0; i < network->nodeCount; i++) {
      network->conductanceMatrix[i][i] += network->nodes[i].leakageConductance;
    }
  }
  static void UpdateCurrentVector(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      network->currentVector[i] = 0.0f;

      if (network->nodes[i].isCurrentSource) {
        network->currentVector[i] += network->nodes[i].sourceCurrent;
      }
      if (network->nodes[i].isVoltageSource &&
          network->nodes[i].sourceResistance > 0) {
        float g_internal = 1.0f / network->nodes[i].sourceResistance;
        network->currentVector[i] +=
            g_internal * network->nodes[i].sourceVoltage;
        network->conductanceMatrix[i][i] += g_internal;
      }
    }
  }
  static void CalculateEdgeCurrents(ElectricalNetwork* network) {
    for (int e = 0; e < network->edgeCount; e++) {
      ElectricalEdge* edge = &network->edges[e];
      if (!edge->base.isActive)
        continue;

      int fromNode = edge->base.fromNodeId;
      int toNode = edge->base.toNodeId;

      if (fromNode >= 0 && fromNode < network->nodeCount && toNode >= 0 &&
          toNode < network->nodeCount) {
        float voltageDrop =
            network->voltageVector[fromNode] - network->voltageVector[toNode];
        edge->current = voltageDrop * edge->conductance;
        edge->voltageDrop = voltageDrop;
        edge->powerDissipation =
            edge->current * edge->current * edge->resistance;

        edge->currentHistory[2] = edge->currentHistory[1];
        edge->currentHistory[1] = edge->currentHistory[0];
        edge->currentHistory[0] = edge->current;
      }
    }
  }
  static void CalculateNodeCurrents(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      network->nodes[i].netCurrent = 0.0f;

      for (int e = 0; e < network->edgeCount; e++) {
        if (!network->edges[e].base.isActive)
          continue;

        if (network->edges[e].base.fromNodeId == i) {
          network->nodes[i].netCurrent -= network->edges[e].current;
        } else if (network->edges[e].base.toNodeId == i) {
          network->nodes[i].netCurrent += network->edges[e].current;
        }
      }
      if (network->nodes[i].isCurrentSource) {
        network->nodes[i].netCurrent += network->nodes[i].sourceCurrent;
      }

      network->nodes[i].current = network->nodes[i].netCurrent;

      network->nodes[i].voltageHistory[2] = network->nodes[i].voltageHistory[1];
      network->nodes[i].voltageHistory[1] = network->nodes[i].voltageHistory[0];
      network->nodes[i].voltageHistory[0] = network->nodes[i].voltage;

      network->nodes[i].currentHistory[2] = network->nodes[i].currentHistory[1];
      network->nodes[i].currentHistory[1] = network->nodes[i].currentHistory[0];
      network->nodes[i].currentHistory[0] = network->nodes[i].current;

      network->nodes[i].voltage = network->voltageVector[i];
    }
  }
};



class PneumaticSolver {
 public:
  static int SolvePneumaticNetwork(PneumaticNetwork* network, float deltaTime) {
    (void)deltaTime;
    if (!network || network->nodeCount == 0)
      return -1;
    if (network->nodeCount > kMaxPneumaticNodes ||
        network->edgeCount > kMaxPneumaticEdges) {
      LogFixedBufferLimitOnce("Pneumatic network", network->nodeCount,
                              kMaxPneumaticNodes, network->edgeCount,
                              kMaxPneumaticEdges);
      return -1;
    }
    InitializeGuess(network);

    network->currentIteration = 0;
    network->isConverged = false;
    int n = network->nodeCount + network->edgeCount;
    if (!network->solverResidualBuffer || !network->solverDeltaBuffer ||
        !network->solverJacobianRows || !network->solverJacobianBuffer ||
        network->solverBufferSize < n) {
      return -1;
    }
    float* residual = network->solverResidualBuffer;
    float* delta = network->solverDeltaBuffer;
    float** jacobian = network->solverJacobianRows;

    while (network->currentIteration < network->maxIterations &&
           !network->isConverged) {
      CalculateResidualAndJacobian(network, residual, jacobian);

      SolveLinearSystem(jacobian, residual, delta,
                        network->nodeCount + network->edgeCount);

      UpdateSolution(network, delta);

      float residualNorm = MathUtils::VectorNorm(
          residual, network->nodeCount + network->edgeCount);
      network->isConverged = (residualNorm < network->convergenceTolerance);
      network->currentIteration++;

      if (network->currentIteration % 5 == 0) {
        std::cout << "Pneumatic iteration " << network->currentIteration
                  << ", residual = " << residualNorm << std::endl;
      }
    }

    if (network->isConverged) {
      CalculateDerivedQuantities(network);
    }

    return network->isConverged ? 0 : -1;
  }

 private:
  static void InitializeGuess(PneumaticNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      if (!network->nodes[i].isPressureSource) {
        if (network->nodes[i].pressureHistory[0] > 0) {
          network->nodes[i].pressure = network->nodes[i].pressureHistory[0];
        } else {
          network->nodes[i].pressure = network->atmosphericPressure;
        }
      } else {
        network->nodes[i].pressure = network->nodes[i].sourcePressure;
      }

      network->pressureVector[i] = network->nodes[i].pressure;
    }

    for (int e = 0; e < network->edgeCount; e++) {
      network->edges[e].massFlowRate = 0.0f;
      network->massFlowVector[e] = 0.0f;
    }
  }
  static void CalculateResidualAndJacobian(PneumaticNetwork* network,
                                           float* residual, float** jacobian) {
    int n = network->nodeCount + network->edgeCount;

    for (int i = 0; i < n; i++) {
      residual[i] = 0.0f;
      for (int j = 0; j < n; j++) {
        jacobian[i][j] = 0.0f;
      }
    }
    for (int i = 0; i < network->nodeCount; i++) {
      float massBalance = 0.0f;

      if (network->nodes[i].isMassFlowSource) {
        massBalance += network->nodes[i].sourceMassFlowRate;
      }
      for (int e = 0; e < network->edgeCount; e++) {
        if (!network->edges[e].base.isActive)
          continue;

        if (network->edges[e].base.fromNodeId == i) {
          massBalance -= network->edges[e].massFlowRate;
          jacobian[i][network->nodeCount + e] = -1.0f;
        } else if (network->edges[e].base.toNodeId == i) {
          massBalance += network->edges[e].massFlowRate;
          jacobian[i][network->nodeCount + e] = 1.0f;
        }
      }

      residual[i] = massBalance;
    }
    for (int e = 0; e < network->edgeCount; e++) {
      if (!network->edges[e].base.isActive)
        continue;

      int fromNode = network->edges[e].base.fromNodeId;
      int toNode = network->edges[e].base.toNodeId;

      if (fromNode >= 0 && fromNode < network->nodeCount && toNode >= 0 &&
          toNode < network->nodeCount) {
        float P1 = network->pressureVector[fromNode];
        float P2 = network->pressureVector[toNode];
        float mdot = network->massFlowVector[e];

        float pressureDrop = CalculatePressureDrop(&network->edges[e], mdot);

        residual[network->nodeCount + e] = P1 - P2 - pressureDrop;

        jacobian[network->nodeCount + e][fromNode] = 1.0f;
        jacobian[network->nodeCount + e][toNode] = -1.0f;

        float dPdm = CalculatePressureDropDerivative(&network->edges[e], mdot);
        jacobian[network->nodeCount + e][network->nodeCount + e] = -dPdm;
      }
    }
  }
  static float CalculatePressureDrop(PneumaticEdge* edge, float massFlowRate) {
    if (std::abs(massFlowRate) < 1e-9f)
      return 0.0f;

    float A = edge->crossSectionArea * 1e-6f;
    float D = edge->diameter * 1e-3f;          // mm ??m
    float L = edge->length * 1e-3f;            // mm ??m

    float rho = 1.225f;

    float velocity = std::abs(massFlowRate) / (rho * A);

    float mu = 1.8e-5f;
    float Re = rho * velocity * D / mu;
    edge->reynoldsNumber = Re;

    float roughness = edge->roughness * 1e-6f;
    float f;
    if (Re < 2300) {
      f = 64.0f / Re;
    } else {
      float term1 =
          std::log10(roughness / (3.7f * D) + 5.74f / std::pow(Re, 0.9f));
      f = 0.25f / (term1 * term1);
    }
    edge->frictionFactor = f;

    float frictionLoss = f * (L / D) * (rho * velocity * velocity / 2.0f);

    float localLoss = edge->curvature * (rho * velocity * velocity / 2.0f);

    float totalLoss = frictionLoss + localLoss;

    return (massFlowRate >= 0) ? totalLoss : -totalLoss;
  }
  static float CalculatePressureDropDerivative(PneumaticEdge* edge,
                                               float massFlowRate) {
    if (std::abs(massFlowRate) < 1e-9f) {
      float A = edge->crossSectionArea * 1e-6f;
      float rho = 1.225f;
      return 2.0f * edge->frictionFactor * edge->length /
             (edge->diameter * rho * A * A);
    } else {
      float currentDrop = CalculatePressureDrop(edge, massFlowRate);
      return 2.0f * currentDrop / massFlowRate;
    }
  }
  static void SolveLinearSystem(float** A, float* b, float* x, int n) {

    for (int i = 0; i < n; i++) {
      x[i] = -b[i];
    }
    for (int k = 0; k < n - 1; k++) {
      for (int i = k + 1; i < n; i++) {
        if (std::abs(A[k][k]) < 1e-12f)
          continue;

        float factor = A[i][k] / A[k][k];
        for (int j = k + 1; j < n; j++) {
          A[i][j] -= factor * A[k][j];
        }
        x[i] -= factor * x[k];
      }
    }
    for (int i = n - 1; i >= 0; i--) {
      for (int j = i + 1; j < n; j++) {
        x[i] -= A[i][j] * x[j];
      }
      if (std::abs(A[i][i]) > 1e-12f) {
        x[i] /= A[i][i];
      } else {
        x[i] = 0.0f;
      }
    }
  }
  static void UpdateSolution(PneumaticNetwork* network, float* delta) {
    for (int i = 0; i < network->nodeCount; i++) {
      if (!network->nodes[i].isPressureSource) {
        network->pressureVector[i] += delta[i];

        if (network->pressureVector[i] < 0.1f * network->atmosphericPressure) {
          network->pressureVector[i] = 0.1f * network->atmosphericPressure;
        }

        network->nodes[i].pressure = network->pressureVector[i];
        network->nodes[i].gaugePressure =
            network->pressureVector[i] - network->atmosphericPressure;
      }
    }
    for (int e = 0; e < network->edgeCount; e++) {
      network->massFlowVector[e] += delta[network->nodeCount + e];
      network->edges[e].massFlowRate = network->massFlowVector[e];
    }
  }
  static void CalculateDerivedQuantities(PneumaticNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      PneumaticNode* node = &network->nodes[i];

      node->density =
          node->pressure / (network->gasConstant * node->temperature);

      node->soundSpeed = std::sqrt(network->specificHeatRatio *
                                   network->gasConstant * node->temperature);

      node->pressureHistory[2] = node->pressureHistory[1];
      node->pressureHistory[1] = node->pressureHistory[0];
      node->pressureHistory[0] = node->pressure;
    }

    for (int e = 0; e < network->edgeCount; e++) {
      PneumaticEdge* edge = &network->edges[e];

      float rho_std = 1.225f;
      edge->volumeFlowRate =
          edge->massFlowRate / rho_std * 60000.0f;

      float A = edge->crossSectionArea * 1e-6f;
      edge->velocity = edge->massFlowRate / (edge->density * A);

      edge->massFlowHistory[2] = edge->massFlowHistory[1];
      edge->massFlowHistory[1] = edge->massFlowHistory[0];
      edge->massFlowHistory[0] = edge->massFlowRate;
    }
  }
};



class MechanicalSolver {
 public:
  static int SolveMechanicalSystem(MechanicalSystem* system, float deltaTime) {
    (void)deltaTime;
    if (!system || system->nodeCount == 0)
      return -1;
    if (system->nodeCount > kMaxMechanicalNodes ||
        system->edgeCount > kMaxMechanicalEdges) {
      LogFixedBufferLimitOnce("Mechanical system", system->nodeCount,
                              kMaxMechanicalNodes, system->edgeCount,
                              kMaxMechanicalEdges);
      return -1;
    }

    int dof = 6 * system->nodeCount;
    if (!system->solverState || !system->solverK1 || !system->solverK2 ||
        !system->solverK3 || !system->solverK4 || !system->solverTempState ||
        !system->solverKq || !system->solverCqdot || !system->solverRhs ||
        system->solverDofCapacity < dof) {
      return -1;
    }

    UpdateSystemMatrices(system);

    float* state = system->solverState;  // [q, q?]
    float* k1 = system->solverK1;
    float* k2 = system->solverK2;
    float* k3 = system->solverK3;
    float* k4 = system->solverK4;
    float* temp_state = system->solverTempState;

    PackStateVector(system, state);

    CalculateStateDerivative(system, state, k1);

    for (int i = 0; i < 2 * dof; i++) {
      temp_state[i] = state[i] + deltaTime * k1[i] / 2.0f;
    }
    CalculateStateDerivative(system, temp_state, k2);

    for (int i = 0; i < 2 * dof; i++) {
      temp_state[i] = state[i] + deltaTime * k2[i] / 2.0f;
    }
    CalculateStateDerivative(system, temp_state, k3);

    for (int i = 0; i < 2 * dof; i++) {
      temp_state[i] = state[i] + deltaTime * k3[i];
    }
    CalculateStateDerivative(system, temp_state, k4);

    for (int i = 0; i < 2 * dof; i++) {
      state[i] +=
          deltaTime / 6.0f * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
    }
    UnpackStateVector(system, state);

    system->currentTime += deltaTime;
    system->currentStep++;

    CheckSystemStability(system);

    return 0;
  }

 private:
  static void UpdateSystemMatrices(MechanicalSystem* system) {
    int total_dof = 6 * system->nodeCount;

    for (int i = 0; i < total_dof; i++) {
      for (int j = 0; j < total_dof; j++) {
        system->massMatrix[i][j] = 0.0f;
        system->dampingMatrix[i][j] = 0.0f;
        system->stiffnessMatrix[i][j] = 0.0f;
      }
    }
    for (int n = 0; n < system->nodeCount; n++) {
      MechanicalNode* node = &system->nodes[n];
      int base_idx = 6 * n;

      for (int i = 0; i < 3; i++) {
        system->massMatrix[base_idx + i][base_idx + i] = node->mass;
      }
      for (int i = 0; i < 3; i++) {
        system->massMatrix[base_idx + 3 + i][base_idx + 3 + i] =
            node->momentOfInertia[i];
      }
    }
    for (int e = 0; e < system->edgeCount; e++) {
      if (!system->edges[e].base.isActive)
        continue;

      MechanicalEdge* edge = &system->edges[e];
      int node1 = edge->base.fromNodeId;
      int node2 = edge->base.toNodeId;

      if (node1 >= 0 && node1 < system->nodeCount && node2 >= 0 &&
          node2 < system->nodeCount) {
        int idx1 = 6 * node1;
        int idx2 = 6 * node2;

        for (int dof_idx = 0; dof_idx < 6; dof_idx++) {
          float k = edge->springConstant[dof_idx];
          float c = edge->dampingCoefficient[dof_idx];

          system->stiffnessMatrix[idx1 + dof_idx][idx1 + dof_idx] += k;
          system->stiffnessMatrix[idx2 + dof_idx][idx2 + dof_idx] += k;
          system->dampingMatrix[idx1 + dof_idx][idx1 + dof_idx] += c;
          system->dampingMatrix[idx2 + dof_idx][idx2 + dof_idx] += c;

          system->stiffnessMatrix[idx1 + dof_idx][idx2 + dof_idx] -= k;
          system->stiffnessMatrix[idx2 + dof_idx][idx1 + dof_idx] -= k;
          system->dampingMatrix[idx1 + dof_idx][idx2 + dof_idx] -= c;
          system->dampingMatrix[idx2 + dof_idx][idx1 + dof_idx] -= c;
        }
      }
    }
  }
  static void PackStateVector(MechanicalSystem* system, float* state) {
    int dof = 6 * system->nodeCount;

    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        state[base_idx + i] = node->position[i];
        state[base_idx + 3 + i] = node->angularPosition[i];
      }
    }
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = dof + 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        state[base_idx + i] = node->velocity[i];
        state[base_idx + 3 + i] = node->angularVelocity[i];
      }
    }
  }
  static void UnpackStateVector(MechanicalSystem* system, const float* state) {
    int dof = 6 * system->nodeCount;

    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->position[i] = state[base_idx + i];
        node->angularPosition[i] = state[base_idx + 3 + i];
      }
    }
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = dof + 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->velocity[i] = state[base_idx + i];
        node->angularVelocity[i] = state[base_idx + 3 + i];
      }
    }
  }

  static void CalculateStateDerivative(MechanicalSystem* system,
                                       const float* state, float* derivative) {
    int dof = 6 * system->nodeCount;

    for (int i = 0; i < dof; i++) {
      derivative[i] = state[dof + i];
    }
    UpdateForceVector(system);

    float* Kq = system->solverKq;  // K * q
    float* Cqdot = system->solverCqdot;  // C * q?
    float* rhs = system->solverRhs;
    MathUtils::MatrixVectorMultiply(system->stiffnessMatrix, state, Kq, dof,
                                    dof);

    MathUtils::MatrixVectorMultiply(system->dampingMatrix, &state[dof], Cqdot,
                                    dof, dof);

    for (int i = 0; i < dof; i++) {
      rhs[i] = system->forceVector[i] - Cqdot[i] - Kq[i];
    }

    SolveMassMatrixSystem(system, rhs, &derivative[dof]);

  }
  static void UpdateForceVector(MechanicalSystem* system) {
    int dof = 6 * system->nodeCount;

    for (int i = 0; i < dof; i++) {
      system->forceVector[i] = 0.0f;
    }
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        system->forceVector[base_idx + i] = node->appliedForce[i];
        system->forceVector[base_idx + 3 + i] = node->appliedMoment[i];
      }
      for (int i = 0; i < 3; i++) {
        system->forceVector[base_idx + i] += node->mass * system->gravity[i];
      }
    }
    for (int e = 0; e < system->edgeCount; e++) {
      if (!system->edges[e].base.isActive)
        continue;

      CalculateEdgeForces(system, &system->edges[e]);
    }
  }
  static void CalculateEdgeForces(MechanicalSystem* system,
                                  MechanicalEdge* edge) {
    int node1 = edge->base.fromNodeId;
    int node2 = edge->base.toNodeId;

    if (node1 < 0 || node1 >= system->nodeCount || node2 < 0 ||
        node2 >= system->nodeCount)
      return;

    MechanicalNode* n1 = &system->nodes[node1];
    MechanicalNode* n2 = &system->nodes[node2];

    int idx1 = 6 * node1;
    int idx2 = 6 * node2;

    for (int dof = 0; dof < 6; dof++) {
      float pos1 = (dof < 3) ? n1->position[dof] : n1->angularPosition[dof - 3];
      float pos2 = (dof < 3) ? n2->position[dof] : n2->angularPosition[dof - 3];
      float vel1 = (dof < 3) ? n1->velocity[dof] : n1->angularVelocity[dof - 3];
      float vel2 = (dof < 3) ? n2->velocity[dof] : n2->angularVelocity[dof - 3];

      float displacement = pos1 - pos2 - edge->naturalLength[dof];
      float relativeVelocity = vel1 - vel2;

      float springForce = -edge->springConstant[dof] * displacement;

      float damperForce = -edge->dampingCoefficient[dof] * relativeVelocity;

      float totalForce = springForce + damperForce;

      system->forceVector[idx1 + dof] += totalForce;
      system->forceVector[idx2 + dof] -= totalForce;

      edge->displacement[dof] = displacement;
      edge->relativeVelocity[dof] = relativeVelocity;
      edge->springForce[dof] = springForce;
      edge->damperForce[dof] = damperForce;
      edge->totalForce[dof] = totalForce;
    }
  }
  static void SolveMassMatrixSystem(MechanicalSystem* system, const float* rhs,
                                    float* acceleration) {
    int dof = 6 * system->nodeCount;

    for (int i = 0; i < dof; i++) {
      if (std::abs(system->massMatrix[i][i]) > 1e-12f) {
        acceleration[i] = rhs[i] / system->massMatrix[i][i];
      } else {
        acceleration[i] = 0.0f;
      }
    }
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->acceleration[i] = acceleration[base_idx + i];
        node->angularAcceleration[i] = acceleration[base_idx + 3 + i];
      }
    }
  }
  static void CheckSystemStability(MechanicalSystem* system) {
    system->isStable = true;

    for (int n = 0; n < system->nodeCount; n++) {
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        if (std::abs(node->position[i]) > 1e6f ||
            std::abs(node->velocity[i]) > 1e6f) {
          system->isStable = false;
          std::cout << "Warning: System instability detected at node " << n
                    << std::endl;

          if (std::abs(node->velocity[i]) > 1e6f) {
            node->velocity[i] = std::copysign(1e6f, node->velocity[i]);
          }
        }
      }
    }
  }
};

}
    // namespace plc


extern "C" {
int SolveElectricalNetworkC(plc::ElectricalNetwork* network, float deltaTime) {
  return plc::ElectricalSolver::SolveElectricalNetwork(network, deltaTime);
}
int SolvePneumaticNetworkC(plc::PneumaticNetwork* network, float deltaTime) {
  return plc::PneumaticSolver::SolvePneumaticNetwork(network, deltaTime);
}
int SolveMechanicalSystemC(plc::MechanicalSystem* system, float deltaTime) {
  return plc::MechanicalSolver::SolveMechanicalSystem(system, deltaTime);
}
}
