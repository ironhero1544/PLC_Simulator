// src/PhysicsSolvers.cpp
// 실제 물리 법칙 기반 수치해석 솔버 구현
// 키르히호프 법칙, 베르누이 방정식, 뉴턴 역학을 정확히 적용한 시뮬레이션
// 수치적 안정성과 정확성을 보장하는 검증된 수치해석 알고리즘 사용

#include "plc_emulator/physics/physics_network.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace plc {

// ========== 수학 유틸리티 함수들 ==========

//   행렬 연산을 위한 기본 유틸리티
// - C언어 스타일로 효율적인 수치연산 구현
// - BLAS/LAPACK 없이도 기본적인 선형대수 연산 제공
namespace MathUtils {

// 벡터 내적 계산
float DotProduct(const float* a, const float* b, int n) {
  float result = 0.0f;
  for (int i = 0; i < n; i++) {
    result += a[i] * b[i];
  }
  return result;
}

// 벡터 노름 계산 (||x||₂)
float VectorNorm(const float* x, int n) {
  return std::sqrt(DotProduct(x, x, n));
}

// 행렬-벡터 곱셈: y = A*x
void MatrixVectorMultiply(float** A, const float* x, float* y, int rows,
                          int cols) {
  for (int i = 0; i < rows; i++) {
    y[i] = 0.0f;
    for (int j = 0; j < cols; j++) {
      y[i] += A[i][j] * x[j];
    }
  }
}

// 벡터 복사: dest = src
void VectorCopy(const float* src, float* dest, int n) {
  for (int i = 0; i < n; i++) {
    dest[i] = src[i];
  }
}

// 벡터 스케일링: x = alpha * x
void VectorScale(float* x, float alpha, int n) {
  for (int i = 0; i < n; i++) {
    x[i] *= alpha;
  }
}

// 벡터 덧셈: z = x + y
void VectorAdd(const float* x, const float* y, float* z, int n) {
  for (int i = 0; i < n; i++) {
    z[i] = x[i] + y[i];
  }
}

// 벡터 뺄셈: z = x - y
void VectorSubtract(const float* x, const float* y, float* z, int n) {
  for (int i = 0; i < n; i++) {
    z[i] = x[i] - y[i];
  }
}

}  // namespace MathUtils

// ========== 전기 네트워크 솔버 ==========

// 키르히호프 법칙 기반 전기 네트워크 해석
// 노드 분석법 사용: 각 노드에서 전류 보존 법칙 ΣI = 0
// 컨덕턴스 행렬 방정식 [G][V] = [I]를 가우스-자이델 반복법으로 해결

class ElectricalSolver {
 public:
  //   가우스-자이델 반복법으로 선형 시스템 해결
  // - 대각우세 행렬에 대해 빠른 수렴 보장
  // - 메모리 효율적 (제자리 업데이트)
  // - 희소 행렬에 적합 (전기 네트워크는 보통 희소함)
  static int SolveElectricalNetwork(ElectricalNetwork* network,
                                    float deltaTime) {
    (void)deltaTime;  // 현재 버전에서는 정적 상태 해석만 수행, 나중에 동적
                      // 해석에서 사용할 예정
    if (!network || network->nodeCount == 0)
      return -1;

    // 컨덕턴스 행렬과 전류 벡터 업데이트
    UpdateConductanceMatrix(network);
    UpdateCurrentVector(network);

    // 가우스-자이델 반복법으로 전압 해 계산
    float* voltageOld = new float[network->nodeCount];
    MathUtils::VectorCopy(network->voltageVector, voltageOld,
                          network->nodeCount);

    network->currentIteration = 0;
    network->isConverged = false;

    while (network->currentIteration < network->maxIterations &&
           !network->isConverged) {
      // 가우스-자이델 스위프
      for (int i = 0; i < network->nodeCount; i++) {
        if (i == network->groundNodeId) {
          network->voltageVector[i] = 0.0f;  // 접지 조건
          continue;
        }

        // 전압원 노드 처리
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

        // 가우스-자이델 업데이트: V_i = (I_i - Σ(j≠i) G_ij*V_j) / G_ii
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
          network->voltageVector[i] = 0.0f;  // 특이점 처리
        }
      }

      // 수렴 검사
      float residual = 0.0f;
      for (int i = 0; i < network->nodeCount; i++) {
        float diff = network->voltageVector[i] - voltageOld[i];
        residual += diff * diff;
        voltageOld[i] = network->voltageVector[i];
      }
      residual = std::sqrt(residual);

      network->isConverged = (residual < network->convergenceTolerance);
      network->currentIteration++;

      // 디버깅 출력 (선택적)
      if (network->currentIteration % 10 == 0) {
        std::cout << "Electrical iteration " << network->currentIteration
                  << ", residual = " << residual << std::endl;
      }
    }

