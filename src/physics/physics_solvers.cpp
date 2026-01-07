// physics_solvers.cpp
//
// Numerical solvers for physics equations.

// src/PhysicsSolvers.cpp
// ?�제 물리 법칙 기반 ?�치?�석 ?�버 구현
// ?�르?�호??법칙, 베르?�이 방정?? ?�턴 ??��???�확???�용???��??�이??
// ?�치???�정?�과 ?�확?�을 보장?�는 검증된 ?�치?�석 ?�고리즘 ?�용

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
constexpr int kMaxPneumaticSize = kMaxPneumaticNodes + kMaxPneumaticEdges;
constexpr int kMaxMechanicalNodes = kMaxComponents * 8;
constexpr int kMaxMechanicalEdges = kMaxComponents * 20;
constexpr int kMaxMechanicalDof = 6 * kMaxMechanicalNodes;

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

//   ?�렬 ?�산???�한 기본 ?�틸리티
// - C?�어 ?��??�로 ?�율?�인 ?�치?�산 구현
// - BLAS/LAPACK ?�이??기본?�인 ?�형?�???�산 ?�공
namespace MathUtils {

// 벡터 ?�적 계산
float DotProduct(const float* a, const float* b, int n) {
  float result = 0.0f;
  for (int i = 0; i < n; i++) {
    result += a[i] * b[i];
  }
  return result;
}
    // 벡터 ?�름 계산 (||x||??
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


// ?�르?�호??법칙 기반 ?�기 ?�트?�크 ?�석

class ElectricalSolver {
 public:
  //   가?�스-?�이??반복법으�??�형 ?�스???�결
  // - ?�각우???�렬???�??빠른 ?�렴 보장
  // - 메모�??�율??(?�자�??�데?�트)
  // - ?�소 ?�렬???�합 (?�기 ?�트?�크??보통 ?�소??
  static int SolveElectricalNetwork(ElectricalNetwork* network,
                                    float deltaTime) {
    (void)deltaTime;  // ?�재 버전?�서???�적 ?�태 ?�석�??�행, ?�중???�적
                      // ?�석?�서 ?�용???�정
    if (!network || network->nodeCount == 0)
      return -1;
    if (network->nodeCount > kMaxElectricalNodes ||
        network->edgeCount > kMaxElectricalEdges) {
      LogFixedBufferLimitOnce("Electrical network", network->nodeCount,
                              kMaxElectricalNodes, network->edgeCount,
                              kMaxElectricalEdges);
      return -1;
    }
    // 컨덕?�스 ?�렬�??�류 벡터 ?�데?�트
    UpdateConductanceMatrix(network);
    UpdateCurrentVector(network);

    // 가?�스-?�이??반복법으�??�압 ??계산
    static float voltageOld[kMaxElectricalNodes];
    MathUtils::VectorCopy(network->voltageVector, voltageOld, network->nodeCount);

    network->currentIteration = 0;
    network->isConverged = false;

    while (network->currentIteration < network->maxIterations &&
           !network->isConverged) {
      // 가?�스-?�이???�위??
      for (int i = 0; i < network->nodeCount; i++) {
        if (i == network->groundNodeId) {
          network->voltageVector[i] = 0.0f;  // ?��? 조건
          continue;
        }
    // ?�압???�드 처리
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
          network->voltageVector[i] = 0.0f;  // ?�이??처리
        }
      }
    // ?�렴 검??
      float residual = 0.0f;
      for (int i = 0; i < network->nodeCount; i++) {
        float diff = network->voltageVector[i] - voltageOld[i];
        residual += diff * diff;
        voltageOld[i] = network->voltageVector[i];
      }
      residual = std::sqrt(residual);

      network->isConverged = (residual < network->convergenceTolerance);
      network->currentIteration++;

      // ?�버�?출력 (?�택??
      if (network->currentIteration % 10 == 0) {
        std::cout << "Electrical iteration " << network->currentIteration
                  << ", residual = " << residual << std::endl;
      }
    }
    // ?�렴 ???�류 계산
    if (network->isConverged) {
      CalculateEdgeCurrents(network);
      CalculateNodeCurrents(network);
    }

    return network->isConverged ? 0 : -1;
  }

