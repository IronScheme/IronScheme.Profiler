extern EnterStub:proc
extern LeaveStub:proc
extern TailcallStub:proc


_TEXT segment para 'CODE'

;typedef void FunctionEnterNaked(
;         rcx = FunctionID funcId, 
;         rdx = UINT_PTR clientData, 
;         r8  = COR_PRF_FRAME_INFO func, 
;         r9  = COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo);

        align   16

        public  FunctionEnterNaked

FunctionEnterNaked    proc    frame

        ; save registers
        push    rax
        .allocstack 8

        push    r10
        .allocstack 8

        push    r11
        .allocstack 8

        sub     rsp, 20h
        .allocstack 20h

        .endprolog

        call    EnterStub

        add     rsp, 20h

        ; restore registers
        pop     r11
        pop     r10
        pop     rax

        ; return
        ret

FunctionEnterNaked    endp

;typedef void FunctionLeaveNaked(
;         rcx =  FunctionID funcId, 
;         rdx =  UINT_PTR clientData, 
;         r8  =  COR_PRF_FRAME_INFO func, 
;         r9  =  COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange);

        align   16

        public  FunctionLeaveNaked

FunctionLeaveNaked    proc    frame

        ; save integer return register
        push    rax
        .allocstack 8

        sub     rsp, 20h
        .allocstack 20h

        .endprolog

        call    LeaveStub

        add     rsp, 20h

        ; restore integer return register
        pop     rax

        ; return
        ret

FunctionLeaveNaked    endp

;typedef void FunctionTailcallNaked(
;         rcx =  FunctionID funcId, 
;         rdx =  UINT_PTR clientData, 
;         t8  =  COR_PRF_FRAME_INFO,

        align   16

        public  FunctionTailcallNaked

FunctionTailcallNaked   proc    frame

        ; save rax
        push    rax
        .allocstack 8

        sub     rsp, 20h
        .allocstack 20h

        .endprolog

        call    TailcallStub

        add     rsp, 20h

        ; restore rax
        pop     rax

        ; return
        ret

FunctionTailcallNaked   endp

_TEXT ends

end