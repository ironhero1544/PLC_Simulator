#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_APPLICATION_PHYSICS_PHYSICS_TASK_RUNNER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_APPLICATION_PHYSICS_PHYSICS_TASK_RUNNER_H_

#include <cstddef>

namespace plc {

using ComponentTaskFn = void (*)(size_t index, void* context);

void RunComponentTasks(size_t count, ComponentTaskFn fn, void* context);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_APPLICATION_PHYSICS_PHYSICS_TASK_RUNNER_H_ */
