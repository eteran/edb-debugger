cmake_minimum_required(VERSION 3.1)

add_library(ELF INTERFACE
)

target_include_directories(ELF
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(ELF INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_model.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_auxv.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_binary.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_dyn.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_header.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_move.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_nhdr.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_phdr.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_rela.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_rel.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_shdr.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_sym.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_syminfo.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_verdaux.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_verdef.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_vernaux.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libELF/elf_verneed.h
)

