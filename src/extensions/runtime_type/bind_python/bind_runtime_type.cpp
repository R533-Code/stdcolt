#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>
#include <nanobind/make_iterator.h>
#include <nanobind/eval.h>

#include <stdcolt_runtime_type/cpp/runtime_type.h>

void bind_python_bind_runtime_type(nanobind::module_& mod)
{
  namespace nb = nanobind;
  using namespace stdcolt::ext::rt;

  nb::class_<Any>(mod, "Any");
}