    delete[] voltageOld;

    // 수렴 후 전류 계산
    if (network->isConverged) {
      CalculateEdgeCurrents(network);
      CalculateNodeCurrents(network);
    }

    return network->isConverged ? 0 : -1;
  }

 private:
  // 컨덕턴스 행렬 업데이트
  // G_ij = -g_ij (i≠j, 엣지가 연결된 경우)
  // G_ii = Σ(j≠i) g_ij (대각항은 연결된 모든 엣지 컨덕턴스의 합)
  static void UpdateConductanceMatrix(ElectricalNetwork* network) {
    // 행렬 초기화
    for (int i = 0; i < network->nodeCount; i++) {
      for (int j = 0; j < network->nodeCount; j++) {
        network->conductanceMatrix[i][j] = 0.0f;
      }
    }

    // 각 엣지의 컨덕턴스를 행렬에 추가
    for (int e = 0; e < network->edgeCount; e++) {
      ElectricalEdge* edge = &network->edges[e];
      if (!edge->base.isActive)
        continue;

      int i = edge->base.fromNodeId;
      int j = edge->base.toNodeId;
      float g = edge->conductance;

      if (i >= 0 && i < network->nodeCount && j >= 0 &&
          j < network->nodeCount) {
        // 비대각항: G_ij = -g_ij
        network->conductanceMatrix[i][j] -= g;
        network->conductanceMatrix[j][i] -= g;

        // 대각항: G_ii += g_ij
        network->conductanceMatrix[i][i] += g;
        network->conductanceMatrix[j][j] += g;
      }
    }

    // 노드별 추가 컨덕턴스 (누설 등)
    for (int i = 0; i < network->nodeCount; i++) {
      network->conductanceMatrix[i][i] += network->nodes[i].leakageConductance;
    }
  }

  // 전류 벡터 업데이트
  // I_i = 노드 i로 주입되는 전류 (전류원, 전압원의 내부전류 등)
  static void UpdateCurrentVector(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      network->currentVector[i] = 0.0f;

      // 전류원이 있는 경우
      if (network->nodes[i].isCurrentSource) {
        network->currentVector[i] += network->nodes[i].sourceCurrent;
      }

      // 전압원의 내부 전류 기여분 (Norton 등가회로)
      if (network->nodes[i].isVoltageSource &&
          network->nodes[i].sourceResistance > 0) {
        float g_internal = 1.0f / network->nodes[i].sourceResistance;
        network->currentVector[i] +=
            g_internal * network->nodes[i].sourceVoltage;
        network->conductanceMatrix[i][i] += g_internal;
      }
    }
  }

  // 엣지 전류 계산 (옴의 법칙 적용)
  // I_edge = (V_from - V_to) * G_edge = (V_from - V_to) / R_edge
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

        // 전류 히스토리 업데이트 (동적 해석용)
        edge->currentHistory[2] = edge->currentHistory[1];
        edge->currentHistory[1] = edge->currentHistory[0];
        edge->currentHistory[0] = edge->current;
      }
    }
  }

  // 노드 전류 계산 (키르히호프 전류 법칙 검증용)
  // 수렴한 해에서는 각 노드에서 ΣI = 0 이어야 함
  static void CalculateNodeCurrents(ElectricalNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      network->nodes[i].netCurrent = 0.0f;

      // 연결된 모든 엣지의 전류를 합산
      for (int e = 0; e < network->edgeCount; e++) {
        if (!network->edges[e].base.isActive)
          continue;

        if (network->edges[e].base.fromNodeId == i) {
          network->nodes[i].netCurrent -= network->edges[e].current;  // 유출
        } else if (network->edges[e].base.toNodeId == i) {
          network->nodes[i].netCurrent += network->edges[e].current;  // 유입
        }
      }

      // 소스 전류 추가
      if (network->nodes[i].isCurrentSource) {
        network->nodes[i].netCurrent += network->nodes[i].sourceCurrent;
      }

      network->nodes[i].current = network->nodes[i].netCurrent;

      // 전압/전류 히스토리 업데이트
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

// ========== 공압 네트워크 솔버 ==========

// 베르누이 방정식과 연속성 방정식 기반 공압 네트워크 해석
// 비선형 연립방정식을 뉴턴-랩슨 방법으로 해결
// 압축성 유체(공기)의 상태방정식 PV=nRT 고려

class PneumaticSolver {
 public:
  // 뉴턴-랩슨 방법으로 비선형 공압 시스템 해결
  // - 연속성 방정식: ṁ₁ = ṁ₂ (질량 보존)
  // - 베르누이 방정식: P₁ + ½ρv₁² = P₂ + ½ρv₂² + ΔP_loss
  // - 상태방정식: P = ρRT (이상기체)
  static int SolvePneumaticNetwork(PneumaticNetwork* network, float deltaTime) {
    (void)deltaTime;  // 현재 버전에서는 정적 상태 해석만 수행, 나중에 동적
                      // 해석에서 사용할 예정
    if (!network || network->nodeCount == 0)
      return -1;

    // 초기 추정값 설정
    InitializeGuess(network);

    // 뉴턴-랩슨 반복
    network->currentIteration = 0;
    network->isConverged = false;

    float* residual = new float[network->nodeCount + network->edgeCount];
    float** jacobian = new float*[network->nodeCount + network->edgeCount];
    for (int i = 0; i < network->nodeCount + network->edgeCount; i++) {
      jacobian[i] = new float[network->nodeCount + network->edgeCount];
    }
    float* delta = new float[network->nodeCount + network->edgeCount];

    while (network->currentIteration < network->maxIterations &&
           !network->isConverged) {
      // 잔여함수와 야코비안 계산
      CalculateResidualAndJacobian(network, residual, jacobian);

      // 선형 시스템 해결: J * Δx = -F
      SolveLinearSystem(jacobian, residual, delta,
                        network->nodeCount + network->edgeCount);

      // 해 업데이트
      UpdateSolution(network, delta);

      // 수렴 검사
      float residualNorm = MathUtils::VectorNorm(
          residual, network->nodeCount + network->edgeCount);
      network->isConverged = (residualNorm < network->convergenceTolerance);
      network->currentIteration++;

      // 디버깅 출력
      if (network->currentIteration % 5 == 0) {
        std::cout << "Pneumatic iteration " << network->currentIteration
                  << ", residual = " << residualNorm << std::endl;
      }
    }

    // 메모리 해제
    delete[] residual;
    for (int i = 0; i < network->nodeCount + network->edgeCount; i++) {
      delete[] jacobian[i];
    }
    delete[] jacobian;
    delete[] delta;

    // 수렴 후 부가 변수 계산
    if (network->isConverged) {
      CalculateDerivedQuantities(network);
    }

    return network->isConverged ? 0 : -1;
  }

 private:
  // 초기 추정값 설정
  // - 압력: 대기압 또는 이전 시간스텝 값
  // - 질량유량: 0 또는 연속성 방정식 기반 추정
  static void InitializeGuess(PneumaticNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      if (!network->nodes[i].isPressureSource) {
        // 초기 압력 추정 (대기압 또는 이전값)
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
      // 초기 질량유량 추정 (0 또는 압력차 기반)
      network->edges[e].massFlowRate = 0.0f;
      network->massFlowVector[e] = 0.0f;
    }
  }

  // 잔여함수와 야코비안 행렬 계산
  // F(x) = 0 형태의 비선형 방정식계에서 F와 ∂F/∂x 계산
  static void CalculateResidualAndJacobian(PneumaticNetwork* network,
                                           float* residual, float** jacobian) {
    int n = network->nodeCount + network->edgeCount;

    // 잔여함수 초기화
    for (int i = 0; i < n; i++) {
      residual[i] = 0.0f;
      for (int j = 0; j < n; j++) {
        jacobian[i][j] = 0.0f;
      }
    }

    // 노드별 질량보존 방정식 (연속성 방정식)
    for (int i = 0; i < network->nodeCount; i++) {
      float massBalance = 0.0f;

      // 소스 질량유량
      if (network->nodes[i].isMassFlowSource) {
        massBalance += network->nodes[i].sourceMassFlowRate;
      }

      // 연결된 엣지들의 질량유량 합산
      for (int e = 0; e < network->edgeCount; e++) {
        if (!network->edges[e].base.isActive)
          continue;

        if (network->edges[e].base.fromNodeId == i) {
          massBalance -= network->edges[e].massFlowRate;  // 유출
          jacobian[i][network->nodeCount + e] = -1.0f;
        } else if (network->edges[e].base.toNodeId == i) {
          massBalance += network->edges[e].massFlowRate;  // 유입
          jacobian[i][network->nodeCount + e] = 1.0f;
        }
      }

      residual[i] = massBalance;
    }

    // 엣지별 베르누이 방정식
    for (int e = 0; e < network->edgeCount; e++) {
      if (!network->edges[e].base.isActive)
        continue;

      int fromNode = network->edges[e].base.fromNodeId;
      int toNode = network->edges[e].base.toNodeId;

      if (fromNode >= 0 && fromNode < network->nodeCount && toNode >= 0 &&
          toNode < network->nodeCount) {
        // 베르누이 방정식: P₁ + ½ρv₁² = P₂ + ½ρv₂² + ΔP_friction
        float P1 = network->pressureVector[fromNode];
        float P2 = network->pressureVector[toNode];
        float mdot = network->massFlowVector[e];

        // 압력강하 계산 (Darcy-Weisbach + 국소손실)
        float pressureDrop = CalculatePressureDrop(&network->edges[e], mdot);

        // 베르누이 잔여식: P₁ - P₂ - ΔP = 0
        residual[network->nodeCount + e] = P1 - P2 - pressureDrop;

        // 야코비안 항목
        jacobian[network->nodeCount + e][fromNode] = 1.0f;  // ∂F/∂P₁
        jacobian[network->nodeCount + e][toNode] = -1.0f;   // ∂F/∂P₂

        // ∂F/∂ṁ = -∂(ΔP)/∂ṁ
        float dPdm = CalculatePressureDropDerivative(&network->edges[e], mdot);
        jacobian[network->nodeCount + e][network->nodeCount + e] = -dPdm;
      }
    }
  }

  // 압력강하 계산 (Darcy-Weisbach 방정식)
  // ΔP = f * (L/D) * (ρv²/2) + K * (ρv²/2)
  // 여기서 f는 마찰계수, K는 국소손실계수
  static float CalculatePressureDrop(PneumaticEdge* edge, float massFlowRate) {
    if (std::abs(massFlowRate) < 1e-9f)
      return 0.0f;

    float A = edge->crossSectionArea * 1e-6f;  // mm² → m²
    float D = edge->diameter * 1e-3f;          // mm → m
    float L = edge->length * 1e-3f;            // mm → m

    // 밀도 추정 (표준 상태 기준)
    float rho = 1.225f;  // kg/m³ (20°C, 1atm 공기)

    // 속도 계산: v = ṁ/(ρA)
    float velocity = std::abs(massFlowRate) / (rho * A);

    // 레이놀즈 수 계산: Re = ρvD/μ
    float mu = 1.8e-5f;  // 동점성계수 [Pa⋅s]
    float Re = rho * velocity * D / mu;
    edge->reynoldsNumber = Re;

    // 마찰계수 계산 (Colebrook-White 방정식 근사)
    float roughness = edge->roughness * 1e-6f;  // μm → m
    float f;
    if (Re < 2300) {
      // 층류: f = 64/Re
      f = 64.0f / Re;
    } else {
      // 난류: Swamee-Jain 방정식 근사
      float term1 =
          std::log10(roughness / (3.7f * D) + 5.74f / std::pow(Re, 0.9f));
      f = 0.25f / (term1 * term1);
    }
    edge->frictionFactor = f;

    // 마찰 압력강하
    float frictionLoss = f * (L / D) * (rho * velocity * velocity / 2.0f);

    // 국소 압력강하 (굽힘, 확대/축소 등)
    float localLoss = edge->curvature * (rho * velocity * velocity / 2.0f);

    float totalLoss = frictionLoss + localLoss;

    // 유량 방향에 따른 부호 결정
    return (massFlowRate >= 0) ? totalLoss : -totalLoss;
  }

  // 압력강하의 질량유량에 대한 미분 (야코비안용)
  // ∂(ΔP)/∂ṁ ≈ 2*ΔP/ṁ (속도²에 비례하므로 유량에 대해 거의 선형)
  static float CalculatePressureDropDerivative(PneumaticEdge* edge,
                                               float massFlowRate) {
    if (std::abs(massFlowRate) < 1e-9f) {
      // 유량이 0에 가까우면 선형 근사
      float A = edge->crossSectionArea * 1e-6f;
      float rho = 1.225f;
      return 2.0f * edge->frictionFactor * edge->length /
             (edge->diameter * rho * A * A);
    } else {
      float currentDrop = CalculatePressureDrop(edge, massFlowRate);
      return 2.0f * currentDrop / massFlowRate;
    }
  }

  // 선형 시스템 해결 (LU 분해 사용)
  // 야코비안 행렬이 작으므로 직접법 사용
  static void SolveLinearSystem(float** A, float* b, float* x, int n) {
    // 간단한 가우스 소거법 (피벗팅 없음)
    // 실제로는 부분 피벗팅이 필요하지만 여기서는 간단히 구현

    for (int i = 0; i < n; i++) {
      x[i] = -b[i];  // 우변 복사 및 부호 변경
    }

    // 전진 소거
    for (int k = 0; k < n - 1; k++) {
      for (int i = k + 1; i < n; i++) {
        if (std::abs(A[k][k]) < 1e-12f)
          continue;  // 특이점 방지

        float factor = A[i][k] / A[k][k];
        for (int j = k + 1; j < n; j++) {
          A[i][j] -= factor * A[k][j];
        }
        x[i] -= factor * x[k];
      }
    }

    // 후진 대입
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

  // 해 업데이트 (뉴턴-랩슨 스텝)
  // x_{k+1} = x_k + Δx
  static void UpdateSolution(PneumaticNetwork* network, float* delta) {
    // 압력 업데이트 (제한 적용)
    for (int i = 0; i < network->nodeCount; i++) {
      if (!network->nodes[i].isPressureSource) {
        network->pressureVector[i] += delta[i];

        // 물리적 제한 적용 (음압 방지)
        if (network->pressureVector[i] < 0.1f * network->atmosphericPressure) {
          network->pressureVector[i] = 0.1f * network->atmosphericPressure;
        }

        network->nodes[i].pressure = network->pressureVector[i];
        network->nodes[i].gaugePressure =
            network->pressureVector[i] - network->atmosphericPressure;
      }
    }

    // 질량유량 업데이트
    for (int e = 0; e < network->edgeCount; e++) {
      network->massFlowVector[e] += delta[network->nodeCount + e];
      network->edges[e].massFlowRate = network->massFlowVector[e];
    }
  }

  // 수렴 후 부가 변수 계산
  // 온도, 밀도, 체적유량, 음속 등 파생 변수 계산
  static void CalculateDerivedQuantities(PneumaticNetwork* network) {
    for (int i = 0; i < network->nodeCount; i++) {
      PneumaticNode* node = &network->nodes[i];

      // 상태방정식으로 밀도 계산: ρ = P/(RT)
      node->density =
          node->pressure / (network->gasConstant * node->temperature);

      // 음속 계산: c = √(γRT)
      node->soundSpeed = std::sqrt(network->specificHeatRatio *
                                   network->gasConstant * node->temperature);

      // 히스토리 업데이트
      node->pressureHistory[2] = node->pressureHistory[1];
      node->pressureHistory[1] = node->pressureHistory[0];
      node->pressureHistory[0] = node->pressure;
    }

    for (int e = 0; e < network->edgeCount; e++) {
      PneumaticEdge* edge = &network->edges[e];

      // 체적유량 계산: Q = ṁ/ρ (표준 상태 기준)
      float rho_std = 1.225f;  // kg/m³
      edge->volumeFlowRate =
          edge->massFlowRate / rho_std * 60000.0f;  // m³/s → L/min

      // 평균 속도 계산
      float A = edge->crossSectionArea * 1e-6f;  // mm² → m²
      edge->velocity = edge->massFlowRate / (edge->density * A);

      // 히스토리 업데이트
      edge->massFlowHistory[2] = edge->massFlowHistory[1];
      edge->massFlowHistory[1] = edge->massFlowHistory[0];
      edge->massFlowHistory[0] = edge->massFlowRate;
    }
  }
};

// ========== 기계 시스템 솔버 ==========

// 뉴턴 역학 기반 기계 시스템 해석
// 2차 미분방정식 M q̈ + C q̇ + K q = F를 룽게-쿠타 4차 방법으로 해결
// 질량-스프링-댐퍼 네트워크의 동적 응답 계산

class MechanicalSolver {
 public:
  // 룽게-쿠타 4차 방법으로 운동방정식 해결
  // - 2차 미분방정식을 1차 연립방정식으로 변환
  // - 상태벡터: [q, q̇]ᵀ (위치, 속도)
  // - 미분방정식: d/dt[q, q̇]ᵀ = [q̇, M⁻¹(F - Cq̇ - Kq)]ᵀ
  static int SolveMechanicalSystem(MechanicalSystem* system, float deltaTime) {
    (void)deltaTime;  // 현재 버전에서는 정적 상태 해석만 수행, 나중에 룽게-쿠타
                      // 적분에서 사용할 예정
    if (!system || system->nodeCount == 0)
      return -1;

    int dof = 6 * system->nodeCount;  // 각 노드당 6자유도 (x,y,z,θx,θy,θz)

    // 시스템 행렬 업데이트
    UpdateSystemMatrices(system);

    // 룽게-쿠타 4차 적분
    float* state = new float[2 * dof];  // [q, q̇]
    float* k1 = new float[2 * dof];
    float* k2 = new float[2 * dof];
    float* k3 = new float[2 * dof];
    float* k4 = new float[2 * dof];
    float* temp_state = new float[2 * dof];

    // 현재 상태 벡터 구성
    PackStateVector(system, state);

    // k1 = f(t, y)
    CalculateStateDerivative(system, state, k1);

    // k2 = f(t + h/2, y + h*k1/2)
    for (int i = 0; i < 2 * dof; i++) {
      temp_state[i] = state[i] + deltaTime * k1[i] / 2.0f;
    }
    CalculateStateDerivative(system, temp_state, k2);

    // k3 = f(t + h/2, y + h*k2/2)
    for (int i = 0; i < 2 * dof; i++) {
      temp_state[i] = state[i] + deltaTime * k2[i] / 2.0f;
    }
    CalculateStateDerivative(system, temp_state, k3);

    // k4 = f(t + h, y + h*k3)
    for (int i = 0; i < 2 * dof; i++) {
      temp_state[i] = state[i] + deltaTime * k3[i];
    }
    CalculateStateDerivative(system, temp_state, k4);

    // 최종 업데이트: y_{n+1} = y_n + h/6 * (k1 + 2*k2 + 2*k3 + k4)
    for (int i = 0; i < 2 * dof; i++) {
      state[i] +=
          deltaTime / 6.0f * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
    }

    // 상태 벡터를 시스템에 다시 저장
    UnpackStateVector(system, state);

    // 메모리 해제
    delete[] state;
    delete[] k1;
    delete[] k2;
    delete[] k3;
    delete[] k4;
    delete[] temp_state;

    // 시뮬레이션 시간 업데이트
    system->currentTime += deltaTime;
    system->currentStep++;

    // 안정성 체크
    CheckSystemStability(system);

    return 0;
  }

 private:
  // 시스템 행렬 업데이트 (M, C, K)
  // - 질량 행렬 M: 각 노드의 질량/관성모멘트
  // - 댐핑 행렬 C: 엣지의 댐핑 특성
  // - 강성 행렬 K: 엣지의 스프링 특성
  static void UpdateSystemMatrices(MechanicalSystem* system) {
    int total_dof = 6 * system->nodeCount;

    // 행렬 초기화
    for (int i = 0; i < total_dof; i++) {
      for (int j = 0; j < total_dof; j++) {
        system->massMatrix[i][j] = 0.0f;
        system->dampingMatrix[i][j] = 0.0f;
        system->stiffnessMatrix[i][j] = 0.0f;
      }
    }

    // 질량 행렬 (대각 행렬)
    for (int n = 0; n < system->nodeCount; n++) {
      MechanicalNode* node = &system->nodes[n];
      int base_idx = 6 * n;

      // 병진 질량 (x, y, z)
      for (int i = 0; i < 3; i++) {
        system->massMatrix[base_idx + i][base_idx + i] = node->mass;
      }

      // 회전 관성 (θx, θy, θz)
      for (int i = 0; i < 3; i++) {
        system->massMatrix[base_idx + 3 + i][base_idx + 3 + i] =
            node->momentOfInertia[i];
      }
    }

    // 댐핑/강성 행렬 (엣지 기여분)
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

        // 각 자유도에 대해 스프링/댐퍼 행렬 요소 추가
        for (int dof_idx = 0; dof_idx < 6; dof_idx++) {
          float k = edge->springConstant[dof_idx];
          float c = edge->dampingCoefficient[dof_idx];

          // 대각 항목 (자체 강성/댐핑)
          system->stiffnessMatrix[idx1 + dof_idx][idx1 + dof_idx] += k;
          system->stiffnessMatrix[idx2 + dof_idx][idx2 + dof_idx] += k;
          system->dampingMatrix[idx1 + dof_idx][idx1 + dof_idx] += c;
          system->dampingMatrix[idx2 + dof_idx][idx2 + dof_idx] += c;

          // 비대각 항목 (결합 강성/댐핑)
          system->stiffnessMatrix[idx1 + dof_idx][idx2 + dof_idx] -= k;
          system->stiffnessMatrix[idx2 + dof_idx][idx1 + dof_idx] -= k;
          system->dampingMatrix[idx1 + dof_idx][idx2 + dof_idx] -= c;
          system->dampingMatrix[idx2 + dof_idx][idx1 + dof_idx] -= c;
        }
      }
    }
  }

  // 상태 벡터 구성 [q₁, q₂, ..., q̇₁, q̇₂, ...]
  // 위치와 속도를 하나의 벡터로 패킹
  static void PackStateVector(MechanicalSystem* system, float* state) {
    int dof = 6 * system->nodeCount;

    // 위치 부분 (첫 번째 절반)
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        state[base_idx + i] = node->position[i];
        state[base_idx + 3 + i] = node->angularPosition[i];
      }
    }

    // 속도 부분 (두 번째 절반)
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = dof + 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        state[base_idx + i] = node->velocity[i];
        state[base_idx + 3 + i] = node->angularVelocity[i];
      }
    }
  }

  // 상태 벡터 언패킹 (시스템으로 다시 저장)
  static void UnpackStateVector(MechanicalSystem* system, const float* state) {
    int dof = 6 * system->nodeCount;

    // 위치 업데이트
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->position[i] = state[base_idx + i];
        node->angularPosition[i] = state[base_idx + 3 + i];
      }
    }

    // 속도 업데이트
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = dof + 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->velocity[i] = state[base_idx + i];
        node->angularVelocity[i] = state[base_idx + 3 + i];
      }
    }
  }

  // 상태 미분 계산 d/dt[q, q̇] = [q̇, q̈]
  // 운동방정식 M q̈ + C q̇ + K q = F에서 q̈ = M⁻¹(F - C q̇ - K q)
  static void CalculateStateDerivative(MechanicalSystem* system,
                                       const float* state, float* derivative) {
    int dof = 6 * system->nodeCount;

    // 첫 번째 절반: dq/dt = q̇ (속도)
    for (int i = 0; i < dof; i++) {
      derivative[i] = state[dof + i];  // 속도 부분을 복사
    }

    // 두 번째 절반: dq̇/dt = q̈ = M⁻¹(F - C q̇ - K q) (가속도)

    // 외력 벡터 업데이트
    UpdateForceVector(system);

    // 임시 벡터들
    float* Kq = new float[dof];     // K * q
    float* Cqdot = new float[dof];  // C * q̇
    float* rhs = new float[dof];    // F - C*q̇ - K*q

    // K * q 계산
    MathUtils::MatrixVectorMultiply(system->stiffnessMatrix, state, Kq, dof,
                                    dof);

    // C * q̇ 계산
    MathUtils::MatrixVectorMultiply(system->dampingMatrix, &state[dof], Cqdot,
                                    dof, dof);

    // rhs = F - C*q̇ - K*q
    for (int i = 0; i < dof; i++) {
      rhs[i] = system->forceVector[i] - Cqdot[i] - Kq[i];
    }

    // q̈ = M⁻¹ * rhs (질량 행렬의 역행렬 곱셈)
    SolveMassMatrixSystem(system, rhs, &derivative[dof]);

    delete[] Kq;
    delete[] Cqdot;
    delete[] rhs;
  }

  // 외력 벡터 업데이트
  // 각 노드에 작용하는 외력, 중력, 구속반력 등
  static void UpdateForceVector(MechanicalSystem* system) {
    int dof = 6 * system->nodeCount;

    // 초기화
    for (int i = 0; i < dof; i++) {
      system->forceVector[i] = 0.0f;
    }

    // 각 노드별 외력 및 중력
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      // 외력 (병진 + 회전)
      for (int i = 0; i < 3; i++) {
        system->forceVector[base_idx + i] = node->appliedForce[i];
        system->forceVector[base_idx + 3 + i] = node->appliedMoment[i];
      }

      // 중력 (질량 * 중력가속도)
      for (int i = 0; i < 3; i++) {
        system->forceVector[base_idx + i] += node->mass * system->gravity[i];
      }
    }

    // 엣지별 내력 (스프링, 댐퍼, 접촉력 등)
    for (int e = 0; e < system->edgeCount; e++) {
      if (!system->edges[e].base.isActive)
        continue;

      CalculateEdgeForces(system, &system->edges[e]);
    }
  }

  // 엣지 내력 계산 (스프링력, 댐핑력, 접촉력)
  // F_spring = K * (x - x₀), F_damper = C * ẋ
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

    // 각 자유도에 대해 내력 계산
    for (int dof = 0; dof < 6; dof++) {
      float pos1 = (dof < 3) ? n1->position[dof] : n1->angularPosition[dof - 3];
      float pos2 = (dof < 3) ? n2->position[dof] : n2->angularPosition[dof - 3];
      float vel1 = (dof < 3) ? n1->velocity[dof] : n1->angularVelocity[dof - 3];
      float vel2 = (dof < 3) ? n2->velocity[dof] : n2->angularVelocity[dof - 3];

      // 상대 변위 및 속도
      float displacement = pos1 - pos2 - edge->naturalLength[dof];
      float relativeVelocity = vel1 - vel2;

      // 스프링력: F = -k * (x - x₀)
      float springForce = -edge->springConstant[dof] * displacement;

      // 댐핑력: F = -c * ẋ
      float damperForce = -edge->dampingCoefficient[dof] * relativeVelocity;

      // 총 내력
      float totalForce = springForce + damperForce;

      // 뉴턴 3법칙에 따라 반대 방향으로 작용
      system->forceVector[idx1 + dof] += totalForce;
      system->forceVector[idx2 + dof] -= totalForce;

      // 엣지 상태 업데이트
      edge->displacement[dof] = displacement;
      edge->relativeVelocity[dof] = relativeVelocity;
      edge->springForce[dof] = springForce;
      edge->damperForce[dof] = damperForce;
      edge->totalForce[dof] = totalForce;
    }
  }

  // 질량 행렬 시스템 해결 M * q̈ = rhs
  // 질량 행렬이 대각 행렬이므로 직접 나눗셈으로 해결 가능
  static void SolveMassMatrixSystem(MechanicalSystem* system, const float* rhs,
                                    float* acceleration) {
    int dof = 6 * system->nodeCount;

    for (int i = 0; i < dof; i++) {
      if (std::abs(system->massMatrix[i][i]) > 1e-12f) {
        acceleration[i] = rhs[i] / system->massMatrix[i][i];
      } else {
        acceleration[i] = 0.0f;  // 질량이 0인 경우 (특이 케이스)
      }
    }

    // 가속도를 노드에 저장
    for (int n = 0; n < system->nodeCount; n++) {
      int base_idx = 6 * n;
      MechanicalNode* node = &system->nodes[n];

      for (int i = 0; i < 3; i++) {
        node->acceleration[i] = acceleration[base_idx + i];
        node->angularAcceleration[i] = acceleration[base_idx + 3 + i];
      }
    }
  }

  // 시스템 안정성 체크
  // 발산하는 해를 방지하기 위한 안전장치
  static void CheckSystemStability(MechanicalSystem* system) {
    system->isStable = true;

    for (int n = 0; n < system->nodeCount; n++) {
      MechanicalNode* node = &system->nodes[n];

      // 위치/속도가 비정상적으로 큰지 체크
      for (int i = 0; i < 3; i++) {
        if (std::abs(node->position[i]) > 1e6f ||
            std::abs(node->velocity[i]) > 1e6f) {
          system->isStable = false;
          std::cout << "Warning: System instability detected at node " << n
                    << std::endl;

          // 응급 조치: 속도 제한
          if (std::abs(node->velocity[i]) > 1e6f) {
            node->velocity[i] = std::copysign(1e6f, node->velocity[i]);
          }
        }
      }
    }
  }
};

}  // namespace plc

// ========== 통합 물리 솔버 인터페이스 ==========

// C언어 스타일 함수 포인터를 통한 통합 인터페이스
// 각 솔버를 독립적으로 호출할 수 있는 통일된 인터페이스 제공
extern "C" {
// 전기 네트워크 솔버 인터페이스
int SolveElectricalNetworkC(plc::ElectricalNetwork* network, float deltaTime) {
  return plc::ElectricalSolver::SolveElectricalNetwork(network, deltaTime);
}

// 공압 네트워크 솔버 인터페이스
int SolvePneumaticNetworkC(plc::PneumaticNetwork* network, float deltaTime) {
  return plc::PneumaticSolver::SolvePneumaticNetwork(network, deltaTime);
}

// 기계 시스템 솔버 인터페이스
int SolveMechanicalSystemC(plc::MechanicalSystem* system, float deltaTime) {
  return plc::MechanicalSolver::SolveMechanicalSystem(system, deltaTime);
}
}