#include <caml/alloc.h>
#include <caml/mlvalues.h>

#include "engine/api.h"

CAMLprim value caml_engine_run(value unit) {
  (void)unit;
  return Val_int(engine_run());
}

CAMLprim value caml_engine_version(value unit) {
  (void)unit;
  return caml_copy_string(engine_version());
}
