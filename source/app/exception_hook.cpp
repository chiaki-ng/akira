#include "app/crash_report.hpp"

#include <format>

asm(R"(
    .section .text.__libnx_exception_entry, "ax", %progbits
    .global __libnx_exception_entry
    .type __libnx_exception_entry, %function
    .align 2
__libnx_exception_entry:
    cbz x1, 2f
    adrp x2, __nx_exceptiondump
    add x2, x2, #:lo12:__nx_exceptiondump
    mov x5, x2
    str w0, [x2], #4
    str wzr, [x2], #4
    str wzr, [x2], #4
    str wzr, [x2], #4
    ldp x3, x4, [x1]
    str x5, [x1], #16
    stp x3, x4, [x2], #16
    ldp x3, x4, [x1], #16
    stp x3, x4, [x2], #16
    ldp x3, x4, [x1], #16
    stp x3, x4, [x2], #16
    ldp x3, x4, [x1], #16
    stp x3, x4, [x2], #16
    ldr x3, [x1], #8
    str x3, [x2], #8
    str x9, [x2], #8
    stp x10, x11, [x2], #16
    stp x12, x13, [x2], #16
    stp x14, x15, [x2], #16
    stp x16, x17, [x2], #16
    stp x18, x19, [x2], #16
    stp x20, x21, [x2], #16
    stp x22, x23, [x2], #16
    stp x24, x25, [x2], #16
    stp x26, x27, [x2], #16
    str x28, [x2], #8
    str x29, [x2], #8
    ldr x3, [x1], #8
    str x3, [x2], #8
    adrp x4, __nx_exception_stack
    add x4, x4, #:lo12:__nx_exception_stack
    adrp x5, __nx_exception_stack_size
    ldr x5, [x5, #:lo12:__nx_exception_stack_size]
    add x4, x4, x5
    ldr x3, [x1]
    str x4, [x1], #8
    str x3, [x2], #8
    adrp x4, akira_exception_return
    add x4, x4, #:lo12:akira_exception_return
    ldr x3, [x1]
    str x4, [x1], #8
    str x3, [x2], #8
    str xzr, [x2], #8
    stp q0, q1, [x2], #32
    stp q2, q3, [x2], #32
    stp q4, q5, [x2], #32
    stp q6, q7, [x2], #32
    stp q8, q9, [x2], #32
    stp q10, q11, [x2], #32
    stp q12, q13, [x2], #32
    stp q14, q15, [x2], #32
    stp q16, q17, [x2], #32
    stp q18, q19, [x2], #32
    stp q20, q21, [x2], #32
    stp q22, q23, [x2], #32
    stp q24, q25, [x2], #32
    stp q26, q27, [x2], #32
    stp q28, q29, [x2], #32
    stp q30, q31, [x2], #32
    ldr w3, [x1], #4
    str w3, [x2], #4
    ldr w3, [x1], #4
    str w3, [x2], #4
    ldr w3, [x1], #4
    str w3, [x2], #4
    ldr w3, [x1], #4
    str w3, [x2], #4
    ldr x3, [x1], #8
    str x3, [x2], #8
    mov w0, wzr
    b 3f
2:
    mov w0, #0xf801
3:
    bl svcReturnFromException
    b .
    .size __libnx_exception_entry, . - __libnx_exception_entry

    .section .text.akira_exception_return, "ax", %progbits
    .type akira_exception_return, %function
    .align 2
akira_exception_return:
    bl __libnx_exception_handler
    mov w0, wzr
    mov x1, #0
    mov x2, #0
    bl svcBreak
    b .
    .size akira_exception_return, . - akira_exception_return
    .text
)");

extern "C" {

alignas(16) u8 __nx_exception_stack[0x10000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __real_abort();
void __real___assert_func(const char* file, int line, const char* function, const char* expression);
void __real_fatalThrow(Result result);
void __real_fatalThrowWithPolicy(Result result, FatalPolicy policy);

void __libnx_exception_handler(ThreadExceptionDump* dump)
{
    akira::app::CrashReport::onException(*dump);
}

void diagAbortWithResult(Result result)
{
    akira::app::CrashReport::onResult("diag abort", result, reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    svcBreak(BreakReason_Panic, reinterpret_cast<uintptr_t>(&result), sizeof(result));
    __builtin_unreachable();
}

void __wrap_fatalThrow(Result result)
{
    akira::app::CrashReport::onResult("fatal", result, reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real_fatalThrow(result);
    __builtin_unreachable();
}

void __wrap_fatalThrowWithPolicy(Result result, FatalPolicy policy)
{
    akira::app::CrashReport::onResult("fatal", result, reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real_fatalThrowWithPolicy(result, policy);
    __builtin_unreachable();
}

void __wrap_abort()
{
    akira::app::CrashReport::onAbort("abort", "", reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real_abort();
    __builtin_unreachable();
}

void __wrap___assert_func(const char* file, int line, const char* function, const char* expression)
{
    std::array<char, 384> detail{};
    const auto out = std::format_to_n(detail.data(), detail.size() - 1, "assert {} at {}:{} in {}", expression ? expression : "?",
        file ? file : "?", line, function ? function : "?");
    akira::app::CrashReport::onAbort("assert", {detail.data(), std::min<std::size_t>(static_cast<std::size_t>(out.size), detail.size() - 1)},
        reinterpret_cast<std::uint64_t>(__builtin_return_address(0)), reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real___assert_func(file, line, function, expression);
    __builtin_unreachable();
}

}
