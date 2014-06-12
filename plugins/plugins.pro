TEMPLATE = subdirs

# Directories
SUBDIRS += \
	Analyzer \
	Assembler \
	BinaryInfo \
	BinarySearcher \
	Bookmarks \
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
	SymbolViewer

unix {
	!macx {
		SUBDIRS += HeapAnalyzer
	}
}

