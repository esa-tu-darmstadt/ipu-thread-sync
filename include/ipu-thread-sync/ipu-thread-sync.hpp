#pragma once

#include <TileConstants.hpp>

/**
 * @namespace ipu
 * @brief Namespace containing functions and utilities for thread
 * synchronization on the IPU.
 */
namespace ipu {

/**
 * @brief Returns the thread ID of the current worker thread.
 * @return The thread ID of the current worker thread.
 */
[[clang::always_inline]] __attribute__((target("worker"))) inline unsigned
getWorkerID() {
  return __builtin_ipu_get(CSR_W_WSR__INDEX) & (CSR_W_WSR__CTXTID_M1__MASK);
}

/**
 * @brief Gets the vertex base pointer and casts it to a reference to the given
 * vertex type.
 * @tparam vertex_t The type of the vertex.
 * @return A reference to the vertex of type vertex_t.
 */
template <typename vertex_t>
[[clang::always_inline]] __attribute__((target("worker"))) inline vertex_t &
getVertexPtr() {
  return *static_cast<vertex_t *>(__builtin_ipu_get_vertex_base());
}

namespace detail {
/**
 * @brief Entry point function for a worker thread.
 * Sets up the worker thread's stack and calls the given member function on the
 * vertex.
 * @tparam vertex_t The type of the vertex.
 * @tparam func The member function to be called on the vertex.
 * @return True if the member function returns true, false otherwise.
 */
template <typename vertex_t, bool (vertex_t::*func)(unsigned)>
__attribute__((target("worker"))) __attribute__((colossus_vertex)) bool
workerThreadEntryPoint() {
  vertex_t &vertex = getVertexPtr<vertex_t>();
  unsigned workerId = getWorkerID();
  return (vertex.*func)(workerId);
}
}  // namespace detail

/**
 * @brief Starts the given member function on all worker threads on the current
 * tile.
 * @tparam vertex_t The type of the vertex.
 * @tparam func The member function to be started on all worker threads.
 * @param vertex A pointer to the vertex object.
 */
template <typename vertex_t, bool (vertex_t::*func)(unsigned)>
[[clang::always_inline]] __attribute__((target("supervisor"))) inline void
startOnAllWorkers(vertex_t *vertex) {
  void *vertexBase = reinterpret_cast<void *>(vertex);
  void *entryPoint =
      reinterpret_cast<void *>(&detail::workerThreadEntryPoint<vertex_t, func>);

  __asm__("runall %[func], %[vertex_base], 0\n\t"
          :
          : [func] "r"(entryPoint), [vertex_base] "r"(vertexBase)
          :);
}

/**
 * @brief Syncs all worker threads on the current tile and then starts the given
 * member function on all worker threads.
 * @tparam vertex_t The type of the vertex.
 * @tparam func The member function to be started on all worker threads.
 * @param vertex A pointer to the vertex object.
 */
template <typename vertex_t, bool (vertex_t::*func)(unsigned)>
[[clang::always_inline]] __attribute__((target("supervisor"))) inline void
syncAndStartOnAllWorkers(vertex_t *vertex) {
  void *vertexBase = reinterpret_cast<void *>(vertex);
  void *entryPoint =
      reinterpret_cast<void *>(&detail::workerThreadEntryPoint<vertex_t, func>);

  __asm__(
      "sync %[group]\n\t"
      "runall %[func], %[vertex_base], 0\n\t"
      :
      : [group] "i"(TEXCH_SYNCZONE_LOCAL), [func] "r"(entryPoint),
        [vertex_base] "r"(vertexBase)
      :);
}

/**
 * @brief Syncs all worker threads on the current tile.
 */
[[clang::always_inline]] __attribute__((target("supervisor"))) inline void
syncAllWorkers() {
  __asm__("sync %[group]\n\t" : : [group] "i"(TEXCH_SYNCZONE_LOCAL) :);
}

/**
 * @brief Enum class representing patched breakpoint values.
 */
enum class PatchedBreakpoint : char { PBRK0 = 0, PBRK1 = 1 };

/**
 * @brief Triggers a patched breakpoint exception on the current tile.
 * @tparam breakpoint The patched breakpoint value to trigger.
 */
template <PatchedBreakpoint breakpoint = PatchedBreakpoint::PBRK0>
[[clang::always_inline]] [[noreturn]] void trap() {
  __asm__("trap %[breakpoint]\n\t" : : [breakpoint] "i"(breakpoint) :);
  __builtin_unreachable();
}

/**
 * @brief Returns from the current worker thread.
 * This is only needed if the thread's entry point function does not have the
 * __attribute__((colossus_vertex)) attribute.
 */
[[clang::always_inline]] [[noreturn]] __attribute__((
    target("worker"))) inline void
returnFromWorker() {
  __asm__("exitz $mzero");
  __builtin_unreachable();
}

/**
 * @brief Defines the attributes required by a worker thread member function.
 */
#define WORKER_FUNC [[clang::always_inline]] __attribute__((target("worker")))

/**
 * @brief Defines the attributes required by a supervisor thread member
 * function.
 */
#define SUPERVISOR_FUNC \
  [[clang::always_inline]] __attribute__((target("supervisor")))

}  // namespace ipu
