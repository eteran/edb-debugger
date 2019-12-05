PUBLIC long_double_to_double
PUBLIC float64_to_float80

ALIAS <@long_double_to_double@8> = <long_double_to_double>
ALIAS <@float64_to_float80@8> = <float64_to_float80>

IFDEF EDB_X86

.386

_TEXT SEGMENT

long_double_to_double PROC NEAR; FASTCALL
fld TBYTE PTR [ecx]
fstp QWORD PTR [edx]
ret
long_double_to_double ENDP

float64_to_float80 PROC NEAR; FASTCALL
fld QWORD PTR [ecx]
fstp TBYTE PTR [edx]
ret
float64_to_float80 ENDP

_TEXT ENDS

ELSE

_TEXT SEGMENT

long_double_to_double PROC
fld TBYTE PTR [rcx]
fstp QWORD PTR [rdx]
ret
long_double_to_double ENDP

float64_to_float80 PROC
fld QWORD PTR [rcx]
fstp TBYTE PTR [rdx]
ret
float64_to_float80 ENDP

_TEXT ENDS

ENDIF

END
