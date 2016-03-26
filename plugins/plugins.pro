TEMPLATE = subdirs

# Directories
SUBDIRS += \
	Analyzer \
	Assembler \
	BinaryInfo \
	BinarySearcher \
	Bookmarks \
	ODbgRegisterView \
	BreakpointManager \
	CheckVersion \
	DebuggerCore \
	DumpState \
	FunctionFinder \
	HardwareBreakpoints \
	OpcodeSearcher \
	ProcessProperties \
	ROPTool \
	References \
	SymbolViewer \
    Backtrace

unix {
	!macx {
		SUBDIRS += HeapAnalyzer
	}
}