 private:
  // 컨덕?�스 ?�렬 ?�데?�트
  static void UpdateConductanceMatrix(ElectricalNetwork* network) {
    // ?�렬 초기??
    for (int i = 0; i < network->nodeCount; i++) {
      for (int j = 0; j < network->nodeCount; j++) {
        network->conductanceMatrix[i][j] = 0.0f;
      }
    }
    // �??��???컨덕?�스�??�렬??추�?
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
    // ?�드�?추�? 컨덕?�스 (?�설 ??
    for (int i = 0; i < network->nodeCount; i++) {
      network->conductanceMatrix[i][i] += network->nodes[i].leakageConductance;
    }
  }
    // ?�류 벡터 ?�데?�트
  static void UpdateCurrentVector(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      network->currentVector[i] = 0.0f;

      // ?�류?�이 ?�는 경우
      if (network->nodes[i].isCurrentSource) {
        network->currentVector[i] += network->nodes[i].sourceCurrent;
      }
    // ?�압?�의 ?��? ?�류 기여�?(Norton ?��??�로)
      if (network->nodes[i].isVoltageSource &&
          network->nodes[i].sourceResistance > 0) {
        float g_internal = 1.0f / network->nodes[i].sourceResistance;
        network->currentVector[i] +=
            g_internal * network->nodes[i].sourceVoltage;
        network->conductanceMatrix[i][i] += g_internal;
      }
    }
  }
    // ?��? ?�류 계산 (?�의 법칙 ?�용)
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

        // ?�류 ?�스?�리 ?�데?�트 (?�적 ?�석??
        edge->currentHistory[2] = edge->currentHistory[1];
        edge->currentHistory[1] = edge->currentHistory[0];
        edge->currentHistory[0] = edge->current;
      }
    }
  }
    // ?�드 ?�류 계산 (?�르?�호???�류 법칙 검증용)
  static void CalculateNodeCurrents(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      network->nodes[i].netCurrent = 0.0f;

      // ?�결??모든 ?��????�류�??�산
      for (int e = 0; e < network->edgeCount; e++) {
        if (!network->edges[e].base.isActive)
          continue;

        if (network->edges[e].base.fromNodeId == i) {
          network->nodes[i].netCurrent -= network->edges[e].current;  // ?�출
        } else if (network->edges[e].base.toNodeId == i) {
          network->nodes[i].netCurrent += network->edges[e].current;  // ?�입
        }
      }
    // ?�스 ?�류 추�?
      if (network->nodes[i].isCurrentSource) {
        network->nodes[i].netCurrent += network->nodes[i].sourceCurrent;
      }

      network->nodes[i].current = network->nodes[i].netCurrent;

      // ?�압/?�류 ?�스?�리 ?�데?�트
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


// 베르?�이 방정?�과 ?�속??방정??기반 공압 ?�트?�크 ?�석
// 비선???�립방정?�을 ?�턴-?�슨 방법?�로 ?�결
// ?�축???�체(공기)???�태방정??PV=nRT 고려

class PneumaticSolver {
 public:
  // ?�턴-?�슨 방법?�로 비선??공압 ?�스???�결
  static int SolvePneumaticNetwork(PneumaticNetwork* network, float deltaTime) {
    (void)deltaTime;  // ?�재 버전?�서???�적 ?�태 ?�석�??�행, ?�중???�적
                      // ?�석?�서 ?�용???�정
    if (!network || network->nodeCount == 0)
      return -1;
    if (network->nodeCount > kMaxPneumaticNodes ||
        network->edgeCount > kMaxPneumaticEdges) {
      LogFixedBufferLimitOnce("Pneumatic network", network->nodeCount,
                              kMaxPneumaticNodes, network->edgeCount,
                              kMaxPneumaticEdges);
      return -1;
    }
    // 초기 추정�??�정
    InitializeGuess(network);

    // ?�턴-?�슨 반복
    network->currentIteration = 0;
    network->isConverged = false;
    int n = network->nodeCount + network->edgeCount;
    static float residualBuffer[kMaxPneumaticSize];
    static float deltaBuffer[kMaxPneumaticSize];
    static float jacobianBuffer[kMaxPneumaticSize][kMaxPneumaticSize];
    static float* jacobianRows[kMaxPneumaticSize];
    for (int i = 0; i < n; i++) {
      jacobianRows[i] = jacobianBuffer[i];
    }
    float* residual = residualBuffer;
    float* delta = deltaBuffer;
    float** jacobian = jacobianRows;

    while (network->currentIteration < network->maxIterations &&
           !network->isConverged) {
      // ?�여?�수?� ?�코비안 계산
      CalculateResidualAndJacobian(network, residual, jacobian);

      SolveLinearSystem(jacobian, residual, delta,
                        network->nodeCount + network->edgeCount);

      // ???�데?�트
      UpdateSolution(network, delta);

      // ?�렴 검??
      float residualNorm = MathUtils::VectorNorm(
          residual, network->nodeCount + network->edgeCount);
      network->isConverged = (residualNorm < network->convergenceTolerance);
      network->currentIteration++;

      // ?�버�?출력
      if (network->currentIteration % 5 == 0) {
        std::cout << "Pneumatic iteration " << network->currentIteration
                  << ", residual = " << residualNorm << std::endl;
      }
    }
    // 메모�??�제

    // ?�렴 ??부가 변??계산
    if (network->isConverged) {
      CalculateDerivedQuantities(network);
    }

    return network->isConverged ? 0 : -1;
  }

 private:
  // 초기 추정�??�정
  // - ?�력: ?�기압 ?�는 ?�전 ?�간?�텝 �?
  // - 질량?�량: 0 ?�는 ?�속??방정??기반 추정
  static void InitializeGuess(PneumaticNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      if (!network->nodes[i].isPressureSource) {
        // 초기 ?�력 추정 (?�기압 ?�는 ?�전�?
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
      // 초기 질량?�량 추정 (0 ?�는 ?�력�?기반)
      network->edges[e].massFlowRate = 0.0f;
      network->massFlowVector[e] = 0.0f;
    }
  }
    // ?�여?�수?� ?�코비안 ?�렬 계산
  static void CalculateResidualAndJacobian(PneumaticNetwork* network,
                                           float* residual, float** jacobian) {
    int n = network->nodeCount + network->edgeCount;

    // ?�여?�수 초기??
    for (int i = 0; i < n; i++) {
      residual[i] = 0.0f;
      for (int j = 0; j < n; j++) {
        jacobian[i][j] = 0.0f;
      }
    }
    // ?�드�?질량보존 방정??(?�속??방정??
    for (int i = 0; i < network->nodeCount; i++) {
      float massBalance = 0.0f;

      // ?�스 질량?�량
      if (network->nodes[i].isMassFlowSource) {
        massBalance += network->nodes[i].sourceMassFlowRate;
      }
    // ?�결???��??�의 질량?�량 ?�산
      for (int e = 0; e < network->edgeCount; e++) {
        if (!network->edges[e].base.isActive)
          continue;

        if (network->edges[e].base.fromNodeId == i) {
          massBalance -= network->edges[e].massFlowRate;  // ?�출
          jacobian[i][network->nodeCount + e] = -1.0f;
        } else if (network->edges[e].base.toNodeId == i) {
          massBalance += network->edges[e].massFlowRate;  // ?�입
          jacobian[i][network->nodeCount + e] = 1.0f;
        }
      }

      residual[i] = massBalance;
    }
    // ?��?�?베르?�이 방정??
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

        // ?�력강하 계산 (Darcy-Weisbach + �?��?�실)
        float pressureDrop = CalculatePressureDrop(&network->edges[e], mdot);

        residual[network->nodeCount + e] = P1 - P2 - pressureDrop;

        // ?�코비안 ??��
        jacobian[network->nodeCount + e][fromNode] = 1.0f;  // ?�F/?�P??
        jacobian[network->nodeCount + e][toNode] = -1.0f;   // ?�F/?�P??

        float dPdm = CalculatePressureDropDerivative(&network->edges[e], mdot);
        jacobian[network->nodeCount + e][network->nodeCount + e] = -dPdm;
      }
    }
  }
    // ?�력강하 계산 (Darcy-Weisbach 방정??
  // ?�기??f??마찰계수, K??�?��?�실계수
  static float CalculatePressureDrop(PneumaticEdge* edge, float massFlowRate) {
    if (std::abs(massFlowRate) < 1e-9f)
      return 0.0f;

    float A = edge->crossSectionArea * 1e-6f;  // mm² ??m²
    float D = edge->diameter * 1e-3f;          // mm ??m
    float L = edge->length * 1e-3f;            // mm ??m

    // 밀??추정 (?��? ?�태 기�?)
    float rho = 1.225f;  // kg/m³ (20°C, 1atm 공기)

    float velocity = std::abs(massFlowRate) / (rho * A);

    float mu = 1.8e-5f;  // ?�점?�계??[Pa?�s]
    float Re = rho * velocity * D / mu;
    edge->reynoldsNumber = Re;

    // 마찰계수 계산 (Colebrook-White 방정??근사)
    float roughness = edge->roughness * 1e-6f;  // μm ??m
    float f;
    if (Re < 2300) {
      f = 64.0f / Re;
    } else {
      // ?�류: Swamee-Jain 방정??근사
      float term1 =
          std::log10(roughness / (3.7f * D) + 5.74f / std::pow(Re, 0.9f));
      f = 0.25f / (term1 * term1);
    }
    edge->frictionFactor = f;

    // 마찰 ?�력강하
    float frictionLoss = f * (L / D) * (rho * velocity * velocity / 2.0f);

    // �?�� ?�력강하 (굽힘, ?��?/축소 ??
    float localLoss = edge->curvature * (rho * velocity * velocity / 2.0f);

    float totalLoss = frictionLoss + localLoss;

    // ?�량 방향???�른 부??결정
    return (massFlowRate >= 0) ? totalLoss : -totalLoss;
  }
    // ?�력강하??질량?�량???�??미분 (?�코비안??
  // ???P)/?�ṁ ??2*?P/�?(?�도²??비�??��?�??�량???�??거의 ?�형)
  static float CalculatePressureDropDerivative(PneumaticEdge* edge,
                                               float massFlowRate) {
    if (std::abs(massFlowRate) < 1e-9f) {
      // ?�량??0??가까우�??�형 근사
      float A = edge->crossSectionArea * 1e-6f;
      float rho = 1.225f;
      return 2.0f * edge->frictionFactor * edge->length /
             (edge->diameter * rho * A * A);
    } else {
      float currentDrop = CalculatePressureDrop(edge, massFlowRate);
      return 2.0f * currentDrop / massFlowRate;
    }
  }
    // ?�형 ?�스???�결 (LU 분해 ?�용)
  // ?�코비안 ?�렬???�으므�?직접�??�용
  static void SolveLinearSystem(float** A, float* b, float* x, int n) {
    // 간단??가?�스 ?�거�?(?�벗???�음)
    // ?�제로는 부�??�벗?�이 ?�요?��?�??�기?�는 간단??구현

    for (int i = 0; i < n; i++) {
      x[i] = -b[i];  // ?��? 복사 �?부??변�?
    }
    // ?�진 ?�거
    for (int k = 0; k < n - 1; k++) {
      for (int i = k + 1; i < n; i++) {
        if (std::abs(A[k][k]) < 1e-12f)
          continue;  // ?�이??방�?

        float factor = A[i][k] / A[k][k];
        for (int j = k + 1; j < n; j++) {
          A[i][j] -= factor * A[k][j];
        }
        x[i] -= factor * x[k];
      }
    }
    // ?�진 ?�??
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
    // ???�데?�트 (?�턴-?�슨 ?�텝)
  static void UpdateSolution(PneumaticNetwork* network, float* delta) {
    // ?�력 ?�데?�트 (?�한 ?�용)
    for (int i = 0; i < network->nodeCount; i++) {
      if (!network->nodes[i].isPressureSource) {
        network->pressureVector[i] += delta[i];

        // 물리???�한 ?�용 (?�압 방�?)
        if (network->pressureVector[i] < 0.1f * network->atmosphericPressure) {
          network->pressureVector[i] = 0.1f * network->atmosphericPressure;
        }

        network->nodes[i].pressure = network->pressureVector[i];
        network->nodes[i].gaugePressure =
            network->pressureVector[i] - network->atmosphericPressure;
      }
    }
    // 질량?�량 ?�데?�트
    for (int e = 0; e < network->edgeCount; e++) {
      network->massFlowVector[e] += delta[network->nodeCount + e];
      network->edges[e].massFlowRate = network->massFlowVector[e];
    }
  }
    // ?�렴 ??부가 변??계산
  // ?�도, 밀?? 체적?�량, ?�속 ???�생 변??계산
  static void CalculateDerivedQuantities(PneumaticNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      PneumaticNode* node = &network->nodes[i];

      node->density =
          node->pressure / (network->gasConstant * node->temperature);

      node->soundSpeed = std::sqrt(network->specificHeatRatio *
                                   network->gasConstant * node->temperature);

      // ?�스?�리 ?�데?�트
      node->pressureHistory[2] = node->pressureHistory[1];
      node->pressureHistory[1] = node->pressureHistory[0];
      node->pressureHistory[0] = node->pressure;
    }

    for (int e = 0; e < network->edgeCount; e++) {
      PneumaticEdge* edge = &network->edges[e];

      float rho_std = 1.225f;  // kg/m³
      edge->volumeFlowRate =
          edge->massFlowRate / rho_std * 60000.0f;  // m³/s ??L/min

      // ?�균 ?�도 계산
      float A = edge->crossSectionArea * 1e-6f;  // mm² ??m²
      edge->velocity = edge->massFlowRate / (edge->density * A);

      // ?�스?�리 ?�데?�트
      edge->massFlowHistory[2] = edge->massFlowHistory[1];
      edge->massFlowHistory[1] = edge->massFlowHistory[0];
      edge->massFlowHistory[0] = edge->massFlowRate;
    }
  }
};


