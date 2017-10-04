PUBLIC long_double_to_double

ALIAS <@long_double_to_double@8> = <long_double_to_double>

IFDEF EDB_X86

.386

_TEXT SEGMENT
long_double_to_double PROC NEAR; FASTCALL
fld TBYTE PTR [ecx]
fstp QWORD PTR [edx]
ret
long_double_to_double ENDP
_TEXT ENDS

ELSE

_TEXT SEGMENT
long_double_to_double PROC
fld TBYTE PTR [rcx]
fstp QWORD PTR [rdx]
ret
long_double_to_double ENDP
_TEXT ENDS

ENDIF

END
