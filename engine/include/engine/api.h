#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* C API surface for OCaml (and other language) FFI. */
int engine_run(void);
const char *engine_version(void);

#ifdef __cplusplus
}
#endif
