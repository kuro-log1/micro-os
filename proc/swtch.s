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
