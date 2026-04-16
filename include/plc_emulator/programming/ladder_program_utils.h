/*
 * ladder_program_utils.h
 *
 * Shared ladder structure normalization helpers.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_LADDER_PROGRAM_UTILS_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_LADDER_PROGRAM_UTILS_H_

#include "plc_emulator/programming/programming_mode.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace plc {

inline void CanonicalizeVerticalConnections(
    std::vector<VerticalConnection>* connections, int max_rung_index) {
  if (!connections) {
    return;
  }

  std::vector<VerticalConnection> components;
  components.reserve(connections->size());
  for (auto connection : *connections) {
    std::vector<int> filtered;
    filtered.reserve(connection.rungs.size());
    for (int rung_index : connection.rungs) {
      if (rung_index >= 0 && rung_index <= max_rung_index) {
        filtered.push_back(rung_index);
      }
    }
    std::sort(filtered.begin(), filtered.end());
    filtered.erase(std::unique(filtered.begin(), filtered.end()),
                   filtered.end());
    if (filtered.size() < 2) {
      continue;
    }

    size_t segment_start = 0;
    for (size_t i = 1; i <= filtered.size(); ++i) {
      if (i < filtered.size() &&
          filtered[i] == filtered[i - 1] + 1) {
        continue;
      }

      if (i - segment_start >= 2) {
        VerticalConnection component;
        component.x = connection.x;
        component.rungs.assign(filtered.begin() + segment_start,
                               filtered.begin() + i);
        components.push_back(component);
      }
      segment_start = i;
    }
  }

  std::sort(components.begin(), components.end(),
            [](const VerticalConnection& lhs, const VerticalConnection& rhs) {
              if (lhs.x != rhs.x) {
                return lhs.x < rhs.x;
              }
              const int lhs_start =
                  lhs.rungs.empty() ? std::numeric_limits<int>::max()
                                    : lhs.rungs.front();
              const int rhs_start =
                  rhs.rungs.empty() ? std::numeric_limits<int>::max()
                                    : rhs.rungs.front();
              if (lhs_start != rhs_start) {
                return lhs_start < rhs_start;
              }
              const int lhs_end =
                  lhs.rungs.empty() ? std::numeric_limits<int>::min()
                                    : lhs.rungs.back();
              const int rhs_end =
                  rhs.rungs.empty() ? std::numeric_limits<int>::min()
                                    : rhs.rungs.back();
              if (lhs_end != rhs_end) {
                return lhs_end < rhs_end;
              }
              return lhs.rungs.size() < rhs.rungs.size();
            });

  std::vector<VerticalConnection> merged;
  merged.reserve(components.size());
  for (const auto& component : components) {
    if (!merged.empty() && merged.back().x == component.x &&
        !merged.back().rungs.empty() && !component.rungs.empty() &&
        component.rungs.front() <= merged.back().rungs.back()) {
      const int merged_start = merged.back().rungs.front();
      const int merged_end =
          std::max(merged.back().rungs.back(), component.rungs.back());
      merged.back().rungs.clear();
      for (int rung = merged_start; rung <= merged_end; ++rung) {
        merged.back().rungs.push_back(rung);
      }
      continue;
    }

    merged.push_back(component);
  }

  std::sort(merged.begin(), merged.end(),
            [](const VerticalConnection& lhs, const VerticalConnection& rhs) {
              const int lhs_start =
                  lhs.rungs.empty() ? std::numeric_limits<int>::max()
                                    : lhs.rungs.front();
              const int rhs_start =
                  rhs.rungs.empty() ? std::numeric_limits<int>::max()
                                    : rhs.rungs.front();
              if (lhs_start != rhs_start) {
                return lhs_start < rhs_start;
              }
              if (lhs.x != rhs.x) {
                return lhs.x < rhs.x;
              }
              const int lhs_end =
                  lhs.rungs.empty() ? std::numeric_limits<int>::min()
                                    : lhs.rungs.back();
              const int rhs_end =
                  rhs.rungs.empty() ? std::numeric_limits<int>::min()
                                    : rhs.rungs.back();
              if (lhs_end != rhs_end) {
                return lhs_end < rhs_end;
              }
              return lhs.rungs.size() < rhs.rungs.size();
            });
  connections->swap(merged);
}

inline void CanonicalizeLadderProgram(LadderProgram* program) {
  if (!program) {
    return;
  }

  std::vector<Rung> ordered_rungs;
  ordered_rungs.reserve(program->rungs.size() + 1);

  for (const auto& rung : program->rungs) {
    if (rung.isEndRung) {
      continue;
    }
    ordered_rungs.push_back(rung);
  }

  for (size_t i = 0; i < ordered_rungs.size(); ++i) {
    ordered_rungs[i].number = static_cast<int>(i);
    ordered_rungs[i].isEndRung = false;
  }

  Rung end_rung;
  end_rung.isEndRung = true;
  end_rung.number = static_cast<int>(ordered_rungs.size());
  ordered_rungs.push_back(end_rung);
  program->rungs.swap(ordered_rungs);

  CanonicalizeVerticalConnections(
      &program->verticalConnections,
      static_cast<int>(program->rungs.size()) - 2);
}

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_LADDER_PROGRAM_UTILS_H_
