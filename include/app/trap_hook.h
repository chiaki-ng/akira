#pragma once

#ifdef __cplusplus
extern "C" {
#endif
__attribute__((noreturn)) void akira_trap(void);
#ifdef __cplusplus
}
#endif

#define __builtin_trap() akira_trap()