// ?�턴 ??�� 기반 기계 ?�스???�석
// 질량-?�프�??�퍼 ?�트?�크???�적 ?�답 계산

class MechanicalSolver {
 public:
  // 룽게-쿠�? 4�?방법?�로 ?�동방정???�결
  // - 2�?미분방정?�을 1�??�립방정?�으�?변??
  // - ?�태벡터: [q, q?]ᵀ (?�치, ?�도)
  static int SolveMechanicalSystem(MechanicalSystem* system, float deltaTime) {
    (void)deltaTime;  // ?�재 버전?�서???�적 ?�태 ?�석�??�행, ?�중??룽게-쿠�?
                      // ?�분?�서 ?�용???�정
    if (!system || system->nodeCount == 0)
      return -1;
    if (system->nodeCount > kMaxMechanicalNodes ||
        system->edgeCount > kMaxMechanicalEdges) {
      LogFixedBufferLimitOnce("Mechanical system", system->nodeCount,
                              kMaxMechanicalNodes, system->edgeCount,
                              kMaxMechanicalEdges);
      return -1;
    }

    int dof = 6 * system->nodeCount;  // �??�드??6?�유??(x,y,z,θx,θy,θz)

    // ?�스???�렬 ?�데?�트
    UpdateSystemMatrices(system);

    // 룽게-쿠�? 4�??�분
    static float state[2 * kMaxMechanicalDof];  // [q, q?]
    static float k1[2 * kMaxMechanicalDof];
    static float k2[2 * kMaxMechanicalDof];
    static float k3[2 * kMaxMechanicalDof];
    static float k4[2 * kMaxMechanicalDof];
    static float temp_state[2 * kMaxMechanicalDof];

    // ?�재 ?�태 벡터 구성
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
    // ?�태 벡터�??�스?�에 ?�시 ?�??
    UnpackStateVector(system, state);

    // 메모�??�제
    // ?��??�이???�간 ?�데?�트
    system->currentTime += deltaTime;
    system->currentStep++;

    // ?�정??체크
    CheckSystemStability(system);

    return 0;
  }

