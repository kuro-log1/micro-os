/* ============================================================
 * idt_asm.s — CPU 예외 스텁 (0~31번)
 *
 * CPU가 예외 발생 시 스택에 자동으로 push하는 것:
 *   [rflags][cs][rip]              ← 에러코드 없는 예외
 *   [rflags][cs][rip][error_code]  ← 에러코드 있는 예외
 *
 * ISR_NOERR: 에러코드 없는 예외 → 가짜 0 push
 * ISR_ERR  : 에러코드 있는 예외 → CPU가 이미 push
 * ============================================================ */

.macro ISR_NOERR num
.global isr\num
isr\num:
    pushq $0        /* 가짜 에러코드 */
    pushq $\num     /* 예외 번호 */
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr\num
isr\num:
    pushq $\num     /* 예외 번호 (에러코드는 CPU가 이미 push) */
    jmp isr_common
.endm

ISR_NOERR 0   /* Divide by Zero        */
ISR_NOERR 1   /* Debug                 */
ISR_NOERR 2   /* NMI                   */
ISR_NOERR 3   /* Breakpoint            */
ISR_NOERR 4   /* Overflow              */
ISR_NOERR 5   /* Bound Range           */
ISR_NOERR 6   /* Invalid Opcode        */
ISR_NOERR 7   /* Device Not Available  */
ISR_ERR   8   /* Double Fault          */
ISR_NOERR 9   /* Coprocessor Seg Overrun */
ISR_ERR   10  /* Invalid TSS           */
ISR_ERR   11  /* Segment Not Present   */
ISR_ERR   12  /* Stack Fault           */
ISR_ERR   13  /* General Protection    */
ISR_ERR   14  /* Page Fault            */
ISR_NOERR 15  /* Reserved              */
ISR_NOERR 16  /* x87 FP Exception      */
ISR_ERR   17  /* Alignment Check       */
ISR_NOERR 18  /* Machine Check         */
ISR_NOERR 19  /* SIMD FP               */
ISR_NOERR 20  /* Virtualization        */
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

/* ============================================================
 * 공통 핸들러: 레지스터 저장 → C 핸들러 호출 → 복원
 * ============================================================ */
.extern exception_handler

isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* rdi = 스택 포인터 (registers_t*) → exception_handler 첫 번째 인자 */
    movq %rsp, %rdi
    call exception_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp  /* int_no + err_code 스킵 */
    iretq

/* ============================================================
 * IRQ 스텁 (IRQ0-15 → 벡터 0x20-0x2F)
 * PIC 리매핑 후 하드웨어 인터럽트용
 * ============================================================ */
.macro IRQ_STUB irq, vec
.global irq\irq
irq\irq:
    pushq $0
    pushq $\vec
    jmp irq_common
.endm

IRQ_STUB 0,  32
IRQ_STUB 1,  33
IRQ_STUB 2,  34
IRQ_STUB 3,  35
IRQ_STUB 4,  36
IRQ_STUB 5,  37
IRQ_STUB 6,  38
IRQ_STUB 7,  39
IRQ_STUB 8,  40
IRQ_STUB 9,  41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

.extern irq_handler

irq_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call irq_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp
    iretq
