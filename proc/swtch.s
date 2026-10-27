/* ============================================================
 * swtch(uint64_t *old_rsp_out, uint64_t new_rsp)
 *
 * 컨텍스트 스위치 핵심 루틴.
 * callee-saved 레지스터(rbp, rbx, r12-r15)와 rsp만 교환한다.
 * 나머지 레지스터는 호출 규약상 caller가 보존 책임.
 *
 * rdi = &prev->rsp  (이전 프로세스 rsp 저장 위치)
 * rsi =  next->rsp  (다음 프로세스 rsp 값)
 * ============================================================ */
.global swtch
swtch:
    /* 현재 프로세스의 callee-saved 레지스터 저장 */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)   /* prev->rsp = 현재 rsp */
    movq %rsi, %rsp     /* rsp = next->rsp */

    /* 다음 프로세스의 callee-saved 레지스터 복원 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    /* 새 프로세스가 멈췄던 위치(또는 entry 함수)로 점프 */
    ret

/* ============================================================
 * fork_return — fork로 생성된 자식 프로세스의 첫 실행 진입점.
 *
 * swtch가 callee-saved 6개를 pop하고 ret으로 여기에 도달하면
 * RSP는 복사해둔 ring3 syscall 프레임(registers_t)의 r15를 가리킨다.
 * 레지스터를 복원하고 iretq로 ring3에 복귀한다.
 * ============================================================ */
.global fork_return
fork_return:
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
    addq $16, %rsp   /* int_no + err_code 스킵 */
    iretq