 private:
  // ?�스???�렬 ?�데?�트 (M, C, K)
  // - 질량 ?�렬 M: �??�드??질량/관?�모멘트
  // - ?�핑 ?�렬 C: ?��????�핑 ?�성
  // - 강성 ?�렬 K: ?��????�프�??�성
  static void UpdateSystemMatrices(MechanicalSystem* system) {
    int total_dof = 6 * system->nodeCount;

    // ?�렬 초기??
    for (int i = 0; i < total_dof; i++) {
      for (int j = 0; j < total_dof; j++) {
        system->massMatrix[i][j] = 0.0f;
        system->dampingMatrix[i][j] = 0.0f;
        system->stiffnessMatrix[i][j] = 0.0f;
      }
    }
    // 질량 ?�렬 (?��??�렬)
    for (int n = 0; n < system->nodeCount; n++) {
      MechanicalNode* node = &system->nodes[n];
      int base_idx = 6 * n;

      // 병진 질량 (x, y, z)
      for (int i = 0; i < 3; i++) {
        system->massMatrix[base_idx + i][base_idx + i] = node->mass;
      }
    // ?�전 관??(θx, θy, θz)
      for (int i = 0; i < 3; i++) {
        system->massMatrix[base_idx + 3 + i][base_idx + 3 + i] =
            node->momentOfInertia[i];
      }
    }
    // ?�핑/강성 ?�렬 (?��? 기여�?
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

        // �??�유?�에 ?�???�프�??�퍼 ?�렬 ?�소 추�?
        for (int dof_idx = 0; dof_idx < 6; dof_idx++) {
          float k = edge->springConstant[dof_idx];
          float c = edge->dampingCoefficient[dof_idx];

          // ?��???�� (?�체 강성/?�핑)
          system->stiffnessMatrix[idx1 + dof_idx][idx1 + dof_idx] += k;
          system->stiffnessMatrix[idx2 + dof_idx][idx2 + dof_idx] += k;
          system->dampingMatrix[idx1 + dof_idx][idx1 + dof_idx] += c;
          system->dampingMatrix[idx2 + dof_idx][idx2 + dof_idx] += c;

          // 비�?�???�� (결합 강성/?�핑)
          system->stiffnessMatrix[idx1 + dof_idx][idx2 + dof_idx] -= k;
          system->stiffnessMatrix[idx2 + dof_idx][idx1 + dof_idx] -= k;
          system->dampingMatrix[idx1 + dof_idx][idx2 + dof_idx] -= c;
          system->dampingMatrix[idx2 + dof_idx][idx1 + dof_idx] -= c;
        }
      }
    }
  }
    // ?�태 벡터 구성 [q?? q?? ..., q??? q??? ...]
  // ?�치?� ?�도�??�나??벡터�??�킹
  static void PackStateVector(MechanicalSystem* system, float* state) {
    int dof = 6 * system->nodeCount;

    // ?�치 부�?(�?번째 ?�반)
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        state[base_idx + i] = node->position[i];
        state[base_idx + 3 + i] = node->angularPosition[i];
      }
    }
    // ?�도 부�?(??번째 ?�반)
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = dof + 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        state[base_idx + i] = node->velocity[i];
        state[base_idx + 3 + i] = node->angularVelocity[i];
      }
    }
  }
    // ?�태 벡터 ?�패??(?�스?�으�??�시 ?�??
  static void UnpackStateVector(MechanicalSystem* system, const float* state) {
    int dof = 6 * system->nodeCount;

    // ?�치 ?�데?�트
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->position[i] = state[base_idx + i];
        node->angularPosition[i] = state[base_idx + 3 + i];
      }
    }
    // ?�도 ?�데?�트
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
      derivative[i] = state[dof + i];  // ?�도 부분을 복사
    }
    // ?�력 벡터 ?�데?�트
    UpdateForceVector(system);

    // ?�시 벡터??
    static float Kq[kMaxMechanicalDof];  // K * q
    static float Cqdot[kMaxMechanicalDof];  // C * q?
    static float rhs[kMaxMechanicalDof];  // F - C*q? - K*q    // K * q 계산
    MathUtils::MatrixVectorMultiply(system->stiffnessMatrix, state, Kq, dof,
                                    dof);

    // C * q? 계산
    MathUtils::MatrixVectorMultiply(system->dampingMatrix, &state[dof], Cqdot,
                                    dof, dof);

    for (int i = 0; i < dof; i++) {
      rhs[i] = system->forceVector[i] - Cqdot[i] - Kq[i];
    }

    SolveMassMatrixSystem(system, rhs, &derivative[dof]);

  }
    // ?�력 벡터 ?�데?�트
  // �??�드???�용?�는 ?�력, 중력, 구속반력 ??
  static void UpdateForceVector(MechanicalSystem* system) {
    int dof = 6 * system->nodeCount;

    // 초기??
    for (int i = 0; i < dof; i++) {
      system->forceVector[i] = 0.0f;
    }
    // �??�드�??�력 �?중력
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      // ?�력 (병진 + ?�전)
      for (int i = 0; i < 3; i++) {
        system->forceVector[base_idx + i] = node->appliedForce[i];
        system->forceVector[base_idx + 3 + i] = node->appliedMoment[i];
      }
    // 중력 (질량 * 중력가?�도)
      for (int i = 0; i < 3; i++) {
        system->forceVector[base_idx + i] += node->mass * system->gravity[i];
      }
    }
    // ?��?�??�력 (?�프�? ?�퍼, ?�촉????
    for (int e = 0; e < system->edgeCount; e++) {
      if (!system->edges[e].base.isActive)
        continue;

      CalculateEdgeForces(system, &system->edges[e]);
    }
  }
    // ?��? ?�력 계산 (?�프링력, ?�핑?? ?�촉??
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

    // �??�유?�에 ?�???�력 계산
    for (int dof = 0; dof < 6; dof++) {
      float pos1 = (dof < 3) ? n1->position[dof] : n1->angularPosition[dof - 3];
      float pos2 = (dof < 3) ? n2->position[dof] : n2->angularPosition[dof - 3];
      float vel1 = (dof < 3) ? n1->velocity[dof] : n1->angularVelocity[dof - 3];
      float vel2 = (dof < 3) ? n2->velocity[dof] : n2->angularVelocity[dof - 3];

      // ?��? 변??�??�도
      float displacement = pos1 - pos2 - edge->naturalLength[dof];
      float relativeVelocity = vel1 - vel2;

      float springForce = -edge->springConstant[dof] * displacement;

      float damperForce = -edge->dampingCoefficient[dof] * relativeVelocity;

      // �??�력
      float totalForce = springForce + damperForce;

      // ?�턴 3법칙???�라 반�? 방향?�로 ?�용
      system->forceVector[idx1 + dof] += totalForce;
      system->forceVector[idx2 + dof] -= totalForce;

      // ?��? ?�태 ?�데?�트
      edge->displacement[dof] = displacement;
      edge->relativeVelocity[dof] = relativeVelocity;
      edge->springForce[dof] = springForce;
      edge->damperForce[dof] = damperForce;
      edge->totalForce[dof] = totalForce;
    }
  }
    // 질량 ?�렬???��??�렬?��?�?직접 ?�눗?�으�??�결 가??
  static void SolveMassMatrixSystem(MechanicalSystem* system, const float* rhs,
                                    float* acceleration) {
    int dof = 6 * system->nodeCount;

    for (int i = 0; i < dof; i++) {
      if (std::abs(system->massMatrix[i][i]) > 1e-12f) {
        acceleration[i] = rhs[i] / system->massMatrix[i][i];
      } else {
        acceleration[i] = 0.0f;  // 질량??0??경우 (?�이 케?�스)
      }
    }
    // 가?�도�??�드???�??
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->acceleration[i] = acceleration[base_idx + i];
        node->angularAcceleration[i] = acceleration[base_idx + 3 + i];
      }
    }
  }
    // ?�스???�정??체크
  // 발산?�는 ?��? 방�??�기 ?�한 ?�전?�치
  static void CheckSystemStability(MechanicalSystem* system) {
    system->isStable = true;

    for (int n = 0; n < system->nodeCount; n++) {
      MechanicalNode* node = &system->nodes[n];

      // ?�치/?�도가 비정?�적?�로 ?��? 체크
      for (int i = 0; i < 3; i++) {
        if (std::abs(node->position[i]) > 1e6f ||
            std::abs(node->velocity[i]) > 1e6f) {
          system->isStable = false;
          std::cout << "Warning: System instability detected at node " << n
                    << std::endl;

          // ?�급 조치: ?�도 ?�한
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


// C?�어 ?��????�수 ?�인?��? ?�한 ?�합 ?�터?�이??
// �??�버�??�립?�으�??�출?????�는 ?�일???�터?�이???�공
extern "C" {
// ?�기 ?�트?�크 ?�버 ?�터?�이??
int SolveElectricalNetworkC(plc::ElectricalNetwork* network, float deltaTime) {
  return plc::ElectricalSolver::SolveElectricalNetwork(network, deltaTime);
}
    // 공압 ?�트?�크 ?�버 ?�터?�이??
int SolvePneumaticNetworkC(plc::PneumaticNetwork* network, float deltaTime) {
  return plc::PneumaticSolver::SolvePneumaticNetwork(network, deltaTime);
}
    // 기계 ?�스???�버 ?�터?�이??
int SolveMechanicalSystemC(plc::MechanicalSystem* system, float deltaTime) {
  return plc::MechanicalSolver::SolveMechanicalSystem(system, deltaTime);
}
}
